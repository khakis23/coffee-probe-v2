#pragma once

#include <array>
#include <cstdint>

namespace core {

namespace reg {  // Input registers (function code 4). Function code 3 returns the same values.
inline constexpr uint16_t kBtF10 = 0;            // Bean temperature F x10, int16.
inline constexpr uint16_t kBtC10 = 1;            // Bean temperature C x10, int16.
inline constexpr uint16_t kStatus = 2;           // Status bits.
inline constexpr uint16_t kDieC10 = 3;           // Chip die temperature C x10.
inline constexpr uint16_t kColdJunctionC10 = 4;  // kNotAvailable on MAX6675.
inline constexpr uint16_t kBatteryPct = 5;       // 0xFFFF until the battery feature.
inline constexpr uint16_t kUptimeHi = 6;         // Uptime seconds, uint32, high word first.
inline constexpr uint16_t kUptimeLo = 7;
inline constexpr uint16_t kFirmwareVersion = 8;  // (major << 8) | minor.
inline constexpr uint16_t kCount = 9;
}  // namespace reg

inline constexpr int16_t kFaultValue = -10;        // Artisan shows -1 after dividing by 10.
inline constexpr uint16_t kNotAvailable = 0x8000;  // INT16_MIN as a raw register.
inline constexpr uint16_t kBatteryUnknown = 0xFFFF;

enum StatusBit : uint16_t {
    kProbeOpen = 1u << 0,
    kProbeShort = 1u << 1,
    kStale = 1u << 2,
    kOverheatWarn = 1u << 3,
    kOverheatCrit = 1u << 4,
    kLowBattery = 1u << 5,
    kBusError = 1u << 6,
};

struct RegisterBlock {
    std::array<uint16_t, reg::kCount> regs{};
};

constexpr uint16_t to_reg(int16_t v) { return static_cast<uint16_t>(v); }

}  // namespace core
