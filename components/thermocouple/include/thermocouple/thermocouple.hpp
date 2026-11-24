#pragma once

#include "core/thermocouple.hpp"
#include "esp_err.h"

namespace thermocouple {

// Called by the sensor task. Safe to retry after a failure.
esp_err_t init();

// Reads one frame. SPI errors return BusError, with at most ~70 ms of SPI waits. Waits first
// if the previous read was less than one conversion time ago. Restarts the chip if a transfer
// has been stuck for 5 s.
core::TcReading read();

const char* chip_name();

}  // namespace thermocouple
