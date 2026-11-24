#include <doctest/doctest.h>

#include <array>
#include <string>

#include "core/ui.hpp"

using core::DeciC;
using core::Framebuffer;
using core::TcFault;
namespace ui = core::ui;

namespace {

// Geometry of the Live screen, mirrored from ui.cpp: three 26x46 digit cells 32 px apart,
// starting at x = (128 - 114) / 2 = 7, y = 15.
constexpr int kX0 = 7;
constexpr int kPitch = 32;
constexpr int kY = 15;

// Reads the digit shown in slot s the way a person would: by which segments are lit.
// Returns '0'..'9', '-', ' ' (blank) or '!' (not a valid 7-segment pattern).
char read_slot(const Framebuffer& fb, int s) {
    const int x = kX0 + s * kPitch;
    // A centred "1" is a 6-px bar at x + 10..15: the left and right edge columns stay dark.
    const bool left = fb.pixel(x + 2, kY + 11) || fb.pixel(x + 2, kY + 34);
    const bool right = fb.pixel(x + 23, kY + 11) || fb.pixel(x + 23, kY + 34);
    const bool centre = fb.pixel(x + 13, kY + 11) && fb.pixel(x + 13, kY + 34);
    if (!left && !right && centre) {
        return '1';
    }
    int mask = 0;
    const std::array<std::pair<int, int>, 7> probes{{
        {13, 2},   // a
        {23, 11},  // b
        {23, 34},  // c
        {13, 43},  // d
        {2, 34},   // e
        {2, 11},   // f
        {13, 23},  // g
    }};
    for (int bit = 0; bit < 7; ++bit) {
        if (fb.pixel(x + probes[static_cast<std::size_t>(bit)].first,
                     kY + probes[static_cast<std::size_t>(bit)].second)) {
            mask |= 1 << bit;
        }
    }
    constexpr std::array<int, 10> kDigits{0x3F, 0x06, 0x5B, 0x4F, 0x66,
                                          0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    for (int d = 0; d < 10; ++d) {
        if (mask == kDigits[static_cast<std::size_t>(d)]) return static_cast<char>('0' + d);
    }
    if (mask == 0x40) return '-';
    if (mask == 0) return ' ';
    return '!';
}

std::string read_number(const Framebuffer& fb) {
    return {read_slot(fb, 0), read_slot(fb, 1), read_slot(fb, 2)};
}

bool any_pixel(const Framebuffer& fb, int x0, int y0, int x1, int y1) {
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            if (fb.pixel(x, y)) return true;
        }
    }
    return false;
}

// A fresh, valid reading at t = 10 s with nothing connected.
ui::Inputs live(int16_t deci_c) {
    ui::Inputs in;
    in.state.bt = DeciC{deci_c};
    in.state.fault = TcFault::None;
    in.state.stale = false;
    in.state.bt_updated_ms = 10'000;
    in.now_ms = 10'250;
    in.ssid = "CoffeeProbe-3FA2";
    in.password = "coffeeprobe";
    return in;
}

Framebuffer draw(const ui::Inputs& in) {
    Framebuffer fb;
    ui::render(in, fb);
    return fb;
}

}  // namespace

TEST_CASE("Screen choice: temperature whenever it is trustworthy, never a stale number") {
    // The PR's ProbeState -> screen table, row by row.
    auto in = live(1000);
    CHECK(ui::choose_screen(in) == ui::Screen::Live);

    for (const auto fault : {TcFault::Open, TcFault::ShortGnd, TcFault::ShortVcc, TcFault::BusError}) {
        CAPTURE(static_cast<int>(fault));
        auto faulted = live(1000);
        faulted.state.fault = fault;
        CHECK(ui::choose_screen(faulted) == ui::Screen::Probe);
    }

    auto stale_flag = live(1000);
    stale_flag.state.stale = true;
    CHECK(ui::choose_screen(stale_flag) == ui::Screen::Probe);

    // The sensor job froze: the flag still says fresh, but the reading is 2001 ms old.
    auto too_old = live(1000);
    too_old.now_ms = 12'000;
    CHECK(ui::choose_screen(too_old) == ui::Screen::Live);  // Exactly 2000 ms: still valid.
    too_old.now_ms = 12'001;
    CHECK(ui::choose_screen(too_old) == ui::Screen::Probe);

    // Just after boot, no reading yet: Starting until 3 s, then PROBE?.
    ui::Inputs boot;  // Default state: BusError, stale.
    boot.now_ms = 2'999;
    CHECK(ui::choose_screen(boot) == ui::Screen::Starting);
    boot.now_ms = 3'000;
    CHECK(ui::choose_screen(boot) == ui::Screen::Probe);

    // A valid reading wins even during start-up.
    auto early = live(1000);
    early.state.bt_updated_ms = 800;
    early.now_ms = 900;
    CHECK(ui::choose_screen(early) == ui::Screen::Live);
}

