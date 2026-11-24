#include "core/sensor_pipeline.hpp"

#include <algorithm>

#include "core/probe_state.hpp"

namespace core {

SensorPipeline::SensorPipeline(const SensorConfig& cfg) : config_(cfg) {}

bool SensorPipeline::live(uint32_t now_ms) const {
    return has_good_sample_ && latest_.fault == TcFault::None &&
           now_ms - latest_.bt_updated_ms <= kStaleMs;
}

void SensorPipeline::reset_window() {
    window_size_ = 0;
    next_sample_ = 0;
}

void SensorPipeline::feed(const TcReading& reading, uint32_t now_ms) {
    if (reading.fault != TcFault::None) {
        // The MAX31855 still measures its cold junction with an open or shorted probe.
        if (reading.cold_junction.has_value()) {
            latest_.cold_junction = reading.cold_junction;
        }
        if (consecutive_faults_ < UINT8_MAX) {
            ++consecutive_faults_;
        }
        // While recovering, any faulty sample restarts the count: recovery needs three
        // CONSECUTIVE good samples, so a bouncing connector cannot assemble a window from
        // good-looking frames between faults. While live, a short glitch is debounced instead.
        if (!live(now_ms)) {
            reset_window();
        }
        if (consecutive_faults_ >= config_.fault_debounce) {
            latest_.fault = reading.fault;
            latest_.cold_junction = reading.cold_junction;  // Empty on a bus error: unknown.
            reset_window();  // Samples from before the fault must never mix into recovery.
        }
        return;
    }

    consecutive_faults_ = 0;
    latest_.cold_junction = reading.cold_junction;
    // After a gap longer than the stale limit (the task was held up), old samples say nothing
    // about the probe now, so they must not outvote the new one in the median.
    if (window_size_ != 0 && now_ms - last_good_ms_ > kStaleMs) {
        reset_window();
    }
    last_good_ms_ = now_ms;
    window_[next_sample_] = {
        clamp_i16(static_cast<int32_t>(reading.hot.v) + config_.calibration_offset.v)};
    window_ms_[next_sample_] = now_ms;
    next_sample_ = (next_sample_ + 1) % window_.size();
    window_size_ = std::min(window_size_ + 1, window_.size());
    if (window_size_ < window_.size()) {
        return;
    }

    const auto [min, max] = std::minmax({window_[0].v, window_[1].v, window_[2].v});
    int32_t target = window_[0].v + window_[1].v + window_[2].v - min - max;  // The median.

    if (live(now_ms)) {
        // Catch spikes that pass the median. 50 C/s is far above real bean changes,
        // including the drop at charge. Bench test #13 tunes this limit.
        const uint32_t elapsed_ms = now_ms - latest_.bt_updated_ms;
        const int64_t step = std::max<int64_t>(
            1, static_cast<int64_t>(config_.max_rate_deci_c_per_s) * elapsed_ms / 1000);
        const int64_t previous = latest_.bt.v;
        target =
            static_cast<int32_t>(std::clamp<int64_t>(target, previous - step, previous + step));
    } else {
        // At boot, after a fault or after a stale gap: publish only when three consecutive
        // samples agree, so connector bounce never reaches Artisan. "Agree" means a spread a real
        // probe could produce: at most the max rate over the samples' time span (the same
        // physical limit the live filter uses), and never less than the noise floor. No rate
        // limit against the old value: it says nothing about the probe now. The window slides
        // until the samples agree. next_sample_ now indexes the oldest sample.
        const uint32_t span_ms = now_ms - window_ms_[next_sample_];
        const int64_t allowed = std::max<int64_t>(
            config_.recovery_spread,
            static_cast<int64_t>(config_.max_rate_deci_c_per_s) * span_ms / 1000);
        if (max - min > allowed) {
            return;
        }
    }

    has_good_sample_ = true;
    latest_.bt = {static_cast<int16_t>(target)};
    latest_.fault = TcFault::None;
    latest_.bt_updated_ms = now_ms;
}

SensorOutput SensorPipeline::output(uint32_t now_ms) const {
    SensorOutput result = latest_;
    result.stale = !has_good_sample_ || now_ms - result.bt_updated_ms > kStaleMs;
    return result;
}

}  // namespace core
