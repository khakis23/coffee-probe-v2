#include "tasks.hpp"

#include <cstdint>
#include <cstdlib>

#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "net/net.hpp"
#include "state/state_store.hpp"

namespace {

constexpr const char* kTag = "probe_tasks";
constexpr int64_t kHeartbeatIntervalUs = 10'000'000;

// Initial sizes in bytes. Check the high-water mark on hardware and keep >= 25 % free.
constexpr uint32_t kSensorStackBytes = 4096;
constexpr uint32_t kNetStackBytes = 4096;
constexpr uint32_t kUiStackBytes = 4096;
constexpr uint32_t kHealthStackBytes = 3072;
static_assert(sizeof(StackType_t) == 1);  // ESP-IDF's ESP32-C3 port uses byte-sized stack entries.

alignas(portBYTE_ALIGNMENT) StackType_t sensor_stack[kSensorStackBytes];
alignas(portBYTE_ALIGNMENT) StackType_t net_stack[kNetStackBytes];
alignas(portBYTE_ALIGNMENT) StackType_t ui_stack[kUiStackBytes];
alignas(portBYTE_ALIGNMENT) StackType_t health_stack[kHealthStackBytes];

void sensor_step() {
    const auto uptime_s = static_cast<uint32_t>(esp_timer_get_time() / 1'000'000);
    state::update([uptime_s](core::ProbeState& s) { s.uptime_s = uptime_s; });
}

void net_step() {
    const uint8_t clients = net::client_count();
    state::update([clients](core::ProbeState& s) { s.wifi_clients = clients; });
}

void ui_step() {}      // OLED work starts in #18.
void health_step() {}  // Thermal protection starts in #24.

struct Task {
    const char* name;
    UBaseType_t priority;
    uint32_t stack_bytes;
    uint32_t period_ms;
    void (*step)();
    StackType_t* stack;
    StaticTask_t storage{};
    TaskHandle_t handle{};
};

Task tasks[] = {
    {"sensor", 5, kSensorStackBytes, 250, sensor_step, sensor_stack},
    {"net", 4, kNetStackBytes, 100, net_step, net_stack},
    {"ui", 3, kUiStackBytes, 100, ui_step, ui_stack},
    {"health", 2, kHealthStackBytes, 1000, health_step, health_stack},
};

void run_task(void* arg) {
    const auto& task = *static_cast<const Task*>(arg);

    // A hang in any of our tasks must cause a panic and reboot (reliability R3).
    const esp_err_t result = esp_task_wdt_add(nullptr);
    if (result != ESP_OK) {
        ESP_LOGE(kTag, "Unable to watch task %s: %s", task.name, esp_err_to_name(result));
        // Retry setup by rebooting because an unwatched task would violate R3.
        esp_restart();
    }
    ESP_LOGI(kTag, "task %s started", task.name);

    const TickType_t period = pdMS_TO_TICKS(task.period_ms);
    TickType_t wake = xTaskGetTickCount();
    int64_t last_heartbeat_us = esp_timer_get_time();
    while (true) {
        task.step();
        const int64_t now = esp_timer_get_time();
        if (now - last_heartbeat_us >= kHeartbeatIntervalUs) {
            ESP_LOGD(kTag, "task %s heartbeat", task.name);
            last_heartbeat_us = now;
        }
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_task_wdt_reset());
        vTaskDelayUntil(&wake, period);
    }
}

}  // namespace

void start_tasks() {
    for (auto& task : tasks) {
        task.handle = xTaskCreateStatic(run_task, task.name, task.stack_bytes, &task, task.priority,
                                        task.stack, &task.storage);
        if (task.handle == nullptr) {
            ESP_LOGE(kTag, "Unable to create task %s", task.name);
            // The probe cannot finish setup with a missing task. Panic reboots it to retry.
            std::abort();
        }
    }
}

void log_stack_high_water() {
    for (const auto& task : tasks) {
        // ESP-IDF reports the minimum free stack in bytes.
        const auto free_bytes = uxTaskGetStackHighWaterMark(task.handle);
        ESP_LOGI(kTag, "task %s stack high-water mark: %u B free", task.name,
                 static_cast<unsigned>(free_bytes));
    }
}
