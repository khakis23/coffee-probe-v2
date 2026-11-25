#include "health/health.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>

#include "driver/temperature_sensor.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace health {
namespace {

constexpr const char* kTag = "health";
constexpr int64_t kRetryIntervalUs = 10'000'000;
constexpr int64_t kWarningIntervalUs = 10'000'000;
constexpr uint64_t kSleepDurationUs = 60'000'000;
constexpr uint32_t kUiWaitMs = 1500;
constexpr uint32_t kWdtStepMs = 100;

// Survives restarts, panics, watchdog resets and deep sleep; random after a power-on, which the
// record's magic and check catch. Raw bytes, not a BootRecord: a C++ object with default member
// initializers would be zeroed by a static constructor at every boot, defeating "noinit".
RTC_NOINIT_ATTR uint8_t rtc_record[sizeof(core::BootRecord)];
core::BootRecord current_record{};  // app_main writes it once at boot; the health task after.

core::BootRecord read_rtc_record() {
    core::BootRecord r{};
    std::memcpy(&r, rtc_record, sizeof(r));
    return r;
}

void write_rtc_record(const core::BootRecord& r) { std::memcpy(rtc_record, &r, sizeof(r)); }

core::ResetCause cause_from(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_POWERON:
            return core::ResetCause::PowerOn;
        case ESP_RST_SW:
            return core::ResetCause::Software;
        case ESP_RST_PANIC:
            return core::ResetCause::Crash;
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT:
            return core::ResetCause::Watchdog;
        case ESP_RST_BROWNOUT:
            return core::ResetCause::Brownout;
        case ESP_RST_DEEPSLEEP:
            return core::ResetCause::DeepSleep;
        default:
            return core::ResetCause::Other;
    }
}

temperature_sensor_handle_t sensor = nullptr;
bool sensor_enabled = false;
int64_t last_init_failure_us = -kRetryIntervalUs;

void warn_sensor_error(const char* operation, esp_err_t error) {
    static int64_t last_warning_us = -kWarningIntervalUs;
    const int64_t now_us = esp_timer_get_time();
    if (now_us - last_warning_us >= kWarningIntervalUs) {
        ESP_LOGW(kTag, "Die sensor %s failed: %s", operation, esp_err_to_name(error));
        last_warning_us = now_us;
    }
}

}  // namespace

BootInfo record_boot() {
    const core::BootRecord stored = read_rtc_record();
    const core::ResetCause cause = cause_from(esp_reset_reason());
    BootInfo info{core::next_boot_record(stored, cause), cause, core::boot_record_valid(stored)};
    write_rtc_record(info.record);
    current_record = info.record;
    return info;
}

void clear_brownouts() {
    current_record = core::cleared_brownouts(current_record);
    write_rtc_record(current_record);
}

esp_err_t init() {
    if (sensor_enabled) {
        return ESP_OK;
    }
    if (sensor == nullptr) {
        // C3 ranges trade accuracy for span. 50..125 C gives the best accuracy across both
        // thresholds (90 and 105 C). Readings below 50 C are less accurate but far from either.
        const temperature_sensor_config_t config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(50, 125);
        const esp_err_t error = temperature_sensor_install(&config, &sensor);
        if (error != ESP_OK) {
            last_init_failure_us = esp_timer_get_time();
            return error;
        }
    }

    // Keep the installed handle if enabling fails so the next attempt can retry just enable.
    const esp_err_t error = temperature_sensor_enable(sensor);
    if (error != ESP_OK) {
        last_init_failure_us = esp_timer_get_time();
        return error;
    }
    sensor_enabled = true;
    return ESP_OK;
}

std::optional<core::DeciC> read_die() {
    if (!sensor_enabled) {
        if (esp_timer_get_time() - last_init_failure_us < kRetryIntervalUs) {
            return std::nullopt;
        }
        const esp_err_t error = init();
        if (error != ESP_OK) {
            warn_sensor_error("initialization", error);
            return std::nullopt;
        }
    }

    float celsius = 0;
    const esp_err_t error = temperature_sensor_get_celsius(sensor, &celsius);
    if (error != ESP_OK) {
        warn_sensor_error("read", error);
        return std::nullopt;
    }
    if (!std::isfinite(celsius)) {
        warn_sensor_error("read", ESP_ERR_INVALID_RESPONSE);
        return std::nullopt;
    }
    const auto tenths = static_cast<int32_t>(std::lround(celsius * 10));
    return core::DeciC{core::clamp_i16(tenths)};
}

[[noreturn]] void enter_protective_sleep() {
    ESP_LOGE(kTag, "too hot, sleeping 60 s to cool down");

    // The UI refreshes every 100 ms. Let it draw TooHot before sleep so the panel
    // can keep showing the message while the chip sleeps.
    for (uint32_t waited_ms = 0; waited_ms < kUiWaitMs; waited_ms += kWdtStepMs) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_task_wdt_reset());
        vTaskDelay(pdMS_TO_TICKS(kWdtStepMs));
    }

    // The timer is the only wake source until the firmware supports buttons (#23).
    // A floating wake pin could wake the chip immediately and defeat the protection.
    const esp_err_t error = esp_sleep_enable_timer_wakeup(kSleepDurationUs);
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "Cooling timer setup failed: %s", esp_err_to_name(error));
        // Recover by rebooting because sleep without a timer could strand the probe.
        esp_restart();
    }
    // Deep sleep stops Wi-Fi and the CPU, the main on-board heat sources. Wake reboots
    // into Normal and rechecks the temperatures. If still hot, five samples confirm
    // Critical and the probe sleeps again (R7).
    esp_deep_sleep_start();
}

}  // namespace health
