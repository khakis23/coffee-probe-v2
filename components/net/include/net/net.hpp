#pragma once

#include <cstdint>

#include "esp_err.h"

namespace net {

// Starts Wi-Fi in AP-only mode. Call once from app_main after nvs_flash_init.
// Returns ESP_OK or the failing step's error.
esp_err_t start_softap();

// SSID actually in use, valid after start_softap succeeds. Points to static storage.
const char* ssid();

// Number of associated stations, updated by Wi-Fi events. Safe to read from any task.
uint8_t client_count();

}  // namespace net
