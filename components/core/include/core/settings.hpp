#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "core/temperature.hpp"

namespace core {

inline constexpr uint16_t kSettingsVersion = 1;

enum class TcChip : uint8_t { Max31855 = 0, Max6675 = 1 };

struct Settings {
    TcChip tc_chip{TcChip::Max31855};
    bool grounded_probe{false};
    int16_t offset_deci_f{0};  // Calibration offset, tenths of a degree F, -100..100.
    uint8_t wifi_channel{6};   // 1, 6 or 11 (non-overlapping 2.4 GHz channels).
    uint8_t tx_power_qdbm{0};  // 0 selects board default, otherwise 8..84 quarter-dBm.
    uint8_t brightness{3};     // 0..3.
    bool display_flip{false};
    bool show_password{true};

    bool operator==(const Settings&) const = default;
};

enum class SettingsStatus : uint8_t { Ok, Missing, Corrupt, UnknownVersion, Repaired };

struct SettingsLoad {
    Settings settings{};
    SettingsStatus status{SettingsStatus::Missing};
    uint8_t repaired_fields{0};  // Number of out-of-range fields reset to their defaults.
};

inline constexpr std::size_t kSettingsHeaderSize = 8;
inline constexpr std::size_t kSettingsPayloadSize = 10;
inline constexpr std::size_t kSettingsBlobSize = kSettingsHeaderSize + kSettingsPayloadSize + 4;

// The calibration offset in tenths of a degree C. An offset, so there is no +32: 1.8 F = 1.0 C.
constexpr DeciC offset_deci_c(int16_t offset_deci_f) {
    return {clamp_i16(div_round_away(static_cast<int32_t>(offset_deci_f) * 5, 9))};
}

// The Wi-Fi TX power to apply, in quarter-dBm: the setting (0 = Auto, the board's limit),
// never above the board's limit (some boards' regulators misbehave at full power).
constexpr int8_t effective_tx_power_qdbm(uint8_t setting, int8_t board_max) {
    if (setting == 0 || static_cast<int>(setting) > board_max) {
        return board_max;
    }
    return static_cast<int8_t>(setting);
}

// CRC-32/ISO-HDLC with reflected polynomial 0xEDB88320 and all-ones init and final XOR.
uint32_t crc32(std::span<const uint8_t> data);
std::array<uint8_t, kSettingsBlobSize> encode_settings(const Settings& s);
SettingsLoad decode_settings(std::span<const uint8_t> blob);
Settings repair_settings(const Settings& s, uint8_t* repaired_fields);

}  // namespace core
