#include <doctest/doctest.h>

#include "core/probe_state.hpp"
#include "core/sensor_pipeline.hpp"

using core::DeciC;
using core::SensorPipeline;
using core::TcFault;

TEST_CASE("Sensor pipeline calibrates each sample and uses a sliding median window") {
    SensorPipeline pipeline{{.calibration_offset = {10}}};
    pipeline.feed({{-15}, DeciC{250}, TcFault::None}, 0);
    auto out = pipeline.output(0);
    // -15 + 10 = -5 tenths. A good reading at time zero is already valid.
    CHECK(out.bt == DeciC{-5});
    CHECK(out.fault == TcFault::None);
    CHECK_FALSE(out.stale);
    CHECK(out.bt_updated_ms == 0);
    REQUIRE(out.cold_junction.has_value());
    CHECK(*out.cold_junction == DeciC{250});

    pipeline.feed({{-12}, DeciC{260}, TcFault::None}, 250);
    out = pipeline.output(250);
    // -12 + 10 = -2. Mean of -5 and -2 = -3.5, rounded away from zero to -4.
    CHECK(out.bt == DeciC{-4});
    CHECK(out.bt_updated_ms == 250);
    REQUIRE(out.cold_junction.has_value());
    CHECK(*out.cold_junction == DeciC{260});

    pipeline.feed({{1000}, std::nullopt, TcFault::None}, 500);
    // The calibrated window [-5, -2, 1010] has median -2.
    CHECK(pipeline.output(500).bt == DeciC{-2});
    CHECK_FALSE(pipeline.output(500).cold_junction.has_value());

    pipeline.feed({{10}, std::nullopt, TcFault::None}, 750);
    // The newest sample replaces -5. The window [20, -2, 1010] has median 20.
    CHECK(pipeline.output(750).bt == DeciC{20});
}

TEST_CASE("Sensor pipeline saturates calibrated samples before averaging") {
    SensorPipeline high{{.calibration_offset = {10}}};
    high.feed({{32765}, std::nullopt, TcFault::None}, 0);
    high.feed({{32749}, std::nullopt, TcFault::None}, 250);
    // 32775 saturates to 32767. Mean of 32767 and 32759 is 32763.
    CHECK(high.output(250).bt == DeciC{32763});

    SensorPipeline low{{.calibration_offset = {-10}}};
    low.feed({{-32765}, std::nullopt, TcFault::None}, 0);
    low.feed({{-32750}, std::nullopt, TcFault::None}, 250);
    // -32775 saturates to -32768. Mean of -32768 and -32760 is -32764.
    CHECK(low.output(250).bt == DeciC{-32764});
}

TEST_CASE("Sensor pipeline rate limits in both directions using elapsed milliseconds") {
    SensorPipeline pipeline{{}};
    pipeline.feed({{0}, std::nullopt, TcFault::None}, 100);
    pipeline.feed({{1000}, std::nullopt, TcFault::None}, 350);
    // Target 500. At 500 tenths/s for 250 ms the output can rise by 125.
    CHECK(pipeline.output(350).bt == DeciC{125});

    pipeline.feed({{-1000}, std::nullopt, TcFault::None}, 451);
    // Target 0 (median). At 500 tenths/s for 101 ms it can fall by 50, to 75.
    CHECK(pipeline.output(451).bt == DeciC{75});

    pipeline.feed({{-1000}, std::nullopt, TcFault::None}, 451);
    // Zero elapsed time still permits a one-tenth step toward -1000.
    CHECK(pipeline.output(451).bt == DeciC{74});

    pipeline.feed({{-1000}, std::nullopt, TcFault::None}, 10'000'451);
    // 10,000 s allows 5,000,000 tenths, enough to reach -1000 even though
    // the rate times milliseconds intermediate exceeds the int32_t range.
    CHECK(pipeline.output(10'000'451).bt == DeciC{-1000});
}

TEST_CASE("Sensor pipeline starts stale and measures freshness across clock rollover") {
    SensorPipeline pipeline{{}};
    const auto initial = pipeline.output(0);
    CHECK(initial.bt == DeciC{0});
    CHECK(initial.fault == TcFault::BusError);
    CHECK(initial.stale);
    CHECK(initial.bt_updated_ms == 0);
    CHECK_FALSE(initial.cold_junction.has_value());
    pipeline.feed({{0}, std::nullopt, TcFault::BusError}, 1);
    CHECK(pipeline.output(1).stale);
    CHECK(pipeline.output(1).fault == TcFault::BusError);

    pipeline.feed({{100}, std::nullopt, TcFault::None}, UINT32_MAX - 99);
    pipeline.feed({{1000}, std::nullopt, TcFault::None}, 150);
    // 100 ms to wrap plus 150 ms afterward permits a rise of 125 toward 550.
    CHECK(pipeline.output(150).bt == DeciC{225});
    CHECK_FALSE(pipeline.output(2150).stale);  // Exactly 2000 ms after the last good sample.
    CHECK(pipeline.output(2151).stale);

    SensorPipeline stale_across_wrap{{}};
    stale_across_wrap.feed({{100}, std::nullopt, TcFault::None}, UINT32_MAX - 99);
    // 100 ms to wrap plus 1900 ms afterward is exactly the stale boundary.
    CHECK_FALSE(stale_across_wrap.output(1900).stale);
    CHECK(stale_across_wrap.output(1901).stale);
}

