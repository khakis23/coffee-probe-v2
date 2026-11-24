# Status

*The handoff between sessions. Agents update it at the end of every session; Tyler can read it any
time to see where things stand.*

**Last updated:** 2026-11-24 (Tuesday)

## In plain English

**Goal: a probe that Artisan can read over Wi-Fi by Saturday, November 28** (the MVP). The firmware
now has the whole Artisan path:
- it reads the thermocouple four times a second
- it filters out glitches and spots an unplugged probe within half a second
- it serves the temperature to Artisan over its own Wi-Fi network

What's left for the MVP is the small screen and the setup guide. **None of it has run on a wired
board yet.** The wiring (#3) is still the thing that unblocks real testing. Tyler couldn't do it on
Tuesday as planned, so all hardware checks are queued for when he can.

## MVP plan (D-008)

| When | Claude | Tyler (about 2 hours in total) |
|---|---|---|
| Sat 21 – Sun 22 | Foundations ✓, temperature math + decoders (#8–#11) ✓, Modbus codec (#15) ✓ | — |
| Mon 23 | Wi-Fi (#14) ✓, task skeleton (#6) ✓ | — |
| Tue 24 | Sensor pipeline (#12) ✓, Modbus server (#16) ✓ | Wiring (#3) not done yet: hardware not available |
| Wed 25 | Artisan setup guide; minimal screen (#18–#20) | **Wire the board (#3), about 45 min**, as soon as possible |
| Thu 26 | Screen; fixes | (Thanksgiving: nothing needed) |
| Fri 27 | Fixes from the first hardware run | **First Artisan connection + 1-hour session + unplug / power-cycle / laptop-sleep checks (#17), about 1 h 15 min** |
| Sat 28 | Final fixes; tag `v0.1.0-mvp`; one-page quick-start | Read the quick-start |

Biggest risk: nothing has touched real hardware yet. The parts (which screen, which temperature
chip, grounded tip or not) stay unconfirmed until #3, and every hardware check is now packed into
the last three days.

## Milestone

**M0–M2 code merged; M3 (Wi-Fi + Modbus) code merged.** Hardware checks for #12, #14 and #16 are
pending (their issues stay open until they pass). Next: the M4 minimal screen for the MVP.

## Merged on 2026-11-24

| PR | Ticket | What it does |
|---|---|---|
| #45 | #12 | Reads the thermocouple 4×/s, filters glitches, reports an unplugged probe within 0.5 s, recovers only once three readings agree, marks old readings stale (issue open for hardware checks) |
| this | #16 | The Modbus server Artisan talks to: 4 connection slots, oldest dropped when full, idle connections closed after 10 s, −1 for a bad or old reading; `tools/modbus_poll.py` (issue open for hardware checks) |

## Merged on 2026-11-23

| PR | Ticket | What it does |
|---|---|---|
| #43 | #14 | The probe's own Wi-Fi network `CoffeeProbe-XXXX` (issue stays open for its hardware checks) |
| #44 | #6 | Shared state, the four jobs (sensor, network, screen, health) with crash/freeze restart, version in the log |

Earlier merges (#33–#42, 2026-11-21) are in the git log.

## Next for Claude

1. **Artisan setup guide** (staff-proof, numbered steps; `artisan/`): Modbus TCP to 192.168.4.1:502,
   input registers, BT from register 0 with ÷10, sampling interval. Gemini drafts the prose; screenshots
   wait for the first hardware session.
2. **Minimal screen #18–#20 (MVP parts):** OLED driver + framebuffer, big °F on the Live screen,
   `PROBE?` on a fault, Wi-Fi name/password and connection status otherwise.
3. When the board is plugged in, one batch of hardware checks (`device` skill, then `tyler-test`):
   - flash the diagnostics firmware and finish #3's analysis (which chip, which screen)
   - set the Kconfig chip choice if it's a MAX6675
   - #14: SSID visible, the Mac joins, no captive-portal sheet
   - #12: the 1 Hz `BT ... F, 4 samples/s` log line, unplug → fault within 1 s → replug recovers
   - #16: `tools/modbus_poll.py` reads live °F, kill a client mid-connection, `--clients 5` eviction
   - stack high-water marks ≥ 25 % free

## Scope (confirmed with Tyler, D-007)

A generic ESP32-C3 DevKit is the main board. Tyler and his staff will use a few units. Bean
temperature only. The enclosure is Tyler's; this project delivers firmware, guides and a wiring guide.

## Waiting on Tyler

- **#3 (TYLER), ready now**: wire the DevKit, thermocouple chip and screen (plain steps in the issue, about
  45 min), then leave it plugged into the Mac. Everything hardware-related depends on it.

## Known issues / notes

- Nothing has run on real hardware yet. Which screen, which temperature chip, and whether the probe
  tip is grounded stay unconfirmed until #3. The firmware defaults to the MAX31855, not grounded.
- CI builds only the default MAX31855 configuration. The MAX6675 path was built once by hand on
  2026-11-24 and compiled cleanly.
- The scripts that load firmware onto the board and read its output have only been tested without a
  board attached.
- IDF's DHCP server always tells clients to use the probe for DNS; nothing answers, so lookups fail fast and
  no captive-portal sheet should appear. Unverified on a Mac until the board is wired (#14 comment, PR #43).
- Task heartbeats log at DEBUG; at the default INFO level the 1 s `alive` line from app_main and the
  sensor task's 1 s `BT` line show liveness. Stack high-water marks are logged at boot and at 60 s.
- `tools/modbus_poll.py` needs `pip install pymodbus` (not installed on the Mac yet); it has only been
  syntax-checked.
- `.scratch/tc-monitor.txt` (Sun 22) is a diagnostics log from a board with nothing answering on the
  thermocouple bus (frames all 0s or all 1s). Expected before #3's wiring; re-check after.
