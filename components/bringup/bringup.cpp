// CoffeeProbe — bring-up diagnostics (#2/#3). See bringup.hpp.
//
// Deliberately self-contained and verbose: this is throwaway bench code whose
// only job is to print enough to identify the OLED and thermocouple chips.

#include "bringup/bringup.hpp"

#include <array>
#include <cinttypes>
#include <cstring>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace bringup {

// ---------------------------------------------------------------------------
// Pure decoders
// ---------------------------------------------------------------------------

Max31855Frame decode_max31855(uint32_t frame) {
    Max31855Frame out{};
    // Casting to int32_t and then shifting right is an *arithmetic* shift in
    // C++20 and later: the sign bit is copied in from the left, so a 14-bit
    // two's-complement field at the top of the word comes out sign-extended.
    const int32_t tc_raw = static_cast<int32_t>(frame) >> 18;  // D31..D18
    // Same trick for D15..D4: shift the field up to the top first, then down.
    const int32_t internal_raw = static_cast<int32_t>(frame << 16) >> 20;
    out.thermocouple_c = static_cast<float>(tc_raw) * 0.25f;
    out.internal_c = static_cast<float>(internal_raw) * 0.0625f;
    out.fault = (frame & (1u << 16)) != 0;
    out.short_to_vcc = (frame & (1u << 2)) != 0;
    out.short_to_gnd = (frame & (1u << 1)) != 0;
    out.open_circuit = (frame & (1u << 0)) != 0;
    return out;
}

Max6675Frame decode_max6675(uint32_t frame) {
    const uint16_t word = static_cast<uint16_t>(frame >> 16);
    Max6675Frame out{};
    out.thermocouple_c = static_cast<float>((word >> 3) & 0x0FFF) * 0.25f;  // D14..D3
    out.open_circuit = (word & (1u << 2)) != 0;
    out.reserved_ok = (word & (1u << 15)) == 0 && (word & (1u << 1)) == 0;
    return out;
}

