#pragma once

#include <cstdint>

#include "core/menu.hpp"
#include "esp_err.h"

namespace buttons {

// Configures the four button pins as inputs with pull-ups (active low). Safe to call again.
esp_err_t init();

// Call every 10 ms from the UI task (the only caller). Debounces all four buttons and writes up
// to one press per button into out. Returns how many were written.
inline constexpr int kButtons = 4;
int poll(uint32_t now_ms, core::menu::Press (&out)[kButtons]);

// The OK button's raw level, for the power-on factory reset before the UI task starts.
bool ok_down();

}  // namespace buttons
