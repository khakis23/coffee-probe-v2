#pragma once

#include "core/settings.hpp"
#include "esp_err.h"

namespace storage {

core::SettingsLoad load();
esp_err_t save(const core::Settings& settings);

}  // namespace storage
