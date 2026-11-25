#include <doctest/doctest.h>

#include <set>
#include <utility>

#include "core/burn_in.hpp"
#include "core/ui.hpp"

using core::DeciC;
using core::Framebuffer;
namespace ui = core::ui;

namespace {

ui::Inputs live() {
    ui::Inputs in;
    in.state.bt = DeciC{2000};
    in.state.fault = core::TcFault::None;
    in.state.stale = false;
    in.state.bt_updated_ms = 10'000;
    in.now_ms = 10'000;
    in.ssid = "CoffeeProbe-3FA2";
    in.password = "coffeeprobe";
    return in;
}

// Every lit pixel in the edge columns or the bottom row belongs to a full-width line or bar,
// so a one-pixel shift never cuts off text or digits.
void check_margins(const Framebuffer& fb) {
    for (int y = 0; y < Framebuffer::kHeight; ++y) {
        for (const int x : {0, Framebuffer::kWidth - 1}) {
            if (fb.pixel(x, y)) {
                CAPTURE(x);
                CAPTURE(y);
                CHECK(fb.pixel(1, y));
                CHECK(fb.pixel(Framebuffer::kWidth - 2, y));
            }
        }
    }
    for (int x = 0; x < Framebuffer::kWidth; ++x) {
        CAPTURE(x);
        CHECK_FALSE(fb.pixel(x, Framebuffer::kHeight - 1));
    }
}

}  // namespace

TEST_CASE("burn-in: offsets cycle through neighbouring positions, one step per period") {
    std::set<std::pair<int, int>> seen;
    ui::Offset prev = ui::burn_in_offset(0);
    CHECK(prev.dx == 0);
    CHECK(prev.dy == 0);
    for (uint32_t step = 1; step <= 12; ++step) {
        const ui::Offset o = ui::burn_in_offset(step * ui::kShiftPeriodMs);
        CHECK(ui::burn_in_offset(step * ui::kShiftPeriodMs + ui::kShiftPeriodMs - 1).dx == o.dx);
        CHECK(o.dx >= -1);
        CHECK(o.dx <= 1);
        CHECK(o.dy >= 0);  // Never up.
        CHECK(o.dy <= 1);
        CHECK(std::abs(o.dx - prev.dx) + std::abs(o.dy - prev.dy) == 1);  // One pixel per step.
        seen.insert({o.dx, o.dy});
        prev = o;
    }
    CHECK(seen.size() == 6);
}

TEST_CASE("burn-in: shift moves every pixel and clears what it uncovers") {
    Framebuffer fb;
    fb.set_pixel(10, 20);
    fb.set_pixel(127, 63);
    fb.set_pixel(0, 0);
    for (const auto [dx, dy] : {std::pair{1, 1}, std::pair{-1, 0}, std::pair{1, 0}, std::pair{0, 1},
                                std::pair{-1, 1}}) {
        Framebuffer f = fb;
        ui::shift(f, dx, dy);
        CAPTURE(dx);
        CAPTURE(dy);
        CHECK(f.pixel(10 + dx, 20 + dy));
        CHECK_FALSE(f.pixel(10, 20));
        int lit = 0;
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 128; ++x) lit += f.pixel(x, y) ? 1 : 0;
        const int expected = 1 + ((127 + dx < 128 && 63 + dy < 64) ? 1 : 0) +
                             ((0 + dx >= 0) ? 1 : 0);
        CHECK(lit == expected);
    }
    Framebuffer same = fb;
    ui::shift(same, 0, 0);
    CHECK(same == fb);
}

TEST_CASE("burn-in: no shifted screen loses text or digits at the edges") {
    Framebuffer fb;
    ui::Inputs in = live();
    ui::render(in, fb);  // Live, "Join ..." top line
    check_margins(fb);
    in.now_ms += ui::kJoinAlternateMs;
    in.state.bt_updated_ms = in.now_ms;
    ui::render(in, fb);  // "Password: ..."
    check_margins(fb);
    in.state.power_warning = true;
    ui::render(in, fb);  // Inverted power warning bar
    check_margins(fb);
    in.state.power_warning = false;
    in.state.thermal = core::ThermalLevel::Warn;
    ui::render(in, fb);  // Overheat banner
    check_margins(fb);
    in.state.thermal = core::ThermalLevel::Critical;
    ui::render(in, fb);  // TOO HOT
    check_margins(fb);
    in.state.thermal = core::ThermalLevel::Normal;
    in.state.bt = DeciC{9999};
    ui::render(in, fb);  // HI
    check_margins(fb);
    in.state.bt = DeciC{-400};
    ui::render(in, fb);  // -40 F
    check_margins(fb);
    for (const auto fault : {core::TcFault::Open, core::TcFault::ShortGnd, core::TcFault::BusError}) {
        in.state.fault = fault;
        ui::render(in, fb);  // PROBE?
        check_margins(fb);
    }
    ui::Inputs starting = live();
    starting.now_ms = 100;
    starting.state.fault = core::TcFault::BusError;
    starting.state.bt_updated_ms = 0;  // Never read yet.
    ui::render(starting, fb);
    CHECK(ui::choose_screen(starting) == ui::Screen::Starting);
    check_margins(fb);
}

TEST_CASE("idle dimmer: dims PROBE?/Starting after 10 min, never Live, menu or TOO HOT") {
    using ui::Screen;
    ui::IdleDimmer d;
    CHECK_FALSE(d.update(Screen::Probe, false, 0));
    CHECK_FALSE(d.update(Screen::Probe, false, ui::kDimAfterMs - 1));
    CHECK(d.update(Screen::Probe, false, ui::kDimAfterMs));
    CHECK(d.update(Screen::Starting, false, ui::kDimAfterMs + 5));
    // A press brightens it and restarts the count.
    CHECK_FALSE(d.update(Screen::Probe, true, ui::kDimAfterMs + 10));
    CHECK_FALSE(d.update(Screen::Probe, false, 2 * ui::kDimAfterMs + 9));
    CHECK(d.update(Screen::Probe, false, 2 * ui::kDimAfterMs + 10));
    // A reading brightens it at once.
    CHECK_FALSE(d.update(Screen::Live, false, 3 * ui::kDimAfterMs));
    for (const auto s : {Screen::Live, Screen::Menu, Screen::TooHot}) {
        ui::IdleDimmer never;
        for (uint32_t t = 0; t <= 5 * ui::kDimAfterMs; t += 60'000) {
            CHECK_FALSE(never.update(s, false, t));
        }
    }
}

TEST_CASE("idle dimmer: the millisecond clock wrapping doesn't matter") {
    ui::IdleDimmer d;
    const uint32_t t0 = UINT32_MAX - 1000;
    CHECK_FALSE(d.update(ui::Screen::Probe, false, t0));
    CHECK_FALSE(d.update(ui::Screen::Probe, false, t0 + 60'000));
    CHECK(d.update(ui::Screen::Probe, false, t0 + ui::kDimAfterMs));
}
