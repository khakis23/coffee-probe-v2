#include <doctest/doctest.h>

#include <climits>

#include "core/font5x7.hpp"
#include "core/framebuffer.hpp"

using core::Framebuffer;

TEST_CASE("Framebuffer uses the SSD1306 page layout") {
    Framebuffer fb;
    fb.set_pixel(0, 0);
    // Page 0, column 0, bit 0.
    CHECK(fb.bytes()[0] == 0x01);
    fb.set_pixel(5, 9);
    // Row 9 is page 1 (rows 8..15), bit 1; column 5 -> byte 128 + 5.
    CHECK(fb.bytes()[133] == 0x02);
    CHECK(fb.page(1)[5] == 0x02);
    fb.set_pixel(127, 63);
    // Last byte, top bit.
    CHECK(fb.bytes()[1023] == 0x80);
    CHECK(fb.pixel(5, 9));
    CHECK_FALSE(fb.pixel(5, 8));
    fb.set_pixel(5, 9, false);
    CHECK(fb.bytes()[133] == 0x00);
}

TEST_CASE("Framebuffer clips everything outside 128x64") {
    Framebuffer fb;
    const Framebuffer empty;
    fb.set_pixel(-1, 0);
    fb.set_pixel(128, 0);
    fb.set_pixel(0, -1);
    fb.set_pixel(0, 64);
    fb.fill_rect(-50, -50, 40, 40);
    fb.fill_rect(200, 10, 5, 5);
    CHECK(fb == empty);
    CHECK_FALSE(fb.pixel(-1, 0));
    CHECK_FALSE(fb.pixel(0, 64));

    // A rectangle hanging off the bottom-right corner keeps only its visible 2x3 pixels.
    fb.fill_rect(126, 61, 10, 10);
    int lit = 0;
    for (int y = 0; y < Framebuffer::kHeight; ++y) {
        for (int x = 0; x < Framebuffer::kWidth; ++x) {
            lit += fb.pixel(x, y) ? 1 : 0;
        }
    }
    CHECK(lit == 6);
    // Huge sizes clip without overflowing: the same 6 pixels (126..127 x 61..63).
    Framebuffer huge;
    huge.fill_rect(126, 61, INT_MAX, INT_MAX);
    Framebuffer corner;
    corner.fill_rect(126, 61, 2, 3);
    CHECK(huge == corner);
    // Extreme negative edges draw nothing: x + w = INT_MIN - 1 must not wrap to a large value.
    Framebuffer negative;
    negative.fill_rect(INT_MIN, 0, -1, 1);
    negative.fill_rect(0, INT_MIN, 1, -1);
    negative.fill_rect(INT_MIN, INT_MIN, INT_MAX, INT_MAX);  // Right edge -1: still nothing.
    CHECK(negative == empty);
    fb.clear();
    CHECK(fb == empty);
}

TEST_CASE("Text advances 6 pixels per glyph and measures without the trailing gap") {
    CHECK(Framebuffer::text_width("") == 0);
    CHECK(Framebuffer::text_width("A") == 5);
    CHECK(Framebuffer::text_width("AB") == 11);       // 2 * 6 - 1.
    CHECK(Framebuffer::text_width("AB", 2) == 22);    // Everything doubles at scale 2.
    CHECK(Framebuffer::text_width("AB", 0) == 11);    // Scale 0 is treated as 1.
    // 21 characters fill the 128-pixel line: 21 * 6 - 1 = 125.
    CHECK(Framebuffer::text_width("Password: coffeeprobe") == 125);

    Framebuffer fb;
    CHECK(fb.text(3, 0, "AB") == 15);
    CHECK(fb.text(0, 20, "AB", 3) == 36);
}

TEST_CASE("Text draws the font's columns, scaled, and '?' for unknown characters") {
    // '!' is drawn in the generator as column 2, rows 0-4 and 6: 0b1011111 = 0x5F.
    CHECK(core::font5x7::kGlyphs[('!' - 0x20) * 5 + 2] == 0x5F);

    Framebuffer fb;
    fb.text(10, 10, "!");
    for (int row = 0; row < 7; ++row) {
        CAPTURE(row);
        CHECK(fb.pixel(12, 10 + row) == (row != 5));
        CHECK_FALSE(fb.pixel(11, 10 + row));
    }

    Framebuffer big;
    big.text(0, 0, "!", 2);
    // Column 2 at scale 2 covers x 4..5; row 5 (the gap) covers y 10..11.
    CHECK(big.pixel(4, 0));
    CHECK(big.pixel(5, 9));
    CHECK_FALSE(big.pixel(5, 10));
    CHECK(big.pixel(5, 12));

    Framebuffer unknown;
    Framebuffer question;
    unknown.text(0, 0, std::string_view("\x01", 1));
    question.text(0, 0, "?");
    CHECK(unknown == question);
}
