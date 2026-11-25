#pragma once

#include <cstdint>
#include <string_view>

// Restart and power-dip bookkeeping (#25). Pure: the health component maps the chip's reset
// reason to a ResetCause and keeps the record in RTC memory, which survives restarts, crashes
// and watchdog resets but not a power-off.
namespace core {

enum class ResetCause : uint8_t {
    PowerOn,    // Plugged in.
    Software,   // esp_restart(): a settings restart, a recovery reboot.
    Crash,      // Panic (incl. a hang the health task turned into a panic).
    Watchdog,   // Task or interrupt watchdog.
    Brownout,   // The supply dipped below the brownout level.
    DeepSleep,  // Woke from the overheat cooling sleep.
    Other,
};

struct BootRecord {
    uint32_t magic{0};
    uint16_t boots{0};      // Boots since power-on, including this one.
    uint16_t brownouts{0};  // Power dips since power-on (or since the warning last cleared).
    uint16_t crashes{0};    // Crashes and watchdog resets since power-on.
    uint16_t check{0};      // Detects RTC memory that holds leftovers, not a record.
};

inline constexpr uint32_t kBootRecordMagic = 0xC0FFEE25;
// Power dips before the probe warns and lowers its Wi-Fi power: one can be a fluke at plug-in,
// two mean the cable or supply can't carry the Wi-Fi bursts (~350 mA).
inline constexpr uint16_t kBrownoutsForWarning = 2;
// The warning clears after this long without a dip (R2: recover once the cause is gone). A dip
// resets the chip, so uptime without a reset is time without a dip.
inline constexpr uint32_t kPowerCalmS = 30 * 60;
// Wi-Fi power while the warning is on (8 dBm, the menu's "Low"): shorter current peaks, still
// plenty for a laptop in the same room.
inline constexpr int8_t kPowerWarningTxQdbm = 32;

uint16_t boot_record_check(const BootRecord& r);
bool boot_record_valid(const BootRecord& r);
// The record for this boot, from what RTC memory held and why the chip reset. Power-on, or
// memory that isn't a valid record, starts a fresh count.
BootRecord next_boot_record(const BootRecord& stored, ResetCause cause);
// The record with the power-dip count cleared (the warning ended), still valid.
BootRecord cleared_brownouts(const BootRecord& r);

// Whether to warn about the power supply (and lower Wi-Fi power) at this uptime.
// If RTC memory doesn't survive a brownout on this chip, every dip looks like a fresh record
// with one brownout: stored_was_valid=false with a Brownout cause then warns at once, so the
// warning works either way (#26 checks which on hardware).
bool power_warning(const BootRecord& r, bool stored_was_valid, ResetCause cause,
                   uint32_t uptime_s);

// Short text for the Diagnostics page, at most 9 characters.
std::string_view reset_cause_text(ResetCause cause);

// Turns a counter that a task increments every round into "has it stopped?". The task watchdog
// catches a task stuck inside a step; this catches one that stopped running its steps at all.
inline constexpr uint32_t kStallMs = 10'000;

class StallDetector {
   public:
    // True once the counter hasn't changed for kStallMs. Call about once a second.
    bool update(uint32_t counter, uint32_t now_ms);

   private:
    bool started_{false};
    uint32_t last_counter_{0};
    uint32_t last_change_ms_{0};
};

}  // namespace core
