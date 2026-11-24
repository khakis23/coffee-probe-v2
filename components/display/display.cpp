#include "display/display.hpp"

#include <array>
#include <cstring>

#include "board/board.hpp"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

namespace display {
namespace {

constexpr const char* kTag = "display";
// OLED modules carry 4.7–10 kOhm pull-ups. A frame takes ~25 ms at 400 kHz vs ~100 ms at 100 kHz.
constexpr uint32_t kI2cHz = 400'000;
constexpr int kI2cTimeoutMs = 50;
constexpr uint32_t kRetryMs = 2'000;
constexpr uint32_t kWarnMs = 10'000;
// ESD or a brownout can reset the panel to a blank, unconfigured state without a bus error.
constexpr uint32_t kReinitMs = 30'000;
// Leave room for a final transfer timeout and the driver's bounded bus clear.
constexpr int64_t kStartTransferBudgetUs = 40'000;
constexpr uint8_t kCommand = 0x00;
constexpr uint8_t kData = 0x40;
constexpr uint16_t kAddresses[] = {0x3C, 0x3D};

#if CONFIG_PROBE_OLED_SH1106
// The SH1106 has 132 RAM columns, of which the panel shows 2..129.
constexpr uint8_t kColumnStart = 2;
#else
constexpr uint8_t kColumnStart = 0;
#endif

// Sent before the setup sequence only on a first start or after an error: switching the panel
// off for the periodic refresh would make the screen blink every 30 s.
constexpr uint8_t kPanelOff[] = {kCommand, 0xAE};

// Safe to resend at any time; leaves the on/off state alone (0xAF is sent separately).
constexpr uint8_t kPanelSetup[] = {
    kCommand,
    0xD5, 0x80,  // Clock divide ratio / oscillator frequency.
    0xA8, 0x3F,  // Multiplex ratio for 64 rows.
    0xD3, 0x00,  // Display offset.
    0x40,        // Start at line zero.
#if CONFIG_PROBE_OLED_SH1106
    0xAD, 0x8B,  // SH1106 internal charge pump.
#else
    0x8D, 0x14,  // SSD1306 internal charge pump.
    0x20, 0x02,  // Page addressing, native on SH1106.
#endif
    0xA1,        // Remap segments for the panel's left-to-right orientation.
    0xC8,        // Remap COM scan direction.
    0xDA, 0x12,  // COM pins for a 128x64 panel.
    0x81, 0xCF,  // Contrast.
    0xD9, 0xF1,  // Pre-charge period.
    0xDB, 0x40,  // VCOMH deselect level.
    0xA4,        // Display follows RAM.
    0xA6,        // Normal polarity.
};
constexpr uint8_t kPanelOn[] = {kCommand, 0xAF};

// The UI task owns these handles and buffers. No other task waits on this bus.
i2c_master_bus_handle_t bus = nullptr;
i2c_master_dev_handle_t device = nullptr;
std::array<uint8_t, core::Framebuffer::kBytes> sent{};
std::array<bool, core::Framebuffer::kPages> dirty{};
std::array<uint8_t, 4> page_address{kCommand, 0, kColumnStart & 0x0F, 0x10 | (kColumnStart >> 4)};
std::array<uint8_t, core::Framebuffer::kWidth + 1> page_data{kData};
bool panel_ready = false;
// After a first start or an error the panel stays off until a whole frame has been redrawn, so a
// number left in its RAM from before the error never flashes back as if it were current.
bool panel_on_pending = true;
bool init_failed = false;
bool transfer_ok = false;
bool recovering = false;
bool warned = false;
uint32_t last_init_ms = 0;
uint32_t last_warning_ms = 0;

// An idle I2C bus has SCL high. If something holds it low, IDF v5.3.1's bus clear on the C3
// waits for the hardware with no timeout (i2c_ll_master_clr_bus) and would hang this task until
// the watchdog restarts the probe, taking Artisan's link down with it. So never hand the driver
// a bus whose clock line is stuck: check it first, and retry later. IDF v6 bounds that wait.
bool scl_idle_high() {
    const auto scl = board::kBoard.pins.i2c_scl;
    for (int sample = 0; sample < 3; ++sample) {
        if (gpio_get_level(scl) == 0) {
            return false;
        }
    }
    return true;
}

// Every transfer goes through here, so the stuck-clock check runs right before each one. A jam
// that starts in the middle of a transfer can still reach IDF's bus clear on v5.3.1; v6 bounds it.
esp_err_t transmit(const uint8_t* data, std::size_t size) {
    if (!scl_idle_high()) {
        return ESP_ERR_INVALID_STATE;
    }
    return i2c_master_transmit(device, data, size, kI2cTimeoutMs);
}

void warn(const char* message, esp_err_t error) {
    const auto now_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    if (!warned || now_ms - last_warning_ms >= kWarnMs) {
        ESP_LOGW(kTag, "%s: %s", message, esp_err_to_name(error));
        last_warning_ms = now_ms;
        warned = true;
    }
}

// Everything is unknown after a failure: redraw every page, keep the panel off until then.
void mark_failed() {
    dirty.fill(true);
    panel_ready = false;
    panel_on_pending = true;
    transfer_ok = false;
    recovering = true;
}

void record_error(const char* operation, esp_err_t error) {
    mark_failed();
    // Re-probe the address on recovery: a replaced module may answer at the other one.
    if (device != nullptr && i2c_master_bus_rm_device(device) == ESP_OK) {
        device = nullptr;
    }
    // The explicit bus reset runs the same bus clear, so it gets the same stuck-clock guard.
    if (bus != nullptr && scl_idle_high()) {
        const esp_err_t reset_error = i2c_master_bus_reset(bus);
        if (reset_error != ESP_OK) {
            warn("display bus reset failed", reset_error);
        }
    }
    warn(operation, error);
}

esp_err_t ensure_device() {
    if (device != nullptr) {
        return ESP_OK;
    }
    if (bus == nullptr) {
        const auto& pins = board::kBoard.pins;
        i2c_master_bus_config_t config{};
        config.i2c_port = -1;  // Let the driver choose a free port.
        config.sda_io_num = pins.i2c_sda;
        config.scl_io_num = pins.i2c_scl;
        config.clk_source = I2C_CLK_SRC_DEFAULT;
        config.glitch_ignore_cnt = 7;
        // Weak internal pull-ups help keep the lines high during the probe. The module's
        // stronger pull-ups set the rise time for 400 kHz transfers.
        config.flags.enable_internal_pullup = 1;
        const esp_err_t error = i2c_new_master_bus(&config, &bus);
        if (error != ESP_OK) {
            return error;
        }
    }

    for (const uint16_t address : kAddresses) {
        if (!scl_idle_high()) {
            return ESP_ERR_INVALID_STATE;  // See transmit().
        }
        const esp_err_t error = i2c_master_probe(bus, address, kI2cTimeoutMs);
        if (error == ESP_ERR_NOT_FOUND) {
            continue;  // A NACK is expected when the module uses the other address.
        }
        if (error != ESP_OK) {
            return error;
        }
        i2c_device_config_t config{};
        config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        config.device_address = address;
        config.scl_speed_hz = kI2cHz;
        return i2c_master_bus_add_device(bus, &config, &device);
    }
    return ESP_ERR_NOT_FOUND;
}

}  // namespace

esp_err_t init() {
    last_init_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    // panel_ready is true only for the periodic refresh of a panel that is working.
    const bool refresh = panel_ready;
    if (bus != nullptr && !scl_idle_high()) {
        init_failed = true;
        mark_failed();
        warn("display I2C clock line held low, not touching the bus", ESP_ERR_INVALID_STATE);
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t error = ensure_device();
    if (error == ESP_OK && !refresh) {
        error = transmit(kPanelOff, sizeof(kPanelOff));
    }
    if (error == ESP_OK) {
        error = transmit(kPanelSetup, sizeof(kPanelSetup));
    }
    init_failed = error != ESP_OK;
    if (init_failed) {
        record_error("display initialization failed", error);
        return error;
    }
    panel_ready = true;
    // Switched on only after the full redraw (see show()), on every path including the periodic
    // refresh: a panel reset by ESD may still hold an old frame, which must not reappear.
    panel_on_pending = true;
    transfer_ok = true;
    dirty.fill(true);
    return ESP_OK;
}

void show(const core::Framebuffer& frame, uint32_t now_ms) {
    const int64_t deadline_us = esp_timer_get_time() + kStartTransferBudgetUs;
    if (init_failed && now_ms - last_init_ms < kRetryMs) {
        return;
    }
    if (!panel_ready || now_ms - last_init_ms >= kReinitMs) {
        if (init() != ESP_OK) {
            return;
        }
    }
    if (!scl_idle_high()) {
        // Retry through init() in kRetryMs; see scl_idle_high() for why the driver isn't called.
        init_failed = true;
        last_init_ms = now_ms;
        mark_failed();
        warn("display I2C clock line held low, not touching the bus", ESP_ERR_INVALID_STATE);
        return;
    }

    for (int page = 0; page < core::Framebuffer::kPages; ++page) {
        const uint8_t* const pixels = frame.page(page);
        uint8_t* const previous = sent.data() + page * core::Framebuffer::kWidth;
        if (!dirty[page] && std::memcmp(previous, pixels, core::Framebuffer::kWidth) == 0) {
            continue;
        }
        if (esp_timer_get_time() >= deadline_us) {
            return;
        }

        page_address[1] = static_cast<uint8_t>(0xB0 | page);
        esp_err_t error =
            transmit(page_address.data(), page_address.size());
        if (error == ESP_OK) {
            // Address the page again next time if the pixel write has to wait for another tick.
            if (esp_timer_get_time() >= deadline_us) {
                return;
            }
            std::memcpy(page_data.data() + 1, pixels, core::Framebuffer::kWidth);
            error = transmit(page_data.data(), page_data.size());
        }
        if (error != ESP_OK) {
            record_error("display page write failed", error);
            return;
        }
        std::memcpy(previous, pixels, core::Framebuffer::kWidth);
        dirty[page] = false;
        transfer_ok = true;
    }

    if (panel_on_pending) {
        // Only once every page holds the current frame (the loop returned early otherwise).
        const esp_err_t error =
            transmit(kPanelOn, sizeof(kPanelOn));
        if (error != ESP_OK) {
            record_error("display switch-on failed", error);
            return;
        }
        panel_on_pending = false;
    }
    if (recovering) {
        ESP_LOGI(kTag, "display recovered");
        recovering = false;
    }
}

bool ok() { return transfer_ok; }

}  // namespace display
