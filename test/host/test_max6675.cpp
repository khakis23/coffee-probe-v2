#include <doctest/doctest.h>

#include "core/thermocouple.hpp"

using core::DeciC;
using core::TcFault;

TEST_CASE("MAX6675 decodes unsigned quarter degrees and ignores D0") {
    constexpr struct {
        uint16_t frame;
        DeciC hot;
    } vectors[]{
        {0x0000, {0}},      // Digitization: all temperature bits low gives 0 C.
        {0x0008, {3}},      // Figure 2: D3 is the LSB, yielding 0.25 C.
        {0x0320, {250}},    // Figure 2: 100 counts in D14..D3 gives 25 C.
        {0x0321, {250}},    // Figure 2: same 25 C with the three-state D0 high.
        {0x1908, {2003}},   // Figure 2: 801 counts in D14..D3 gives 200.25 C.
        {0x4000, {5120}},   // Figure 2: only D14 set gives 512 C.
        {0x7FF8, {10238}},  // Digitization: all temperature bits high gives 1023.75 C.
    };

    for (const auto& vector : vectors) {
        CAPTURE(vector.frame);
        const auto reading = core::decode_max6675(vector.frame);
        CHECK(reading.fault == TcFault::None);
        CHECK(reading.hot == vector.hot);
        CHECK_FALSE(reading.cold_junction.has_value());
    }
}

TEST_CASE("MAX6675 reports an open probe without either temperature") {
    // MAX6675 Figure 2: D2 is set with 25 C in the temperature field.
    const auto reading = core::decode_max6675(0x0324);
    CHECK(reading.fault == TcFault::Open);
    CHECK(reading.hot == DeciC{0});
    CHECK_FALSE(reading.cold_junction.has_value());
}

TEST_CASE("MAX6675 rejects invalid bus frames before checking for an open probe") {
    constexpr uint16_t frames[]{
        0xFFFF,  // Project bus check: all bits high.
        0x8320,  // MAX6675 Figure 2: dummy D15 must read low.
        0x0322,  // Figure 2: device ID D1 must read low.
        0x8324,  // Figure 2: invalid D15 with open D2.
        0x0326,  // Figure 2: invalid D1 with open D2.
    };

    for (const uint16_t frame : frames) {
        CAPTURE(frame);
        const auto reading = core::decode_max6675(frame);
        CHECK(reading.fault == TcFault::BusError);
        CHECK(reading.hot == DeciC{0});
        CHECK_FALSE(reading.cold_junction.has_value());
    }
}
