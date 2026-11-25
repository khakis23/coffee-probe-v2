// The settings menu's pages (#23). Full screen, small text, the same layout on every page:
// a title on row 0, a divider on row 9, content below. Every page says which button does what,
// so staff can use it without a manual.

#include <algorithm>
#include <cstdio>
#include <optional>

#include "core/menu.hpp"
#include "core/temperature.hpp"
#include "ui_draw.hpp"

namespace core::ui::detail {
namespace {

using Fb = Framebuffer;
using menu::Item;
using menu::Menu;
using menu::Mode;

constexpr int kDividerY = 9;
constexpr int kRowH = 10;
constexpr int kFirstRowY = 12;
constexpr int kVisibleRows = 5;  // Rows at y = 12, 22, 32, 42, 52.

void title(Fb& fb, std::string_view left, std::string_view right = {}) {
    fb.text(0, 0, left);
    fb.text(Fb::kWidth - Fb::text_width(right), 0, right);
    fb.hline(0, kDividerY, Fb::kWidth);
}

std::string_view as_view(const char* buf, int n, std::size_t cap) {
    return {buf, static_cast<std::size_t>(std::clamp(n, 0, static_cast<int>(cap) - 1))};
}

// ---- List ----------------------------------------------------------------------------------

void draw_list(const Menu& m, Fb& fb) {
    const int cursor = static_cast<int>(m.cursor());
    char pos[8];
    const int n = std::snprintf(pos, sizeof(pos), "%d/%d", cursor + 1, menu::kItemCount);
    title(fb, "SETTINGS", as_view(pos, n, sizeof(pos)));

    // Keep the cursor in the middle row where possible, so the items around it stay visible.
    const int top = std::clamp(cursor - kVisibleRows / 2, 0, menu::kItemCount - kVisibleRows);
    for (int row = 0; row < kVisibleRows; ++row) {
        const int i = top + row;
        const auto item = static_cast<Item>(i);
        const int y = kFirstRowY + row * kRowH;
        const bool selected = i == cursor;
        if (selected) {
            fb.fill_rect(0, y - 1, Fb::kWidth, kRowH - 1);  // Lit bar, dark text.
        }
        char buf[12];
        const std::string_view value = menu::value_text(item, m.settings(), buf);
        fb.text(2, y, menu::label(item), 1, !selected);
        fb.text(Fb::kWidth - 2 - Fb::text_width(value), y, value, 1, !selected);
    }
}

// ---- Editor --------------------------------------------------------------------------------

void draw_edit(const Menu& m, Fb& fb) {
    title(fb, menu::label(m.cursor()));
    char buf[12];
    const std::string_view value = menu::value_text(m.cursor(), m.draft(), buf);
    centered(fb, 16, value, {}, 2);  // Rows 16..29.
    if (m.draft() != m.settings()) {
        // A small mark that the shown value is not saved yet.
        centered(fb, 32, "(not saved)");
    }
    centered(fb, 43, m.cursor() == Item::Offset ? "UP/DOWN, hold = 1 F" : "UP/DOWN to change");
    centered(fb, 54, "OK save  BACK cancel");
}

// ---- Prompts -------------------------------------------------------------------------------

void draw_restart_prompt(Fb& fb) {
    title(fb, "SAVED");
    centered(fb, 14, "Restart to apply");
    centered(fb, 24, "this change?");
    centered(fb, 40, "Hold OK: restart");
    centered(fb, 52, "BACK: next power-on");
}

void draw_confirm_reset(Fb& fb) {
    title(fb, "FACTORY RESET");
    centered(fb, 14, "All settings go back");
    centered(fb, 24, "to how they shipped.");
    centered(fb, 40, "Hold OK: reset");
    centered(fb, 52, "BACK: keep settings");
}

// ---- Diagnostics ---------------------------------------------------------------------------

// Whole degrees F straight from tenths of C, rounded once: F = (9c + 1600) / 50.
int whole_f(DeciC c) { return div_round_away(9 * static_cast<int32_t>(c.v) + 1600, 50); }

void line(Fb& fb, int row, std::string_view label, std::string_view value, int pitch = kRowH) {
    const int y = kFirstRowY + row * pitch;
    fb.text(2, y, label);
    fb.text(Fb::kWidth - 2 - Fb::text_width(value), y, value);
}

void temperature_line(Fb& fb, int row, std::string_view label, std::optional<DeciC> t) {
    char buf[12];
    if (!t) {
        line(fb, row, label, "--");
        return;
    }
    const int n = std::snprintf(buf, sizeof(buf), "%d F", whole_f(*t));
    line(fb, row, label, as_view(buf, n, sizeof(buf)));
}

void count_line(Fb& fb, int row, std::string_view label, unsigned long value, const char* unit,
                int pitch) {
    char buf[14];
    const int n = std::snprintf(buf, sizeof(buf), "%lu%s", value, unit);
    line(fb, row, label, as_view(buf, n, sizeof(buf)), pitch);
}

// Page 1, for staff: is it the right firmware, is it hot, is a laptop connected.
void draw_diagnostics_status(const Inputs& in, Fb& fb) {
    // A long git-describe version is cut so it never runs into the label.
    line(fb, 0, "Firmware", in.version.empty() ? std::string_view{"?"} : in.version.substr(0, 12));

    char buf[16];
    const uint32_t up = in.state.uptime_s;
    const int n = std::snprintf(buf, sizeof(buf), "%luh %02lum",
                                static_cast<unsigned long>(up / 3600),
                                static_cast<unsigned long>((up / 60) % 60));
    line(fb, 1, "Running for", as_view(buf, n, sizeof(buf)));

    temperature_line(fb, 2, "Box temp", in.state.cold_junction);
    temperature_line(fb, 3, "Chip temp", in.state.die);

    char clients[8];
    const int c = std::snprintf(clients, sizeof(clients), "%u",
                                static_cast<unsigned>(in.state.wifi_clients));
    line(fb, 4, "Wi-Fi devices", as_view(clients, c, sizeof(clients)));
}

// Page 2, for whoever helps when something went wrong (#25): restarts since power-on, why the
// last one happened, power dips, and whether a crash dump is waiting to be read.
void draw_diagnostics_restarts(const Inputs& in, Fb& fb) {
    const ProbeState& s = in.state;
    line(fb, 0, "Last reset", reset_cause_text(s.reset_cause));
    count_line(fb, 1, "Restarts", s.boots > 0 ? s.boots - 1u : 0u, "", kRowH);
    count_line(fb, 2, "Crashes", s.crashes, "", kRowH);
    count_line(fb, 3, "Power dips", s.brownouts, "", kRowH);
    line(fb, 4, "Crash record",
         s.crash_record == CrashRecord::Saved
             ? "Saved"
             : (s.crash_record == CrashRecord::Damaged ? "Damaged" : "None"));
}

// Page 3: where Artisan finds the probe, how many connections it has, and the memory margins.
void draw_diagnostics_system(const Inputs& in, Fb& fb) {
    const ProbeState& s = in.state;
    line(fb, 0, "Address", in.address.empty() ? std::string_view{"--"} : in.address);
    // Open Modbus connections (#16): normally 1, Artisan. More means another laptop or a stale
    // connection the server will drop; 4 is the limit.
    count_line(fb, 1, "Artisan links", s.modbus_clients, "", kRowH);
    if (s.min_free_heap == 0) {
        line(fb, 2, "Free memory", "--");
    } else {
        count_line(fb, 2, "Free memory", s.min_free_heap / 1024, " KB", kRowH);
    }
    if (s.min_stack_free == 0) {
        line(fb, 3, "Stack spare", "--");
    } else {
        count_line(fb, 3, "Stack spare", s.min_stack_free, " B", kRowH);
    }
}

void draw_diagnostics(const Inputs& in, const Menu& m, Fb& fb) {
    constexpr std::string_view kPageNames[menu::kDiagnosticsPages] = {"1/3", "2/3", "3/3"};
    const uint8_t page = m.diagnostics_page() < menu::kDiagnosticsPages ? m.diagnostics_page() : 0;
    title(fb, "DIAGNOSTICS", kPageNames[page]);
    switch (page) {
        case 1:
            draw_diagnostics_restarts(in, fb);
            break;
        case 2:
            draw_diagnostics_system(in, fb);
            break;
        default:
            draw_diagnostics_status(in, fb);
            break;
    }
}

}  // namespace

void draw_menu(const Inputs& in, Framebuffer& fb) {
    if (in.menu == nullptr) {
        return;
    }
    const Menu& m = *in.menu;
    switch (m.mode()) {
        case Mode::List:
            draw_list(m, fb);
            break;
        case Mode::Edit:
            draw_edit(m, fb);
            break;
        case Mode::Diagnostics:
            draw_diagnostics(in, m, fb);
            break;
        case Mode::ConfirmReset:
            draw_confirm_reset(fb);
            break;
        case Mode::RestartPrompt:
            draw_restart_prompt(fb);
            break;
        case Mode::Closed:
            break;
    }
}

}  // namespace core::ui::detail

namespace core::ui {

void render_boot_reset(menu::BootHold hold, uint8_t seconds_left, Framebuffer& fb) {
    using detail::centered;
    fb.clear();
    fb.text(0, 0, "FACTORY RESET");
    fb.hline(0, 9, Framebuffer::kWidth);
    if (hold == menu::BootHold::Reset) {
        centered(fb, 22, "Settings reset");
        centered(fb, 34, "Starting...");
        return;
    }
    centered(fb, 13, "Keep holding OK");
    const char digit[2] = {static_cast<char>('0' + std::min<int>(seconds_left, 9)), '\0'};
    centered(fb, 25, std::string_view{digit, 1}, {}, 3);  // Rows 25..45.
    centered(fb, 54, "Let go to cancel");
}

}  // namespace core::ui
