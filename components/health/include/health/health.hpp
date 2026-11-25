#pragma once

#include <optional>

#include "core/boot_record.hpp"
#include "core/temperature.hpp"
#include "esp_err.h"

namespace health {

struct BootInfo {
    core::BootRecord record;  // Counts for this boot, already written back to RTC memory.
    core::ResetCause cause;
    bool stored_was_valid;  // RTC memory held a valid record from before this reset.
};

// Call once, first thing in app_main: reads why the chip reset and updates the RTC record.
BootInfo record_boot();
// Whether a valid crash dump is saved in flash (read with `idf.py coredump-info`). Reads and
// checksums the dump partition: call once at boot, not from a task loop.
bool crash_record_saved();

// The power warning ended (kPowerCalmS without a dip): the next dip starts a fresh count.
void clear_brownouts();

// Called by the health task. Safe to call again after success or failure.
esp_err_t init();
std::optional<core::DeciC> read_die();
[[noreturn]] void enter_protective_sleep();

}  // namespace health