namespace {

constexpr const char* kTag = "bringup";

// ---------------------------------------------------------------------------
// Pin plan (XIAO ESP32C3 default board profile, docs/architecture.md)
// TODO(#4): use board.hpp
// ---------------------------------------------------------------------------
constexpr gpio_num_t kI2cSda = GPIO_NUM_6;
constexpr gpio_num_t kI2cScl = GPIO_NUM_7;
// GPIO8 and GPIO9 are strapping pins on the C3: their level at reset picks the
// boot mode (GPIO9 is also the BOOT button). That's fine for SPI once we've
// booted; the MAX chip's SO output is high-impedance while CS is high, so it
// can't disturb the straps during reset.
constexpr gpio_num_t kTcSck = GPIO_NUM_8;
constexpr gpio_num_t kTcMiso = GPIO_NUM_9;
// CS is active-low. The board has a 10 kΩ pull-up so the chip stays deselected
// (and its SO pin stays off the bus) until our SPI driver takes over the pin.
constexpr gpio_num_t kTcCs = GPIO_NUM_10;

// ---------------------------------------------------------------------------
// I2C scan
// ---------------------------------------------------------------------------
constexpr uint32_t kI2cHz = 100'000;
constexpr int kI2cTimeoutMs = 50;

// Returns the first OLED-looking address found (0x3C/0x3D), or 0 if none.
uint16_t i2c_scan(i2c_master_bus_handle_t bus) {
    ESP_LOGI(kTag, "I2C scan 0x08..0x77 on SDA=GPIO%d SCL=GPIO%d", kI2cSda, kI2cScl);
    uint16_t oled_addr = 0;
    int found = 0;
    for (uint16_t addr = 0x08; addr <= 0x77; ++addr) {
        // A probe is just "START, address+W, STOP": a device that ACKs exists.
        if (i2c_master_probe(bus, addr, kI2cTimeoutMs) == ESP_OK) {
            ++found;
            const bool oled = addr == 0x3C || addr == 0x3D;
            ESP_LOGI(kTag, "  found device at 0x%02X%s", addr, oled ? "  <- likely OLED" : "");
            if (oled && oled_addr == 0) oled_addr = addr;
        }
    }
    if (found == 0) {
        ESP_LOGW(kTag, "  no I2C devices answered: check SDA/SCL wiring, power, pull-ups");
    } else {
        ESP_LOGI(kTag, "I2C scan done: %d device(s)", found);
    }
    return oled_addr;
}

// ---------------------------------------------------------------------------
// SSD1306 test pattern
// ---------------------------------------------------------------------------
constexpr int kOledW = 128;
constexpr int kOledH = 64;
constexpr int kOledPages = kOledH / 8;  // 8 vertical pixels per byte ("page")

// Static so it lives in .bss rather than on the (small) main task stack, and
// so we never touch the heap.
std::array<uint8_t, kOledW * kOledPages> g_framebuffer{};

void set_pixel(int x, int y) {
    if (x < 0 || x >= kOledW || y < 0 || y >= kOledH) return;
    // SSD1306 memory layout: byte (page, column) holds 8 pixels stacked
    // vertically, bit 0 = top pixel of that page.
    g_framebuffer[static_cast<size_t>((y / 8) * kOledW + x)] |=
        static_cast<uint8_t>(1u << (y % 8));
}

void draw_test_pattern() {
    g_framebuffer.fill(0);
    // 1-px border around the whole panel.
    for (int x = 0; x < kOledW; ++x) {
        set_pixel(x, 0);
        set_pixel(x, kOledH - 1);
    }
    for (int y = 0; y < kOledH; ++y) {
        set_pixel(0, y);
        set_pixel(kOledW - 1, y);
    }
    // An X from corner to corner.
    for (int x = 0; x < kOledW; ++x) {
        const int y = x * (kOledH - 1) / (kOledW - 1);
        set_pixel(x, y);
        set_pixel(x, kOledH - 1 - y);
    }
    // Solid 8x8 block in the top-left corner, so a mirrored/flipped panel is obvious.
    for (int y = 3; y < 11; ++y) {
        for (int x = 3; x < 11; ++x) set_pixel(x, y);
    }
}

// SSD1306 I2C protocol: every write starts with a control byte.
// 0x00 = "the following bytes are commands", 0x40 = "the following bytes are pixel data".
constexpr uint8_t kCtrlCommand = 0x00;
constexpr uint8_t kCtrlData = 0x40;

esp_err_t oled_commands(i2c_master_dev_handle_t dev, const uint8_t* cmds, size_t n) {
    std::array<uint8_t, 32> buf{};
    if (n + 1 > buf.size()) return ESP_ERR_INVALID_SIZE;
    buf[0] = kCtrlCommand;
    std::memcpy(&buf[1], cmds, n);
    return i2c_master_transmit(dev, buf.data(), n + 1, kI2cTimeoutMs);
}

esp_err_t oled_init_and_draw(i2c_master_dev_handle_t dev) {
    // Standard SSD1306 128x64 init with the internal charge pump enabled
    // (the common modules have no external VCC supply for the panel).
    static constexpr uint8_t kInit[] = {
        0xAE,        // display off
        0xD5, 0x80,  // clock divide ratio / oscillator frequency
        0xA8, 0x3F,  // multiplex ratio = 64 rows
        0xD3, 0x00,  // display offset = 0
        0x40,        // start line = 0
        0x8D, 0x14,  // charge pump ON
        0x20, 0x00,  // horizontal addressing mode (SH1106 ignores this)
        0xA1,        // segment remap: column 127 -> SEG0 (normal left-right)
        0xC8,        // COM scan direction remapped (normal top-bottom)
        0xDA, 0x12,  // COM pins config for 128x64
        0x81, 0xCF,  // contrast
        0xD9, 0xF1,  // pre-charge period
        0xDB, 0x40,  // VCOMH deselect level
        0xA4,        // display follows RAM
        0xA6,        // normal (not inverted)
        0xAF,        // display on
    };
    esp_err_t err = oled_commands(dev, kInit, sizeof(kInit));
    if (err != ESP_OK) return err;

    // Write window = whole screen; the column pointer wraps to the next page.
    static constexpr uint8_t kWindow[] = {0x21, 0x00, kOledW - 1, 0x22, 0x00, kOledPages - 1};
    err = oled_commands(dev, kWindow, sizeof(kWindow));
    if (err != ESP_OK) return err;

    draw_test_pattern();

    // Send the framebuffer one page (128 bytes) at a time, each prefixed with
    // the data control byte.
    std::array<uint8_t, kOledW + 1> chunk{};
    chunk[0] = kCtrlData;
    for (int page = 0; page < kOledPages; ++page) {
        std::memcpy(&chunk[1], &g_framebuffer[static_cast<size_t>(page * kOledW)], kOledW);
        err = i2c_master_transmit(dev, chunk.data(), chunk.size(), kI2cTimeoutMs);
        if (err != ESP_OK) return err;
    }
    return ESP_OK;
}

void run_i2c_and_oled() {
    i2c_master_bus_config_t bus_cfg{};
    bus_cfg.i2c_port = -1;  // let the driver pick a free port
    bus_cfg.sda_io_num = kI2cSda;
    bus_cfg.scl_io_num = kI2cScl;
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7;
    // The ~45 kΩ internal pull-ups are weak but enough at 100 kHz if the OLED
    // module's own pull-ups are missing. Real pull-ups on the module win.
    bus_cfg.flags.enable_internal_pullup = 1;

    i2c_master_bus_handle_t bus = nullptr;
    esp_err_t err = i2c_new_master_bus(&bus_cfg, &bus);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "i2c_new_master_bus failed: %s", esp_err_to_name(err));
        return;
    }

    const uint16_t oled_addr = i2c_scan(bus);
    if (oled_addr == 0) {
        ESP_LOGW(kTag, "No OLED at 0x3C/0x3D, skipping display test");
        return;
    }

    i2c_device_config_t dev_cfg{};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = oled_addr;
    dev_cfg.scl_speed_hz = kI2cHz;

    i2c_master_dev_handle_t dev = nullptr;
    err = i2c_master_bus_add_device(bus, &dev_cfg, &dev);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "i2c_master_bus_add_device failed: %s", esp_err_to_name(err));
        return;
    }

    err = oled_init_and_draw(dev);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "OLED write failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(kTag, "OLED test pattern sent to 0x%02X (border + X + top-left block).", oled_addr);
    ESP_LOGI(kTag, "  Clean border = SSD1306.");
    ESP_LOGI(kTag, "  Border shifted ~2 px / garbage column at the right edge = SH1106.");
    ESP_LOGI(kTag, "  Blank screen: wrong controller, bad power, or no charge pump.");
}

