# Status

*The handoff between sessions. Agents update it at the end of every session; Tyler can read it any
time to see where things stand.*

**Last updated:** 2026-11-21

## In plain English

The foundations are built but not merged yet: project layout, board pin profiles, crash-safety
settings, a hardware-diagnostics mode, automatic checks, and the rules every AI agent follows.
Nothing measures coffee yet. Next comes the core temperature logic, then reading the real
thermocouple (milestone M2).

## Milestone

**M0 Toolchain & bring-up** and **M1 Foundation**, in progress.

## Open PRs (merge in this order)

CI only exists from #36 on, so #33–#35 have no checks of their own. #36 contains all of them, and
its CI (and a full local CI run) is green.

| PR | Issue | What | Base |
|---|---|---|---|
| #33 | #5 | Reliability sdkconfig + partition table with coredump | `main` |
| #34 | #2 | Bring-up diagnostics firmware | `main` |
| #35 | #4 | Component layout, board profiles, `board.hpp` (contains #33, #34) | `main` |
| #36 | #7 | Host tests, GitHub CI, `scripts/ci-local.sh` | #35's branch |
| #37 | (none) | Claude as sole developer; `AGENTS.md`, `.agents/` rules and skills, guards | #36's branch |

## Next for Claude

1. Merge the stack above (needs Tyler's OK on self-merging).
2. #6 StateStore, task skeleton, firmware version from git.
3. #8 temperature helpers, #9 decode interface, #10/#11 MAX31855/MAX6675 decoders (move the
   decoders out of `components/bringup` into `core` with datasheet test vectors).
4. #2 follow-up: v6.0.3 builds already pass locally; decide the IDF version and make v6 CI blocking.

## Waiting on Tyler

- **Decision**: OK for Claude to merge its own PRs once CI is green (PR #37 describes it)?
- **#3 (TYLER)**: wire the breadboard and identify the parts. Claude will rewrite it as plain steps
  once #34 is merged. Leaving the board plugged into the Mac lets Claude do the flashing.

## Known issues / notes

- Nothing has run on real hardware yet. The parts (OLED controller, MAX31855 vs MAX6675, probe
  grounding) are unconfirmed until #3.
- `scripts/flash.sh` and `scripts/serial_capture.py` are tested without a board only.
