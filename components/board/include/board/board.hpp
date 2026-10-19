// board.hpp — the one place GPIO numbers live.
//
// Every driver takes its pins from `board::kBoard` instead of writing GPIO_NUM_x itself, so
// porting to another C3 board means editing this file (plus a Kconfig entry and an
// sdkconfig.defaults.<board> file) and nothing else.
//
// How the board is chosen: components/board/Kconfig defines a choice PROBE_BOARD. The selected
// option ends up as `#define CONFIG_PROBE_BOARD_XIAO_C3 1` (or ..._GENERIC_C3) in the generated
// sdkconfig.h, and the #if below picks the matching table at compile time.
//
// Why constexpr: the table is a compile-time constant, so it costs no RAM, the compiler folds
// each pin into the driver call as an immediate value, and we can check it with static_assert
// (e.g. "no two signals share a pin") so a bad edit fails the build instead of the hardware.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "driver/gpio.h"  // gpio_num_t, GPIO_NUM_x, GPIO_NUM_NC (= -1, "not connected")
#include "sdkconfig.h"    // CONFIG_* macros from Kconfig

namespace board {

struct Pins {
    // I2C OLED
    gpio_num_t i2c_sda;
    gpio_num_t i2c_scl;
    // Thermocouple amplifier (MAX31855 / MAX6675) on SPI. Read-only chip, so no MOSI.
    gpio_num_t tc_sck;
    gpio_num_t tc_miso;
    gpio_num_t tc_cs;
    // Buttons (active low, internal pull-ups)
    gpio_num_t btn_up;
    gpio_num_t btn_down;
    gpio_num_t btn_ok;
    gpio_num_t btn_back;
    // Battery voltage divider on an ADC1 pin, or GPIO_NUM_NC if the board has none.
    gpio_num_t battery_adc;
};

struct Board {
    const char* name;
    Pins pins;
    // Upper limit for esp_wifi_set_max_tx_power(), which takes units of 0.25 dBm
    // (so 80 = 20 dBm; the C3 maximum is 84 = 21 dBm).
    int8_t wifi_max_tx_power_qdbm;
};

#if CONFIG_PROBE_BOARD_XIAO_C3

// Seeed Studio XIAO ESP32C3. Pin plan: docs/architecture.md → "Hardware / pin plan".
// C3 strapping pins are GPIO2, GPIO8 and GPIO9: the boot ROM samples them at reset to choose the
// boot mode, so nothing external may pull them the wrong way during reset. After boot they are
// ordinary GPIOs.
inline constexpr Board kBoard{
    .name = "Seeed XIAO ESP32C3",
    .pins =
        {
            .i2c_sda = GPIO_NUM_6,      // D4
            .i2c_scl = GPIO_NUM_7,      // D5
            .tc_sck = GPIO_NUM_8,       // D8, strapping: fine as an output after boot
            .tc_miso = GPIO_NUM_9,      // D9, strapping + BOOT button; MAX is hi-Z while CS high
            .tc_cs = GPIO_NUM_10,       // D10, 10 kΩ pull-up keeps the MAX quiet during boot
            .btn_up = GPIO_NUM_3,       // D1
            .btn_down = GPIO_NUM_4,     // D2
            .btn_ok = GPIO_NUM_5,       // D3
            .btn_back = GPIO_NUM_20,    // D7, UART0 RX: free because the console is USB only
            .battery_adc = GPIO_NUM_2,  // D0, ADC1, strapping: divider must not upset boot
        },
    // The XIAO's small antenna/regulator is known to misbehave at full power; 8.5 dBm is the
    // commonly used safe cap. Revisit when the Wi-Fi tickets (M3) measure range.
    .wifi_max_tx_power_qdbm = 34,
};

#elif CONFIG_PROBE_BOARD_GENERIC_C3

// Generic ESP32-C3 DevKit (DevKitM-1, SuperMini, ...). Pins that are off limits:
//   GPIO11-17  SPI flash (and VDD_SPI) on most modules
//   GPIO18/19  native USB D-/D+ (keeps USB-Serial-JTAG flashing and logs working)
//   GPIO20/21  UART0 RX/TX: this profile keeps the UART console for USB-UART bridge boards
// That leaves 0-10. GPIO2/8/9 are strapping pins, and GPIO8 drives the RGB LED on many DevKits,
// so we use only one of them, GPIO9, for the BACK button: DevKits already wire their BOOT button
// to GPIO9 (with a pull-up), so the onboard BOOT button doubles as BACK. Holding it while
// pressing RESET enters download mode, which is what that button does anyway.
// SPI and I2C go through the GPIO matrix, so any free pin works for them at these slow clocks.
inline constexpr Board kBoard{
    .name = "Generic ESP32-C3 DevKit",
    .pins =
        {
            .i2c_sda = GPIO_NUM_0,  // GPIO0/1 are the 32 kHz crystal pins; unused on DevKits
            .i2c_scl = GPIO_NUM_1,
            .tc_sck = GPIO_NUM_6,
            .tc_miso = GPIO_NUM_5,
            .tc_cs = GPIO_NUM_10,  // add a 10 kΩ pull-up, as on the XIAO
            .btn_up = GPIO_NUM_3,
            .btn_down = GPIO_NUM_4,
            .btn_ok = GPIO_NUM_7,
            .btn_back = GPIO_NUM_9,  // strapping pin = onboard BOOT button
            .battery_adc = GPIO_NUM_NC,
        },
    .wifi_max_tx_power_qdbm = 78,  // 19.5 dBm, just under the C3 maximum
};

#else
#error "No board profile selected: set PROBE_BOARD in menuconfig (see docs/dev-setup.md)"
#endif

// The XIAO uses GPIO20 (UART0 RX) as the BACK button, so a UART console would fight it. This
// catches an sdkconfig that was generated for a different board (delete sdkconfig and rebuild).
#if CONFIG_PROBE_BOARD_XIAO_C3 && CONFIG_ESP_CONSOLE_UART
#error "XIAO profile needs the console on USB-Serial-JTAG only; delete sdkconfig and rebuild"
#endif

// ---- Compile-time checks ---------------------------------------------------------------------

// All signals as one array so constexpr code can loop over them.
constexpr std::array<gpio_num_t, 10> all_pins(const Pins& p) {
    return {p.i2c_sda, p.i2c_scl, p.tc_sck,  p.tc_miso,  p.tc_cs,
            p.btn_up,  p.btn_down, p.btn_ok, p.btn_back, p.battery_adc};
}

// True if no two connected signals use the same GPIO (GPIO_NUM_NC may repeat).
// constexpr, so the compiler runs this loop during the build for the static_assert below.
constexpr bool pins_unique(const Pins& p) {
    const auto pins = all_pins(p);
    for (std::size_t i = 0; i < pins.size(); ++i) {
        for (std::size_t j = i + 1; j < pins.size(); ++j) {
            if (pins[i] != GPIO_NUM_NC && pins[i] == pins[j]) {
                return false;
            }
        }
    }
    return true;
}

static_assert(pins_unique(kBoard.pins), "board.hpp: two signals share a GPIO");
static_assert(kBoard.wifi_max_tx_power_qdbm >= 8 && kBoard.wifi_max_tx_power_qdbm <= 84,
              "board.hpp: Wi-Fi TX power must be 2..21 dBm (8..84 quarter-dBm)");

}  // namespace board