TEST_CASE("Sensor pipeline reports unplugging after 500 ms and recovers on the first good sample") {
    SensorPipeline pipeline{{}};
    pipeline.feed({{500}, DeciC{250}, TcFault::None}, 0);
    pipeline.feed({{500}, DeciC{250}, TcFault::None}, 250);
    pipeline.feed({{500}, DeciC{250}, TcFault::None}, 500);
    pipeline.feed({{0}, DeciC{300}, TcFault::Open}, 750);
    pipeline.feed({{0}, DeciC{300}, TcFault::Open}, 1000);
    const auto pending = pipeline.output(1000);
    // Two faults leave the last good output intact, including its age and junction reading.
    CHECK(pending.fault == TcFault::None);
    CHECK(pending.bt == DeciC{500});
    CHECK_FALSE(pending.stale);
    CHECK(pending.bt_updated_ms == 500);
    REQUIRE(pending.cold_junction.has_value());
    CHECK(*pending.cold_junction == DeciC{250});

    pipeline.feed({{0}, DeciC{300}, TcFault::Open}, 1250);
    // 1250 - 750 = 500 ms from the first Open sample.
    CHECK(pipeline.output(1250).fault == TcFault::Open);
    CHECK(pipeline.output(1250).bt_updated_ms == 500);
    CHECK_FALSE(pipeline.output(1250).stale);
    CHECK(pipeline.output(2501).stale);

    pipeline.feed({{2500}, DeciC{310}, TcFault::None}, 2750);
    // The first good sample publishes 2500 immediately. A 2250 ms rate limit
    // against the old output would only allow a rise of 1125 tenths.
    const auto recovered = pipeline.output(2750);
    CHECK(recovered.fault == TcFault::None);
    CHECK_FALSE(recovered.stale);
    CHECK(recovered.bt == DeciC{2500});
    CHECK(recovered.bt_updated_ms == 2750);
    REQUIRE(recovered.cold_junction.has_value());
    CHECK(*recovered.cold_junction == DeciC{310});
}

TEST_CASE("Sensor pipeline ignores one bus glitch and resets its debounce on a good sample") {
    SensorPipeline pipeline{{.fault_debounce = 2}};
    pipeline.feed({{100}, std::nullopt, TcFault::None}, 0);
    pipeline.feed({{0}, std::nullopt, TcFault::BusError}, 250);
    CHECK(pipeline.output(250).fault == TcFault::None);
    pipeline.feed({{102}, std::nullopt, TcFault::None}, 500);
    // The glitch stays out of the window. Mean of 100 and 102 is 101.
    CHECK(pipeline.output(500).bt == DeciC{101});
    CHECK(pipeline.output(500).fault == TcFault::None);
    pipeline.feed({{0}, std::nullopt, TcFault::Open}, 750);
    CHECK(pipeline.output(750).fault == TcFault::None);
    pipeline.feed({{0}, std::nullopt, TcFault::BusError}, 1000);
    // Both faulty samples count, and the second sample determines the reported fault.
    CHECK(pipeline.output(1000).fault == TcFault::BusError);
}

TEST_CASE("Sensor pipeline fault count saturates and still reports the latest fault") {
    SensorPipeline pipeline{{.fault_debounce = UINT8_MAX}};
    pipeline.feed({{100}, std::nullopt, TcFault::None}, 0);
    for (uint32_t sample = 1; sample < 255; ++sample) {
        pipeline.feed({{0}, std::nullopt, TcFault::Open}, sample * 250);
    }
    // 254 faults are below the configured threshold of 255.
    CHECK(pipeline.output(254 * 250).fault == TcFault::None);
    pipeline.feed({{0}, std::nullopt, TcFault::Open}, 255 * 250);
    CHECK(pipeline.output(255 * 250).fault == TcFault::Open);
    pipeline.feed({{0}, std::nullopt, TcFault::BusError}, 256 * 250);
    // The 256th fault keeps the counter at 255 and updates the reported cause.
    CHECK(pipeline.output(256 * 250).fault == TcFault::BusError);
}
