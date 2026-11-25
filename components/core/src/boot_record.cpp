#include "core/boot_record.hpp"

namespace core {
namespace {

uint16_t saturating_inc(uint16_t v) { return v == UINT16_MAX ? v : static_cast<uint16_t>(v + 1); }

BootRecord sealed(BootRecord r) {
    r.magic = kBootRecordMagic;
    r.check = boot_record_check(r);
    return r;
}

}  // namespace

uint16_t boot_record_check(const BootRecord& r) {
    // Not a CRC, just enough that random RAM after a power-on is very unlikely to pass.
    const uint32_t x = r.magic ^ (static_cast<uint32_t>(r.boots) << 16) ^ r.brownouts ^
                       (static_cast<uint32_t>(r.crashes) << 8) ^ 0x5A5Au;
    return static_cast<uint16_t>((x >> 16) ^ (x & 0xFFFFu));
}

bool boot_record_valid(const BootRecord& r) {
    return r.magic == kBootRecordMagic && r.check == boot_record_check(r) && r.boots > 0;
}

BootRecord next_boot_record(const BootRecord& stored, ResetCause cause) {
    BootRecord r{};
    if (cause != ResetCause::PowerOn && boot_record_valid(stored)) {
        r = stored;
    }
    r.boots = saturating_inc(r.boots);
    if (cause == ResetCause::Brownout) {
        r.brownouts = saturating_inc(r.brownouts);
    }
    if (cause == ResetCause::Crash || cause == ResetCause::Watchdog) {
        r.crashes = saturating_inc(r.crashes);
    }
    return sealed(r);
}

BootRecord cleared_brownouts(const BootRecord& r) {
    BootRecord c = r;
    c.brownouts = 0;
    return sealed(c);
}

bool power_warning(const BootRecord& r, bool stored_was_valid, ResetCause cause,
                   uint32_t uptime_s) {
    if (uptime_s >= kPowerCalmS) {
        return false;
    }
    if (r.brownouts >= kBrownoutsForWarning) {
        return true;
    }
    return cause == ResetCause::Brownout && !stored_was_valid;
}

std::string_view reset_cause_text(ResetCause cause) {
    switch (cause) {
        case ResetCause::PowerOn:
            return "Power on";
        case ResetCause::Software:
            return "Restart";
        case ResetCause::Crash:
            return "Crash";
        case ResetCause::Watchdog:
            return "Freeze";
        case ResetCause::Brownout:
            return "Power dip";
        case ResetCause::DeepSleep:
            return "Cooling";
        case ResetCause::Other:
            break;
    }
    return "Other";
}

bool StallDetector::update(uint32_t counter, uint32_t now_ms) {
    if (!started_ || counter != last_counter_) {
        started_ = true;
        last_counter_ = counter;
        last_change_ms_ = now_ms;
        return false;
    }
    return now_ms - last_change_ms_ >= kStallMs;
}

}  // namespace core
