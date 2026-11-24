#include "core/settings.hpp"

namespace core {
namespace {

constexpr uint32_t kMagic = 0x54535043;  // CPST in little-endian byte order.
constexpr std::size_t kCrcOffset = kSettingsHeaderSize + kSettingsPayloadSize;  // In a v1 blob.
constexpr std::size_t kCrcSize = 4;

uint16_t read_u16(std::span<const uint8_t> bytes, std::size_t offset) {
    return static_cast<uint16_t>(bytes[offset] | (bytes[offset + 1] << 8));
}

uint32_t read_u32(std::span<const uint8_t> bytes, std::size_t offset) {
    uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        value |= static_cast<uint32_t>(bytes[offset + i]) << (8 * i);
    }
    return value;
}

void write_u16(std::span<uint8_t> bytes, std::size_t offset, uint16_t value) {
    bytes[offset] = static_cast<uint8_t>(value);
    bytes[offset + 1] = static_cast<uint8_t>(value >> 8);
}

void write_u32(std::span<uint8_t> bytes, std::size_t offset, uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
        bytes[offset + i] = static_cast<uint8_t>(value >> (8 * i));
    }
}

SettingsLoad decode_v1(std::span<const uint8_t> payload) {
    const Settings defaults{};
    uint8_t invalid_booleans = 0;
    const auto read_bool = [&invalid_booleans](uint8_t value, bool fallback) {
        if (value > 1) {
            ++invalid_booleans;
            return fallback;
        }
        return value == 1;
    };

    const Settings decoded{
        .tc_chip = static_cast<TcChip>(payload[0]),
        .grounded_probe = read_bool(payload[1], defaults.grounded_probe),
        .offset_deci_f = static_cast<int16_t>(read_u16(payload, 2)),
        .wifi_channel = payload[4],
        .tx_power_qdbm = payload[5],
        .brightness = payload[6],
        .display_flip = read_bool(payload[7], defaults.display_flip),
        .show_password = read_bool(payload[8], defaults.show_password),
    };

    SettingsLoad result{};
    result.settings = repair_settings(decoded, &result.repaired_fields);
    result.repaired_fields += invalid_booleans;
    result.status = result.repaired_fields > 0 ? SettingsStatus::Repaired : SettingsStatus::Ok;
    return result;
}

// Each version checks its own payload length: the header and CRC checks only prove the blob is
// intact, not that it has the layout this version expects.
SettingsLoad migrate(uint16_t version, std::span<const uint8_t> payload) {
    switch (version) {
        // Add the v2 case here and migrate older decoded data as the model evolves.
        case 1:
            if (payload.size() != kSettingsPayloadSize) {
                return {.status = SettingsStatus::Corrupt};
            }
            return decode_v1(payload);
        default:
            return {.status = SettingsStatus::Corrupt};
    }
}

}  // namespace

uint32_t crc32(std::span<const uint8_t> data) {
    constexpr uint32_t kPolynomial = 0xEDB88320;
    uint32_t crc = 0xFFFFFFFF;
    for (const uint8_t byte : data) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1) != 0 ? kPolynomial : 0);
        }
    }
    return crc ^ 0xFFFFFFFF;
}

std::array<uint8_t, kSettingsBlobSize> encode_settings(const Settings& s) {
    std::array<uint8_t, kSettingsBlobSize> blob{};
    write_u32(blob, 0, kMagic);
    write_u16(blob, 4, kSettingsVersion);
    write_u16(blob, 6, kSettingsPayloadSize);

    auto payload = std::span{blob}.subspan<kSettingsHeaderSize, kSettingsPayloadSize>();
    payload[0] = static_cast<uint8_t>(s.tc_chip);
    payload[1] = static_cast<uint8_t>(s.grounded_probe);
    write_u16(payload, 2, static_cast<uint16_t>(s.offset_deci_f));
    payload[4] = s.wifi_channel;
    payload[5] = s.tx_power_qdbm;
    payload[6] = s.brightness;
    payload[7] = static_cast<uint8_t>(s.display_flip);
    payload[8] = static_cast<uint8_t>(s.show_password);
    payload[9] = 0;

    write_u32(blob, kCrcOffset, crc32(std::span{blob}.first<kCrcOffset>()));
    return blob;
}

SettingsLoad decode_settings(std::span<const uint8_t> blob) {
    if (blob.empty()) {
        return {};
    }
    // The blob describes itself (magic, version, payload length, CRC at the end), so any
    // version's blob can be checked for integrity before its layout is known: a later firmware's
    // settings read here as UnknownVersion, not Corrupt, and a later firmware can migrate ours.
    if (blob.size() < kSettingsHeaderSize + kCrcSize || read_u32(blob, 0) != kMagic) {
        return {.status = SettingsStatus::Corrupt};
    }
    const std::size_t payload_size = read_u16(blob, 6);
    if (blob.size() != kSettingsHeaderSize + payload_size + kCrcSize) {
        return {.status = SettingsStatus::Corrupt};
    }
    const std::size_t crc_offset = kSettingsHeaderSize + payload_size;
    if (read_u32(blob, crc_offset) != crc32(blob.first(crc_offset))) {
        return {.status = SettingsStatus::Corrupt};
    }
    const uint16_t version = read_u16(blob, 4);
    if (version > kSettingsVersion) {
        return {.status = SettingsStatus::UnknownVersion};
    }
    return migrate(version, blob.subspan(kSettingsHeaderSize, payload_size));
}

Settings repair_settings(const Settings& s, uint8_t* repaired_fields) {
    // Repair individual fields (R6) so an unrelated bad value preserves a valid calibration offset.
    const Settings defaults{};
    Settings result = s;
    uint8_t count = 0;
    if (s.tc_chip != TcChip::Max31855 && s.tc_chip != TcChip::Max6675) {
        result.tc_chip = defaults.tc_chip;
        ++count;
    }
    if (s.offset_deci_f < -100 || s.offset_deci_f > 100) {
        result.offset_deci_f = defaults.offset_deci_f;
        ++count;
    }
    if (s.wifi_channel != 1 && s.wifi_channel != 6 && s.wifi_channel != 11) {
        result.wifi_channel = defaults.wifi_channel;
        ++count;
    }
    if (s.tx_power_qdbm != 0 && (s.tx_power_qdbm < 8 || s.tx_power_qdbm > 84)) {
        result.tx_power_qdbm = defaults.tx_power_qdbm;
        ++count;
    }
    if (s.brightness > 3) {
        result.brightness = defaults.brightness;
        ++count;
    }
    if (repaired_fields != nullptr) {
        *repaired_fields = count;
    }
    return result;
}

}  // namespace core
