#pragma once

#include "core/settings.hpp"
#include "core/thermocouple.hpp"
#include "esp_err.h"

namespace thermocouple {

// Which converter is fitted and whether the probe tip is grounded (settings, #23). Call once
// before init(); the choice is fixed until the next restart.
void configure(core::TcChip chip, bool grounded_probe);
// The configured choice: the build options (Kconfig) until configure() is called, so they are
// the factory default.
core::TcChip chip();
bool grounded_probe();

// Called by the sensor task. Safe to retry after a failure.
esp_err_t init();

// Reads one frame. SPI errors return BusError, with at most ~70 ms of SPI waits. Waits first
// if the previous read was less than one conversion time ago. Panics (coredump, reboot) if a
// transfer has been stuck for 5 s: reliability R3.
core::TcReading read();

const char* chip_name();

}  // namespace thermocouple
