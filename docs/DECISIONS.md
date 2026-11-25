# Decisions

Append-only log. Each entry: date, decision, why, who decided. Don't edit old entries; add a new one
that supersedes it. The original key decisions (SoftAP + Modbus TCP, own Modbus server, IDF v6,
own display stack, no exceptions/RTTI, pure `core`) are in `docs/architecture.md` → *Key decisions*.
The IDF version there (v6.0.x) is the target; until #2 confirms it, code must build on v5.3.1 and v6.0.3.

## D-001 · 2026-11-21 · Claude is the sole developer (Tyler)

Tyler no longer has time to write code. Claude leads all development and uses Codex and Gemini as
helpers according to Tyler's global routing rules; Tyler is the customer and does physical tasks only.
Former `HUMAN` coding tickets became `CLAUDE`; the label is now `TYLER` for hands-on work.

**Pending Tyler's OK** (proposed by Claude): Claude merges its own PRs once local and GitHub CI are
green, unless Tyler writes "hold". Until he confirms, PRs wait for him to merge.

## D-002 · 2026-11-21 · Agent environment: AGENTS.md + .agents/ (Tyler, Claude)

Same structure as Tyler's other AI-built projects: a short `CLAUDE.md` that imports `AGENTS.md`;
detailed rules in `.agents/rules/`; runbooks in `.agents/skills/` linked into `.claude/skills/`; a
Stop hook that flags uncommitted work. Keeps every model and session consistent.

## D-003 · 2026-11-21 · Mechanical guards (Claude)

`scripts/guards.sh` enforces the rules a grep can check (pins only in `board.hpp`, no heap, static
tasks, pure `core`, reliability sdkconfig present, no tracked generated files) in local and GitHub CI.
Changing a guard needs a new entry here and a second-model review.

## D-004 · 2026-11-21 · Host tests compile as C++23 (Claude)

The architecture's error type is `std::expected`, which is C++23. The host build was C++20, so the
first `core` code using it would have failed the host tests. Host build is now C++23 (no extensions),
checked with Apple clang 21. The firmware already builds as gnu++2b.

## D-005 · 2026-11-21 · Script build output lives outside the repo (Claude)

The repo is in iCloud Drive, which made conflict copies inside CLion's build folder. `ci-local.sh` and
`flash.sh` build in `~/Library/Caches/coffee-probe/`.

## D-006 · 2026-11-21 · Claude merges its own PRs (Tyler)

Tyler: "yes always merge yourself. you're the dev." Supersedes the pending part of D-001. Claude
squash-merges its own PRs once local CI and GitHub CI are green and the required reviews approve.
Tyler can still write "hold" on any PR to stop a merge.

## D-007 · 2026-11-21 · Scope check with Tyler (Tyler)

Retro of the requirements in `ideas.md`:
- **Board**: a generic ESP32-C3 DevKit is the main board and the default profile (as `ideas.md` says).
  The XIAO stays as a second profile for battery power later. The earlier plan had this the other way round.
- **Users**: Tyler and his non-technical staff, a few units. Screen messages and guides must work for
  staff with no help.
- **Channels**: bean temperature (BT) only in v1. ET would be a backlog item.
- **Enclosure**: out of scope. The project delivers firmware, docs and a wiring guide.

Follow-ups from the review of #33–#36 that this affects: the XIAO's thermocouple MISO moves from GPIO9 (the
BOOT-button pin) to GPIO21, so flashing stays reliable; diagnostics CI jobs build the default (generic) board.

## D-008 · 2026-11-21 · One-week MVP: Artisan connected by 2026-11-28 (Tyler)

Tyler needs a working probe connected to Artisan within a week. **MVP** (GitHub label `MVP`):
reliable thermocouple readings → the probe's own Wi-Fi network → Modbus TCP → stock Artisan plots BT; faults
show as −1; a minimal OLED (big °F while reading, Wi-Fi name/password and connection status otherwise,
PROBE? on a fault); the crash-safety config; an Artisan setup guide staff can follow; a 1-hour session
plus quick fault checks on Tyler's setup.

