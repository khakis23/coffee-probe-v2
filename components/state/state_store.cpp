#include "state/state_store.hpp"

#include <atomic>
#include <cinttypes>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace state {
namespace {

constexpr const char* kTag = "state_store";
// Far longer than any copy, short enough that no task stalls. A timeout means a bug.
constexpr uint32_t kLockTimeoutMs = 20;
constexpr int64_t kWarningIntervalUs = 10'000'000;

StaticSemaphore_t mutex_storage;
SemaphoreHandle_t mutex;
core::ProbeState shared_state;
std::atomic<uint32_t> timeout_count{0};
std::atomic<int64_t> last_warning_us{-kWarningIntervalUs};

void record_timeout() {
    timeout_count.fetch_add(1, std::memory_order_relaxed);

    // Claim the warning slot atomically so simultaneous timeouts cannot flood the log.
    int64_t previous = last_warning_us.load(std::memory_order_relaxed);
    const int64_t now = esp_timer_get_time();
    if (now - previous >= kWarningIntervalUs &&
        last_warning_us.compare_exchange_strong(previous, now, std::memory_order_relaxed)) {
        ESP_LOGW(kTag, "State lock timed out (total: %" PRIu32 ")", lock_timeouts());
    }
}

}  // namespace

bool init() {
    mutex = xSemaphoreCreateMutexStatic(&mutex_storage);
    return mutex != nullptr;
}

bool snapshot(core::ProbeState& out) {
    const auto* shared = detail::lock_state();
    if (shared == nullptr) {
        return false;
    }
    out = *shared;
    detail::unlock_state();
    return true;
}

uint32_t lock_timeouts() {
    return timeout_count.load(std::memory_order_relaxed);
}

namespace detail {

core::ProbeState* lock_state() {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(kLockTimeoutMs)) != pdTRUE) {
        record_timeout();
        return nullptr;
    }
    return &shared_state;
}

void unlock_state() {
    xSemaphoreGive(mutex);
}

}  // namespace detail
}  // namespace state
