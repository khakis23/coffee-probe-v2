#pragma once

#include <cstdint>
#include <optional>

#include "core/temperature.hpp"

namespace core {

enum class TcFault : uint8_t { None, Open, ShortGnd, ShortVcc, BusError };

struct TcReading {
    DeciC hot;  // Zero when fault is set. Use this field only when fault is None.
    std::optional<DeciC> cold_junction;
    TcFault fault;
};

TcReading decode_max31855(uint32_t frame);
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
