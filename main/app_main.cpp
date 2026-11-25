// CoffeeProbe startup and diagnostics.

#include <cinttypes>
#include <cstdlib>

#include "board/board.hpp"
#include "buttons/buttons.hpp"
#include "core/menu.hpp"
#include "core/ui.hpp"
#include "display/display.hpp"
#include "health/health.hpp"
#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "net/net.hpp"
#include "net/softap_config.hpp"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "state/state_store.hpp"
#include "storage/storage.hpp"
#include "tasks.hpp"
#include "thermocouple/thermocouple.hpp"

#if CONFIG_PROBE_BRINGUP_DIAG
#include "bringup/bringup.hpp"
#endif

namespace {

constexpr const char* kTag = "coffee_probe";

const char* settings_status_name(core::SettingsStatus status) {
    switch (status) {
        case core::SettingsStatus::Ok:
            return "Ok";
        case core::SettingsStatus::Missing:
            return "Missing";
        case core::SettingsStatus::Corrupt:
            return "Corrupt";
        case core::SettingsStatus::UnknownVersion:
            return "UnknownVersion";
        case core::SettingsStatus::Repaired:
            return "Repaired";
    }
    return "Invalid";
}

void log_chip_info() {
    esp_chip_info_t chip{};
    esp_chip_info(&chip);

    uint32_t flash_bytes = 0;
    esp_flash_get_size(nullptr, &flash_bytes);

    ESP_LOGI(kTag, "ESP-IDF %s, board: %s", esp_get_idf_version(), board::kBoard.name);
    ESP_LOGI(kTag, "Chip model %d rev v%d.%d, %d core(s), %" PRIu32 " MB flash", chip.model,
             chip.revision / 100, chip.revision % 100, chip.cores, flash_bytes / (1024 * 1024));
    ESP_LOGI(kTag, "Reset reason: %d", static_cast<int>(esp_reset_reason()));
}

// OK held from power-on for 5 s resets the settings (#23), with a countdown on the screen.
// Returns at once when OK isn't down, so a normal boot isn't slowed. Runs before the tasks
// start, so nothing else uses the buttons or the display yet. Bounded: the hold resolves
// within kBootResetHoldMs, and every display call has its own time budget.
bool factory_reset_held() {
    if (const esp_err_t error = buttons::init(); error != ESP_OK) {
        ESP_LOGW(kTag, "Button setup failed: %s", esp_err_to_name(error));
        return false;
    }
    if (!buttons::ok_down()) {
        return false;
    }
    ESP_LOGW(kTag, "OK held at power-on: factory reset in 5 s unless released");
    (void)display::init();  // Logs its own errors; the countdown is a courtesy.
    core::menu::BootResetHold hold;
    core::Framebuffer frame;
    TickType_t wake = xTaskGetTickCount();
    while (true) {
        const auto now_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000);
        const auto result = hold.update(buttons::ok_down(), now_ms);
        if (result == core::menu::BootHold::Released) {
            ESP_LOGI(kTag, "OK released: no factory reset");
            return false;
        }
        core::ui::render_boot_reset(result, hold.seconds_left(), frame);
        display::show(frame, now_ms);
        if (result == core::menu::BootHold::Reset) {
            return true;
        }
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(20));
    }
}

}  // namespace

