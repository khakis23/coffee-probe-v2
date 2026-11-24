#pragma once

#include <cstdint>
#include <optional>

#include "core/temperature.hpp"
#include "core/thermocouple.hpp"

namespace core {

struct ProbeState {
    // Sensor task
    DeciC bt{0};                       // valid only when fault == None && !stale
    TcFault fault{TcFault::BusError};  // starts as "no reading yet"
    bool stale{true};                  // no good reading for > kStaleMs
    uint32_t bt_updated_ms{0};         // time of the last good reading (ms since boot)
    std::optional<DeciC> cold_junction{};
    // Network / Modbus tasks
    uint8_t wifi_clients{0};
    uint8_t modbus_clients{0};
    uint32_t modbus_last_request_ms{0};
    // System
    uint32_t uptime_s{0};
};

inline constexpr uint32_t kStaleMs = 2000;

// True when the reading is older than kStaleMs at now_ms, whatever s.stale says: only the sensor
// task refreshes s.stale, so a frozen sensor task must not leave an old reading looking fresh.
// Unsigned subtraction handles the millisecond clock wrapping.
constexpr bool bt_stale(const ProbeState& s, uint32_t now_ms) {
    return s.stale || now_ms - s.bt_updated_ms > kStaleMs;
}

// True when bt can be shown to the user / sent to Artisan as a real temperature at now_ms.
// Takes the time so no consumer can forget the age check (reliability R1).
constexpr bool bt_valid(const ProbeState& s, uint32_t now_ms) {
    return s.fault == TcFault::None && !bt_stale(s, now_ms);
}

}  // namespace core
