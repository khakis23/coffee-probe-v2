#include <doctest/doctest.h>

#include "core/thermocouple.hpp"

using core::DeciC;
using core::TcFault;

TEST_CASE("Sign extension handles both MAX31855 field widths") {
    CHECK(core::detail::sign_extend<14>(0x1FFF) == 8191);
    CHECK(core::detail::sign_extend<14>(0x2000) == -8192);
    CHECK(core::detail::sign_extend<14>(0x3FFF) == -1);
    CHECK(core::detail::sign_extend<12>(0x000) == 0);
    CHECK(core::detail::sign_extend<12>(0x7FF) == 2047);
    CHECK(core::detail::sign_extend<12>(0x800) == -2048);
    CHECK(core::detail::sign_extend<12>(0x1FFF) == -1);
}

TEST_CASE("MAX31855 decodes datasheet thermocouple and reference junction temperatures") {
    constexpr struct {
        uint32_t hot_field;
        uint32_t cold_field;
        DeciC hot;
        DeciC cold;
    } vectors[]{
        // MAX31855 Table 4 hot row +1600.00 C and Table 5 cold row +25.0000 C.
        {0b0110'0100'0000'00, 0b0001'1001'0000, {16000}, {250}},
        // Table 4 hot row +1000.00 C and Table 5 cold row +25.0000 C.
        {0b0011'1110'1000'00, 0b0001'1001'0000, {10000}, {250}},
        // Table 4 hot row +100.75 C and Table 5 cold row +25.0000 C.
        {0b0000'0110'0100'11, 0b0001'1001'0000, {1008}, {250}},
        // Table 4 hot row +25.00 C and Table 5 cold row +25.0000 C.
        {0b0000'0001'1001'00, 0b0001'1001'0000, {250}, {250}},
        // Table 4 hot row 0.00 C and Table 5 cold row +25.0000 C.
        {0b0000'0000'0000'00, 0b0001'1001'0000, {0}, {250}},
        // Table 4 hot row -0.25 C and Table 5 cold row -0.0625 C.
        {0b1111'1111'1111'11, 0b1111'1111'1111, {-3}, {-1}},
        // Table 4 hot row -1.00 C and Table 5 cold row -0.0625 C.
        {0b1111'1111'1111'00, 0b1111'1111'1111, {-10}, {-1}},
        // Table 4 hot row -250.00 C and Table 5 cold row -0.0625 C.
        {0b1111'0000'0110'00, 0b1111'1111'1111, {-2500}, {-1}},
        // Table 3 field limits: +2047.75 C hot and +127.9375 C cold.
        {0x1FFF, 0x7FF, {20478}, {1279}},
        // Table 3 field limits: -2048 C hot and -128 C cold.
        {0x2000, 0x800, {-20480}, {-1280}},
    };

    for (const auto& vector : vectors) {
        const uint32_t frame = (vector.hot_field << 18) | (vector.cold_field << 4);
        CAPTURE(frame);
        const auto reading = core::decode_max31855(frame);
        CHECK(reading.fault == TcFault::None);
        CHECK(reading.hot == vector.hot);
        REQUIRE(reading.cold_junction.has_value());
        CHECK(*reading.cold_junction == vector.cold);
    }
}

TEST_CASE("MAX31855 keeps the cold junction on sensor faults and prioritizes their causes") {
    constexpr struct {
        uint32_t cause;
        TcFault fault;
    } vectors[]{
        // MAX31855 Table 3: D0 open, D1 short to ground, D2 short to VCC.
        {0b001, TcFault::Open},
        {0b010, TcFault::ShortGnd},
        {0b100, TcFault::ShortVcc},
        // Project priority for simultaneous Table 3 causes.
        {0b011, TcFault::Open},
        {0b101, TcFault::Open},
        {0b110, TcFault::ShortGnd},
        {0b111, TcFault::Open},
    };

    for (const auto& vector : vectors) {
        // Table 4 hot +25.00 C, Table 5 cold +25.0000 C, Table 3 D16 set.
        const uint32_t frame = 0x01911900 | vector.cause;
        CAPTURE(frame);
        const auto reading = core::decode_max31855(frame);
        CHECK(reading.fault == vector.fault);
        CHECK(reading.hot == DeciC{0});
        REQUIRE(reading.cold_junction.has_value());
        CHECK(*reading.cold_junction == DeciC{250});
    }
}

TEST_CASE("MAX31855 rejects invalid bus frames without either temperature") {
    constexpr uint32_t frames[]{
        0x00000000,  // Project bus check: all bits low.
        0xFFFFFFFF,  // Project bus check: all bits high.
        0x01911900,  // Table 3: D16 is set with none of D0..D2.
        0x01901901,  // Table 3: D0 cause without D16.
        0x01901902,  // Table 3: D1 cause without D16.
        0x01901904,  // Table 3: D2 cause without D16.
        0x01921900,  // Table 3: D17 reserved.
        0x01901908,  // Table 3: D3 reserved.
        0x01931901,  // Table 3: D17 reserved with D16 and D0 set.
        0x01911909,  // Table 3: D3 reserved with D16 and D0 set.
    };

    for (const uint32_t frame : frames) {
        CAPTURE(frame);
        const auto reading = core::decode_max31855(frame);
        CHECK(reading.fault == TcFault::BusError);
        CHECK(reading.hot == DeciC{0});
        CHECK_FALSE(reading.cold_junction.has_value());
    }
}
