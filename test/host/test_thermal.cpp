#include <doctest/doctest.h>

#include <optional>

#include "core/thermal.hpp"

using core::DeciC;
using core::ThermalLevel;
using core::ThermalPolicy;
using core::ThermalSensorPolicy;

namespace {

// Feeds the same reading n times and returns the level after the last one.
ThermalLevel feed(ThermalSensorPolicy& p, std::optional<DeciC> reading, int n) {
    ThermalLevel level = p.level();
    for (int i = 0; i < n; ++i) {
        level = p.update(reading);
    }
    return level;
}

// Ambient thresholds: warn 50.0 C, critical 65.0 C, hysteresis 5.0 C; 5 samples to confirm.
ThermalSensorPolicy ambient() { return ThermalSensorPolicy{core::kAmbientThresholds}; }

}  // namespace

TEST_CASE("A level rises only after 5 consecutive samples at or above the threshold") {
    auto p = ambient();
    CHECK(feed(p, DeciC{500}, 4) == ThermalLevel::Normal);  // 50.0 C exactly, 4 times: not yet.
    CHECK(p.update(DeciC{500}) == ThermalLevel::Warn);       // The 5th sample.

    auto below = ambient();
    CHECK(feed(below, DeciC{499}, 20) == ThermalLevel::Normal);  // 49.9 C never warns.
}

TEST_CASE("One cool sample restarts the count; so does a missing reading") {
    auto p = ambient();
    feed(p, DeciC{510}, 4);
    p.update(DeciC{400});  // 40.0 C: back below, the count restarts.
    CHECK(feed(p, DeciC{510}, 4) == ThermalLevel::Normal);
    CHECK(p.update(DeciC{510}) == ThermalLevel::Warn);  // 5 consecutive again.

    auto gap = ambient();
    feed(gap, DeciC{510}, 4);
    gap.update(std::nullopt);  // No cold-junction reading this second.
    CHECK(feed(gap, DeciC{510}, 4) == ThermalLevel::Normal);
    CHECK(gap.update(DeciC{510}) == ThermalLevel::Warn);
}

TEST_CASE("Falling needs 5 samples below threshold minus hysteresis") {
    auto p = ambient();
    feed(p, DeciC{550}, 5);
    REQUIRE(p.level() == ThermalLevel::Warn);
    // 45.0 C is exactly warn - hysteresis: still Warn, however long.
    CHECK(feed(p, DeciC{450}, 20) == ThermalLevel::Warn);
    // 44.9 C: below 45.0, so Normal after 5 samples.
    CHECK(feed(p, DeciC{449}, 4) == ThermalLevel::Warn);
    CHECK(p.update(DeciC{449}) == ThermalLevel::Normal);
}

TEST_CASE("Normal can go straight to Critical and back down step by step") {
    auto p = ambient();
    CHECK(feed(p, DeciC{650}, 5) == ThermalLevel::Critical);  // 65.0 C exactly.
    // 60.0 C = critical - hysteresis: stays Critical.
    CHECK(feed(p, DeciC{600}, 20) == ThermalLevel::Critical);
    // 59.9 C is below 60.0 but at or above 45.0: Warn after 5.
    CHECK(feed(p, DeciC{599}, 5) == ThermalLevel::Warn);
    // 47.0 C: above 45.0, stays Warn (no flicker at the warn threshold).
    CHECK(feed(p, DeciC{470}, 20) == ThermalLevel::Warn);
    CHECK(feed(p, DeciC{300}, 5) == ThermalLevel::Normal);
}

TEST_CASE("A change of direction while a change is pending restarts the count") {
    auto p = ambient();
    feed(p, DeciC{550}, 3);  // Warn pending, 3 of 5.
    // 66.0 C: now Critical is wanted. The count restarts at 1, so 4 more are needed after this.
    CHECK(p.update(DeciC{660}) == ThermalLevel::Normal);
    CHECK(feed(p, DeciC{660}, 3) == ThermalLevel::Normal);  // 4 of 5.
    CHECK(p.update(DeciC{660}) == ThermalLevel::Critical);  // 5 of 5.
}

TEST_CASE("The overall level is the worse sensor; the die sensor alone covers a MAX6675") {
    ThermalPolicy policy;
    // Ambient 30.0 C (Normal); die 95.0 C (die warn 90.0 C) for 5 samples -> Warn.
    for (int i = 0; i < 4; ++i) {
        CHECK(policy.update(DeciC{300}, DeciC{950}) == ThermalLevel::Normal);
    }
    CHECK(policy.update(DeciC{300}, DeciC{950}) == ThermalLevel::Warn);
    // Ambient climbs to 66.0 C (ambient critical 65.0 C): Critical wins after 5 samples.
    for (int i = 0; i < 4; ++i) {
        CHECK(policy.update(DeciC{660}, DeciC{950}) == ThermalLevel::Warn);
    }
    CHECK(policy.update(DeciC{660}, DeciC{950}) == ThermalLevel::Critical);
    CHECK(policy.level() == ThermalLevel::Critical);

    ThermalPolicy die_only;  // MAX6675: no cold junction ever.
    for (int i = 0; i < 4; ++i) {
        CHECK(die_only.update(std::nullopt, DeciC{1050}) == ThermalLevel::Normal);
    }
    // 105.0 C is the die's critical threshold.
    CHECK(die_only.update(std::nullopt, DeciC{1050}) == ThermalLevel::Critical);

    ThermalPolicy blind;  // No readings at all: never invents a level.
    for (int i = 0; i < 20; ++i) {
        CHECK(blind.update(std::nullopt, std::nullopt) == ThermalLevel::Normal);
    }
}
