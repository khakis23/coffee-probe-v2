#include <doctest/doctest.h>

#include <string>

#include "core/menu.hpp"
#include "core/ui.hpp"

using core::ButtonEvent;
using core::Framebuffer;
using core::Settings;
using core::TcChip;
namespace menu = core::menu;
namespace ui = core::ui;
using menu::Action;
using menu::Button;
using menu::Item;
using menu::Menu;
using menu::Mode;

namespace {

constexpr menu::Press kUp{Button::Up, ButtonEvent::Short};
constexpr menu::Press kDown{Button::Down, ButtonEvent::Short};
constexpr menu::Press kUpLong{Button::Up, ButtonEvent::Long};
constexpr menu::Press kDownLong{Button::Down, ButtonEvent::Long};
constexpr menu::Press kOk{Button::Ok, ButtonEvent::Short};
constexpr menu::Press kOkLong{Button::Ok, ButtonEvent::Long};
constexpr menu::Press kBack{Button::Back, ButtonEvent::Short};

// Opens the menu and moves the cursor to item.
void open_at(Menu& m, Item item, uint32_t now = 0) {
    REQUIRE(m.handle(kOkLong, now) == Action::None);
    REQUIRE(m.mode() == Mode::List);
    while (m.cursor() != item) {
        m.handle(kDown, now);
    }
}

std::string text(Item item, const Settings& s) {
    char buf[12];
    return std::string(menu::value_text(item, s, buf));
}

}  // namespace

TEST_CASE("menu: only a long OK opens it") {
    Menu m{Settings{}};
    for (const auto p : {kUp, kDown, kOk, kBack, kUpLong, kDownLong,
                         menu::Press{Button::Back, ButtonEvent::Long}}) {
        CHECK(m.handle(p, 0) == Action::None);
        CHECK(m.mode() == Mode::Closed);
    }
    CHECK(m.handle(kOkLong, 0) == Action::None);
    CHECK(m.mode() == Mode::List);
    CHECK(m.cursor() == Item::TcChip);
}

TEST_CASE("menu: list moves and wraps, BACK closes") {
    Menu m{Settings{}};
    open_at(m, Item::TcChip);
    m.handle(kUp, 0);
    CHECK(m.cursor() == Item::FactoryReset);
    m.handle(kDown, 0);
    CHECK(m.cursor() == Item::TcChip);
    for (int i = 0; i < menu::kItemCount; ++i) {
        CHECK(static_cast<int>(m.cursor()) == i);
        m.handle(kDown, 0);
    }
    CHECK(m.cursor() == Item::TcChip);
    m.handle(kBack, 0);
    CHECK(m.mode() == Mode::Closed);
}

TEST_CASE("menu: reopening starts at the top with the saved values") {
    Menu m{Settings{}};
    open_at(m, Item::Brightness);
    m.handle(kOk, 0);
    m.handle(kDown, 0);  // Unsaved change.
    m.close();
    CHECK(m.mode() == Mode::Closed);
    CHECK(m.draft() == m.settings());
    m.handle(kOkLong, 0);
    CHECK(m.cursor() == Item::TcChip);
}

TEST_CASE("menu: OK on a setting opens its editor, BACK cancels the edit") {
    Settings start{};
    Menu m{start};
    open_at(m, Item::Flip);
    m.handle(kOk, 0);
    CHECK(m.mode() == Mode::Edit);
    m.handle(kUp, 0);
    CHECK(m.draft().display_flip);
    CHECK_FALSE(m.settings().display_flip);
    CHECK(m.handle(kBack, 0) == Action::None);
    CHECK(m.mode() == Mode::List);
    CHECK(m.settings() == start);
    CHECK(m.draft() == start);
}

TEST_CASE("menu: OK with nothing changed writes nothing") {
    Menu m{Settings{}};
    open_at(m, Item::Channel);
    m.handle(kOk, 0);
    m.handle(kUp, 0);
    m.handle(kDown, 0);  // Back where it started.
    CHECK(m.handle(kOk, 0) == Action::None);
    CHECK(m.mode() == Mode::List);
}

