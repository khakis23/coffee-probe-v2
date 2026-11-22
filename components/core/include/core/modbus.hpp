#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "core/registers.hpp"

namespace core::modbus {

// Total ADU length (7-byte MBAP including unit ID + PDU), or nullopt if fewer than 6 bytes.
std::optional<std::size_t> frame_length(std::span<const uint8_t> buf);

// Handles one complete ADU. Returns bytes written, or 0 to close the connection without a response.
std::size_t handle_request(std::span<const uint8_t> req, const RegisterBlock& regs,
                           std::span<uint8_t> resp);

inline constexpr std::size_t kMaxAduSize = 260;

}  // namespace core::modbus
