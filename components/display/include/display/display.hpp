#pragma once

#include <cstdint>

#include "core/framebuffer.hpp"
#include "esp_err.h"

namespace display {

// The UI task owns the bus and calls this API. Initialization may be retried after failure.
esp_err_t init();

// Sends changed pages within the flush budget. now_ms is the monotonic uptime in milliseconds.
void show(const core::Framebuffer& frame, uint32_t now_ms);

// Whether the last transfer succeeded.
bool ok();

}  // namespace display
