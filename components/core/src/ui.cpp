#include "core/ui.hpp"

#include <array>
#include <cstdlib>

namespace core::ui {
namespace {

using Fb = Framebuffer;

// ---- Top bar ------------------------------------------------------------------------------
// One line of small text on rows 0-6 and a divider on row 9, on every screen, so staff can
// always see whether Artisan is connected and, if nothing has joined, how to join.

constexpr int kBarY = 0;
constexpr int kDividerY = 9;

void centered(Fb& fb, int y, std::string_view a, std::string_view b = {}, int scale = 1) {
    // a and b are drawn as one line (no heap to join them).
    const int gap = (a.empty() || b.empty()) ? 0 : (Fb::kGlyphAdvance - Fb::kGlyphWidth) * scale;
    const int w = Fb::text_width(a, scale) + gap + Fb::text_width(b, scale);
    const int x = (Fb::kWidth - w) / 2;
    fb.text(fb.text(x, y, a, scale), y, b, scale);
}

void draw_top_bar(const Inputs& in, Fb& fb) {
    switch (link_status(in)) {
        case Link::Artisan:
            centered(fb, kBarY, "Artisan connected");
            break;
        case Link::WaitArtisan:
            centered(fb, kBarY, "Waiting for Artisan");
            break;
        case Link::JoinWifi:
            if (in.ssid.empty()) {
                centered(fb, kBarY, "Wi-Fi starting");
            } else if ((in.now_ms / kJoinAlternateMs) % 2 == 0) {
                centered(fb, kBarY, "Join ", in.ssid);
            } else {
                centered(fb, kBarY, "Password: ", in.password);
            }
            break;
    }
    fb.hline(0, kDividerY, Fb::kWidth);
}

// ---- Big 7-segment digits ------------------------------------------------------------------
// Drawn from filled rectangles: crisp at any size, no font data, readable across a roastery.

constexpr int kDigitW = 26;
constexpr int kDigitH = 46;  // Even, so the middle segment sits exactly halfway.
constexpr int kSegT = 6;     // Segment thickness.
constexpr int kDigitGap = 6;
constexpr int kDigitY = 15;  // Rows 15..60, below the divider.
constexpr int kSlots = 3;    // Bean temperatures stay well below 1000 F.

// Bits a..g = 0..6: a top, b upper right, c lower right, d bottom, e lower left, f upper left,
// g middle.
constexpr std::array<uint8_t, 10> kDigitSegments{0x3F, 0x06, 0x5B, 0x4F, 0x66,
                                                 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
constexpr uint8_t kMinusSegments = 0x40;

void draw_segments(Fb& fb, int x, int y, uint8_t mask) {
    constexpr int half = kDigitH / 2;
    if (mask & 0x01) fb.fill_rect(x, y, kDigitW, kSegT);                                  // a
    if (mask & 0x02) fb.fill_rect(x + kDigitW - kSegT, y, kSegT, half);                   // b
    if (mask & 0x04) fb.fill_rect(x + kDigitW - kSegT, y + half, kSegT, half);            // c
    if (mask & 0x08) fb.fill_rect(x, y + kDigitH - kSegT, kDigitW, kSegT);                // d
    if (mask & 0x10) fb.fill_rect(x, y + half, kSegT, half);                              // e
    if (mask & 0x20) fb.fill_rect(x, y, kSegT, half);                                     // f
    if (mask & 0x40) fb.fill_rect(x, y + half - kSegT / 2, kDigitW, kSegT);               // g
}

// Whole degrees, rounded half away from zero (212.5 F shows 213), as staff expect.
int whole_degrees(DeciF f) {
    const int v = f.v;
    return v >= 0 ? (v + 5) / 10 : -((-v + 5) / 10);
}

void draw_live(const Inputs& in, Fb& fb) {
    const int degrees = whole_degrees(to_f(in.state.bt));
    // Unit: a small degree ring and an F at scale 2, right of the digits.
    constexpr int kUnitW = 6 + 2 + Fb::kGlyphWidth * 2;
    constexpr int kTotalW = kSlots * kDigitW + (kSlots - 1) * kDigitGap + kDigitGap + kUnitW;
    constexpr int x0 = (Fb::kWidth - kTotalW) / 2;

    if (degrees > 999 || degrees < -99) {
        // Not a bean temperature: never show a truncated number. HI/LO in big text.
        centered(fb, 24, degrees > 999 ? "HI" : "LO", {}, 4);
        return;
    }

    // Right-aligned in three slots, so digits don't jump around as the value crosses 100.
    std::array<int, kSlots> slot{-1, -1, -1};  // -1 blank, 0..9 digit, 10 minus.
    int magnitude = std::abs(degrees);
    int i = kSlots - 1;
    do {
        slot[static_cast<std::size_t>(i--)] = magnitude % 10;
        magnitude /= 10;
    } while (magnitude > 0 && i >= 0);
    if (degrees < 0 && i >= 0) {
        slot[static_cast<std::size_t>(i)] = 10;
    }
    for (int s = 0; s < kSlots; ++s) {
        const int d = slot[static_cast<std::size_t>(s)];
        if (d < 0) continue;
        const uint8_t mask = d == 10 ? kMinusSegments : kDigitSegments[static_cast<std::size_t>(d)];
        // A 7-segment "1" hugs the right edge of its cell, which makes 416 read as "4 16".
        // Centre it instead, so the spacing between digits looks even.
        const int shift = d == 1 ? -(kDigitW - kSegT) / 2 : 0;
        draw_segments(fb, x0 + s * (kDigitW + kDigitGap) + shift, kDigitY, mask);
    }

    const int ux = x0 + kSlots * (kDigitW + kDigitGap);
    // Degree ring: a 6x6 square outline with the corners cut, at the top of the digit row.
    fb.fill_rect(ux + 1, kDigitY, 4, 1);
    fb.fill_rect(ux + 1, kDigitY + 5, 4, 1);
    fb.fill_rect(ux, kDigitY + 1, 1, 4);
    fb.fill_rect(ux + 5, kDigitY + 1, 1, 4);
    fb.text(ux + 8, kDigitY, "F", 2);
}

// ---- Probe problem -------------------------------------------------------------------------

struct Cause {
    std::string_view what;
    std::string_view action;
};

Cause probe_cause(const ProbeState& s) {
    switch (s.fault) {
        case TcFault::Open:
            return {"Probe unplugged?", "Check the plug"};
        case TcFault::ShortGnd:
        case TcFault::ShortVcc:
            return {"Probe wire short", "Check the wires"};
        case TcFault::BusError:
            return {"No probe signal", "Check probe board"};
        case TcFault::None:
            break;
    }
    // No fault but no fresh reading: the sensor job stalled; the watchdog restarts it.
    return {"No fresh reading", "Please wait"};
}

void draw_probe(const Inputs& in, Fb& fb) {
    centered(fb, 15, "PROBE?", {}, 3);  // Rows 15..35.
    const Cause cause = probe_cause(in.state);
    centered(fb, 43, cause.what);
    centered(fb, 54, cause.action);
}

void draw_starting(Fb& fb) { centered(fb, 30, "Starting", {}, 2); }

}  // namespace

Link link_status(const Inputs& in) {
    const ProbeState& s = in.state;
    const bool recent_request = s.modbus_last_request_ms != 0 &&
                                in.now_ms - s.modbus_last_request_ms <= kArtisanRecentMs;
    if (s.modbus_clients > 0 && recent_request) {
        return Link::Artisan;
    }
    if (s.wifi_clients > 0 || s.modbus_clients > 0) {
        return Link::WaitArtisan;
    }
    return Link::JoinWifi;
}

Screen choose_screen(const Inputs& in) {
    // Temperature first whenever it is trustworthy. bt_valid checks the reading's age itself,
    // so a stale number is never drawn as if it were live.
    if (bt_valid(in.state, in.now_ms)) {
        return Screen::Live;
    }
    // The first good reading needs three agreeing samples (~0.75 s); don't cry PROBE? before.
    if (in.now_ms < kStartupMs) {
        return Screen::Starting;
    }
    return Screen::Probe;
}

void render(const Inputs& in, Framebuffer& fb) {
    fb.clear();
    draw_top_bar(in, fb);
    switch (choose_screen(in)) {
        case Screen::Live:
            draw_live(in, fb);
            break;
        case Screen::Probe:
            draw_probe(in, fb);
            break;
        case Screen::Starting:
            draw_starting(fb);
            break;
    }
}

}  // namespace core::ui
