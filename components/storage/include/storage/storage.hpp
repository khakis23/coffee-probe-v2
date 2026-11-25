#pragma once

#include "core/settings.hpp"
#include "esp_err.h"

namespace storage {

core::SettingsLoad load();
esp_err_t save(const core::Settings& settings);
// Factory reset: removes the saved settings, so the next load() reports Missing.
esp_err_t erase();

}  // namespace storage
