#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace core {

// A 128x64 monochrome frame in the SSD1306/SH1106 memory layout, so a driver can send it as is:
// 8 pages of 8 rows; byte [page * 128 + x] holds column x of that page, bit (y % 8) is row y.
// Drawing clips silently at the edges: callers never need bounds checks.
class Framebuffer {
   public:
    static constexpr int kWidth = 128;
    static constexpr int kHeight = 64;
    static constexpr int kPages = kHeight / 8;
    static constexpr std::size_t kBytes = static_cast<std::size_t>(kWidth) * kPages;  // 1 KiB.

    // The built-in font: 5x7 glyphs on a 6-pixel advance (one blank column), printable ASCII.
    // Characters outside 0x20..0x7E draw as '?'. At scale s each font pixel is an s x s block.
    static constexpr int kGlyphWidth = 5;
    static constexpr int kGlyphHeight = 7;
    static constexpr int kGlyphAdvance = 6;

    void clear();
    void set_pixel(int x, int y, bool on = true);
    bool pixel(int x, int y) const;
    void fill_rect(int x, int y, int w, int h, bool on = true);
    void hline(int x, int y, int w, bool on = true);

    // Draws text with its top-left corner at (x, y). Returns the x just after the last glyph's
    // advance, so text can be chained. Scale 0 is treated as 1. With on = false the glyphs are
    // cleared instead, for dark text on a lit bar.
    int text(int x, int y, std::string_view s, int scale = 1, bool on = true);
    // Width in pixels text() would advance, without the trailing blank column.
    static int text_width(std::string_view s, int scale = 1);

    // Page p (0..7) as the 128 bytes a controller expects for that page.
    const uint8_t* page(int p) const { return &bytes_[static_cast<std::size_t>(p) * kWidth]; }
    const std::array<uint8_t, kBytes>& bytes() const { return bytes_; }

    bool operator==(const Framebuffer&) const = default;

   private:
    std::array<uint8_t, kBytes> bytes_{};
};

}  // namespace core
