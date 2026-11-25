#include <doctest/doctest.h>

#include <string>

#include "core/boot_record.hpp"

using core::BootRecord;
using core::ResetCause;

TEST_CASE("boot record: power-on starts fresh, whatever RTC memory holds") {
    BootRecord junk{};
    junk.magic = 0x12345678;
    junk.boots = 999;
    const BootRecord r = core::next_boot_record(junk, ResetCause::PowerOn);
    CHECK(core::boot_record_valid(r));
    CHECK(r.boots == 1);
    CHECK(r.brownouts == 0);
    CHECK(r.crashes == 0);

    // Even a valid record from before is dropped on a power-on: it describes the last power-up.
    BootRecord old = core::next_boot_record(BootRecord{}, ResetCause::Brownout);
    old = core::next_boot_record(old, ResetCause::Brownout);
    REQUIRE(old.brownouts == 2);
    CHECK(core::next_boot_record(old, ResetCause::PowerOn).brownouts == 0);
}

TEST_CASE("boot record: restarts count up; brownouts and crashes are counted") {
    BootRecord r = core::next_boot_record(BootRecord{}, ResetCause::PowerOn);
    r = core::next_boot_record(r, ResetCause::Software);
    r = core::next_boot_record(r, ResetCause::Crash);
    r = core::next_boot_record(r, ResetCause::Watchdog);
    r = core::next_boot_record(r, ResetCause::Brownout);
    r = core::next_boot_record(r, ResetCause::DeepSleep);
    CHECK(core::boot_record_valid(r));
    CHECK(r.boots == 6);
    CHECK(r.crashes == 2);
    CHECK(r.brownouts == 1);
}

TEST_CASE("boot record: leftovers that aren't a record start fresh, still counting this boot") {
    BootRecord tampered = core::next_boot_record(BootRecord{}, ResetCause::PowerOn);
    tampered.boots = 40;  // check no longer matches
    CHECK_FALSE(core::boot_record_valid(tampered));
    const BootRecord r = core::next_boot_record(tampered, ResetCause::Brownout);
    CHECK(r.boots == 1);
    CHECK(r.brownouts == 1);
    CHECK_FALSE(core::boot_record_valid(BootRecord{}));
}

TEST_CASE("boot record: counters saturate instead of wrapping") {
    BootRecord r = core::next_boot_record(BootRecord{}, ResetCause::PowerOn);
    r.boots = UINT16_MAX;
    r.crashes = UINT16_MAX;
    r.check = core::boot_record_check(r);
    r = core::next_boot_record(r, ResetCause::Crash);
    CHECK(r.boots == UINT16_MAX);
    CHECK(r.crashes == UINT16_MAX);
}

TEST_CASE("power warning: after two dips, clears after 30 calm minutes") {
    BootRecord r = core::next_boot_record(BootRecord{}, ResetCause::PowerOn);
    CHECK_FALSE(core::power_warning(r, false, ResetCause::PowerOn, 0));
    r = core::next_boot_record(r, ResetCause::Brownout);
    CHECK_FALSE(core::power_warning(r, true, ResetCause::Brownout, 0));  // One can be a fluke.
    r = core::next_boot_record(r, ResetCause::Brownout);
    CHECK(core::power_warning(r, true, ResetCause::Brownout, 0));
    CHECK(core::power_warning(r, true, ResetCause::Brownout, core::kPowerCalmS - 1));
    CHECK_FALSE(core::power_warning(r, true, ResetCause::Brownout, core::kPowerCalmS));
    // Cleared: the next dip starts a fresh count.
    r = core::cleared_brownouts(r);
    CHECK(core::boot_record_valid(r));
    CHECK(r.brownouts == 0);
    r = core::next_boot_record(r, ResetCause::Brownout);
    CHECK_FALSE(core::power_warning(r, true, ResetCause::Brownout, 0));
}

TEST_CASE("power warning: if RTC memory didn't survive the dip, one dip is enough") {
    const BootRecord r = core::next_boot_record(BootRecord{}, ResetCause::Brownout);
    CHECK(core::power_warning(r, false, ResetCause::Brownout, 0));
    CHECK_FALSE(core::power_warning(r, false, ResetCause::Brownout, core::kPowerCalmS));
    // Not after a crash with lost memory: that isn't a power problem.
    const BootRecord c = core::next_boot_record(BootRecord{}, ResetCause::Crash);
    CHECK_FALSE(core::power_warning(c, false, ResetCause::Crash, 0));
}

TEST_CASE("reset cause text fits the Diagnostics value column") {
    for (int i = 0; i <= static_cast<int>(ResetCause::Other); ++i) {
        const auto t = core::reset_cause_text(static_cast<ResetCause>(i));
        CAPTURE(std::string(t));
        CHECK(!t.empty());
        CHECK(t.size() <= 9);
    }
}

TEST_CASE("stall detector: stalls after 10 s without a change, clock wrap included") {
    core::StallDetector d;
    CHECK_FALSE(d.update(5, 1000));
    CHECK_FALSE(d.update(5, 1000 + core::kStallMs - 1));
    CHECK(d.update(5, 1000 + core::kStallMs));
    CHECK_FALSE(d.update(6, 1000 + core::kStallMs + 1));  // Moving again.

    core::StallDetector w;
    const uint32_t t0 = UINT32_MAX - 3000;
    CHECK_FALSE(w.update(1, t0));
    CHECK_FALSE(w.update(1, t0 + 5000));
    CHECK(w.update(1, t0 + core::kStallMs));
    // A counter that wraps still counts as moving.
    core::StallDetector c;
    CHECK_FALSE(c.update(UINT32_MAX, 0));
    CHECK_FALSE(c.update(0, 20'000));
}
