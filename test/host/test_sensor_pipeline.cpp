#include <doctest/doctest.h>

#include "core/probe_state.hpp"
#include "core/sensor_pipeline.hpp"

using core::DeciC;
using core::SensorPipeline;
using core::TcFault;

// Every expected value below is computed by hand in the comment next to it.

TEST_CASE("Sensor pipeline publishes the calibrated median once three good samples agree") {
    SensorPipeline pipeline{{.calibration_offset = {10}}};
    pipeline.feed({{-15}, DeciC{250}, TcFault::None}, 0);
    auto out = pipeline.output(0);
    // One sample is not enough: still "no reading yet", but the cold junction is already known.
    CHECK(out.fault == TcFault::BusError);
    CHECK(out.stale);
    REQUIRE(out.cold_junction.has_value());
    CHECK(*out.cold_junction == DeciC{250});

    pipeline.feed({{-12}, DeciC{260}, TcFault::None}, 250);
    CHECK(pipeline.output(250).fault == TcFault::BusError);
    CHECK(pipeline.output(250).stale);

    pipeline.feed({{-10}, std::nullopt, TcFault::None}, 500);
    out = pipeline.output(500);
    // Calibrated window [-15+10, -12+10, -10+10] = [-5, -2, 0]: spread 5 <= 50, median -2.
    CHECK(out.fault == TcFault::None);
    CHECK_FALSE(out.stale);
    CHECK(out.bt == DeciC{-2});
    CHECK(out.bt_updated_ms == 500);
    CHECK_FALSE(out.cold_junction.has_value());

    pipeline.feed({{1000}, std::nullopt, TcFault::None}, 750);
    // 1010 replaces the oldest (-5): [1010, -2, 0], median 0. The rate limit allows
    // 500 * 250 / 1000 = 125 from -2, so 0 passes unchanged and the spike never shows.
    CHECK(pipeline.output(750).bt == DeciC{0});
}

TEST_CASE("Sensor pipeline saturates calibrated samples") {
    SensorPipeline high{{.calibration_offset = {10}}};
    high.feed({{32765}, std::nullopt, TcFault::None}, 0);
    high.feed({{32760}, std::nullopt, TcFault::None}, 0);
    high.feed({{32750}, std::nullopt, TcFault::None}, 0);
    // 32775 and 32770 saturate to 32767; 32760 stays. Median of [32767, 32767, 32760] = 32767.
    // Without saturation 32775 would wrap to a large negative value.
    CHECK(high.output(0).bt == DeciC{32767});

    SensorPipeline low{{.calibration_offset = {-10}}};
    low.feed({{-32765}, std::nullopt, TcFault::None}, 0);
    low.feed({{-32762}, std::nullopt, TcFault::None}, 0);
    low.feed({{-32750}, std::nullopt, TcFault::None}, 0);
    // -32775 and -32772 saturate to -32768; -32760 stays. Median = -32768.
    CHECK(low.output(0).bt == DeciC{-32768});
}

