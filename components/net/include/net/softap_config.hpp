#pragma once

#include <cstdint>

namespace net {

inline constexpr char kSsidPrefix[] = "CoffeeProbe-";
// Fixed for the MVP per D-008. Per-unit passwords come with settings (#22).
inline constexpr char kPassword[] = "coffeeprobe";
inline constexpr uint8_t kChannel = 6;
inline constexpr uint8_t kMaxClients = 4;
inline constexpr uint16_t kBeaconIntervalTu = 100;
// ESP-IDF's world-safe mode allows channels 1–11, including our fixed channel 6.
inline constexpr char kCountryCode[] = "01";

// Keep the address used in the Artisan guide even if the IDF default changes.
inline constexpr char kApIp[] = "192.168.4.1";
inline constexpr char kApNetmask[] = "255.255.255.0";

}  // namespace net
