#pragma once

#include <cstdint>

#include "core/framebuffer.hpp"
#include "core/ui.hpp"

// OLED burn-in protection (#20). An OLED pixel that stays lit for hundreds of hours wears faster
// than its neighbours and leaves a ghost image. Two measures, both pure so they're host-tested:
// - pixel shift: the whole picture moves by one pixel every few minutes, so no pixel is lit all
//   the time (the Live screen never dims, so this is its protection);
// - idle dimming: a screen nobody needs at full brightness (no reading, no menu) dims after a while.
namespace core::ui {

inline constexpr uint32_t kShiftPeriodMs = 3 * 60'000;
inline constexpr uint32_t kDimAfterMs = 10 * 60'000;

struct Offset {
    int8_t dx;
    int8_t dy;
};

// The shift at now_ms. One pixel at most, never up: the shifted screens keep their text and
// digits 1 px clear of the left and right edges and the bottom row, so nothing is cut off; only
// full-width lines and bars lose an end pixel (host-tested).
Offset burn_in_offset(uint32_t now_ms);

// Whether a screen gets the pixel shift. Not the menu: it is open for seconds and its rows are
// laid out to the edge.
constexpr bool shifts(Screen s) { return s != Screen::Menu; }

// Moves the picture by (dx, dy). Pixels pushed off an edge are dropped; uncovered ones are dark.
void shift(Framebuffer& fb, int dx, int dy);

class IdleDimmer {
   public:
    // Call every frame with the screen being shown and whether there is activity: a button press
    // or a button held, or a warning on screen (overheat banner, power warning). Returns true
    // while the panel should be dimmed. The Live screen (staff glance at it mid-roast), the menu
    // and TOO HOT are never dimmed. Activity or one of those screens brightens it at once and
    // restarts the count. Once dimmed it stays dimmed until then, so the clock wrapping after
    // 49.7 days can't brighten it.
    bool update(Screen screen, bool activity, uint32_t now_ms);

   private:
    bool started_{false};
    bool dimmed_{false};
    uint32_t active_ms_{0};  // Last time something kept the screen bright.
};

}  // namespace core::ui