TEST_CASE("Sensor pipeline rate limits in both directions using elapsed milliseconds") {
    SensorPipeline pipeline{{}};
    pipeline.feed({{0}, std::nullopt, TcFault::None}, 0);
    pipeline.feed({{0}, std::nullopt, TcFault::None}, 100);
    pipeline.feed({{0}, std::nullopt, TcFault::None}, 200);
    CHECK(pipeline.output(200).bt == DeciC{0});  // First output: median 0, no rate limit.

    pipeline.feed({{1000}, std::nullopt, TcFault::None}, 450);
    // [1000, 0, 0], median 0: a single jump never passes the median.
    CHECK(pipeline.output(450).bt == DeciC{0});

    pipeline.feed({{1000}, std::nullopt, TcFault::None}, 700);
    // [1000, 1000, 0], median 1000. 250 ms at 500 tenths/s allows +125.
    CHECK(pipeline.output(700).bt == DeciC{125});

    pipeline.feed({{-1000}, std::nullopt, TcFault::None}, 801);
    // [1000, 1000, -1000], median 1000. 101 ms allows 500 * 101 / 1000 = 50 (truncated): 175.
    CHECK(pipeline.output(801).bt == DeciC{175});

    pipeline.feed({{-1000}, std::nullopt, TcFault::None}, 1001);
    // [-1000, 1000, -1000], median -1000. 200 ms allows -100: 75.
    CHECK(pipeline.output(1001).bt == DeciC{75});

    pipeline.feed({{-1000}, std::nullopt, TcFault::None}, 1001);
    // [-1000, -1000, -1000]. Zero elapsed time still allows a one-tenth step: 74.
    CHECK(pipeline.output(1001).bt == DeciC{74});

    pipeline.feed({{-1000}, std::nullopt, TcFault::None}, 3001);
    // 2000 ms later is still live (exactly the stale limit) and within the gap limit, so the
    // window is kept. The largest step the limit ever allows: 500 * 2000 / 1000 = 1000,
    // 74 - 1000 = -926. (A longer gap restarts the window instead; see the gap test.)
    CHECK(pipeline.output(3001).bt == DeciC{-926});
}

TEST_CASE("Sensor pipeline starts stale and measures time across clock rollover") {
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

    // 2^32 - 350, 2^32 - 600 and 2^32 - 850 written as UINT32_MAX - 349 and so on.
    pipeline.feed({{100}, std::nullopt, TcFault::None}, UINT32_MAX - 849);
    pipeline.feed({{100}, std::nullopt, TcFault::None}, UINT32_MAX - 599);
    pipeline.feed({{100}, std::nullopt, TcFault::None}, UINT32_MAX - 349);
    CHECK(pipeline.output(UINT32_MAX - 349).bt == DeciC{100});
    pipeline.feed({{1000}, std::nullopt, TcFault::None}, UINT32_MAX - 99);
    // [1000, 100, 100], median 100.
    CHECK(pipeline.output(UINT32_MAX - 99).bt == DeciC{100});
    pipeline.feed({{1000}, std::nullopt, TcFault::None}, 150);
    // [1000, 1000, 100], median 1000. 100 ms to the wrap plus 150 after = 250 ms: +125 = 225.
    CHECK(pipeline.output(150).bt == DeciC{225});
    CHECK_FALSE(pipeline.output(2150).stale);  // Exactly 2000 ms after the last output.
    CHECK(pipeline.output(2151).stale);

    SensorPipeline across_wrap{{}};
    across_wrap.feed({{100}, std::nullopt, TcFault::None}, UINT32_MAX - 599);
    across_wrap.feed({{100}, std::nullopt, TcFault::None}, UINT32_MAX - 349);
    across_wrap.feed({{100}, std::nullopt, TcFault::None}, UINT32_MAX - 99);
    // 100 ms to the wrap plus 1900 ms after is exactly the 2000 ms boundary.
    CHECK_FALSE(across_wrap.output(1900).stale);
    CHECK(across_wrap.output(1901).stale);
}

