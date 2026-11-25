#include "core/menu.hpp"

#include <algorithm>
#include <cstdio>

namespace core::menu {
namespace {

Item next_item(Item item, int direction) {
    const int i = (static_cast<int>(item) + direction + kItemCount) % kItemCount;
    return static_cast<Item>(i);
}

// Index of value in choices, or -1 if it is not one of them (a value written by another
// firmware version, still valid for the settings model).
template <std::size_t N>
int index_of(const uint8_t (&choices)[N], uint8_t value) {
    for (std::size_t i = 0; i < N; ++i) {
        if (choices[i] == value) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// Steps through a list of choices, wrapping. An unknown value steps to the first choice.
template <std::size_t N>
uint8_t step_choice(const uint8_t (&choices)[N], uint8_t value, int direction) {
    const int i = index_of(choices, value);
    if (i < 0) {
        return choices[0];
    }
    const int n = static_cast<int>(N);
    return choices[(i + direction + n) % n];
}

}  // namespace

bool needs_restart(Item item) {
    // Wi-Fi is configured once at boot, and so are the thermocouple driver and its decoder.
    switch (item) {
        case Item::TcChip:
        case Item::Grounded:
        case Item::Channel:
        case Item::TxPower:
            return true;
        default:
            return false;
    }
}

std::string_view label(Item item) {
    switch (item) {
        case Item::TcChip:
            return "Probe chip";
        case Item::Grounded:
            return "Grounded tip";
        case Item::Offset:
            return "Offset";
        case Item::Channel:
            return "Wi-Fi channel";
        case Item::TxPower:
            return "Wi-Fi power";
        case Item::Brightness:
            return "Brightness";
        case Item::Flip:
            return "Flip screen";
        case Item::ShowPassword:
            return "Show password";
        case Item::Diagnostics:
            return "Diagnostics";
        case Item::FactoryReset:
            return "Factory reset";
    }
    return "?";
}

std::string_view value_text(Item item, const Settings& s, char (&buf)[12]) {
    switch (item) {
        case Item::TcChip:
            return s.tc_chip == TcChip::Max6675 ? "MAX6675" : "MAX31855";
        case Item::Grounded:
            return s.grounded_probe ? "Yes" : "No";
        case Item::Offset: {
            const int v = s.offset_deci_f;
            const int mag = v < 0 ? -v : v;
            const char* sign = v > 0 ? "+" : (v < 0 ? "-" : "");
            const int n = std::snprintf(buf, sizeof(buf), "%s%d.%d F", sign, mag / 10, mag % 10);
            return {buf, static_cast<std::size_t>(std::clamp(n, 0, 11))};
        }
        case Item::Channel: {
            const int n = std::snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(s.wifi_channel));
            return {buf, static_cast<std::size_t>(std::clamp(n, 0, 11))};
        }
        case Item::TxPower:
            switch (index_of(kTxPowerChoices, s.tx_power_qdbm)) {
                case 0:
                    return "Auto";
                case 1:
                    return "Low";
                case 2:
                    return "Medium";
                case 3:
                    return "High";
                default:
                    return "Custom";
            }
        case Item::Brightness: {
            // Stored 0..3, shown 1..4: "0" would read as "off".
            const int n = std::snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(s.brightness) + 1);
            return {buf, static_cast<std::size_t>(std::clamp(n, 0, 11))};
        }
        case Item::Flip:
            return s.display_flip ? "Yes" : "No";
        case Item::ShowPassword:
            return s.show_password ? "Yes" : "No";
        case Item::Diagnostics:
        case Item::FactoryReset:
            return {};
    }
    return {};
}

Action Menu::handle(Press press, uint32_t now_ms) {
    if (mode_ == Mode::Closed) {
        // Only a long OK opens the menu, so a stray tap during a roast never hides the
        // temperature.
        if (press.button == Button::Ok && press.event == ButtonEvent::Long) {
            mode_ = Mode::List;
            cursor_ = Item::TcChip;
            draft_ = saved_;
            last_press_ms_ = now_ms;
        }
        return Action::None;
    }
    last_press_ms_ = now_ms;
    switch (mode_) {
        case Mode::List:
            return handle_list(press);
        case Mode::Edit:
            return handle_edit(press);
        case Mode::Diagnostics:
            if (press.button == Button::Ok || press.button == Button::Back) {
                mode_ = Mode::List;
            }
            return Action::None;
        case Mode::ConfirmReset:
            // A long OK, so a reset can't happen by tapping through the screens.
            if (press.button == Button::Ok && press.event == ButtonEvent::Long) {
                close();
                return Action::FactoryReset;
            }
            if (press.button == Button::Back) {
                mode_ = Mode::List;
            }
            return Action::None;
        case Mode::RestartPrompt:
            if (press.button == Button::Ok) {
                close();
                return Action::Restart;
            }
            if (press.button == Button::Back) {
                mode_ = Mode::List;  // Saved; it applies at the next power-on.
            }
            return Action::None;
        case Mode::Closed:
            break;
    }
    return Action::None;
}

Action Menu::handle_list(Press press) {
    switch (press.button) {
        case Button::Up:
            cursor_ = next_item(cursor_, -1);
            break;
        case Button::Down:
            cursor_ = next_item(cursor_, +1);
            break;
        case Button::Ok:
            if (cursor_ == Item::Diagnostics) {
                mode_ = Mode::Diagnostics;
            } else if (cursor_ == Item::FactoryReset) {
                mode_ = Mode::ConfirmReset;
            } else {
                draft_ = saved_;
                mode_ = Mode::Edit;
            }
            break;
        case Button::Back:
            close();
            break;
    }
    return Action::None;
}

Action Menu::handle_edit(Press press) {
    switch (press.button) {
        case Button::Up:
            step_draft(+1, press.event == ButtonEvent::Long);
            return Action::None;
        case Button::Down:
            step_draft(-1, press.event == ButtonEvent::Long);
            return Action::None;
        case Button::Back:
            draft_ = saved_;
            mode_ = Mode::List;
            return Action::None;
        case Button::Ok:
            break;
    }
    if (draft_ == saved_) {
        mode_ = Mode::List;  // Nothing changed: nothing to write.
        return Action::None;
    }
    saved_ = draft_;
    mode_ = needs_restart(cursor_) ? Mode::RestartPrompt : Mode::List;
    return Action::Save;
}

void Menu::step_draft(int direction, bool big) {
    Settings& d = draft_;
    switch (cursor_) {
        case Item::TcChip:
            d.tc_chip = d.tc_chip == TcChip::Max31855 ? TcChip::Max6675 : TcChip::Max31855;
            break;
        case Item::Grounded:
            d.grounded_probe = !d.grounded_probe;
            break;
        case Item::Offset: {
            const int step = (big ? kOffsetStepLong : kOffsetStepShort) * direction;
            d.offset_deci_f = static_cast<int16_t>(
                std::clamp(d.offset_deci_f + step, -static_cast<int>(kOffsetLimit),
                           static_cast<int>(kOffsetLimit)));
            break;
        }
        case Item::Channel:
            d.wifi_channel = step_choice(kChannelChoices, d.wifi_channel, direction);
            break;
        case Item::TxPower:
            d.tx_power_qdbm = step_choice(kTxPowerChoices, d.tx_power_qdbm, direction);
            break;
        case Item::Brightness:
            d.brightness = static_cast<uint8_t>(std::clamp(d.brightness + direction, 0, 3));
            break;
        case Item::Flip:
            d.display_flip = !d.display_flip;
            break;
        case Item::ShowPassword:
            d.show_password = !d.show_password;
            break;
        case Item::Diagnostics:
        case Item::FactoryReset:
            break;
    }
}

void Menu::tick(uint32_t now_ms) {
    if (mode_ != Mode::Closed && now_ms - last_press_ms_ >= kIdleCloseMs) {
        close();
    }
}

void Menu::close() {
    draft_ = saved_;
    mode_ = Mode::Closed;
}

BootHold BootResetHold::update(bool ok_pressed, uint32_t now_ms) {
    if (result_ != BootHold::Holding) {
        return result_;
    }
    if (!started_) {
        started_ = true;
        start_ms_ = now_ms;
    }
    if (!ok_pressed) {
        result_ = BootHold::Released;
    } else if (now_ms - start_ms_ >= kBootResetHoldMs) {
        result_ = BootHold::Reset;
    }
    elapsed_ms_ = now_ms - start_ms_;
    return result_;
}

uint8_t BootResetHold::seconds_left() const {
    if (elapsed_ms_ >= kBootResetHoldMs) {
        return 0;
    }
    // Rounded up: the countdown shows 5, 4, 3, 2, 1, never 0 while still holding.
    return static_cast<uint8_t>((kBootResetHoldMs - elapsed_ms_ + 999) / 1000);
}

}  // namespace core::menu
