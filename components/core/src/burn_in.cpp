#include "core/burn_in.hpp"

#include <array>

namespace core::ui {
namespace {

// A loop of neighbouring positions, so each step moves one pixel.
constexpr std::array<Offset, 6> kOffsets{{{0, 0}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}}};

}  // namespace

Offset burn_in_offset(uint32_t now_ms) {
    return kOffsets[(now_ms / kShiftPeriodMs) % kOffsets.size()];
}

void shift(Framebuffer& fb, int dx, int dy) {
    if (dx == 0 && dy == 0) {
        return;
    }
    constexpr int w = Framebuffer::kWidth;
    constexpr int h = Framebuffer::kHeight;
    // In place: walk from the side the picture moves towards, so every source pixel is read
    // before it is overwritten. No second 1 KiB frame on the UI task's stack.
    for (int i = 0; i < h; ++i) {
        const int y = dy > 0 ? h - 1 - i : i;
        for (int j = 0; j < w; ++j) {
            const int x = dx > 0 ? w - 1 - j : j;
            const int sx = x - dx;
            const int sy = y - dy;
            const bool on = sx >= 0 && sx < w && sy >= 0 && sy < h && fb.pixel(sx, sy);
            fb.set_pixel(x, y, on);
        }
    }
}

bool IdleDimmer::update(Screen screen, bool pressed, uint32_t now_ms) {
    const bool keeps_bright =
        pressed || screen == Screen::Live || screen == Screen::Menu || screen == Screen::TooHot;
    if (!started_ || keeps_bright) {
        started_ = true;
        active_ms_ = now_ms;
        return false;
    }
    return now_ms - active_ms_ >= kDimAfterMs;
}

}  // namespace core::ui
