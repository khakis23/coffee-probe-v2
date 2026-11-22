// CoffeeProbe — bring-up diagnostics (#2/#3).
//
// Temporary firmware mode that reports what hardware is actually on the bench:
// which I2C addresses answer, whether the OLED is an SSD1306 or SH1106, and
// whether the thermocouple chip is a MAX31855 or a MAX6675.

#pragma once

#include <cstdint>

namespace bringup {

// Runs the diagnostics forever. Never returns.
[[noreturn]] void run();

// --- Pure frame decoders (no hardware access) -------------------------------
// These will move to components/core later and are the reference for the
// MAX31855 / MAX6675 decoder tickets.

struct Max31855Frame {
    float thermocouple_c;  // D31..D18, 14-bit signed, 0.25 °C per LSB
    float internal_c;      // D15..D4, 12-bit signed, 0.0625 °C per LSB (cold junction)
    bool fault;            // D16, set if any of the three below is set
    bool short_to_vcc;     // D2 (SCV)
    bool short_to_gnd;     // D1 (SCG)
    bool open_circuit;     // D0 (OC)
};

struct Max6675Frame {
    float thermocouple_c;  // D14..D3, 12-bit unsigned, 0.25 °C per LSB
    bool open_circuit;     // D2
    bool reserved_ok;      // D15 (dummy sign bit) and D1 (device ID) must both read 0
};

// `frame` is the 32-bit word as clocked in: the first bit received is bit 31.
Max31855Frame decode_max31855(uint32_t frame);

// A MAX6675 only shifts out 16 meaningful bits, so this decodes the TOP 16
// bits of a 32-bit read and ignores the rest.
Max6675Frame decode_max6675(uint32_t frame);

constexpr float c_to_f(float c) { return c * 9.0f / 5.0f + 32.0f; }

}  // namespace bringup
