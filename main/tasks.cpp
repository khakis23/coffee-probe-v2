#include "tasks.hpp"

#include <cstdint>
#include <cstdlib>

#include "core/sensor_pipeline.hpp"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "net/net.hpp"
#include "state/state_store.hpp"
#include "thermocouple/thermocouple.hpp"

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

// Kconfig selects the converter and grounded mode. The filter uses SensorConfig defaults.
core::SensorPipeline sensor_pipeline{core::SensorConfig{}};

const char* fault_name(core::TcFault fault) {
    switch (fault) {
        case core::TcFault::None:
            return "ok";
        case core::TcFault::Open:
            return "probe open";
        case core::TcFault::ShortGnd:
            return "short to GND";
        case core::TcFault::ShortVcc:
            return "short to VCC";
        case core::TcFault::BusError:
            return "bus error";
    }
    return "unknown";
}

void sensor_init() {
    const esp_err_t error = thermocouple::init();
    if (error != ESP_OK) {
        ESP_LOGW(kTag, "%s initialization failed: %s", thermocouple::chip_name(),
                 esp_err_to_name(error));
    }
}

void sensor_step() {
    static core::TcFault previous_fault = core::TcFault::BusError;

    const auto reading = thermocouple::read();
    const int64_t now_us = esp_timer_get_time();
    const auto now_ms = static_cast<uint32_t>(now_us / 1000);
    sensor_pipeline.feed(reading, now_ms);
    const auto out = sensor_pipeline.output(now_ms);
    const auto uptime_s = static_cast<uint32_t>(now_us / 1'000'000);
    state::update([&out, uptime_s](core::ProbeState& s) {
        s.bt = out.bt;
        s.fault = out.fault;
        s.stale = out.stale;
        s.bt_updated_ms = out.bt_updated_ms;
        s.cold_junction = out.cold_junction;
        s.uptime_s = uptime_s;
    });

    // One INFO line per second with the sample count, so the default log level shows both the
    // published temperature and that sampling runs at 4 Hz without printing every sample.
    static uint32_t samples_this_second = 0;
    static int64_t last_summary_us = now_us;
    ++samples_this_second;
    if (reading.fault != core::TcFault::None) {
        ESP_LOGD(kTag, "thermocouple sample fault %s", fault_name(reading.fault));
    }
    if (now_us - last_summary_us >= 1'000'000) {
        if (out.fault == core::TcFault::None && !out.stale) {
            const int32_t bt_f = core::to_f(out.bt).v;  // Tenths of a degree, printed without floats.
            const char* sign = bt_f < 0 ? "-" : "";
            const int32_t abs_f = bt_f < 0 ? -bt_f : bt_f;
            ESP_LOGI(kTag, "BT %s%ld.%ld F, %lu samples/s", sign, static_cast<long>(abs_f / 10),
                     static_cast<long>(abs_f % 10), static_cast<unsigned long>(samples_this_second));
        } else {
            // Never print a held value next to a fault: it reads like a real temperature.
            ESP_LOGI(kTag, "BT --, %s%s, %lu samples/s", fault_name(out.fault),
                     out.stale ? ", stale" : "", static_cast<unsigned long>(samples_this_second));
        }
        samples_this_second = 0;
        last_summary_us = now_us;
    }
    if (out.fault != previous_fault) {
        ESP_LOGW(kTag, "thermocouple fault changed from %s to %s", fault_name(previous_fault),
                 fault_name(out.fault));
        previous_fault = out.fault;
    }
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
    void (*init)() = nullptr;
    StaticTask_t storage{};
    TaskHandle_t handle{};
};

Task tasks[] = {
    // MAX6675 needs at least 220 ms between reads to finish a conversion.
    {"sensor", 5, kSensorStackBytes, 250, sensor_step, sensor_stack, sensor_init},
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

    if (task.init != nullptr) {
        task.init();
    }
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
