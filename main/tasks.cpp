#include "tasks.hpp"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>

#include "buttons/buttons.hpp"
#include "core/menu.hpp"
#include "core/register_publish.hpp"
#include "core/sensor_pipeline.hpp"
#include "core/thermal.hpp"
#include "core/ui.hpp"
#include "display/display.hpp"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "health/health.hpp"
#include "net/modbus_server.hpp"
#include "net/net.hpp"
#include "net/softap_config.hpp"
#include "state/state_store.hpp"
#include "storage/storage.hpp"
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

// What this boot runs with; set by start_tasks() before any task starts, then read-only. The
// menu's copy (in the UI task) is the one that changes.
core::Settings boot_settings{};
// The calibration offset (tenths of a degree F): written by the UI task when the menu saves
// it, applied by the sensor task from its next sample.
std::atomic<int16_t> offset_deci_f{0};

// The converter and grounded mode were set with thermocouple::configure() before the tasks.
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
    static int32_t applied_offset = INT32_MIN;  // Not a valid offset: applies on the first step.

    const int16_t offset = offset_deci_f.load(std::memory_order_relaxed);
    if (offset != applied_offset) {
        sensor_pipeline.set_calibration_offset(core::offset_deci_c(offset));
        applied_offset = offset;
    }

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

void net_init() {
    // start() logs failures and service() retries.
    (void)net::modbus::start();
}

void net_step() {
    constexpr uint32_t kPublishPeriodMs = 250;
    const auto now_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    static uint32_t next_publish_ms = now_ms;
    static const uint16_t firmware_version =
        core::firmware_version_from(esp_app_get_description()->version);
    const uint32_t publish_lateness_ms = now_ms - next_publish_ms;
    if (publish_lateness_ms <= INT32_MAX) {
        next_publish_ms = now_ms + kPublishPeriodMs - publish_lateness_ms % kPublishPeriodMs;
        core::ProbeState snapshot;
        // A state lock timeout leaves the default fault/stale snapshot in place: Artisan sees -1
        // rather than a held value. build_registers re-checks the reading's age itself.
        (void)state::snapshot(snapshot);
        const auto snapshot_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000);
        net::modbus::publish(core::build_registers(snapshot, snapshot_ms, firmware_version));
    }

    net::modbus::service(now_ms, 0);
    const uint8_t wifi_clients = net::client_count();
    const uint8_t modbus_clients = net::modbus::client_count();
    const uint32_t last_request_ms = net::modbus::last_request_ms();
    state::update([=](core::ProbeState& s) {
        s.wifi_clients = wifi_clients;
        s.modbus_clients = modbus_clients;
        s.modbus_last_request_ms = last_request_ms;
    });
}

// The UI task runs every 10 ms for the buttons (the debouncer needs 10 ms polls) and redraws
// every 100 ms, or at once after a press so the menu feels immediate.
constexpr uint32_t kUiPeriodMs = 10;
constexpr uint32_t kRenderPeriodMs = 100;

void ui_init() {
    if (const esp_err_t error = buttons::init(); error != ESP_OK) {
        ESP_LOGW(kTag, "Button setup failed: %s", esp_err_to_name(error));
    }
    display::configure(boot_settings.brightness, boot_settings.display_flip);
    // The driver logs the error and rate-limits its warnings across retries.
    if (display::init() != ESP_OK) {
        ESP_LOGD(kTag, "UI will retry display initialization");
    }
}

// Carries out what the menu asked for. Saving runs here, on the UI task: an NVS write takes
// tens of milliseconds, well inside the task watchdog, and Artisan's link is on another task.
void carry_out(core::menu::Action action, const core::menu::Menu& menu) {
    switch (action) {
        case core::menu::Action::None:
            return;
        case core::menu::Action::Save: {
            const core::Settings& s = menu.settings();
            if (const esp_err_t error = storage::save(s); error != ESP_OK) {
                // The change still applies until the next restart; it just isn't kept.
                ESP_LOGE(kTag, "Saving settings failed: %s", esp_err_to_name(error));
            } else {
                ESP_LOGI(kTag, "settings saved");
            }
            display::configure(s.brightness, s.display_flip);
            offset_deci_f.store(s.offset_deci_f, std::memory_order_relaxed);
            return;
        }
        case core::menu::Action::Restart:
            ESP_LOGI(kTag, "restarting to apply settings");
            esp_restart();
        case core::menu::Action::FactoryReset:
            if (const esp_err_t error = storage::erase(); error != ESP_OK) {
                ESP_LOGE(kTag, "Factory reset failed: %s", esp_err_to_name(error));
                return;  // Restarting would only come back with the same settings.
            }
            ESP_LOGW(kTag, "factory reset, restarting");
            esp_restart();
    }
}

