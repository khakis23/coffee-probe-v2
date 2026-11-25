# Status

*The handoff between sessions. Agents update it at the end of every session; Tyler can read it any
time to see where things stand.*

**Last updated:** 2026-11-25 (Wednesday)

## In plain English

**Goal: a probe that Artisan can read over Wi-Fi by Saturday, November 28** (the MVP). The firmware
now has the whole Artisan path:
- it reads the thermocouple four times a second
- it filters out glitches and spots an unplugged probe within half a second
- it serves the temperature to Artisan over its own Wi-Fi network

The small screen now has its MVP version too, and a draft setup guide is written. Ahead of plan,
the after-MVP overheat protection, the settings menu (with factory reset), crash/power-problem
diagnostics and screen burn-in protection are written too, and the staff quick-start card is drafted (`artisan/QUICKSTART.md`).
**None of it has run on a wired board yet.** The wiring (#3) is still the thing that unblocks real testing. Tyler couldn't do it on
Tuesday as planned, so all hardware checks are queued for when he can.

## MVP plan (D-008)

| When | Claude | Tyler (about 2 hours in total) |
|---|---|---|
| Sat 21 – Sun 22 | Foundations ✓, temperature math + decoders (#8–#11) ✓, Modbus codec (#15) ✓ | — |
| Mon 23 | Wi-Fi (#14) ✓, task skeleton (#6) ✓ | — |
| Tue 24 | Sensor pipeline (#12) ✓, Modbus server (#16) ✓, draft Artisan setup guide ✓ | Wiring (#3) not done yet: hardware not available |
| Wed 25 | Minimal screen (#18–#20) ✓ (done Tue) | **Wire the board (#3), about 45 min**, as soon as possible |
| Thu 26 | Screen; fixes | (Thanksgiving: nothing needed) |
| Fri 27 | Fixes from the first hardware run | **First Artisan connection + 1-hour session + unplug / power-cycle / laptop-sleep checks (#17), about 1 h 15 min** |
| Sat 28 | Final fixes; tag `v0.1.0-mvp`; one-page quick-start | Read the quick-start |

Biggest risk: nothing has touched real hardware yet. The parts (which screen, which temperature
chip, grounded tip or not) stay unconfirmed until #3, and every hardware check is now packed into
the last three days.

## Milestone

**M0–M2 code merged; M3 (Wi-Fi + Modbus) code merged.** Hardware checks for #12, #14 and #16 are
pending (their issues stay open until they pass). Next: the M4 minimal screen for the MVP.

## Merged on 2026-11-25

| PR | Ticket | What it does |
|---|---|---|
| this | #2, #16, #26, #28 | Build only on the two supported IDF versions; v6 builds now required in CI. Fault-test builds for #26 (`scripts/flash.sh --fault hang/crash`), refused by the guards everywhere else. `scripts/release.sh` (one file per board), `docs/flashing.md`, `CHANGELOG.md`. Diagnostics shows Artisan connections |

## Merged on 2026-11-24

| PR | Ticket | What it does |
|---|---|---|
| #57 | #20 | Burn-in protection: the picture moves one dot every 3 min; an idle screen (no reading, no button, no warning) dims after 10 min. Diagnostics page 2 shows whether a crash report is saved, page 3 the probe's address |
| #55 | #25 | Diagnostics page 2 (why it last restarted, restarts, crashes, power dips, memory); "Check USB cable/power" and lower Wi-Fi power after repeated power dips, clearing after 30 calm minutes; restarts itself if its reading or network job stops. Hardware checks go with #26 |
| #52 | #23 | The settings menu: hold OK to open; probe chip, grounded tip, offset, Wi-Fi channel and power, brightness, flip, show password, diagnostics, factory reset. Holding OK at power-on for 5 s also resets. Buttons not wired yet: hardware test #53 (after the MVP) |
| #51 | #24 | Overheat protection: "BOX TOO HOT: MOVE IT" banner when warm; "TOO HOT" and a 1-minute cooling sleep when critical. Needs a heat-gun test on the wired board (issue open) |
| #50 | #22 | Settings that survive power cuts and can't break the probe: checked with a checksum, anything damaged falls back to defaults, one bad value doesn't wipe the others. Not used yet (the menu, #23) |
| #49 | #21 | Button presses: turns the buttons' noisy signal into clean "short press" and "long press" events (for the settings menu, after the MVP) |
| #48 | #18, #19, #20 | The screen: big °F while reading, `PROBE?` with the likely cause, a top line saying whether Artisan is connected or how to join the Wi-Fi (issues open for hardware checks) |
| #47 | (#17) | Draft `artisan/SETUP.md`: the staff guide for connecting Artisan, labels from Artisan's source, screenshots pending #17 |
| #45 | #12 | Reads the thermocouple 4×/s, filters glitches, reports an unplugged probe within 0.5 s, recovers only once three readings agree, marks old readings stale (issue open for hardware checks) |
| #46 | #16 | The Modbus server Artisan talks to: 4 connection slots, oldest dropped when full, idle connections closed after 10 s, −1 for a bad or old reading; `tools/modbus_poll.py` (issue open for hardware checks) |

## Merged on 2026-11-23

| PR | Ticket | What it does |
|---|---|---|
| #43 | #14 | The probe's own Wi-Fi network `CoffeeProbe-XXXX` (issue stays open for its hardware checks) |
| #44 | #6 | Shared state, the four jobs (sensor, network, screen, health) with crash/freeze restart, version in the log |

Earlier merges (#33–#42, 2026-11-21) are in the git log.

## Next for Claude

The MVP code is complete. Until the board is wired, post-MVP work that needs no hardware (item 3).

1. When the board is plugged in, one batch of hardware checks (`device` skill, then `tyler-test`):
   - flash the diagnostics firmware and finish #3's analysis (which chip, which screen)
   - #14: SSID visible, the Mac joins, no captive-portal sheet
   - #12: the 1 Hz `BT ... F, 4 samples/s` log line, unplug → fault within 1 s → replug recovers
   - #16: `tools/modbus_poll.py` reads live °F, kill a client mid-connection, `--clients 5` eviction
   - #18–#20: which panel it is (set the Kconfig controller if SH1106), the screens look like the host
     previews, text and digits readable from ~2 m, SDA pulled to GND for 1 s → the screen recovers;
     if the panel module has no pull-ups, 400 kHz may fail (drop `kI2cHz` to 100 kHz)
   - **Flash with `--idf v6.0.3` if v6 works on the board** (the project's target, #2): IDF v5.3.1's I2C
     bus clear on the C3 can wait forever if the clock line jams mid-transfer, and the watchdog would then
     restart the probe. The display checks the line before every transfer, which covers most cases
     but not a jam in the middle of a transfer. v6 bounds that wait.
   - stack high-water marks ≥ 25 % free (the UI task now also runs the menu)
   - #20: dimmed contrast readable up close; the one-pixel shift not noticeable
   - set the factory-default chip with Kconfig if it's a MAX6675 (staff can also change it in the menu)
   - #24: heat gun on the box until the banner, then TOO HOT; the panel keeps showing it during the
     sleep; the probe wakes and resumes once cool
2. During #17 (Tyler's first Artisan session): check `artisan/SETUP.md` against the real screens, add
   screenshots, and confirm which MODBUS input Artisan shows as BT (unconfirmed; the guide has a fallback).
3. All code that can be written without the board is merged. What's left is hardware: #26 fault
   injection (fault-test builds ready: `scripts/flash.sh --fault hang|crash`), #27 soak and heat (then
   tune the thermal thresholds), then #28 (release: `scripts/release.sh`, attach the files and
   `CHANGELOG.md` to the GitHub release; check `docs/flashing.md` and the web flasher steps the first time).
   After #3: delete `components/bringup` once its analysis is done (firmware rule F2), and pick the
   shipping IDF version (#2).
5. For the MVP tag on Saturday: `main` now also carries the post-MVP code (menu, overheat, diagnostics).
   With no buttons wired the menu can't open; the BOOT button acts as BACK, which does nothing outside
   the menu. If the first hardware run misbehaves, these are the newest suspects.
4. Not done in #24: waking from the cooling sleep with a button (the buttons weren't in the firmware
   then). Add it once #53 shows the buttons work.

## Scope (confirmed with Tyler, D-007)

A generic ESP32-C3 DevKit is the main board. Tyler and his staff will use a few units. Bean
temperature only. The enclosure is Tyler's; this project delivers firmware, guides and a wiring guide.

## Waiting on Tyler

- **#3 (TYLER), ready now**: wire the DevKit, thermocouple chip and screen (plain steps in the issue, about
  45 min), then leave it plugged into the Mac. Everything hardware-related depends on it.
- **#53 (TYLER), after the MVP, not urgent**: wire three buttons and walk through the settings menu and
  factory reset (about 25 min).

## Known issues / notes

- Nothing has run on real hardware yet. Which screen, which temperature chip, and whether the probe
  tip is grounded stay unconfirmed until #3. The firmware defaults to the MAX31855, not grounded.
- The thermocouple chip is now chosen at runtime (settings menu, D-012), so CI compiles both chip
  paths in every build. The Kconfig chip option only sets the factory default.
- Codex hit its 5-hour cap on 2026-11-24; Gemini did the reviews for #24 and #23 instead. Long Gemini
  review prompts time out after 10 min: split reviews into small focused prompts with a word cap.
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