**Post-MVP** (label `post-MVP`): buttons, settings and the menu (#21–#23), overheat protection (#24),
liveness/brownout diagnostics (#25), the full fault-injection checklist and the soak test (#26/#27), and the
v1.0 release (#28).

Consequences, decided by Claude:
- With no settings menu, the thermocouple chip and grounded-probe mode are fixed at build time (Kconfig),
  from #3's findings.
- The Wi-Fi password is one fixed WPA2 password documented in the setup guide (and shown on the OLED), the
  same on every unit. Acceptable for the MVP: no internet behind it and the Modbus server is read-only.
  Per-unit passwords come with the settings work.
- Order of work: the Artisan path first, the OLED second. If the week runs short, the OLED's big-digit screen
  (#19) is cut first, never Artisan.

## D-009 · 2026-11-21 · Local CI is the only CI gate (Tyler)

Tyler: "turn off and don't run GH CI when running local CI." `.github/workflows/ci.yml` now runs only on manual
dispatch. `scripts/ci-local.sh` on the Mac decides whether a PR can merge; agents paste its summary in the PR.

## D-010 · 2026-11-24 · How the Modbus server keeps R4 (Claude)

- **"Never more than 4 sockets"** means at most 4 client sockets whenever the server isn't running.
  Evicting the oldest client needs `accept()` first, so a 5th exists only until the oldest is closed in
  the same call, with no other work in between. Refusing new clients instead would contradict "the
  oldest is evicted": a laptop that reconnects after sleeping must win over its own dead socket.
- **Pacing:** the net task runs every 20 ms with `vTaskDelayUntil`, and `select()` polls with zero
  timeout. Letting `select()` pace the task was rejected: with readable sockets it returns at once every
  round, the task never sleeps, and the idle task's watchdog reboots the single-core C3. 20 ms of
  added latency is nothing next to Artisan's 1–3 s polling.
- **Bounded work per round:** clients are served before new connections are accepted, at most 2
  accepts per round, one `recv()` per client.

## D-011 · 2026-11-24 · OLED driver uses i2c_master directly, not esp_lcd (Claude)

The architecture planned `esp_lcd` panel I/O over `i2c_master`. The display component instead talks to the
panel with `i2c_master` directly, in page mode for both SSD1306 and SH1106: about 200 lines, the same
approach the bringup diagnostics already proved on both IDF versions, and full control over timeouts,
bus reset and re-init (R2/R3). IDF ships an `esp_lcd` panel driver only for the SSD1306 (the SH1106 would
need an extra managed component, a new dependency), and the framebuffer and fonts are ours anyway, so
`esp_lcd` would add a layer without removing any code.

## D-012 · 2026-11-24 · Settings menu choices (Claude)

- **The Kconfig converter and grounded-probe options become the factory default** instead of being
  removed. When nothing valid is saved (new board, factory reset, damaged NVS) the build options apply
  and the menu shows them, so a unit built for a MAX6675 still works out of the box. A saved setting
  wins. The thermocouple driver now handles both chips at runtime (replaces the build-time choice in
  D-008).
- **A long OK opens the menu.** A short press does nothing on the live screen: staff tapping buttons
  mid-roast must never lose the temperature.
- **Settings set up at boot ask for a restart** (probe chip, grounded tip, Wi-Fi channel and power)
  rather than being re-applied live. Reconfiguring Wi-Fi or the SPI driver while running adds failure
  modes for settings changed once in the probe's life.
- **The calibration offset is in °F** (the unit staff see), stored in tenths, converted to tenths of °C
  for the sensor pipeline.

## D-013 · 2026-11-24 · Restart and power-dip diagnostics (Claude)

- **Counts live in RTC no-init memory**, not NVS: they describe this power-up, so a power-off should
  clear them, and nothing is written to flash on a crash or a brownout. A magic + check field rejects
  the random contents after a power-on.
- **Power warning after 2 dips** since power-on: one can be a fluke at plug-in. If RTC memory doesn't
  survive a brownout on the C3 (unverified; #26 checks), each dip looks like a fresh record, so a
  brownout with no valid record warns at once instead.
- **The warning and the dip count clear after 30 min without a dip**, and full Wi-Fi power returns
  (R2: recover without a power cycle). A lone dip at plug-in is forgotten the same way. A dip resets the chip, so uptime without a reset is time without a dip.
- **Liveness counters** for the sensor and net tasks, checked by the health task: a stall of 10 s
  panics. The task watchdog already catches a task stuck inside a step; this catches one that stopped
  being scheduled.