TEST_CASE("Sensor pipeline reports unplugging after 500 ms and recovers after three good samples") {
    SensorPipeline pipeline{{}};
    pipeline.feed({{500}, DeciC{250}, TcFault::None}, 0);
    pipeline.feed({{500}, DeciC{250}, TcFault::None}, 250);
    pipeline.feed({{500}, DeciC{250}, TcFault::None}, 500);
    pipeline.feed({{0}, DeciC{300}, TcFault::Open}, 750);
    pipeline.feed({{0}, DeciC{300}, TcFault::Open}, 1000);
    const auto pending = pipeline.output(1000);
    // Two faults keep the last output; the MAX31855 still reports its cold junction.
    CHECK(pending.fault == TcFault::None);
    CHECK(pending.bt == DeciC{500});
    CHECK_FALSE(pending.stale);
    CHECK(pending.bt_updated_ms == 500);
    REQUIRE(pending.cold_junction.has_value());
    CHECK(*pending.cold_junction == DeciC{300});

    pipeline.feed({{0}, DeciC{300}, TcFault::Open}, 1250);
    // Reported at the third faulty sample: 1250 - 750 = 500 ms after the first (criterion: < 1 s).
    CHECK(pipeline.output(1250).fault == TcFault::Open);
    CHECK(pipeline.output(1250).bt_updated_ms == 500);
    CHECK(pipeline.output(2501).stale);

    pipeline.feed({{2500}, DeciC{310}, TcFault::None}, 2750);
    pipeline.feed({{2500}, DeciC{310}, TcFault::None}, 3000);
    // Two good samples are not enough to clear the fault.
    CHECK(pipeline.output(3000).fault == TcFault::Open);
    REQUIRE(pipeline.output(3000).cold_junction.has_value());
    CHECK(*pipeline.output(3000).cold_junction == DeciC{310});

    pipeline.feed({{2500}, DeciC{310}, TcFault::None}, 3250);
    // Third good sample: median 2500 published as is. A rate limit against the old 500 over
    // 2750 ms would have allowed only 500 + 1375 = 1875.
    const auto recovered = pipeline.output(3250);
    CHECK(recovered.fault == TcFault::None);
    CHECK_FALSE(recovered.stale);
    CHECK(recovered.bt == DeciC{2500});
    CHECK(recovered.bt_updated_ms == 3250);
}

TEST_CASE("Sensor pipeline never publishes a connector-bounce spike on replug") {
    SensorPipeline pipeline{{}};
    for (uint32_t t = 0; t < 750; t += 250) {
        pipeline.feed({{0}, std::nullopt, TcFault::Open}, t);
    }
    CHECK(pipeline.output(500).fault == TcFault::Open);

    pipeline.feed({{8500}, std::nullopt, TcFault::None}, 750);  // 850 C bounce artifact.
    pipeline.feed({{2000}, std::nullopt, TcFault::None}, 1000);
    CHECK(pipeline.output(1000).fault == TcFault::Open);
    pipeline.feed({{2010}, std::nullopt, TcFault::None}, 1250);
    // [8500, 2000, 2010] spreads 6500 > 50: the samples do not agree, nothing is published.
    CHECK(pipeline.output(1250).fault == TcFault::Open);
    pipeline.feed({{2005}, std::nullopt, TcFault::None}, 1500);
    // The window slides: [2005, 2000, 2010] spreads 10 <= 50, median 2005.
    CHECK(pipeline.output(1500).fault == TcFault::None);
    CHECK(pipeline.output(1500).bt == DeciC{2005});
}

TEST_CASE("Sensor pipeline recovery needs three consecutive good samples") {
    SensorPipeline pipeline{{}};
    for (uint32_t t = 0; t < 750; t += 250) {
        pipeline.feed({{0}, std::nullopt, TcFault::Open}, t);
    }
    // A bouncing connector: good-looking frames between faults. Each fault during recovery
    // restarts the window, so 8500, 8510 and 2000 never form one window.
    pipeline.feed({{8500}, std::nullopt, TcFault::None}, 750);
    pipeline.feed({{0}, std::nullopt, TcFault::Open}, 1000);
    pipeline.feed({{8510}, std::nullopt, TcFault::None}, 1250);
    pipeline.feed({{0}, std::nullopt, TcFault::Open}, 1500);
    pipeline.feed({{2000}, std::nullopt, TcFault::None}, 1750);
    CHECK(pipeline.output(1750).fault == TcFault::Open);
    pipeline.feed({{2000}, std::nullopt, TcFault::None}, 2000);
    CHECK(pipeline.output(2000).fault == TcFault::Open);
    pipeline.feed({{2002}, std::nullopt, TcFault::None}, 2250);
    // Three consecutive: [2000, 2000, 2002], median 2000.
    CHECK(pipeline.output(2250).fault == TcFault::None);
    CHECK(pipeline.output(2250).bt == DeciC{2000});

    SensorPipeline at_boot{{}};
    at_boot.feed({{500}, std::nullopt, TcFault::None}, 0);
    at_boot.feed({{500}, std::nullopt, TcFault::None}, 250);
    at_boot.feed({{0}, std::nullopt, TcFault::BusError}, 500);  // Below the debounce of 3.
    at_boot.feed({{500}, std::nullopt, TcFault::None}, 750);
    // Not yet live, so the glitch restarted the window: one sample, still "no reading yet".
    CHECK(at_boot.output(750).fault == TcFault::BusError);
    CHECK(at_boot.output(750).stale);
}

