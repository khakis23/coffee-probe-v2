#pragma once

#include <optional>

#include "core/boot_record.hpp"
#include "core/probe_state.hpp"
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
// Whether a crash dump is in flash (read with `idf.py coredump-info`). Reads the size word first,
// so a boot with no dump costs nothing; only when one is there is its checksum verified (a dump
// cut short by a power loss, or in another IDF version's format, is Damaged). Call once at boot.
core::CrashRecord crash_record();

// The power warning ended (kPowerCalmS without a dip): the next dip starts a fresh count.
void clear_brownouts();

// Called by the health task. Safe to call again after success or failure.
esp_err_t init();
std::optional<core::DeciC> read_die();
[[noreturn]] void enter_protective_sleep();

}  // namespace health
