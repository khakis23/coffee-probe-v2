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

namespace {

uint8_t count_if(bool condition, uint8_t count) {
    if (!condition) {
        return 0;
    }
    return count < kThermalConfirmSamples ? static_cast<uint8_t>(count + 1) : count;
}

}  // namespace

ThermalLevel ThermalSensorPolicy::update(std::optional<DeciC> reading) {
    if (!reading.has_value()) {
        return level_;
    }
    const ThermalLevel want = desired(*reading);
    warn_or_above_ = count_if(want >= ThermalLevel::Warn, warn_or_above_);
    critical_ = count_if(want == ThermalLevel::Critical, critical_);
    if (want < level_) {
        below_max_ = below_ == 0 ? want : std::max(below_max_, want);
        below_ = count_if(true, below_);
    } else {
        below_ = 0;
    }

    if (critical_ >= kThermalConfirmSamples && level_ < ThermalLevel::Critical) {
        level_ = ThermalLevel::Critical;
        below_ = 0;
    } else if (warn_or_above_ >= kThermalConfirmSamples && level_ < ThermalLevel::Warn) {
        level_ = ThermalLevel::Warn;
        below_ = 0;
    } else if (below_ >= kThermalConfirmSamples) {
        level_ = below_max_;
        below_ = 0;
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
