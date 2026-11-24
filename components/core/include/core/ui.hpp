#pragma once

#include <cstdint>
#include <string_view>

#include "core/framebuffer.hpp"
#include "core/probe_state.hpp"

namespace core::ui {

// Everything a screen needs, gathered by the UI task. Pure data, so screens are host-testable.
struct Inputs {
    ProbeState state;
    uint32_t now_ms{0};         // Milliseconds since boot, same clock as ProbeState's times.
    std::string_view ssid{};    // The probe's Wi-Fi name, e.g. "CoffeeProbe-3FA2".
    std::string_view password{};
};

enum class Screen : uint8_t {
    Starting,  // First moments after boot, before the first reading could exist.
    Live,      // A valid reading: big degrees F.
    Probe,     // No valid reading after start-up: "PROBE?" and the likely cause.
};

// What the one-line top bar says.
enum class Link : uint8_t {
    Artisan,      // Artisan is polling.
    WaitArtisan,  // A device has joined the Wi-Fi but Artisan isn't polling.
    JoinWifi,     // Nothing has joined: show the Wi-Fi name and password, alternating.
};

inline constexpr uint32_t kStartupMs = 3000;          // "Starting" at most this long after boot.
inline constexpr uint32_t kArtisanRecentMs = 5000;    // Artisan polls every 2 s by default.
inline constexpr uint32_t kJoinAlternateMs = 3000;    // Wi-Fi name / password swap period.

Screen choose_screen(const Inputs& in);
Link link_status(const Inputs& in);

// Renders the whole frame for the current inputs. Pure: same inputs, same pixels.
void render(const Inputs& in, Framebuffer& fb);

}  // namespace core::ui
