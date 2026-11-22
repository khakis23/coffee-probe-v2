#include <doctest/doctest.h>

#include <cstdint>

#include "core/temperature.hpp"

using core::DeciC;
using core::DeciF;

TEST_CASE("Celsius and Fahrenheit conversions agree at known temperatures") {
    CHECK(core::to_f({0}) == DeciF{320});
    CHECK(core::to_f({1000}) == DeciF{2120});
    CHECK(core::to_f({-400}) == DeciF{-400});
    CHECK(core::to_c({320}) == DeciC{0});
    CHECK(core::to_c({2120}) == DeciC{1000});
    CHECK(core::to_c({-400}) == DeciC{-400});
}

TEST_CASE("Integer division rounds nearest with halves away from zero") {
    CHECK(core::div_round_away(1, 2) == 1);
    CHECK(core::div_round_away(-1, 2) == -1);
    CHECK(core::div_round_away(1, 3) == 0);
    CHECK(core::div_round_away(-1, 3) == 0);
    CHECK(core::div_round_away(INT32_MIN, 1) == INT32_MIN);
    CHECK(core::div_round_away(INT32_MAX, 1) == INT32_MAX);
}

TEST_CASE("Quarter and sixteenth degree inputs round to tenths") {
    // +/-0.25 C is +/-2.5 tenths; +/-0.0625 C is +/-0.625 tenths.
    CHECK(core::from_quarter_c(1) == DeciC{3});
    CHECK(core::from_quarter_c(-1) == DeciC{-3});
    CHECK(core::from_sixteenth_c(1) == DeciC{1});
    CHECK(core::from_sixteenth_c(-1) == DeciC{-1});
}

TEST_CASE("Temperatures outside the int16 range saturate") {
    CHECK(core::clamp_i16(INT16_MAX + 1) == INT16_MAX);
    CHECK(core::clamp_i16(INT16_MIN - 1) == INT16_MIN);
    CHECK(core::to_f({INT16_MAX}) == DeciF{INT16_MAX});
    CHECK(core::to_f({INT16_MIN}) == DeciF{INT16_MIN});
    CHECK(core::from_quarter_c(INT32_MAX) == DeciC{INT16_MAX});
    CHECK(core::from_quarter_c(INT32_MIN) == DeciC{INT16_MIN});
    CHECK(core::from_sixteenth_c(INT32_MAX) == DeciC{INT16_MAX});
    CHECK(core::from_sixteenth_c(INT32_MIN) == DeciC{INT16_MIN});
}

TEST_CASE("Celsius round-trips through Fahrenheit within one tenth without saturation") {
    // Both endpoints remain representable in Fahrenheit.
    for (int32_t value = -18382; value <= 18026; ++value) {
        const DeciC original{static_cast<int16_t>(value)};
        const int32_t error = core::to_c(core::to_f(original)).v - original.v;
        REQUIRE((error >= -1 && error <= 1));
    }
}
