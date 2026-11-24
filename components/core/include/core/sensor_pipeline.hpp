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
    // Largest max - min of the three consecutive good samples that end a recovery (5 C).
    int16_t recovery_spread{50};
};

struct SensorOutput {
    DeciC bt{0};                       // Valid only when fault == None && !stale.
    // BusError ("no reading yet") until three consecutive good samples agree; after a reported
    // fault or a stale gap, the output stays invalid until that happens again (see feed()).
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
    // True while a valid output is being published: no reported fault and not stale.
    bool live(uint32_t now_ms) const;
    void reset_window();

    SensorConfig config_;
    SensorOutput latest_{};
    std::array<DeciC, 3> window_{};
    std::size_t window_size_{0};
    std::size_t next_sample_{0};
    uint32_t last_good_ms_{0};  // Time of the newest good sample in the window.
    uint8_t consecutive_faults_{0};
    bool has_good_sample_{false};
};

}  // namespace core
