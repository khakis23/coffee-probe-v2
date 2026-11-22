#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "core/modbus.hpp"

namespace {

using core::RegisterBlock;
using core::modbus::frame_length;
using core::modbus::handle_request;
using core::modbus::kMaxAduSize;

constexpr std::array<uint8_t, 12> kArtisanPoll{
    0x00, 0x01, 0x00, 0x00, 0x00, 0x06, 0x01, 0x04, 0x00, 0x00, 0x00, 0x01,
};

void check_response(std::span<const uint8_t> request, const RegisterBlock& regs,
                    std::span<const uint8_t> expected) {
    std::array<uint8_t, kMaxAduSize> response{};
    REQUIRE(handle_request(request, regs, std::span{response}.first(expected.size())) ==
            expected.size());
    CHECK(std::equal(expected.begin(), expected.end(), response.begin()));
}

}  // namespace

TEST_CASE("Modbus encodes the spec's holding register example at address zero") {
    // Modbus Application Protocol V1.1b3, section 6.3. The example's response PDU
    // is unchanged when its three register values are placed at address zero.
    const RegisterBlock regs{{0x022B, 0x0000, 0x0064}};
    constexpr std::array<uint8_t, 12> request{
        0x00, 0x01, 0x00, 0x00, 0x00, 0x06, 0x11, 0x03, 0x00, 0x00, 0x00, 0x03,
    };
    constexpr std::array<uint8_t, 15> expected{
        0x00, 0x01, 0x00, 0x00, 0x00, 0x09, 0x11, 0x03, 0x06, 0x02, 0x2B, 0x00, 0x00, 0x00, 0x64,
    };
    check_response(request, regs, expected);
}

TEST_CASE("Modbus returns bean temperature for the Artisan poll") {
    RegisterBlock regs;
    regs.regs[core::reg::kBtF10] = core::to_reg(1234);
    constexpr std::array<uint8_t, 11> expected{
        0x00, 0x01, 0x00, 0x00, 0x00, 0x05, 0x01, 0x04, 0x02, 0x04, 0xD2,
    };
    check_response(kArtisanPoll, regs, expected);
}

TEST_CASE("Modbus returns the signed Artisan fault value as FF F6") {
    RegisterBlock regs;
    regs.regs[core::reg::kBtF10] = core::to_reg(core::kFaultValue);
    constexpr std::array<uint8_t, 11> expected{
        0x00, 0x01, 0x00, 0x00, 0x00, 0x05, 0x01, 0x04, 0x02, 0xFF, 0xF6,
    };
    check_response(kArtisanPoll, regs, expected);
}

TEST_CASE("Modbus rejects write function codes") {
    // Application Protocol V1.1b3, sections 6.5, 6.6, 6.11, 6.12 and 7.
    constexpr std::array<uint8_t, 12> write_coil{
        0xAB, 0xCD, 0x00, 0x00, 0x00, 0x06, 0x00, 0x05, 0x00, 0x00, 0xFF, 0x00,
    };
    constexpr std::array<uint8_t, 12> write_register{
        0xAB, 0xCD, 0x00, 0x00, 0x00, 0x06, 0x00, 0x06, 0x00, 0x00, 0x00, 0x01,
    };
    constexpr std::array<uint8_t, 14> write_coils{
        0xAB, 0xCD, 0x00, 0x00, 0x00, 0x08, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01,
    };
    constexpr std::array<uint8_t, 15> write_registers{
        0xAB, 0xCD, 0x00, 0x00, 0x00, 0x09, 0x00, 0x10, 0x00, 0x00, 0x00, 0x01, 0x02, 0x00, 0x01,
    };
    const std::array<std::span<const uint8_t>, 4> requests{
        write_coil, write_register, write_coils, write_registers,
    };
    constexpr std::array<uint8_t, 4> exception_functions{0x85, 0x86, 0x8F, 0x90};
    for (std::size_t i = 0; i < requests.size(); ++i) {
        CAPTURE(i);
        const std::array<uint8_t, 9> expected{
            0xAB, 0xCD, 0x00, 0x00, 0x00, 0x03, 0x00, exception_functions[i], 0x01,
        };
        check_response(requests[i], {}, expected);
    }
}

