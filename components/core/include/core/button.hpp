#pragma once

#include <cstdint>
#include <optional>

namespace core {

enum class ButtonEvent : uint8_t { Short, Long };

inline constexpr uint32_t kDebounceMs = 20;   // a level must hold this long before it counts
inline constexpr uint32_t kShortMaxMs = 600;  // released before this after the press: Short
inline constexpr uint32_t kLongMs = 800;      // still held this long after the press: Long, once

class Debouncer {
public:
    // Call every poll (the buttons task polls every 10 ms) with the raw level and the time.
    std::optional<ButtonEvent> update(bool raw_pressed, uint32_t now_ms);
    bool pressed() const;  // the debounced level

private:
    bool debounced_pressed_{false};
    bool differing_{false};
    uint32_t differing_first_ms_{0};
    uint32_t press_ms_{0};
    bool long_fired_{false};
};

}  // namespace core
