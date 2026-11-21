# CoffeeProbe — operating manual for agents

You are building firmware for a wireless thermocouple probe that a coffee roaster uses during real
roasts. An ESP32-C3 reads a K-type thermocouple and serves bean temperature to stock
[Artisan](https://artisan-scope.org/) over its own Wi-Fi network (Modbus TCP). The user is
non-technical and the room is hot. A hang, a frozen reading or a confusing screen ruins a roast.
**Reliability beats features, cleverness and speed of delivery.**

There is one client, **Tyler**. He owns the product but does not write or review code. Agents do all
development; Tyler makes product decisions and does the physical tasks.

This file is the single source of truth for how to work here, for every model and tool (Claude Code,
Codex, Gemini, anything else). Read it fully at the start of every session.

## 1. Start of every session

1. Read [`docs/STATUS.md`](docs/STATUS.md): where the project is, what's next, what's waiting on Tyler.
2. Read the issue you are about to work on (`gh issue view N`) and check its **Depends on** issues are closed.
3. Read the rules files below that apply to the task. Skim the parts of
   [`docs/architecture.md`](docs/architecture.md) the issue references.

You start with no memory of earlier sessions. Everything you need is in the repo. If something you
needed was missing, add it to the right file before you finish so the next agent has it.

## 2. Rules files (read before you work in that area)

| File | Covers | Read when |
|---|---|---|
| [`.agents/rules/workflow.md`](.agents/rules/workflow.md) | Work loop, definition of done, review, delegation, merging, reporting to Tyler | Every session |
| [`.agents/rules/firmware.md`](.agents/rules/firmware.md) | C++ and ESP-IDF code rules, components, conventions | Writing or reviewing any code |
| [`.agents/rules/reliability.md`](.agents/rules/reliability.md) | The reliability contract: invariants the probe must keep | Touching sensor, net, storage, health, tasks or sdkconfig |
| [`.agents/rules/testing.md`](.agents/rules/testing.md) | Host tests, local CI, guards, hardware verification | Writing code or tests |

Runbooks (skills) live in `.agents/skills/` and are linked into `.claude/skills/`:
`local-ci` (running and reading CI), `device` (flash a board and read its serial log),
`tyler-test` (handing a hardware test to Tyler).

## 3. Hard rules

These protect the roast, the hardware and Tyler's trust. `scripts/guards.sh` checks rules 3–6
mechanically. If a guard fires, the code is wrong: never edit a guard or add an exception to pass.

1. **Never push to `main`.** Every change lands through a PR from a `claude/<issue#>-slug` branch.
2. **Never merge red.** `scripts/ci-local.sh` and GitHub CI must both pass. Never delete, skip or
   loosen a failing test to get green.
3. **`components/core` is pure standard C++.** No IDF, FreeRTOS or driver headers. All logic lives
   there and is host-tested.
4. **No heap allocation in our code** (no `new`, `malloc`, allocating `std::` containers). Static
   buffers and `xTaskCreateStatic`. A rare init-time exception carries `// guard-ok: <reason>`.
5. **GPIO numbers appear only in `components/board/include/board/board.hpp`.**
6. **The reliability settings in `sdkconfig.defaults` stay on** (watchdogs, panic → reboot, brownout,
   coredump). Bring-up diagnostics are never enabled in committed defaults.
7. **The probe never stops by itself.** No `abort()` on bad input, no infinite wait on hardware, no
   halting on error. Faults are reported (−1 to Artisan, a message on screen) and recovered from.
8. **Stay inside the plan.** No features, settings, libraries or abstractions the current issue does
   not need. Adding a dependency or changing a key decision needs a `docs/DECISIONS.md` entry.
9. **No secrets or personal data in git**: no Wi-Fi passwords of Tyler's networks, no tokens, nothing
   about his home or business beyond what's already public in the repo.

## 4. Where things are

| Path | What it is | Who changes it |
|---|---|---|
| `docs/STATUS.md` | Current state, next steps, waiting on Tyler. The handoff between sessions | Every session, at the end |
| `docs/architecture.md` | What we build and how: decisions, tasks, register map, reliability design, pins, milestones | With a DECISIONS entry for real changes |
| `docs/DECISIONS.md` | Append-only log of decisions and why | Append only |
| `docs/dev-setup.md` | Toolchain, commands, CLion, crash dumps | When a procedure changes |
| `ideas.md` | Tyler's original requirements | Only with Tyler |
| `CONTRIBUTING.md` | The human-facing summary of how the project runs | Rarely |
| `components/` | `core` (pure logic), `board` (pins), `thermocouple`, `display`, `buttons`, `net`, `storage`, `health`, `bringup` (temporary diagnostics) | Code |
| `main/app_main.cpp` | Wiring only: init order and task creation | Code |
| `test/host/` | doctest host tests for `core` | Code |
| `scripts/` | `ci-local.sh`, `guards.sh`, `flash.sh`, `serial_capture.py` | Rarely; guards need review |
| `.github/workflows/ci.yml` | GitHub CI, mirrored by `ci-local.sh` | Keep both in sync |
| GitHub issues | The plan, one ticket per task, milestones M0–M7, labels `CLAUDE` / `TYLER` | Agents and Tyler |

## 5. Commands

```bash
scripts/ci-local.sh                 # THE gate: guards + host tests + every firmware build (~1.5 min)
scripts/ci-local.sh guards host     # fast loop while iterating on core logic
scripts/guards.sh                   # rule checks only (instant)
scripts/flash.sh [--board generic] [--diag]          # build + flash a plugged-in board
~/.espressif/python_env/idf5.3_py3.14_env/bin/python scripts/serial_capture.py --reset --seconds 15
. ~/esp/esp-idf/export.sh && idf.py build            # by hand (v5.3.1); v6: ~/esp/esp-idf-v6.0
```

- Build output from scripts goes to `~/Library/Caches/coffee-probe/`, not the repo (see traps).
- Scratch files go in `.scratch/` (git-ignored). Don't leave stray files in the repo root.
- Long commands (full CI, builds) run in the background.

## 6. Honesty

Tyler and the next agent act on what you write.

- Say exactly what was verified and how. "Builds" is not "works on the board". If you didn't run it on
  hardware, say so, and make sure a `TYLER` ticket or the PR says how it will be tested.
- Never invent a measurement, test result or approval from Tyler. Label estimates as estimates.
- If you break something or find an earlier mistake, say so in the PR or issue and fix or flag it.
- Stuck after three real attempts at the same problem: stop, write what you tried and saw in the
  issue, mark it `blocked` in `docs/STATUS.md`, and tell Tyler in plain words.

## 7. Known traps

- **The repo is in iCloud Drive** (`~/Documents`). iCloud creates conflict copies named like
  `build 2.ninja` in busy folders. Scripts build in `~/Library/Caches/coffee-probe/`. If a build in
  `build/` or `cmake-build-*/` acts strangely, delete the folder. Never commit a name ending in
  space + number (guards refuse it). Rebases and branch switches are when copies appear: check
  `git status` afterwards. The pre-commit hook runs the guards; enable it in a fresh clone with
  `git config core.hooksPath .agents/git-hooks`.
- **`sdkconfig.defaults` only applies when `sdkconfig` is generated.** Changing a default does nothing
  to an existing `sdkconfig`; delete it (the scripts do this automatically for their own builds).
- **Two IDF versions are installed** (v5.3.1, v6.0.3). Never source both `export.sh` files in one shell;
  the scripts use a clean `env -i` environment for this reason.
- **`idf.py monitor` needs a TTY.** Agents use `scripts/serial_capture.py` instead.
- **ESP32-C3 strapping pins** GPIO2, 8, 9: nothing may pull them the wrong way at reset (GPIO9 low =
  download mode). The thermocouple CS pull-up exists for this reason.
- **Many cheap K-type probes have red as the negative lead.** A reading that falls when the tip is
  warmed means reversed polarity, not a firmware bug.
- **MAX6675 needs ≥ 220 ms between reads**, or it returns the previous conversion.
- Exceptions and RTTI are off in the IDF build (compile error, not a runtime surprise).
