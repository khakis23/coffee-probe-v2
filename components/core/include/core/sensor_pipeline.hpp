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
    // Smallest allowed step between neighbouring samples of the three consecutive good samples
    // that end a recovery (5 C, above converter noise). A larger step is allowed when the max
    // rate over its elapsed time permits, so a probe replugged during a fast real change recovers.
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
    // A changed calibration offset (settings menu) applies from the next sample; the median
    // filter carries the step through within two samples.
    void set_calibration_offset(DeciC offset) { config_.calibration_offset = offset; }

   private:
    // True while a valid output is being published: no reported fault and not stale.
    bool live(uint32_t now_ms) const;
    void reset_window();
    // True when going from window sample `from` to `to` is within the max rate (see feed()).
    bool plausible_step(std::size_t from, std::size_t to) const;

    SensorConfig config_;
    SensorOutput latest_{};
    std::array<DeciC, 3> window_{};
    std::array<uint32_t, 3> window_ms_{};  // When each window sample was taken.
    std::size_t window_size_{0};
    std::size_t next_sample_{0};
    uint32_t last_good_ms_{0};  // Time of the newest good sample in the window.
    uint8_t consecutive_faults_{0};
    bool has_good_sample_{false};
};

}  // namespace core
