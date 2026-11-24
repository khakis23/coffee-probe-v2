#pragma once

#include <cstdint>

#include "core/registers.hpp"
#include "esp_err.h"

namespace net::modbus {

// The net task owns the listener, clients and register block.
esp_err_t start();
void service(uint32_t now_ms, uint32_t wait_ms);
void publish(const core::RegisterBlock& block);
uint8_t client_count();
uint32_t last_request_ms();

// May be read by the health task.
uint32_t loop_count();

}  // namespace net::modbus
