#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

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

// CRC-32/ISO-HDLC with reflected polynomial 0xEDB88320 and all-ones init and final XOR.
uint32_t crc32(std::span<const uint8_t> data);
std::array<uint8_t, kSettingsBlobSize> encode_settings(const Settings& s);
SettingsLoad decode_settings(std::span<const uint8_t> blob);
Settings repair_settings(const Settings& s, uint8_t* repaired_fields);

}  // namespace core
