#pragma once

#include <cstdint>
#include <optional>

#include "core/temperature.hpp"

namespace core {

enum class TcFault : uint8_t { None, Open, ShortGnd, ShortVcc, BusError };

struct TcReading {
    // Valid only when fault is None, including shorts accepted in MAX31855 grounded mode.
    DeciC hot;  // Zero for a reported fault.
    std::optional<DeciC> cold_junction;
    TcFault fault;
};

TcReading decode_max31855(uint32_t frame, bool grounded_probe = false);
TcReading decode_max6675(uint16_t frame);

}  // namespace core

namespace core::detail {

template <unsigned Bits>
constexpr int32_t sign_extend(uint32_t value) {
    static_assert(Bits > 0 && Bits <= 32);
    constexpr unsigned kShift = 32 - Bits;
    return static_cast<int32_t>(value << kShift) >> kShift;
}

}  // namespace core::detail
