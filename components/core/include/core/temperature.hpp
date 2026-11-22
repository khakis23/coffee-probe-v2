#pragma once

#include <algorithm>
#include <cstdint>

namespace core {

// Temperatures are stored in tenths of a degree.
struct DeciC {
    int16_t v;
    friend constexpr bool operator==(DeciC, DeciC) = default;
};

struct DeciF {
    int16_t v;
    friend constexpr bool operator==(DeciF, DeciF) = default;
};

constexpr int16_t clamp_i16(int32_t value) {
    return static_cast<int16_t>(std::clamp<int32_t>(value, INT16_MIN, INT16_MAX));
}

// The denominator must be positive.
constexpr int32_t div_round_away(int32_t num, int32_t den) {
    const int32_t quotient = num / den;
    const int32_t remainder = num % den;
    const int32_t half = den / 2 + den % 2;
    if (remainder >= half) {
        return quotient + 1;
    }
    if (remainder <= -half) {
        return quotient - 1;
    }
    return quotient;
}

constexpr DeciF to_f(DeciC c) {
    const int32_t c10 = c.v;
    return {clamp_i16(div_round_away(c10 * 9, 5) + 320)};
}

constexpr DeciC to_c(DeciF f) {
    const int32_t f10 = f.v;
    return {clamp_i16(div_round_away((f10 - 320) * 5, 9))};
}

constexpr DeciC from_quarter_c(int32_t quarters) {
    // Values beyond the multiplication limits already saturate the result.
    const int32_t value = std::clamp<int32_t>(quarters, INT32_MIN / 10, INT32_MAX / 10);
    return {clamp_i16(div_round_away(value * 10, 4))};
}

constexpr DeciC from_sixteenth_c(int32_t sixteenths) {
    const int32_t value = std::clamp<int32_t>(sixteenths, INT32_MIN / 10, INT32_MAX / 10);
    return {clamp_i16(div_round_away(value * 10, 16))};
}

static_assert(to_f({0}) == DeciF{320});
static_assert(to_f({1000}) == DeciF{2120});
static_assert(to_f({-400}) == DeciF{-400});

}  // namespace core
