# Status

*The handoff between sessions. Agents update it at the end of every session; Tyler can read it any
time to see where things stand.*

**Last updated:** 2026-11-21

## In plain English

The foundations are built but not merged yet: the project's folder layout, pin settings for each
supported board, crash-safety settings, a hardware-check mode for the first wiring test, automatic
checks that run on every change, and the rules every AI agent follows. Nothing measures coffee yet.
Next: the temperature math, then reading the real thermocouple (milestone M2).

## Milestone

**M0 Toolchain & bring-up** and **M1 Foundation**, in progress.

## Changes waiting to be merged (in this order)

| PR | Ticket | What it does |
|---|---|---|
| #33 | #5 | Crash-safety settings: restart on a freeze, keep a record of crashes |
| #34 | #2 | Hardware-check mode: reports which screen and temperature chip are wired up |
| #35 | #4 | Folder layout, and pin settings per board (contains #33 and #34) |
| #36 | #7 | Automatic checks: logic tests on the Mac and on GitHub, plus local CI |
| #37 | (none) | The new way of working, and the rules and tools for AI agents |

The automatic checks were added in #36, so #33–#35 have none of their own. #36 contains all their
changes and passes every check, which covers them. Each PR builds on the one above it.

## Next for Claude

1. Get the changes above merged (see "Waiting on Tyler").
2. #6: the shared store for the latest readings, and the firmware's background tasks.
3. #8–#11: the temperature math and the code that decodes the two possible thermocouple chips, each
   tested against the chip makers' datasheets.
4. #2 follow-up: the newer ESP-IDF version (v6) already builds cleanly; confirm it as the version we
   use and make its checks required.

## Waiting on Tyler

- **Decision**: may Claude merge its own PRs once every check passes? Until then, merging is yours.
- **#3 (TYLER)**: wire the breadboard and identify the parts. Claude will rewrite it as plain steps
  once PR #34 is merged. Leaving the board plugged into the Mac lets Claude load the firmware itself.

## Known issues / notes

- Nothing has run on real hardware yet. Which screen, which temperature chip, and whether the probe
  tip is grounded stay unconfirmed until #3.
- The scripts that load firmware onto the board and read its output have only been tested without a
  board attached.
