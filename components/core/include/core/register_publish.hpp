#pragma once

#include <cstdint>

#include "core/probe_state.hpp"
#include "core/registers.hpp"

namespace core {

RegisterBlock build_registers(const ProbeState& state, uint16_t firmware_version);
uint16_t firmware_version_from(const char* version);

}  // namespace core