// ---------------------------------------------------------------------------
// Thermocouple SPI
// ---------------------------------------------------------------------------
constexpr int kTcSpiHz = 1'000'000;
// MAX6675 needs ~220 ms to finish a conversion after CS goes high; reading
// faster just returns the previous result (or restarts the conversion).
constexpr int kTcPeriodMs = 1000;

spi_device_handle_t tc_init() {
    spi_bus_config_t bus_cfg{};
    bus_cfg.mosi_io_num = -1;  // MAX31855/MAX6675 are read-only: no MOSI pin
    bus_cfg.miso_io_num = kTcMiso;
    bus_cfg.sclk_io_num = kTcSck;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = 4;

    esp_err_t err = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_DISABLED);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "spi_bus_initialize failed: %s", esp_err_to_name(err));
        return nullptr;
    }

    spi_device_interface_config_t dev_cfg{};
    dev_cfg.mode = 0;  // CPOL=0, CPHA=0: both chips change SO on SCK falling edge
    dev_cfg.clock_speed_hz = kTcSpiHz;
    dev_cfg.spics_io_num = kTcCs;  // driver holds CS high (idle) between reads
    dev_cfg.queue_size = 1;

    spi_device_handle_t dev = nullptr;
    err = spi_bus_add_device(SPI2_HOST, &dev_cfg, &dev);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "spi_bus_add_device failed: %s", esp_err_to_name(err));
        return nullptr;
    }
    return dev;
}

esp_err_t tc_read_frame(spi_device_handle_t dev, uint32_t* frame) {
    spi_transaction_t t{};
    t.flags = SPI_TRANS_USE_RXDATA;  // receive into t.rx_data, no buffer needed
    t.length = 32;                   // clock 32 bits...
    t.rxlength = 32;                 // ...and keep all of them
    t.tx_buffer = nullptr;           // receive-only
    const esp_err_t err = spi_device_polling_transmit(dev, &t);
    if (err != ESP_OK) return err;
    // Bytes arrive MSB first: rx_data[0] holds D31..D24.
    *frame = (static_cast<uint32_t>(t.rx_data[0]) << 24) |
             (static_cast<uint32_t>(t.rx_data[1]) << 16) |
             (static_cast<uint32_t>(t.rx_data[2]) << 8) | static_cast<uint32_t>(t.rx_data[3]);
    return ESP_OK;
}

