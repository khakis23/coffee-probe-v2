#pragma once

#include <cstdint>
#include <string_view>

#include "core/button.hpp"
#include "core/settings.hpp"

// The settings menu (#23): a pure state machine driven by button events, so every path is
// host-tested. The UI task feeds it presses and the clock, draws it (core::ui), and carries out
// the Action it returns (save to NVS, restart, factory reset).
namespace core::menu {

enum class Button : uint8_t { Up, Down, Ok, Back };

struct Press {
    Button button;
    ButtonEvent event;
};

// In display order.
enum class Item : uint8_t {
    TcChip,
    Grounded,
    Offset,
    Channel,
    TxPower,
    Brightness,
    Flip,
    ShowPassword,
    Diagnostics,
    FactoryReset,
};
inline constexpr uint8_t kItemCount = 10;

enum class Mode : uint8_t {
    Closed,         // Normal screens. Holding OK opens the menu.
    List,           // The item list with a cursor.
    Edit,           // One setting, full screen: UP/DOWN change it, OK saves, BACK cancels.
    Diagnostics,    // Read-only, three pages: UP/DOWN switch pages, OK or BACK go back.
    ConfirmReset,   // "Reset all settings?" Holding OK confirms; UP, DOWN or BACK go back.
    RestartPrompt,  // A saved setting needs a restart. Holding OK restarts, BACK means later.
};

enum class Action : uint8_t {
    None,
    Save,          // Persist settings() and apply what can change while running.
    Restart,       // Restart the probe (settings() is already saved).
    FactoryReset,  // Erase the saved settings and restart.
};

// The menu closes by itself after this long without a press; an unsaved edit is dropped.
inline constexpr uint32_t kIdleCloseMs = 30'000;
// Offset steps: a short press moves 0.1 °F, a long press 1.0 °F.
inline constexpr int16_t kOffsetStepShort = 1;
inline constexpr int16_t kOffsetStepLong = 10;
inline constexpr int16_t kOffsetLimit = 100;  // ±10.0 °F, the range settings accept.
// Wi-Fi power choices in quarter-dBm; 0 lets the board choose. The net driver still caps the
// value at the board's limit, so "High" on a board with a weak regulator stays safe.
inline constexpr uint8_t kTxPowerChoices[] = {0, 32, 52, 78};  // Auto, 8, 13, 19.5 dBm.
inline constexpr uint8_t kChannelChoices[] = {1, 6, 11};

// True for settings that only take effect after a restart (they configure Wi-Fi or the
// thermocouple driver at boot).
bool needs_restart(Item item);
std::string_view label(Item item);
// The value as shown in the list and the editor, e.g. "MAX31855", "+1.5 F", "Auto".
// Writes into buf when it needs formatting; the result is valid while buf lives.
std::string_view value_text(Item item, const Settings& s, char (&buf)[12]);

class Menu {
   public:
    explicit Menu(const Settings& saved) : saved_(saved), draft_(saved) {}

    Action handle(Press press, uint32_t now_ms);
    // Call regularly (every UI frame). Closes the menu after kIdleCloseMs without a press.
    void tick(uint32_t now_ms);
    // Closes without saving (e.g. the probe is about to sleep because it is too hot).
    void close();

    Mode mode() const { return mode_; }
    Item cursor() const { return cursor_; }
    // 0..2 while in Diagnostics (0 = for staff, 1 = restarts and power, 2 = address and memory).
    uint8_t diagnostics_page() const { return diagnostics_page_; }
    // What is saved: after Action::Save this is what to persist and apply.
    const Settings& settings() const { return saved_; }
    // The value being edited (equal to settings() outside Edit).
    const Settings& draft() const { return draft_; }

   private:
    Action handle_list(Press press);
    Action handle_edit(Press press);
    void step_draft(int direction, bool big);

    Settings saved_;
    Settings draft_;
    Mode mode_{Mode::Closed};
    Item cursor_{Item::TcChip};
    uint32_t last_press_ms_{0};
    uint8_t diagnostics_page_{0};
};
inline constexpr uint8_t kDiagnosticsPages = 3;

// Factory reset at power-on: OK held from boot for this long resets the settings. A way back
// from settings that make the probe hard to use (a flipped screen, the wrong chip).
inline constexpr uint32_t kBootResetHoldMs = 5000;
// OK must read released this long to cancel, so a contact glitch during the hold doesn't.
inline constexpr uint32_t kBootReleaseMs = 60;

enum class BootHold : uint8_t {
    Holding,   // OK is still down; keep showing the countdown.
    Released,  // OK was let go (or never pressed): boot normally.
    Reset,     // Held the whole time: erase the settings.
};

class BootResetHold {
   public:
    // Call every poll (about every 20 ms) from the first moment of boot with the raw OK level.
    // OK up at the first call releases at once; later, OK must stay up kBootReleaseMs to cancel.
    // Once the result is Released or Reset it stays there.
    BootHold update(bool ok_pressed, uint32_t now_ms);
    // Whole seconds left for the on-screen countdown: 5, 4, 3, 2, 1.
    uint8_t seconds_left() const;

   private:
    BootHold result_{BootHold::Holding};
    bool started_{false};
    uint32_t start_ms_{0};
    uint32_t elapsed_ms_{0};
    bool releasing_{false};
    uint32_t release_ms_{0};
};

}  // namespace core::menu
