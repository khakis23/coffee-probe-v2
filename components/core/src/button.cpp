#include "core/button.hpp"

namespace core {

std::optional<ButtonEvent> Debouncer::update(bool raw_pressed, uint32_t now_ms) {
    if (raw_pressed != debounced_pressed_) {
        // Track how long the differing raw level persists continuously.
        if (!differing_) {
            differing_ = true;
            differing_first_ms_ = now_ms;
        }
        if (now_ms - differing_first_ms_ >= kDebounceMs) {
            debounced_pressed_ = raw_pressed;
            differing_ = false;
            if (debounced_pressed_) {
                // Press accepted: remember acceptance timestamp and clear long fired flag.
                press_ms_ = now_ms;
                long_fired_ = false;
                return std::nullopt;
            } else {
                // Release accepted: emit short press only if held briefly without a long press.
                const uint32_t duration_ms = now_ms - press_ms_;
                if (!long_fired_ && duration_ms < kShortMaxMs) {
                    return ButtonEvent::Short;
                }
                return std::nullopt;
            }
        }
    } else {
        // Any sample matching the debounced level cancels pending transitions.
        differing_ = false;
    }

    // While held pressed, emit long event once when the hold threshold is reached.
    if (debounced_pressed_ && !long_fired_ && (now_ms - press_ms_ >= kLongMs)) {
        long_fired_ = true;
        return ButtonEvent::Long;
    }

    return std::nullopt;
}

bool Debouncer::pressed() const {
    return debounced_pressed_;
}

}  // namespace core