TEST_CASE("Top bar: Artisan only while it is actually polling") {
    auto in = live(1000);
    CHECK(ui::link_status(in) == ui::Link::JoinWifi);

    in.state.wifi_clients = 1;
    CHECK(ui::link_status(in) == ui::Link::WaitArtisan);

    in.state.modbus_clients = 1;
    in.state.modbus_last_request_ms = 0;  // Connected but never asked: not polling yet.
    CHECK(ui::link_status(in) == ui::Link::WaitArtisan);

    in.state.modbus_last_request_ms = 5'250;  // 10250 - 5250 = 5000 ms ago: the limit.
    CHECK(ui::link_status(in) == ui::Link::Artisan);
    in.state.modbus_last_request_ms = 5'249;  // 5001 ms ago: Artisan stopped (OFF pressed).
    CHECK(ui::link_status(in) == ui::Link::WaitArtisan);

    // A request timestamp just before the millisecond clock wrapped, 250 ms ago.
    in.now_ms = 150;
    in.state.modbus_last_request_ms = UINT32_MAX - 99;
    CHECK(ui::link_status(in) == ui::Link::Artisan);
}

TEST_CASE("Live screen shows whole degrees F, right-aligned") {
    // 213.1 C = 415.58 F -> 416.
    CHECK(read_number(draw(live(2131))) == "416");
    // 22.2 C = 71.96 F -> " 72": the empty hundreds slot stays blank.
    CHECK(read_number(draw(live(222))) == " 72");
    // 2.5 C = 36.5 F exactly (25 * 9 / 5 + 320 = 365 tenths): halves round away from zero -> 37.
    CHECK(read_number(draw(live(25))) == " 37");
    // -22.5 C = -8.5 F exactly (-405 + 320 = -85 tenths) -> -9, minus in the middle slot.
    CHECK(read_number(draw(live(-225))) == " -9");
    // -20 C = -4 F.
    CHECK(read_number(draw(live(-200))) == " -4");
    // 0 C = 32 F; 100 C = 212 F; every digit appears somewhere in these.
    CHECK(read_number(draw(live(0))) == " 32");
    CHECK(read_number(draw(live(1000))) == "212");
    // 98.9 C = 210.02 F -> 210 ("1" and "0"); 256.7 C = 494.06 F -> 494; 307.2 C = 584.96 F -> 585.
    CHECK(read_number(draw(live(989))) == "210");
    CHECK(read_number(draw(live(2567))) == "494");
    CHECK(read_number(draw(live(3072))) == "585");
    // 260 C = 500 F; 482.2 C = 899.96 F -> 900; 537.2 C = 998.96 F -> 999 (the largest shown).
    CHECK(read_number(draw(live(2600))) == "500");
    CHECK(read_number(draw(live(4822))) == "900");
    CHECK(read_number(draw(live(5372))) == "999");
    // 37.8 C = 100.04 F -> 100; -72.8 C = -99.04 F -> -99 (the lowest shown).
    CHECK(read_number(draw(live(378))) == "100");
    CHECK(read_number(draw(live(-728))) == "-99");
    // 66.7 C = 152.06 F -> 152; 18.3 C = 64.94 F -> 65; 340 C = 644 F; 43.3 C = 109.94 -> 110.
    CHECK(read_number(draw(live(667))) == "152");
    CHECK(read_number(draw(live(183))) == " 65");
    CHECK(read_number(draw(live(3400))) == "644");
    CHECK(read_number(draw(live(433))) == "110");
    // 25.6 C = 78.08 F -> 78; 31.7 C = 89.06 F -> 89; 36.7 C = 98.06 -> 98; 15 C = 59 F.
    CHECK(read_number(draw(live(256))) == " 78");
    CHECK(read_number(draw(live(317))) == " 89");
    CHECK(read_number(draw(live(367))) == " 98");
    CHECK(read_number(draw(live(150))) == " 59");
}

