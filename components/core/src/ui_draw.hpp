#pragma once

// Drawing helpers shared by the screens in ui.cpp and menu_screens.cpp (internal to core).

#include <string_view>

#include "core/framebuffer.hpp"
#include "core/ui.hpp"

namespace core::ui::detail {

// a and b drawn as one centred line (no heap to join them).
inline void centered(Framebuffer& fb, int y, std::string_view a, std::string_view b = {},
                     int scale = 1) {
    using Fb = Framebuffer;
    const int gap = (a.empty() || b.empty()) ? 0 : (Fb::kGlyphAdvance - Fb::kGlyphWidth) * scale;
    const int w = Fb::text_width(a, scale) + gap + Fb::text_width(b, scale);
    const int x = (Fb::kWidth - w) / 2;
    fb.text(fb.text(x, y, a, scale), y, b, scale);
}

// The settings menu, every page. Full screen: the top bar is not drawn.
void draw_menu(const Inputs& in, Framebuffer& fb);

}  // namespace core::ui::detail
