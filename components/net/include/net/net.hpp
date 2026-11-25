#pragma once

#include <cstdint>

#include "esp_err.h"

namespace net {

// Starts Wi-Fi in AP-only mode on channel (1, 6 or 11) with tx_power_qdbm (quarter-dBm, already
// capped at the board's limit). Call once from app_main after nvs_flash_init.
// Returns ESP_OK or the failing step's error.
esp_err_t start_softap(uint8_t channel, int8_t tx_power_qdbm);

// Changes the TX power while running (quarter-dBm, already capped at the board's limit).
esp_err_t set_tx_power(int8_t tx_power_qdbm);

// SSID actually in use, valid after start_softap succeeds. Points to static storage.
const char* ssid();

// Number of associated stations, updated by Wi-Fi events. Safe to read from any task.
uint8_t client_count();

}  // namespace net
