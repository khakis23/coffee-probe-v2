#pragma once

#include "core/thermocouple.hpp"
#include "esp_err.h"

namespace thermocouple {

// Called by the sensor task. Safe to retry after a failure.
esp_err_t init();

// Reads one frame. SPI errors return BusError, with at most ~40 ms of SPI waits.
core::TcReading read();

const char* chip_name();

}  // namespace thermocouple
