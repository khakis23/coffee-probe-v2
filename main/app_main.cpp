// CoffeeProbe startup and diagnostics.

#include <cinttypes>
#include <cstdlib>

#include "board/board.hpp"
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
#include "tasks.hpp"

#if CONFIG_PROBE_BRINGUP_DIAG
#include "bringup/bringup.hpp"
#endif

namespace {

constexpr const char* kTag = "coffee_probe";

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

}  // namespace

extern "C" void app_main() {
#if CONFIG_PROBE_BRINGUP_DIAG
    bringup::run();  // hardware diagnostics (#2/#3); never returns
#endif

    log_chip_info();
    ESP_LOGI(kTag, "Firmware version: %s", esp_app_get_description()->version);

    // Wi-Fi needs NVS for PHY calibration. If Wi-Fi cannot start, the probe is unusable
    // and the configured panic-to-reboot recovery is the right response during setup.
    esp_err_t nvs_error = nvs_flash_init();
    if (nvs_error == ESP_ERR_NVS_NO_FREE_PAGES || nvs_error == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_error = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_error);
    if (!state::init()) {
        ESP_LOGE(kTag, "Unable to initialize shared probe state");
        // The tasks cannot run without shared state. Panic reboots the probe to retry setup.
        std::abort();
    }
    ESP_ERROR_CHECK(net::start_softap());
    ESP_LOGI(kTag, "Wi-Fi SSID %s, password (see Artisan guide), IP %s, channel %u", net::ssid(),
             net::kApIp, static_cast<unsigned>(net::kChannel));

    start_tasks();
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
