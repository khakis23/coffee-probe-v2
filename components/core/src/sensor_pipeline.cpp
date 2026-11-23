#include "core/sensor_pipeline.hpp"

#include <algorithm>

#include "core/probe_state.hpp"

namespace core {

SensorPipeline::SensorPipeline(const SensorConfig& cfg) : config_(cfg) {}

void SensorPipeline::feed(const TcReading& reading, uint32_t now_ms) {
    if (reading.fault != TcFault::None) {
        if (consecutive_faults_ < UINT8_MAX) {
            ++consecutive_faults_;
        }
        if (consecutive_faults_ >= config_.fault_debounce) {
            latest_.fault = reading.fault;
            // Start fresh on recovery so the old output cannot limit the first good sample.
            window_size_ = 0;
            next_sample_ = 0;
        }
        return;
    }

    const bool had_window = window_size_ != 0;
    window_[next_sample_] = {
        clamp_i16(static_cast<int32_t>(reading.hot.v) + config_.calibration_offset.v)};
    next_sample_ = (next_sample_ + 1) % window_.size();
    window_size_ = std::min(window_size_ + 1, window_.size());

    int32_t target = window_[0].v;
    if (window_size_ == 2) {
        target = div_round_away(target + window_[1].v, 2);
    } else if (window_size_ == window_.size()) {
        const auto [min, max] = std::minmax({window_[0].v, window_[1].v, window_[2].v});
        target = target + window_[1].v + window_[2].v - min - max;
    }

    if (had_window) {
        // Catch spikes that pass the median. 50 C/s is far above real bean changes,
        // including the drop at charge. Bench test #13 tunes this limit.
        const uint32_t elapsed_ms = now_ms - latest_.bt_updated_ms;
        const int64_t step = std::max<int64_t>(
            1, static_cast<int64_t>(config_.max_rate_deci_c_per_s) * elapsed_ms / 1000);
        const int64_t previous = latest_.bt.v;
        target =
            static_cast<int32_t>(std::clamp<int64_t>(target, previous - step, previous + step));
    }

    consecutive_faults_ = 0;
    has_good_sample_ = true;
    latest_.bt = {static_cast<int16_t>(target)};
    latest_.fault = TcFault::None;
    latest_.bt_updated_ms = now_ms;
    latest_.cold_junction = reading.cold_junction;
}

SensorOutput SensorPipeline::output(uint32_t now_ms) const {
    SensorOutput result = latest_;
    result.stale = !has_good_sample_ || now_ms - result.bt_updated_ms > kStaleMs;
    return result;
}

}  // namespace core