const char* guess_chip(uint32_t frame) {
    if (frame == 0x00000000u || frame == 0xFFFFFFFFu) {
        return "all 0s or all 1s: check wiring/power";
    }
    const uint16_t hi = static_cast<uint16_t>(frame >> 16);
    const uint16_t lo = static_cast<uint16_t>(frame & 0xFFFF);
    const Max6675Frame m6675 = decode_max6675(frame);
    // A MAX6675 has nothing more to say after 16 bits, so the low half is
    // usually 0x0000 or a repeat of the high half.
    if (m6675.reserved_ok && (lo == 0x0000 || lo == hi)) {
        return "looks like MAX6675";
    }
    // MAX31855: D17 and D3 are reserved (always 0), and the cold-junction
    // temperature on a bench should be close to room temperature.
    const Max31855Frame m31855 = decode_max31855(frame);
    const bool reserved_zero = (frame & (1u << 17)) == 0 && (frame & (1u << 3)) == 0;
    if (reserved_zero && m31855.internal_c > 5.0f && m31855.internal_c < 60.0f) {
        return "looks like MAX31855";
    }
    if (m6675.reserved_ok) return "unclear, maybe MAX6675 (low 16 bits not 0 or a repeat)";
    return "unclear: matches neither layout";
}

void log_tc_frame(uint32_t frame) {
    const Max31855Frame a = decode_max31855(frame);
    const Max6675Frame b = decode_max6675(frame);
    ESP_LOGI(kTag, "TC raw 0x%08" PRIX32, frame);
    ESP_LOGI(kTag, "  as MAX31855: TC %.2f C / %.2f F, internal %.4f C / %.2f F, fault=%d"
                   " (SCV=%d SCG=%d OC=%d)",
             a.thermocouple_c, c_to_f(a.thermocouple_c), a.internal_c, c_to_f(a.internal_c),
             a.fault, a.short_to_vcc, a.short_to_gnd, a.open_circuit);
    ESP_LOGI(kTag, "  as MAX6675:  TC %.2f C / %.2f F, open=%d, D15/D1 zero=%s",
             b.thermocouple_c, c_to_f(b.thermocouple_c), b.open_circuit,
             b.reserved_ok ? "yes" : "NO");
    ESP_LOGI(kTag, "  guess: %s", guess_chip(frame));
}

void log_banner() {
    ESP_LOGI(kTag, "================ CoffeeProbe bring-up diagnostics ================");
    ESP_LOGI(kTag, "ESP-IDF %s. Normal app is NOT running (CONFIG_PROBE_BRINGUP_DIAG=y).",
             esp_get_idf_version());
    ESP_LOGI(kTag, "Tests: I2C scan -> SSD1306 test pattern -> thermocouple frames every %d ms",
             kTcPeriodMs);
    ESP_LOGI(kTag, "Pins (XIAO plan): I2C SDA=GPIO%d SCL=GPIO%d @ %" PRIu32 " Hz", kI2cSda,
             kI2cScl, kI2cHz);
    ESP_LOGI(kTag, "                  TC SPI2 SCK=GPIO%d MISO=GPIO%d CS=GPIO%d, mode 0 @ %d Hz",
             kTcSck, kTcMiso, kTcCs, kTcSpiHz);
    ESP_LOGI(kTag, "==================================================================");
}

}  // namespace

void run() {
    log_banner();
    run_i2c_and_oled();

    spi_device_handle_t tc = tc_init();
    while (true) {
        if (tc != nullptr) {
            uint32_t frame = 0;
            const esp_err_t err = tc_read_frame(tc, &frame);
            if (err == ESP_OK) {
                log_tc_frame(frame);
            } else {
                ESP_LOGE(kTag, "TC read failed: %s", esp_err_to_name(err));
            }
        } else {
            ESP_LOGW(kTag, "TC SPI not initialised; nothing to read");
        }
        // Blocking delay yields to the idle task, which keeps the task watchdog fed.
        vTaskDelay(pdMS_TO_TICKS(kTcPeriodMs));
    }
}

}  // namespace bringup
