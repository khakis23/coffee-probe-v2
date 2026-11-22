# Status

*The handoff between sessions. Agents update it at the end of every session; Tyler can read it any
time to see where things stand.*

**Last updated:** 2026-11-21 (evening)

## In plain English

**Goal: a probe that Artisan can read over Wi-Fi by Saturday, November 28** (the MVP). The foundations are
built and merged, and so are the temperature math, the chip decoders and the Modbus "language" Artisan speaks.
Nothing reads a real probe yet: that needs the wiring. This week: read the thermocouple, start the
probe's own Wi-Fi network, serve the temperature to Artisan, and show it on the little screen. Buttons,
settings and overheat protection come after the MVP.

## MVP plan (D-008)

| When | Claude | Tyler (about 2 hours in total) |
|---|---|---|
| Sat 21 – Sun 22 | Foundations ✓, temperature math + decoders (#8–#11) ✓, Modbus codec (#15) ✓; Wi-Fi (#14) built, review Sun; #6 task skeleton | **Wire the DevKit, thermocouple chip and screen (#3), about 45 min; leave it plugged into the Mac** |
| Sun 22 – Mon 23 | Task skeleton (#6); hardware check on the wired board (#3) | |
| Mon 23 – Tue 24 | Read the real thermocouple (#12); Modbus server (#16) | |
| Wed 25 | Artisan setup guide; fixes | **First Artisan connection, about 15 min** |
| Thu 26 | Minimal screen (#18–#20 MVP parts) | (Thanksgiving: nothing needed) |
| Fri 27 | Fixes from Wednesday | **1-hour Artisan session + unplug / power-cycle / laptop-sleep checks (#17), about 1 h** |
| Sat 28 | Final fixes; tag `v0.1.0-mvp`; one-page quick-start | Read the quick-start |

Biggest risk: the parts are still unidentified until #3 is wired. Everything after Monday depends on it.

## Milestone

**M0–M1** foundations merged; now M1's remaining tickets, then M2 and M3 (the Artisan path) for the MVP.

## Merged on 2026-11-21

| PR | Ticket | What it does |
|---|---|---|
| #33 | #5 | Crash-safety settings: restart on a freeze, keep a record of crashes |
| #34 | #2 | Hardware-check mode: reports which screen and temperature chip are wired up |
| #35 | #4 | Folder layout, and pin settings per board |
| #36 | #7 | Automatic checks: logic tests on the Mac and on GitHub, plus local CI |
| #38 | (none) | The new way of working, and the rules and tools for AI agents |
| #39 | (none) | Fixes from the second-model review, the generic DevKit as default, the MVP scope |
| #40 | #8–#11 | Temperature math (°C/°F) and decoders for both thermocouple chips |
| #41 | #15 | Modbus: how the probe answers Artisan, and which number lives where |

## Next for Claude

1. Review and merge the Wi-Fi network code (#14, built by Codex, on the local branch `claude/14-softap`).
2. #6 task skeleton and shared state (plan written).
3. When the board is plugged in: flash the hardware-check firmware and finish #3's analysis.
4. Then #12 (read the real thermocouple) and #16 (Modbus server), ahead of the plan.

## Scope (confirmed with Tyler, D-007)

A generic ESP32-C3 DevKit is the main board. Tyler and his staff will use a few units. Bean
temperature only. The enclosure is Tyler's; this project delivers firmware, guides and a wiring guide.

## Waiting on Tyler

- **#3 (TYLER), ready now**: wire the DevKit, thermocouple chip and screen (plain steps in the issue, about
  45 min), then leave it plugged into the Mac. Needed by Sunday or Monday to keep the MVP on track.

## Known issues / notes

- Nothing has run on real hardware yet. Which screen, which temperature chip, and whether the probe
  tip is grounded stay unconfirmed until #3.
- The scripts that load firmware onto the board and read its output have only been tested without a
  board attached.
