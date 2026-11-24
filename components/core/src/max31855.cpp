#include "core/thermocouple.hpp"

namespace core {

TcReading decode_max31855(uint32_t frame, bool grounded_probe) {
    constexpr uint32_t kReserved = (1u << 17) | (1u << 3);
    constexpr uint32_t kFaultSummary = 1u << 16;
    constexpr uint32_t kOpen = 1u << 0;
    constexpr uint32_t kShortGnd = 1u << 1;
    constexpr uint32_t kShortVcc = 1u << 2;
    constexpr uint32_t kFaultCauses = kOpen | kShortGnd | kShortVcc;
    constexpr TcReading kBusError{{0}, std::nullopt, TcFault::BusError};

    // A constant-low or constant-high bus can produce these frames.
    if (frame == 0 || frame == 0xFFFFFFFFu) {
        return kBusError;
    }
    if (frame & kReserved) {
        return kBusError;
    }

    const DeciC cold_junction = from_sixteenth_c(detail::sign_extend<12>(frame >> 4));
    // A tip welded to the grounded roaster body can flag SCG permanently while the
    // temperature field is still usable (architecture.md, Reliability design). Only SCG:
    // the roaster body is never at VCC, so SCV still means a real wiring fault.
    const uint32_t causes = frame & kFaultCauses;
    const bool accepted_short = grounded_probe && causes == kShortGnd;
    if ((frame & kFaultSummary) && !accepted_short) {
        if (frame & kOpen) {
            return {{0}, cold_junction, TcFault::Open};
        }
        if (frame & kShortGnd) {
            return {{0}, cold_junction, TcFault::ShortGnd};
        }
        if (frame & kShortVcc) {
            return {{0}, cold_junction, TcFault::ShortVcc};
        }
        return kBusError;
    }
    if (!(frame & kFaultSummary) && causes != 0) {
        return kBusError;
    }

    const DeciC hot = from_quarter_c(detail::sign_extend<14>(frame >> 18));
    return {hot, cold_junction, TcFault::None};
}

}  // namespace core
