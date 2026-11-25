#pragma once

#include <optional>

#include "core/temperature.hpp"
#include "esp_err.h"

namespace health {

// Called by the health task. Safe to call again after success or failure.
esp_err_t init();
std::optional<core::DeciC> read_die();
[[noreturn]] void enter_protective_sleep();

}  // namespace health