extern "C" void app_main() {
#if CONFIG_PROBE_BRINGUP_DIAG
    bringup::run();  // hardware diagnostics (#2/#3); never returns
#endif

    // First, before anything can reset the chip again: why did it reset, and the counts since
    // power-on (#25).
    const health::BootInfo boot = health::record_boot();
    const bool power_warning =
        core::power_warning(boot.record, boot.stored_was_valid, boot.cause, 0);

    log_chip_info();
    ESP_LOGI(kTag, "Firmware version: %s", esp_app_get_description()->version);
    ESP_LOGI(kTag, "Last reset: %.*s; since power-on: %u boots, %u crashes, %u power dips",
             static_cast<int>(core::reset_cause_text(boot.cause).size()),
             core::reset_cause_text(boot.cause).data(), static_cast<unsigned>(boot.record.boots),
             static_cast<unsigned>(boot.record.crashes),
             static_cast<unsigned>(boot.record.brownouts));
    if (power_warning) {
        ESP_LOGW(kTag, "repeated power dips: lowering Wi-Fi power, check the USB cable/power");
    }

    // Wi-Fi needs NVS for PHY calibration. If Wi-Fi cannot start, the probe is unusable
    // and the configured panic-to-reboot recovery is the right response during setup.
    esp_err_t nvs_error = nvs_flash_init();
    if (nvs_error == ESP_ERR_NVS_NO_FREE_PAGES || nvs_error == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_error = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_error);

    auto loaded = storage::load();
    if (loaded.status == core::SettingsStatus::Ok) {
        ESP_LOGI(kTag, "settings loaded");
    } else {
        ESP_LOGW(kTag, "settings %s, repaired fields: %u%s", settings_status_name(loaded.status),
                 static_cast<unsigned>(loaded.repaired_fields),
                 loaded.status == core::SettingsStatus::Repaired ? "" : ", using defaults");
    }
    if (factory_reset_held()) {
        const esp_err_t error = storage::erase();
        if (error == ESP_OK) {
            ESP_LOGW(kTag, "factory reset: settings erased");
        } else {
            ESP_LOGE(kTag, "factory reset: erase failed (%s), using defaults this time",
                     esp_err_to_name(error));
        }
        loaded = {};
    }
    core::Settings settings = loaded.settings;
    if (loaded.status == core::SettingsStatus::Ok ||
        loaded.status == core::SettingsStatus::Repaired) {
        thermocouple::configure(settings.tc_chip, settings.grounded_probe);
    } else {
        // Nothing saved (new board, factory reset, damaged): the build options are the factory
        // default for the converter and grounded mode, so the menu shows what is in use.
        settings.tc_chip = thermocouple::chip();
        settings.grounded_probe = thermocouple::grounded_probe();
    }
    ESP_LOGI(kTag, "thermocouple %s%s, offset %d tenths F", thermocouple::chip_name(),
             thermocouple::grounded_probe() ? ", grounded" : "",
             static_cast<int>(settings.offset_deci_f));
    if (!state::init()) {
        ESP_LOGE(kTag, "Unable to initialize shared probe state");
        // The tasks cannot run without shared state. Panic reboots the probe to retry setup.
        std::abort();
    }
    const bool crash_record = health::crash_record_saved();
    if (crash_record) {
        ESP_LOGW(kTag, "a crash dump is saved in flash (idf.py coredump-info reads it)");
    }
    state::update([&boot, power_warning, crash_record](core::ProbeState& s) {
        s.crash_record = crash_record;
        s.reset_cause = boot.cause;
        s.boots = boot.record.boots;
        s.brownouts = boot.record.brownouts;
        s.crashes = boot.record.crashes;
        s.power_warning = power_warning;
    });
    int8_t tx_power = core::effective_tx_power_qdbm(settings.tx_power_qdbm,
                                                    board::kBoard.wifi_max_tx_power_qdbm);
    if (power_warning && tx_power > core::kPowerWarningTxQdbm) {
        tx_power = core::kPowerWarningTxQdbm;  // Shorter current peaks; restored once calm.
    }
    ESP_ERROR_CHECK(net::start_softap(settings.wifi_channel, tx_power));
    ESP_LOGI(kTag, "Wi-Fi SSID %s, password (see Artisan guide), IP %s, channel %u, %d.%02d dBm",
             net::ssid(), net::kApIp, static_cast<unsigned>(settings.wifi_channel),
             tx_power / 4, (tx_power % 4) * 25);

    start_tasks(settings);
    log_stack_high_water();

    core::ProbeState snapshot{};
    bool logged_stack_at_60_s = false;
    TickType_t wake = xTaskGetTickCount();
    while (true) {
        const int64_t uptime_s = esp_timer_get_time() / 1'000'000;
        // On a lock timeout, keep the last copied counts for the heartbeat.
        state::snapshot(snapshot);
        ESP_LOGI(kTag,
                 "alive, uptime %" PRId64 " s, free heap %" PRIu32
                 " B, wifi_clients=%u, modbus_clients=%u",
                 uptime_s, esp_get_free_heap_size(), static_cast<unsigned>(snapshot.wifi_clients),
                 static_cast<unsigned>(snapshot.modbus_clients));
        if (!logged_stack_at_60_s && uptime_s >= 60) {
            log_stack_high_water();
            logged_stack_at_60_s = true;
        }
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(1000));
    }
}
