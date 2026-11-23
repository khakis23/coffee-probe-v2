# Status

*The handoff between sessions. Agents update it at the end of every session; Tyler can read it any
time to see where things stand.*

**Last updated:** 2026-11-23 (Monday)

## In plain English

**Goal: a probe that Artisan can read over Wi-Fi by Saturday, November 28** (the MVP). Since Saturday the
probe has gained its own Wi-Fi network and the "skeleton" that runs its jobs side by side (read the probe,
talk to Artisan, drive the screen) and restarts it if any of them freezes. Code to read the real
thermocouple is being written now. None of it has run on a wired board yet: the wiring (#3) is the next
thing that unblocks testing, and Tyler plans to do it **Tuesday 24**.

## MVP plan (D-008)

| When | Claude | Tyler (about 2 hours in total) |
|---|---|---|
| Sat 21 – Sun 22 | Foundations ✓, temperature math + decoders (#8–#11) ✓, Modbus codec (#15) ✓ | **Wire the DevKit, thermocouple chip and screen (#3), about 45 min; leave it plugged into the Mac** |
| Mon 23 | Wi-Fi (#14) ✓ merged; task skeleton (#6) ✓ merged; #12 started | Wiring moved to Tue 24 |
| Tue 24 | Finish #12; Modbus server (#16); hardware check once wired (#3) | **Wire the board (#3), about 45 min** |
| Wed 25 | Artisan setup guide; fixes | **First Artisan connection, about 15 min** |
| Thu 26 | Minimal screen (#18–#20 MVP parts) | (Thanksgiving: nothing needed) |
| Fri 27 | Fixes from Wednesday | **1-hour Artisan session + unplug / power-cycle / laptop-sleep checks (#17), about 1 h** |
| Sat 28 | Final fixes; tag `v0.1.0-mvp`; one-page quick-start | Read the quick-start |

Biggest risk: the parts are still unidentified until #3 is wired. Everything after Monday depends on it.

## Milestone

**M0–M1** foundations merged; now M1's remaining tickets, then M2 and M3 (the Artisan path) for the MVP.

## Merged on 2026-11-23

| PR | Ticket | What it does |
|---|---|---|
| #43 | #14 | The probe's own Wi-Fi network `CoffeeProbe-XXXX` (issue stays open for its hardware checks) |
| this | #6 | Shared state, the four jobs (sensor, network, screen, health) with crash/freeze restart, version in the log |

## Merged on 2026-11-21

| PR | Ticket | What it does |
|---|---|---|
| #33 | #5 | Crash-safety settings: restart on a freeze, keep a record of crashes |
| #34 | #2 | Hardware-check mode: reports which screen and temperature chip are wired up |
| #35 | #4 | Folder layout, and pin settings per board |
| #36 | #7 | Automatic checks: logic tests and firmware builds (local CI on the Mac) |
| #38 | (none) | The new way of working, and the rules and tools for AI agents |
| #39 | (none) | Fixes from the second-model review, the generic DevKit as default, the MVP scope |
| #40 | #8–#11 | Temperature math (°C/°F) and decoders for both thermocouple chips |
| #41 | #15 | Modbus: how the probe answers Artisan, and which number lives where |
| next | (none) | GitHub CI turned off; local CI on the Mac is the only check (D-009) |

## Next for Claude

1. **#12, in progress on the local branch `claude/12-sensor`** (worktree `.scratch/wt/12`). Codex (code tier)
   implemented it from `.scratch/briefs/plan-12.md`; saved as the local WIP commit `b70bb47` (not pushed;
   Codex reports guards + 35 host tests + six firmware builds pass). It is **unreviewed**: read every
   non-test line, recompute test vectors from the MAX31855 datasheet, run full CI, Gemini second review
   (thermocouple fault handling area). The branch was cut from #6 before #6 was rebased: rebase it onto
   `main` with `git rebase --onto origin/main cde4ae3`.
2. #16 Modbus server: plan drafted in `.scratch/briefs/plan-16.md` (register publish + connection-slot policy in core, select()-based server in
   net, the net task paces itself with select instead of vTaskDelayUntil). Add `tools/modbus_poll.py` (Gemini).
3. When the board is plugged in: flash the diagnostics firmware and finish #3's analysis, then the #14 checks
   (SSID visible, Mac joins, no captive-portal sheet).
4. Artisan setup guide (Wed), minimal screen #18–#20 (Thu).

## Scope (confirmed with Tyler, D-007)

A generic ESP32-C3 DevKit is the main board. Tyler and his staff will use a few units. Bean
temperature only. The enclosure is Tyler's; this project delivers firmware, guides and a wiring guide.

## Waiting on Tyler

- **#3 (TYLER), ready now**: wire the DevKit, thermocouple chip and screen (plain steps in the issue, about
  45 min), then leave it plugged into the Mac. Planned for Tuesday 24; everything after that depends on it.

## Known issues / notes

- Nothing has run on real hardware yet. Which screen, which temperature chip, and whether the probe
  tip is grounded stay unconfirmed until #3.
- The scripts that load firmware onto the board and read its output have only been tested without a
  board attached.
- IDF's DHCP server always tells clients to use the probe for DNS; nothing answers, so lookups fail fast and
  no captive-portal sheet should appear. Unverified on a Mac until the board is wired (#14 comment, PR #43).
- Task heartbeats log at DEBUG; at the default INFO level the 1 s `alive` line from app_main shows liveness,
  and stack high-water marks are logged at boot and at 60 s. Check them for ≥ 25 % headroom on the first flash.
- `.scratch/tc-monitor.txt` (Sun 22) is a diagnostics log from a board with nothing answering on the
  thermocouple bus (frames all 0s or all 1s). Expected before #3's wiring; re-check after.
