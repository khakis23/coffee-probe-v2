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
    centered(fb, 40, "OK: restart now");
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

void line(Fb& fb, int row, std::string_view label, std::string_view value) {
    const int y = kFirstRowY + row * kRowH;
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

void draw_diagnostics(const Inputs& in, Fb& fb) {
    title(fb, "DIAGNOSTICS", "OK: back");
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
            draw_diagnostics(in, fb);
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