TEST_CASE("Modbus rejects read quantities outside 1 to 125") {
    // Application Protocol V1.1b3, section 6.4.
    constexpr std::array<uint8_t, 9> expected{
        0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x01, 0x84, 0x03,
    };
    auto request = kArtisanPoll;
    request[11] = 0;
    check_response(request, {}, expected);
    request[11] = 126;
    check_response(request, {}, expected);
}

TEST_CASE("Modbus rejects reads past the register block") {
    constexpr std::array<uint8_t, 9> expected{
        0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x01, 0x84, 0x02,
    };
    auto request = kArtisanPoll;
    request[9] = 8;
    request[11] = 2;
    check_response(request, {}, expected);
    request[9] = 9;
    request[11] = 1;
    check_response(request, {}, expected);
}

TEST_CASE("Modbus rejects an incorrectly sized read PDU in a complete frame") {
    constexpr std::array<uint8_t, 13> request{
        0x00, 0x01, 0x00, 0x00, 0x00, 0x07, 0x01, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00,
    };
    constexpr std::array<uint8_t, 9> expected{
        0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x01, 0x84, 0x03,
    };
    check_response(request, {}, expected);
}

TEST_CASE("Modbus rejects every truncated prefix of a request") {
    std::array<uint8_t, kMaxAduSize> response{};
    for (std::size_t length = 0; length < kArtisanPoll.size(); ++length) {
        CAPTURE(length);
        CHECK(handle_request(std::span{kArtisanPoll}.first(length), {}, response) == 0);
    }
    CHECK(handle_request(kArtisanPoll, {}, response) == 11);
}

TEST_CASE("Modbus rejects invalid MBAP fields and oversized frames") {
    // Modbus Messaging on TCP/IP Implementation Guide V1.0b, section 3.1.3.
    std::array<uint8_t, kMaxAduSize> response{};
    auto request = kArtisanPoll;
    request[3] = 1;
    CHECK(handle_request(request, {}, response) == 0);
    request = kArtisanPoll;
    request[5] = 7;
    CHECK(handle_request(request, {}, response) == 0);
    request[5] = 1;
    CHECK(handle_request(request, {}, response) == 0);

    std::array<uint8_t, 261> oversized{};
    oversized[4] = 0x00;
    oversized[5] = 0xFF;  // 261 bytes overall, with a matching length field.
    oversized[7] = 0x04;
    CHECK(handle_request(oversized, {}, response) == 0);
}

TEST_CASE("Modbus needs the full response buffer for both reads and exceptions") {
    std::array<uint8_t, 10> response{};
    CHECK(handle_request(kArtisanPoll, {}, response) == 0);
    auto request = kArtisanPoll;
    request[11] = 0;
    CHECK(handle_request(request, {}, std::span{response}.first<8>()) == 0);
}

TEST_CASE("Modbus frame length can be read after the first six bytes") {
    for (std::size_t length = 0; length < 6; ++length) {
        CAPTURE(length);
        CHECK_FALSE(frame_length(std::span{kArtisanPoll}.first(length)).has_value());
    }
    const auto length = frame_length(std::span{kArtisanPoll}.first<6>());
    REQUIRE(length.has_value());
    CHECK(*length == 12);

    constexpr std::array<uint8_t, 6> max_header{0x00, 0x00, 0x00, 0x00, 0x00, 0xFE};
    const auto max_length = frame_length(max_header);
    REQUIRE(max_length.has_value());
    CHECK(*max_length == 260);
}

TEST_CASE("Modbus reads all nine registers in order") {
    const RegisterBlock regs{{0x0102, 0x0304, 0x0506, 0x0708, 0x090A, 0x0B0C, 0x0D0E, 0x0F10,
                              0x1112}};
    constexpr std::array<uint8_t, 12> request{
        0x12, 0x34, 0x00, 0x00, 0x00, 0x06, 0xFF, 0x04, 0x00, 0x00, 0x00, 0x09,
    };
    constexpr std::array<uint8_t, 27> expected{
        0x12, 0x34, 0x00, 0x00, 0x00, 0x15, 0xFF, 0x04, 0x12,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
        0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12,
    };
    check_response(request, regs, expected);
}
