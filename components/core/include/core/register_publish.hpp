#pragma once

#include <cstdint>

#include "core/probe_state.hpp"
#include "core/registers.hpp"

namespace core {

// now_ms is when the snapshot was taken: an old reading is published as stale (see bt_valid).
RegisterBlock build_registers(const ProbeState& state, uint32_t now_ms, uint16_t firmware_version);
uint16_t firmware_version_from(const char* version);

}  // namespace core