TEST_CASE("Live screen shows HI or LO instead of a number it can't fit") {
    // 537.5 C = 999.5 F -> 1000: too big for three digits. 600 C = 1112 F.
    // -74 C = -101.2 F -> -101: too small. -270 C = -454 F.
    for (const int16_t c : {int16_t{5375}, int16_t{6000}, int16_t{-740}, int16_t{-2700}}) {
        CAPTURE(c);
        const auto fb = draw(live(c));
        // No digit is readable; big text fills the middle instead.
        CHECK(read_number(fb).find_first_of("0123456789") == std::string::npos);
        CHECK(any_pixel(fb, 0, 24, 128, 52));
    }
}

TEST_CASE("PROBE? screen never shows a temperature") {
    for (const auto fault : {TcFault::Open, TcFault::ShortGnd, TcFault::BusError, TcFault::None}) {
        CAPTURE(static_cast<int>(fault));
        auto in = live(2131);  // A real value is still in the state: it must not appear.
        in.state.fault = fault;
        in.state.stale = fault == TcFault::None;  // None + stale = "No fresh reading".
        const auto fb = draw(in);
        // The held temperature has no influence at all: a different one gives the same frame.
        auto other = in;
        other.state.bt = DeciC{1000};
        CHECK(fb == draw(other));
        CHECK(any_pixel(fb, 0, 15, 128, 36));  // "PROBE?" on rows 15..35.
        CHECK(any_pixel(fb, 0, 43, 128, 50));  // The cause.
        CHECK(any_pixel(fb, 0, 54, 128, 61));  // What to do.
    }
}

TEST_CASE("Every screen has the top bar and divider; Join and Password alternate every 3 s") {
    auto in = live(1000);
    in.now_ms = 10'250;  // 10250 / 3000 = 3, odd: the password.
    const auto password = draw(in);
    in.now_ms = 12'250;  // 12250 / 3000 = 4, even: the Wi-Fi name.
    in.state.bt_updated_ms = 12'000;  // Keep the reading fresh (250 ms old).
    const auto join = draw(in);
    for (const auto* fb : {&password, &join}) {
        CHECK(any_pixel(*fb, 0, 0, 128, 7));
        for (int x = 0; x < 128; ++x) CHECK(fb->pixel(x, 9));
    }
    // Only the top bar differs; the number below is the same.
    CHECK(read_number(password) == "212");
    CHECK(read_number(join) == "212");
    bool bar_differs = false;
    for (int x = 0; x < 128; ++x) {
        for (int y = 0; y < 7; ++y) bar_differs |= password.pixel(x, y) != join.pixel(x, y);
    }
    CHECK(bar_differs);

    ui::Inputs boot;
    boot.now_ms = 500;
    const auto starting = draw(boot);
    CHECK(any_pixel(starting, 0, 0, 128, 7));
    CHECK(any_pixel(starting, 0, 30, 128, 44));  // "Starting" at scale 2.
}

TEST_CASE("Rendering is pure: the same inputs give the same frame") {
    const auto in = live(2131);
    CHECK(draw(in) == draw(in));
    Framebuffer dirty;
    dirty.fill_rect(0, 0, 128, 64);
    ui::render(in, dirty);  // Starts by clearing: nothing from the old frame survives.
    CHECK(dirty == draw(in));
}
