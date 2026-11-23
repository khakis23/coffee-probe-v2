#pragma once

#include <cstdint>
#include <utility>

#include "core/probe_state.hpp"

namespace state {

// The single shared ProbeState. Producers update their own fields; consumers copy it out.
bool init();                           // creates the static mutex, call once before tasks start
bool snapshot(core::ProbeState& out);  // false on lock timeout, leaving out unchanged
uint32_t lock_timeouts();              // count of failed lock attempts

namespace detail {

core::ProbeState* lock_state();  // nullptr on lock timeout
void unlock_state();

}  // namespace detail

// Never hold the lock across anything blocking: callbacks must only assign fields.
// Returns false on lock timeout without running the callback.
template <typename F>
bool update(F&& fn) {
    auto* shared = detail::lock_state();
    if (shared == nullptr) {
        return false;
    }
    std::forward<F>(fn)(*shared);
    detail::unlock_state();
    return true;
}

}  // namespace state