TEST_CASE("menu: saving a live setting returns Save and goes back to the list") {
    Menu m{Settings{}};
    open_at(m, Item::Offset);
    m.handle(kOk, 0);
    m.handle(kUp, 0);
    CHECK(m.handle(kOk, 0) == Action::Save);
    CHECK(m.mode() == Mode::List);
    CHECK(m.settings().offset_deci_f == 1);
    CHECK(m.cursor() == Item::Offset);
}

TEST_CASE("menu: settings that need a restart ask for one") {
    for (const Item item : {Item::TcChip, Item::Grounded, Item::Channel, Item::TxPower}) {
        CAPTURE(static_cast<int>(item));
        CHECK(menu::needs_restart(item));

        SUBCASE("a held OK restarts; a second tap doesn't") {
            Menu m{Settings{}};
            open_at(m, item);
            m.handle(kOk, 0);
            m.handle(kUp, 0);
            CHECK(m.handle(kOk, 0) == Action::Save);
            CHECK(m.mode() == Mode::RestartPrompt);
            CHECK(m.handle(kOk, 0) == Action::None);  // Review of #23: a double tap.
            CHECK(m.handle(kUp, 0) == Action::None);
            CHECK(m.mode() == Mode::RestartPrompt);
            CHECK(m.handle(kOkLong, 0) == Action::Restart);
            CHECK(m.mode() == Mode::Closed);
        }
        SUBCASE("BACK keeps the saved value for the next power-on") {
            Menu m{Settings{}};
            open_at(m, item);
            m.handle(kOk, 0);
            m.handle(kUp, 0);
            CHECK(m.handle(kOk, 0) == Action::Save);
            const Settings saved = m.settings();
            CHECK(saved != Settings{});
            CHECK(m.handle(kBack, 0) == Action::None);
            CHECK(m.mode() == Mode::List);
            CHECK(m.settings() == saved);
        }
    }
    for (const Item item : {Item::Offset, Item::Brightness, Item::Flip, Item::ShowPassword}) {
        CHECK_FALSE(menu::needs_restart(item));
    }
}

TEST_CASE("menu: every setting's editor") {
    Menu m{Settings{}};
    auto edit = [&m](Item item) {
        open_at(m, item);
        m.handle(kOk, 0);
        REQUIRE(m.mode() == Mode::Edit);
    };

    SUBCASE("probe chip toggles") {
        edit(Item::TcChip);
        m.handle(kUp, 0);
        CHECK(m.draft().tc_chip == TcChip::Max6675);
        m.handle(kUp, 0);
        CHECK(m.draft().tc_chip == TcChip::Max31855);
        m.handle(kDown, 0);
        CHECK(m.draft().tc_chip == TcChip::Max6675);
    }
    SUBCASE("booleans toggle") {
        edit(Item::Grounded);
        m.handle(kDown, 0);
        CHECK(m.draft().grounded_probe);
        m.handle(kBack, 0);
        while (m.cursor() != Item::ShowPassword) m.handle(kDown, 0);
        m.handle(kOk, 0);
        m.handle(kUp, 0);
        CHECK_FALSE(m.draft().show_password);
    }
    SUBCASE("offset: 0.1 F per press, 1 F per hold, clamped to +-10 F") {
        edit(Item::Offset);
        m.handle(kUp, 0);
        CHECK(m.draft().offset_deci_f == 1);
        m.handle(kUpLong, 0);
        CHECK(m.draft().offset_deci_f == 11);
        m.handle(kDownLong, 0);
        m.handle(kDownLong, 0);
        m.handle(kDown, 0);
        CHECK(m.draft().offset_deci_f == -10);
        for (int i = 0; i < 20; ++i) m.handle(kUpLong, 0);
        CHECK(m.draft().offset_deci_f == 100);
        for (int i = 0; i < 30; ++i) m.handle(kDownLong, 0);
        CHECK(m.draft().offset_deci_f == -100);
    }
    SUBCASE("channel cycles 1, 6, 11") {
        edit(Item::Channel);
        CHECK(m.draft().wifi_channel == 6);
        m.handle(kUp, 0);
        CHECK(m.draft().wifi_channel == 11);
        m.handle(kUp, 0);
        CHECK(m.draft().wifi_channel == 1);
        m.handle(kDown, 0);
        CHECK(m.draft().wifi_channel == 11);
    }
    SUBCASE("Wi-Fi power cycles Auto, Low, Medium, High") {
        edit(Item::TxPower);
        CHECK(text(Item::TxPower, m.draft()) == "Auto");
        m.handle(kUp, 0);
        CHECK(m.draft().tx_power_qdbm == 32);
        CHECK(text(Item::TxPower, m.draft()) == "Low");
        m.handle(kUp, 0);
        CHECK(text(Item::TxPower, m.draft()) == "Medium");
        m.handle(kUp, 0);
        CHECK(m.draft().tx_power_qdbm == 78);
        CHECK(text(Item::TxPower, m.draft()) == "High");
        m.handle(kUp, 0);
        CHECK(m.draft().tx_power_qdbm == 0);
        m.handle(kDown, 0);
        CHECK(m.draft().tx_power_qdbm == 78);
    }
    SUBCASE("brightness 1..4, clamped") {
        edit(Item::Brightness);
        CHECK(m.draft().brightness == 3);
        m.handle(kUp, 0);
        CHECK(m.draft().brightness == 3);
        for (int i = 0; i < 5; ++i) m.handle(kDown, 0);
        CHECK(m.draft().brightness == 0);
        CHECK(text(Item::Brightness, m.draft()) == "1");
    }
    SUBCASE("flip toggles") {
        edit(Item::Flip);
        m.handle(kUp, 0);
        CHECK(m.draft().display_flip);
    }
}

