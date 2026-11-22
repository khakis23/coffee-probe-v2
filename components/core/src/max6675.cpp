#include "core/thermocouple.hpp"

namespace core {

TcReading decode_max6675(uint16_t frame) {
    constexpr uint16_t kDummy = 1u << 15;
    constexpr uint16_t kDeviceId = 1u << 1;
    constexpr uint16_t kOpen = 1u << 2;

    if (frame == 0xFFFFu || (frame & (kDummy | kDeviceId))) {
        return {{0}, std::nullopt, TcFault::BusError};
    }
    if (frame & kOpen) {
        return {{0}, std::nullopt, TcFault::Open};
    }

    const DeciC hot = from_quarter_c((frame >> 3) & 0x0FFFu);
    return {hot, std::nullopt, TcFault::None};
}

}  // namespace core
