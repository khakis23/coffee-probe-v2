#pragma once

#include <cstdint>
#include <optional>

#include "core/temperature.hpp"

namespace core {

enum class ThermalLevel : uint8_t { Normal, Warn, Critical };

struct ThermalThresholds {
    DeciC warn;
    DeciC critical;
    DeciC hysteresis;  // A level is left only this far below the threshold that set it.
};

// The one place to tune overheat behaviour (#27 tunes these next to the real roaster).
// Ambient comes from the MAX31855 cold junction; the die sensor is the C3's own backstop and runs
// several degrees above ambient with Wi-Fi on (architecture.md -> Overheat).
inline constexpr ThermalThresholds kAmbientThresholds{{500}, {650}, {50}};
inline constexpr ThermalThresholds kDieThresholds{{900}, {1050}, {50}};
// Consecutive samples that must agree before a level changes. The health task samples at 1 Hz,
// so a level changes after 5 s: long enough to ignore a glitch, short against slow heat.
inline constexpr uint8_t kThermalConfirmSamples = 5;

// One sensor's level, with hysteresis and confirmation. Pure: time is the sample count.
class ThermalSensorPolicy {
   public:
    explicit constexpr ThermalSensorPolicy(const ThermalThresholds& t) : thresholds_(t) {}
    // A missing reading (no chip output, a probe fault) changes nothing and restarts the count.
    ThermalLevel update(std::optional<DeciC> reading);
    ThermalLevel level() const { return level_; }

   private:
    ThermalLevel desired(DeciC reading) const;

    ThermalThresholds thresholds_;
    ThermalLevel level_{ThermalLevel::Normal};
    ThermalLevel pending_{ThermalLevel::Normal};
    uint8_t pending_count_{0};
};

// Both sensors; the overall level is the worse of the two.
class ThermalPolicy {
   public:
    ThermalLevel update(std::optional<DeciC> ambient, std::optional<DeciC> die);
    ThermalLevel level() const;

   private:
    ThermalSensorPolicy ambient_{kAmbientThresholds};
    ThermalSensorPolicy die_{kDieThresholds};
};

}  // namespace core