TEST_CASE("menu: a Wi-Fi power the menu doesn't offer steps to Auto") {
    Settings s{};
    s.tx_power_qdbm = 34;  // Valid for the settings model, not one of the menu's choices.
    CHECK(text(Item::TxPower, s) == "Custom");
    Menu m{s};
    open_at(m, Item::TxPower);
    m.handle(kOk, 0);
    m.handle(kUp, 0);
    CHECK(m.draft().tx_power_qdbm == 0);
}

TEST_CASE("menu: a channel the menu doesn't offer steps to the first choice") {
    Settings s{};
    s.wifi_channel = 3;  // Not valid for settings either; the menu still never gets stuck.
    Menu m{s};
    open_at(m, Item::Channel);
    m.handle(kOk, 0);
    m.handle(kDown, 0);
    CHECK(m.draft().wifi_channel == 1);
}

TEST_CASE("menu: diagnostics pages") {
    Menu m{Settings{}};
    open_at(m, Item::Diagnostics);
    CHECK(m.handle(kOk, 0) == Action::None);
    CHECK(m.mode() == Mode::Diagnostics);
    CHECK(m.diagnostics_page() == 0);
    m.handle(kDown, 0);
    CHECK(m.diagnostics_page() == 1);
    m.handle(kDown, 0);
    CHECK(m.diagnostics_page() == 2);
    m.handle(kDown, 0);
    CHECK(m.diagnostics_page() == 0);  // Wraps.
    m.handle(kUp, 0);
    CHECK(m.diagnostics_page() == 2);
    m.handle(kUp, 0);
    CHECK(m.diagnostics_page() == 1);
    CHECK(m.mode() == Mode::Diagnostics);
    for (const auto p : {kOk, kBack, kOkLong}) {
        m.handle(p, 0);
        CHECK(m.mode() == Mode::List);
        m.handle(kOk, 0);
        REQUIRE(m.mode() == Mode::Diagnostics);
        CHECK(m.diagnostics_page() == 0);  // Always opens on the first page.
        m.handle(kDown, 0);
    }
}

TEST_CASE("menu: factory reset needs a long OK on its confirm page") {
    Menu m{Settings{}};
    open_at(m, Item::FactoryReset);
    m.handle(kOk, 0);
    CHECK(m.mode() == Mode::ConfirmReset);
    CHECK(m.handle(kOk, 0) == Action::None);  // A tap does nothing: "Hold OK".
    CHECK(m.mode() == Mode::ConfirmReset);
    for (const auto p : {kUp, kDown, kBack}) {
        CHECK(m.handle(p, 0) == Action::None);
        CHECK(m.mode() == Mode::List);
        m.handle(kOk, 0);
        REQUIRE(m.mode() == Mode::ConfirmReset);
    }
    m.handle(kOk, 0);
    CHECK(m.handle(kOkLong, 0) == Action::FactoryReset);
    CHECK(m.mode() == Mode::Closed);
}