void ui_step() {
    static core::menu::Menu menu{boot_settings};  // Built on the first step, after start_tasks().
    static core::Framebuffer frame;
    static core::ui::Inputs in;
    static uint32_t last_render_ms = 0;
    static bool rendered = false;

    auto now_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    core::menu::Press presses[buttons::kButtons];
    const int count = buttons::poll(now_ms, presses);
    for (int i = 0; i < count; ++i) {
        carry_out(menu.handle(presses[i], now_ms), menu);
    }
    menu.tick(now_ms);
    if (rendered && count == 0 && now_ms - last_render_ms < kRenderPeriodMs) {
        return;
    }

    core::ProbeState snapshot;
    // A lock timeout leaves a fault/stale snapshot so the UI shows a probe fault.
    (void)state::snapshot(snapshot);
    if (snapshot.thermal == core::ThermalLevel::Critical) {
        menu.close();  // The probe is about to sleep: drop any unsaved edit.
    }
    now_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    in.state = snapshot;
    in.now_ms = now_ms;
    in.ssid = net::ssid();
    in.password = net::kPassword;
    in.show_password = menu.settings().show_password;
    in.version = esp_app_get_description()->version;
    in.menu = &menu;
    core::ui::render(in, frame);
    display::show(frame, now_ms);
    last_render_ms = now_ms;
    rendered = true;
}

void health_init() {
    const esp_err_t error = health::init();
    if (error != ESP_OK) {
        ESP_LOGW(kTag, "Die sensor initialization failed: %s", esp_err_to_name(error));
    }
}

const char* thermal_level_name(core::ThermalLevel level) {
    switch (level) {
        case core::ThermalLevel::Normal:
            return "Normal";
        case core::ThermalLevel::Warn:
            return "Warn";
        case core::ThermalLevel::Critical:
            return "Critical";
    }
    return "unknown";
}

constexpr size_t kThermalReadingTextSize = 12;

const char* thermal_reading_text(std::optional<core::DeciC> reading,
                                 char (&text)[kThermalReadingTextSize]) {
    if (!reading) {
        return "unavailable";
    }
    const int32_t value = reading->v;
    const int32_t magnitude = value < 0 ? -value : value;
    std::snprintf(text, sizeof(text), "%s%ld.%ld C", value < 0 ? "-" : "",
                  static_cast<long>(magnitude / 10), static_cast<long>(magnitude % 10));
    return text;
}

void health_step() {
    static core::ThermalPolicy policy;
    const auto die = health::read_die();
    core::ProbeState snapshot;
    if (!state::snapshot(snapshot)) {
        return;
    }
    const auto ambient = snapshot.cold_junction;
    const auto level = policy.update(ambient, die);
    if (!state::update([=](core::ProbeState& s) {
            s.die = die;
            s.thermal = level;
        })) {
        // Retry publication next round so the UI can receive Critical before sleep.
        return;
    }

    if (level != snapshot.thermal) {
        char ambient_text[kThermalReadingTextSize];
        char die_text[kThermalReadingTextSize];
        const char* ambient_c = thermal_reading_text(ambient, ambient_text);
        const char* die_c = thermal_reading_text(die, die_text);
        const char* from = thermal_level_name(snapshot.thermal);
        const char* to = thermal_level_name(level);
        if (level > snapshot.thermal) {
            ESP_LOGW(kTag, "Thermal level %s -> %s, ambient %s, die %s", from, to, ambient_c, die_c);
        } else {
            ESP_LOGI(kTag, "Thermal level %s -> %s, ambient %s, die %s", from, to, ambient_c, die_c);
        }
    }
    if (level == core::ThermalLevel::Critical) {
        health::enter_protective_sleep();
    }
}

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
    // Polling select() with zero wait lets vTaskDelayUntil yield to the single-core idle task
    // and its watchdog between rounds. The 20 ms poll period adds at most 20 ms of polling
    // latency to Artisan's 1–3 s polling interval.
    {"net", 4, kNetStackBytes, 20, net_step, net_stack, net_init},
    {"ui", 3, kUiStackBytes, kUiPeriodMs, ui_step, ui_stack, ui_init},
    {"health", 2, kHealthStackBytes, 1000, health_step, health_stack, health_init},
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

void start_tasks(const core::Settings& settings) {
    boot_settings = settings;
    offset_deci_f.store(settings.offset_deci_f, std::memory_order_relaxed);
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
