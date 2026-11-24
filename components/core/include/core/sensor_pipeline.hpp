#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "core/thermocouple.hpp"

namespace core {

struct SensorConfig {
    DeciC calibration_offset{0};  // Added before filtering.
    uint8_t fault_debounce{3};    // Consecutive faulty samples before reporting.
    int16_t max_rate_deci_c_per_s{500};
};

struct SensorOutput {
    DeciC bt{0};                       // Valid only when fault == None && !stale.
    // BusError ("no reading yet") until three good samples agree; after a reported fault, the
    // fault stays until three new good samples agree (see feed()).
    TcFault fault{TcFault::BusError};
    bool stale{true};
    uint32_t bt_updated_ms{0};  // Time of the last good sample that produced bt.
    std::optional<DeciC> cold_junction{};
};

class SensorPipeline {
   public:
    explicit SensorPipeline(const SensorConfig& cfg);
    void feed(const TcReading& reading, uint32_t now_ms);
    SensorOutput output(uint32_t now_ms) const;

   private:
    SensorConfig config_;
    SensorOutput latest_{};
    std::array<DeciC, 3> window_{};
    std::size_t window_size_{0};
    std::size_t next_sample_{0};
    uint8_t consecutive_faults_{0};
    bool has_good_sample_{false};
};

}  // namespace core