TEST_CASE("menu: closes after 30 s without a press, dropping an unsaved edit") {
    Menu m{Settings{}};
    open_at(m, Item::Brightness, 1000);
    m.handle(kOk, 1000);
    m.handle(kDown, 20'000);
    m.tick(20'000 + menu::kIdleCloseMs - 1);
    CHECK(m.mode() == Mode::Edit);  // The press at 20 s restarted the count.
    m.tick(20'000 + menu::kIdleCloseMs);
    CHECK(m.mode() == Mode::Closed);
    CHECK(m.settings().brightness == 3);
    CHECK(m.draft().brightness == 3);
}

TEST_CASE("menu: idle close works across the millisecond clock wrapping") {
    Menu m{Settings{}};
    const uint32_t t0 = UINT32_MAX - 1000;
    open_at(m, Item::TcChip, t0);
    m.tick(t0 + 5000);  // Wrapped: 3999.
    CHECK(m.mode() == Mode::List);
    m.tick(t0 + menu::kIdleCloseMs);
    CHECK(m.mode() == Mode::Closed);
}

TEST_CASE("menu: the restart prompt also closes when idle, without restarting") {
    Menu m{Settings{}};
    open_at(m, Item::Channel);
    m.handle(kOk, 0);
    m.handle(kUp, 0);
    REQUIRE(m.handle(kOk, 0) == Action::Save);
    m.tick(menu::kIdleCloseMs);
    CHECK(m.mode() == Mode::Closed);
    CHECK(m.settings().wifi_channel == 11);
}

TEST_CASE("menu: value text") {
    Settings s{};
    CHECK(text(Item::TcChip, s) == "MAX31855");
    s.tc_chip = TcChip::Max6675;
    CHECK(text(Item::TcChip, s) == "MAX6675");
    CHECK(text(Item::Offset, s) == "0.0 F");
    s.offset_deci_f = 15;
    CHECK(text(Item::Offset, s) == "+1.5 F");
    s.offset_deci_f = -100;
    CHECK(text(Item::Offset, s) == "-10.0 F");
    s.offset_deci_f = -5;
    CHECK(text(Item::Offset, s) == "-0.5 F");
    CHECK(text(Item::Channel, s) == "6");
    s.wifi_channel = 11;
    CHECK(text(Item::Channel, s) == "11");
    CHECK(text(Item::Brightness, s) == "4");
    CHECK(text(Item::Grounded, s) == "No");
    CHECK(text(Item::ShowPassword, s) == "Yes");
    CHECK(text(Item::Diagnostics, s).empty());
    CHECK(text(Item::FactoryReset, s).empty());
}

TEST_CASE("menu: every list row fits the screen with its widest value") {
    Settings widest{};
    widest.offset_deci_f = -100;
    widest.tx_power_qdbm = 52;  // "Medium"
    for (int i = 0; i < menu::kItemCount; ++i) {
        const auto item = static_cast<Item>(i);
        char buf[12];
        const int w = Framebuffer::text_width(menu::label(item)) +
                      Framebuffer::text_width(menu::value_text(item, widest, buf));
        CAPTURE(std::string(menu::label(item)));
        CHECK(w + 4 + Framebuffer::kGlyphAdvance <= Framebuffer::kWidth);  // Margins and a gap.
    }
}

// ---- Boot-time factory reset ---------------------------------------------------------------

TEST_CASE("boot reset: OK not pressed boots normally") {
    menu::BootResetHold h;
    CHECK(h.update(false, 100) == menu::BootHold::Released);
    CHECK(h.update(true, 200) == menu::BootHold::Released);  // Stays released.
}

TEST_CASE("boot reset: held for 5 s resets, with a 5..1 countdown") {
    menu::BootResetHold h;
    const uint32_t t0 = 300;
    CHECK(h.update(true, t0) == menu::BootHold::Holding);
    CHECK(h.seconds_left() == 5);
    CHECK(h.update(true, t0 + 1) == menu::BootHold::Holding);
    CHECK(h.seconds_left() == 5);
    CHECK(h.update(true, t0 + 1000) == menu::BootHold::Holding);
    CHECK(h.seconds_left() == 4);
    CHECK(h.update(true, t0 + 4001) == menu::BootHold::Holding);
    CHECK(h.seconds_left() == 1);
    CHECK(h.update(true, t0 + menu::kBootResetHoldMs - 1) == menu::BootHold::Holding);
    CHECK(h.update(true, t0 + menu::kBootResetHoldMs) == menu::BootHold::Reset);
    CHECK(h.seconds_left() == 0);
    CHECK(h.update(false, t0 + 6000) == menu::BootHold::Reset);  // Sticky.
}

TEST_CASE("boot reset: letting go part way cancels it") {
    menu::BootResetHold h;
    CHECK(h.update(true, 0) == menu::BootHold::Holding);
    CHECK(h.update(true, 3000) == menu::BootHold::Holding);
    CHECK(h.update(false, 3010) == menu::BootHold::Holding);
    CHECK(h.update(false, 3010 + menu::kBootReleaseMs - 1) == menu::BootHold::Holding);
    CHECK(h.update(false, 3010 + menu::kBootReleaseMs) == menu::BootHold::Released);
    CHECK(h.update(true, 9000) == menu::BootHold::Released);
}

TEST_CASE("boot reset: a short glitch during the hold doesn't cancel it") {
    // Review of #23: one bad sample must not abort a reset staff are holding for.
    menu::BootResetHold h;
    CHECK(h.update(true, 0) == menu::BootHold::Holding);
    CHECK(h.update(false, 2000) == menu::BootHold::Holding);
    CHECK(h.update(false, 2040) == menu::BootHold::Holding);
    CHECK(h.update(true, 2060) == menu::BootHold::Holding);
    CHECK(h.update(true, menu::kBootResetHoldMs) == menu::BootHold::Reset);
}

// ---- Rendering -----------------------------------------------------------------------------

namespace {

ui::Inputs live_inputs() {
    ui::Inputs in;
    in.state.bt = core::DeciC{1000};
    in.state.fault = core::TcFault::None;
    in.state.stale = false;
    in.state.bt_updated_ms = 10'000;
    in.now_ms = 10'000;
    in.ssid = "CoffeeProbe-3FA2";
    in.password = "roastme123";
    return in;
}

}  // namespace

TEST_CASE("menu screen: shown while open, TooHot still beats it") {
    Menu m{Settings{}};
    ui::Inputs in = live_inputs();
    in.menu = &m;
    CHECK(ui::choose_screen(in) == ui::Screen::Live);
    m.handle(kOkLong, 0);
    CHECK(ui::choose_screen(in) == ui::Screen::Menu);
    in.state.thermal = core::ThermalLevel::Critical;
    CHECK(ui::choose_screen(in) == ui::Screen::TooHot);
}

TEST_CASE("menu screen: every page draws something different") {
    Menu m{Settings{}};
    ui::Inputs in = live_inputs();
    in.menu = &m;
    Framebuffer live;
    ui::render(in, live);

    m.handle(kOkLong, 0);
    Framebuffer list;
    ui::render(in, list);
    CHECK(list != live);
    // The selected (first) row is a lit bar: its left margin pixel is on.
    CHECK(list.pixel(0, 12));
    CHECK_FALSE(list.pixel(0, 22));

    m.handle(kDown, 0);
    Framebuffer list2;
    ui::render(in, list2);
    CHECK(list2 != list);
    CHECK(list2.pixel(0, 22));

    m.handle(kOk, 0);  // Grounded editor.
    Framebuffer edit;
    ui::render(in, edit);
    m.handle(kUp, 0);
    Framebuffer edited;
    ui::render(in, edited);
    CHECK(edited != edit);  // The value and the "(not saved)" note changed.

    m.handle(kOk, 0);
    REQUIRE(m.mode() == Mode::RestartPrompt);
    Framebuffer prompt;
    ui::render(in, prompt);
    CHECK(prompt != edited);
}

TEST_CASE("menu screen: the list scrolls to keep the cursor visible") {
    Menu m{Settings{}};
    ui::Inputs in = live_inputs();
    in.menu = &m;
    m.handle(kOkLong, 0);
    m.handle(kUp, 0);  // Last item.
    REQUIRE(m.cursor() == Item::FactoryReset);
    Framebuffer fb;
    ui::render(in, fb);
    CHECK(fb.pixel(0, 52));  // Bottom row lit.
}

TEST_CASE("menu screen: diagnostics handles missing temperatures and a long version") {
    Menu m{Settings{}};
    ui::Inputs in = live_inputs();
    in.menu = &m;
    in.version = "v0.1.0-mvp-12-gabcdef0-dirty";
    in.state.cold_junction = std::nullopt;
    in.state.die = core::DeciC{525};
    in.state.uptime_s = 3 * 3600 + 7 * 60;
    open_at(m, Item::Diagnostics);
    m.handle(kOk, 0);
    Framebuffer fb;
    ui::render(in, fb);
    in.state.cold_junction = core::DeciC{300};
    Framebuffer fb2;
    ui::render(in, fb2);
    CHECK(fb != fb2);
}

TEST_CASE("top bar: password hidden when the setting is off") {
    ui::Inputs in = live_inputs();
    in.show_password = false;
    Framebuffer name_phase;
    Framebuffer password_phase;
    in.now_ms = 0;
    in.state.bt_updated_ms = 0;
    ui::render(in, name_phase);
    in.now_ms = ui::kJoinAlternateMs;
    in.state.bt_updated_ms = ui::kJoinAlternateMs;
    ui::render(in, password_phase);
    CHECK(name_phase == password_phase);

    in.show_password = true;
    Framebuffer shown;
    ui::render(in, shown);
    CHECK(shown != password_phase);
}

TEST_CASE("boot reset screen: countdown digits differ, done message differs") {
    Framebuffer five;
    Framebuffer four;
    Framebuffer done;
    ui::render_boot_reset(menu::BootHold::Holding, 5, five);
    ui::render_boot_reset(menu::BootHold::Holding, 4, four);
    ui::render_boot_reset(menu::BootHold::Reset, 0, done);
    CHECK(five != four);
    CHECK(done != four);
}

TEST_CASE("diagnostics pages 2 and 3: restarts, crash record, address, memory") {
    Menu m{Settings{}};
    ui::Inputs in = live_inputs();
    in.menu = &m;
    open_at(m, Item::Diagnostics);
    m.handle(kOk, 0);
    Framebuffer page1;
    ui::render(in, page1);
    m.handle(kDown, 0);
    Framebuffer page2;
    ui::render(in, page2);
    CHECK(page2 != page1);
    in.state.reset_cause = core::ResetCause::Brownout;
    in.state.boots = 4;
    in.state.brownouts = 2;
    in.state.crashes = 1;
    Framebuffer page2_counts;
    ui::render(in, page2_counts);
    CHECK(page2_counts != page2);
    in.state.crash_record = core::CrashRecord::Saved;
    Framebuffer page2_record;
    ui::render(in, page2_record);
    CHECK(page2_record != page2_counts);
    in.state.crash_record = core::CrashRecord::Damaged;
    Framebuffer page2_damaged;
    ui::render(in, page2_damaged);
    CHECK(page2_damaged != page2_record);

    m.handle(kDown, 0);
    Framebuffer unmeasured;
    ui::render(in, unmeasured);
    CHECK(unmeasured != page2_record);
    in.address = "192.168.4.1";
    Framebuffer addressed;
    ui::render(in, addressed);
    in.state.modbus_clients = 2;
    Framebuffer two_links;
    ui::render(in, two_links);
    CHECK(two_links != addressed);
    in.state.min_free_heap = 150 * 1024;
    in.state.min_stack_free = 1200;
    Framebuffer measured;
    ui::render(in, measured);
    CHECK(measured != unmeasured);
}

TEST_CASE("top bar: the power warning alternates with the connection line") {
    ui::Inputs in = live_inputs();
    in.state.power_warning = true;
    Framebuffer even;
    Framebuffer odd;
    in.now_ms = 0;
    in.state.bt_updated_ms = 0;
    ui::render(in, even);
    in.now_ms = ui::kJoinAlternateMs;
    in.state.bt_updated_ms = ui::kJoinAlternateMs;
    ui::render(in, odd);
    CHECK(odd.pixel(0, 0));         // Lit bar on the warning phase.
    CHECK_FALSE(even.pixel(0, 0));  // The normal top line on the other phase.

    // Overheat beats it on both phases.
    in.state.thermal = core::ThermalLevel::Warn;
    Framebuffer hot_odd;
    ui::render(in, hot_odd);
    in.state.power_warning = false;
    Framebuffer hot_only;
    ui::render(in, hot_only);
    CHECK(hot_odd == hot_only);
}
