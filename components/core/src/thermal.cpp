#include "core/thermal.hpp"

#include <algorithm>

namespace core {

ThermalLevel ThermalSensorPolicy::desired(DeciC reading) const {
    const int32_t r = reading.v;
    const int32_t h = thresholds_.hysteresis.v;
    const bool critical_on = r >= thresholds_.critical.v;
    const bool critical_stay = r >= thresholds_.critical.v - h;
    const bool warn_on = r >= thresholds_.warn.v;
    const bool warn_stay = r >= thresholds_.warn.v - h;
    // Rising uses the threshold itself; falling uses threshold - hysteresis, so a reading
    // hovering at a threshold can't make the level (and the screen) flicker.
    switch (level_) {
        case ThermalLevel::Critical:
            return critical_stay ? ThermalLevel::Critical
                                 : (warn_stay ? ThermalLevel::Warn : ThermalLevel::Normal);
        case ThermalLevel::Warn:
            return critical_on ? ThermalLevel::Critical
                               : (warn_stay ? ThermalLevel::Warn : ThermalLevel::Normal);
        case ThermalLevel::Normal:
            break;
    }
    return critical_on ? ThermalLevel::Critical
                       : (warn_on ? ThermalLevel::Warn : ThermalLevel::Normal);
}

ThermalLevel ThermalSensorPolicy::update(std::optional<DeciC> reading) {
    if (!reading.has_value()) {
        pending_count_ = 0;
        return level_;
    }
    const ThermalLevel want = desired(*reading);
    if (want == level_) {
        pending_count_ = 0;
        return level_;
    }
    // A change of direction (e.g. Warn pending, then Critical) starts counting again.
    if (pending_count_ == 0 || want != pending_) {
        pending_ = want;
        pending_count_ = 0;
    }
    if (++pending_count_ >= kThermalConfirmSamples) {
        level_ = want;
        pending_count_ = 0;
    }
    return level_;
}

ThermalLevel ThermalPolicy::update(std::optional<DeciC> ambient, std::optional<DeciC> die) {
    ambient_.update(ambient);
    die_.update(die);
    return level();
}

ThermalLevel ThermalPolicy::level() const { return std::max(ambient_.level(), die_.level()); }

}  // namespace core
