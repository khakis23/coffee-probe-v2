#include <doctest/doctest.h>

#include <array>
#include <span>

#include "core/settings.hpp"

namespace {

using core::decode_settings;
using core::encode_settings;
using core::Settings;
using core::SettingsStatus;
using Blob = std::array<uint8_t, core::kSettingsBlobSize>;

constexpr Settings kNonDefault{
    core::TcChip::Max6675, true, -37, 11, 40, 1, true, false,
};

void update_crc(Blob& blob) {
    const uint32_t crc = core::crc32(std::span{blob}.first<18>());
    for (std::size_t i = 0; i < 4; ++i) {
        blob[18 + i] = static_cast<uint8_t>(crc >> (8 * i));
    }
}

void check_repaired(Blob blob, const Settings& expected, uint8_t count) {
    update_crc(blob);
    const auto result = decode_settings(blob);
    CHECK(result.status == SettingsStatus::Repaired);
    CHECK(result.settings == expected);
    CHECK(result.repaired_fields == count);
}

void check_defaults(std::span<const uint8_t> blob, SettingsStatus status) {
    const auto result = decode_settings(blob);
    CHECK(result.status == status);
    CHECK(result.settings == Settings{});
    CHECK(result.repaired_fields == 0);
}

}  // namespace

TEST_CASE("Settings CRC matches the ISO-HDLC check value") {
    // The standard check uses ASCII digits 1 through 9. An empty input leaves
    // the initial all-ones state intact, which the final XOR reduces to zero.
    constexpr std::array<uint8_t, 9> digits{0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
    CHECK(core::crc32(digits) == 0xCBF43926);
    CHECK(core::crc32({}) == 0);
}

TEST_CASE("Default settings use the exact v1 header and payload bytes") {
    // ASCII CPST, LE version 1 and length 10, then defaults in v1 field order.
    constexpr std::array<uint8_t, 18> expected{
        0x43, 0x50, 0x53, 0x54, 1, 0, 10, 0,
        0, 0, 0, 0, 6, 0, 3, 0, 1, 0,
    };
    const auto blob = encode_settings({});
    CHECK(blob.size() == 22);
    for (std::size_t i = 0; i < expected.size(); ++i) {
        CAPTURE(i);
        CHECK(blob[i] == expected[i]);
    }
    const auto result = decode_settings(blob);
    CHECK(result.status == SettingsStatus::Ok);
    CHECK(result.settings == Settings{});
    CHECK(result.repaired_fields == 0);
}

TEST_CASE("Settings round trip every non-default field including a negative offset") {
    const auto blob = encode_settings(kNonDefault);
    // -37 is 65536 - 37 = 0xFFDB as a signed 16-bit little-endian offset.
    CHECK(blob[10] == 0xDB);
    CHECK(blob[11] == 0xFF);
    const auto result = decode_settings(blob);
    CHECK(result.status == SettingsStatus::Ok);
    CHECK(result.settings == kNonDefault);
    CHECK(result.repaired_fields == 0);
}

TEST_CASE("Settings repair one invalid field and preserve every other field") {
    const Settings defaults{};
    auto expected = kNonDefault;
    auto blob = encode_settings(kNonDefault);

    // The offset limits are -100 and 100 tenths of a degree. +101 is 0x0065,
    // and -101 is 65536 - 101 = 0xFF9B. Both reset just the offset to zero.
    expected.offset_deci_f = defaults.offset_deci_f;
    blob[10] = 0x65;
    blob[11] = 0x00;
    check_repaired(blob, expected, 1);
    blob[10] = 0x9B;
    blob[11] = 0xFF;
    check_repaired(blob, expected, 1);

    const auto check_byte = [](std::size_t index, uint8_t value, const Settings& repaired) {
        CAPTURE(index);
        CAPTURE(value);
        auto patched = encode_settings(kNonDefault);
        patched[index] = value;
        check_repaired(patched, repaired, 1);
    };

    // Only channel 1, 6 or 11 is allowed. The default is 6.
    expected = kNonDefault;
    expected.wifi_channel = defaults.wifi_channel;
    for (const uint8_t channel : {2, 0, 12}) {
        check_byte(12, channel, expected);
    }

    // TX power permits 0 or 8 through 84 quarter-dBm. Brightness permits 0 through 3.
    expected = kNonDefault;
    expected.tx_power_qdbm = defaults.tx_power_qdbm;
    check_byte(13, 7, expected);
    check_byte(13, 85, expected);
    expected = kNonDefault;
    expected.brightness = defaults.brightness;
    check_byte(14, 4, expected);

    // The chip enum and each boolean permit only wire values 0 and 1.
    expected = kNonDefault;
    expected.tc_chip = defaults.tc_chip;
    check_byte(8, 2, expected);
    expected = kNonDefault;
    expected.grounded_probe = defaults.grounded_probe;
    check_byte(9, 2, expected);
    expected = kNonDefault;
    expected.display_flip = defaults.display_flip;
    check_byte(15, 2, expected);
    expected = kNonDefault;
    expected.show_password = defaults.show_password;
    check_byte(16, 2, expected);
}

TEST_CASE("Settings accept the TX power sentinel and both range endpoints") {
    // Zero selects board power. 2 and 21 dBm are 8 and 84 quarter-dBm.
    for (const uint8_t power : {0, 8, 84}) {
        auto settings = kNonDefault;
        settings.tx_power_qdbm = power;
        const auto result = decode_settings(encode_settings(settings));
        CHECK(result.status == SettingsStatus::Ok);
        CHECK(result.settings == settings);
        CHECK(result.repaired_fields == 0);
    }
}

TEST_CASE("Settings count both repairs and retain the calibration offset") {
    // Channel 2 and a boolean byte of 2 both reset to their respective defaults.
    auto blob = encode_settings(kNonDefault);
    blob[12] = 2;
    blob[16] = 2;
    auto expected = kNonDefault;
    expected.wifi_channel = 6;
    expected.show_password = true;
    check_repaired(blob, expected, 2);
}

TEST_CASE("Missing and corrupt settings return defaults") {
    // A complete v1 blob is 8 header bytes + 10 payload bytes + 4 CRC bytes.
    const auto valid = encode_settings(kNonDefault);
    check_defaults({}, SettingsStatus::Missing);
    check_defaults(std::span{valid}.first<21>(), SettingsStatus::Corrupt);
    std::array<uint8_t, 23> oversized{};
    for (std::size_t i = 0; i < valid.size(); ++i) {
        oversized[i] = valid[i];
    }
    check_defaults(oversized, SettingsStatus::Corrupt);

    auto blob = valid;
    blob[0] = 'X';  // The first magic byte must be C.
    update_crc(blob);
    check_defaults(blob, SettingsStatus::Corrupt);
    blob = valid;
    blob[6] = 9;  // The payload length must be 10.
    update_crc(blob);
    check_defaults(blob, SettingsStatus::Corrupt);
    blob = valid;
    blob[10] ^= 1;  // A payload bit flip invalidates the stored CRC.
    check_defaults(blob, SettingsStatus::Corrupt);
    blob = valid;
    blob[18] ^= 1;  // A CRC bit flip also invalidates the match.
    check_defaults(blob, SettingsStatus::Corrupt);
}

TEST_CASE("Settings validate integrity before checking the version") {
    auto blob = encode_settings(kNonDefault);
    blob[4] = 2;  // The only supported version is LE 1.
    check_defaults(blob, SettingsStatus::Corrupt);
    update_crc(blob);
    check_defaults(blob, SettingsStatus::UnknownVersion);
    blob[4] = 0;  // Version zero is invalid even with a valid CRC.
    update_crc(blob);
    check_defaults(blob, SettingsStatus::Corrupt);
}

TEST_CASE("Settings ignore the reserved payload byte") {
    auto blob = encode_settings(kNonDefault);
    blob[17] = 0xFF;  // The last v1 payload byte has no effect on any field.
    update_crc(blob);
    const auto result = decode_settings(blob);
    CHECK(result.status == SettingsStatus::Ok);
    CHECK(result.settings == kNonDefault);
    CHECK(result.repaired_fields == 0);
}

TEST_CASE("A later version's blob of a different size reads as UnknownVersion, not Corrupt") {
    // A v2 blob with a 12-byte payload: 8 + 12 + 4 = 24 bytes, CRC at offset 20.
    std::array<uint8_t, 24> v2{};
    v2[0] = 'C';
    v2[1] = 'P';
    v2[2] = 'S';
    v2[3] = 'T';
    v2[4] = 2;   // Version 2, little-endian.
    v2[6] = 12;  // Payload length 12.
    const uint32_t crc = core::crc32(std::span{v2}.first(20));
    for (int i = 0; i < 4; ++i) {
        v2[static_cast<std::size_t>(20 + i)] = static_cast<uint8_t>(crc >> (8 * i));
    }
    const auto load = core::decode_settings(v2);
    CHECK(load.status == core::SettingsStatus::UnknownVersion);
    CHECK(load.settings == core::Settings{});

    // The same 24-byte layout claiming version 1 is Corrupt: v1 payloads are exactly 10 bytes.
    v2[4] = 1;
    const uint32_t crc1 = core::crc32(std::span{v2}.first(20));
    for (int i = 0; i < 4; ++i) {
        v2[static_cast<std::size_t>(20 + i)] = static_cast<uint8_t>(crc1 >> (8 * i));
    }
    CHECK(core::decode_settings(v2).status == core::SettingsStatus::Corrupt);

    // Shorter than a header plus a CRC: Corrupt, never an out-of-bounds read.
    const std::array<uint8_t, 11> tiny{'C', 'P', 'S', 'T', 1, 0, 0, 0, 0, 0, 0};
    CHECK(core::decode_settings(tiny).status == core::SettingsStatus::Corrupt);
}
