// CoffeeProbe — bring-up smoke test.
//
// Proves the toolchain, flashing, and serial monitor work on whatever C3 board
// is plugged in. Replaced by the real app wiring in Phase 1 of the build plan.

#include <cinttypes>

#include "board/board.hpp"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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
    log_chip_info();

    while (true) {
        const int64_t uptime_s = esp_timer_get_time() / 1'000'000;
        ESP_LOGI(kTag, "alive, uptime %" PRId64 " s, free heap %" PRIu32 " B", uptime_s,
                 esp_get_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