TEST_CASE("Sensor pipeline restarts its window after a gap longer than the stale limit") {
    SensorPipeline pipeline{{}};
    pipeline.feed({{500}, std::nullopt, TcFault::None}, 0);
    pipeline.feed({{500}, std::nullopt, TcFault::None}, 250);
    pipeline.feed({{500}, std::nullopt, TcFault::None}, 500);
    CHECK(pipeline.output(500).bt == DeciC{500});

    // No samples for 2500 ms (the task was held up), then the beans are at 200 C.
    pipeline.feed({{2000}, std::nullopt, TcFault::None}, 3000);
    // 3000 - 500 = 2500 > 2000: the old samples are dropped, nothing new is published yet,
    // and the old 50 C stays stale instead of getting a fresh timestamp.
    CHECK(pipeline.output(3000).stale);
    CHECK(pipeline.output(3000).bt_updated_ms == 500);
    pipeline.feed({{2000}, std::nullopt, TcFault::None}, 3250);
    pipeline.feed({{2000}, std::nullopt, TcFault::None}, 3500);
    // Three new samples agree: 2000, published without a rate limit against the old 500.
    CHECK_FALSE(pipeline.output(3500).stale);
    CHECK(pipeline.output(3500).bt == DeciC{2000});
}

TEST_CASE("Sensor pipeline ignores one bus glitch and resets its debounce on a good sample") {
    SensorPipeline pipeline{{.fault_debounce = 2}};
    pipeline.feed({{100}, DeciC{250}, TcFault::None}, 0);
    pipeline.feed({{100}, DeciC{250}, TcFault::None}, 250);
    pipeline.feed({{100}, DeciC{250}, TcFault::None}, 500);
    pipeline.feed({{0}, std::nullopt, TcFault::BusError}, 750);
    // One glitch: no fault, and the last cold junction is kept (a bus error carries none).
    CHECK(pipeline.output(750).fault == TcFault::None);
    REQUIRE(pipeline.output(750).cold_junction.has_value());
    CHECK(*pipeline.output(750).cold_junction == DeciC{250});

    pipeline.feed({{106}, DeciC{250}, TcFault::None}, 1000);
    // The glitch stays out of the window: [106, 100, 100], median 100.
    CHECK(pipeline.output(1000).bt == DeciC{100});
    CHECK(pipeline.output(1000).fault == TcFault::None);

    pipeline.feed({{0}, DeciC{260}, TcFault::Open}, 1250);
    CHECK(pipeline.output(1250).fault == TcFault::None);
    pipeline.feed({{0}, std::nullopt, TcFault::BusError}, 1500);
    // Both faulty samples count, and the second determines the reported fault. A reported
    // bus error means the cold junction is unknown too.
    CHECK(pipeline.output(1500).fault == TcFault::BusError);
    CHECK_FALSE(pipeline.output(1500).cold_junction.has_value());
}

TEST_CASE("Sensor pipeline fault count saturates and still reports the latest fault") {
    SensorPipeline pipeline{{.fault_debounce = UINT8_MAX}};
    pipeline.feed({{100}, std::nullopt, TcFault::None}, 0);
    pipeline.feed({{100}, std::nullopt, TcFault::None}, 0);
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
