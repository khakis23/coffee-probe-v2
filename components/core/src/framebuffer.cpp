#include "core/framebuffer.hpp"

#include <algorithm>

#include "core/font5x7.hpp"

namespace core {

void Framebuffer::clear() { bytes_.fill(0); }

void Framebuffer::set_pixel(int x, int y, bool on) {
    if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) {
        return;
    }
    const auto index = static_cast<std::size_t>((y / 8) * kWidth + x);
    const auto bit = static_cast<uint8_t>(1u << (y % 8));
    if (on) {
        bytes_[index] |= bit;
    } else {
        bytes_[index] &= static_cast<uint8_t>(~bit);
    }
}

bool Framebuffer::pixel(int x, int y) const {
    if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) {
        return false;
    }
    return (bytes_[static_cast<std::size_t>((y / 8) * kWidth + x)] >> (y % 8)) & 1u;
}

void Framebuffer::fill_rect(int x, int y, int w, int h, bool on) {
    // Clip first so a huge or negative rectangle costs only the visible pixels.
    const int x0 = std::max(x, 0);
    const int y0 = std::max(y, 0);
    const int x1 = std::min(x + w, kWidth);
    const int y1 = std::min(y + h, kHeight);
    for (int py = y0; py < y1; ++py) {
        for (int px = x0; px < x1; ++px) {
            set_pixel(px, py, on);
        }
    }
}

void Framebuffer::hline(int x, int y, int w, bool on) { fill_rect(x, y, w, 1, on); }

int Framebuffer::text(int x, int y, std::string_view s, int scale) {
    const int k = std::max(scale, 1);
    for (const char raw : s) {
        const char c = (raw < font5x7::kFirst || raw > font5x7::kLast) ? '?' : raw;
        const auto glyph = static_cast<std::size_t>(c - font5x7::kFirst) * kGlyphWidth;
        for (int col = 0; col < kGlyphWidth; ++col) {
            const uint8_t bits = font5x7::kGlyphs[glyph + static_cast<std::size_t>(col)];
            for (int row = 0; row < kGlyphHeight; ++row) {
                if ((bits >> row) & 1u) {
                    fill_rect(x + col * k, y + row * k, k, k);
                }
            }
        }
        x += kGlyphAdvance * k;
    }
    return x;
}

int Framebuffer::text_width(std::string_view s, int scale) {
    const int k = std::max(scale, 1);
    if (s.empty()) {
        return 0;
    }
    return static_cast<int>(s.size()) * kGlyphAdvance * k - (kGlyphAdvance - kGlyphWidth) * k;
}

}  // namespace core
