# Reliability contract

The probe is used by a non-technical roaster in a hot room with no router. These invariants hold for
every change. The design behind them is in `docs/architecture.md` → *Reliability design*. A change
that weakens one needs a `docs/DECISIONS.md` entry and a second-model review.

## Invariants

- **R1. Never show a wrong temperature as real.** Probe open, short, stale (> 2 s) or out of range →
  register value −10 (Artisan shows −1) and `PROBE?` on screen within 1 s. Never freeze the last good
  value.
- **R2. Recover by itself.** Every fault (probe unplugged, Wi-Fi client gone, OLED glitch, I2C hang,
  brownout) clears without a button press or a power cycle once the cause is gone.
- **R3. Never hang.** Every wait has a timeout. Our tasks feed the task watchdog. A hang becomes a
  panic → coredump → reboot within ~5 s, and the next boot shows the reset reason on the Diagnostics
  screen.
- **R4. Artisan's link is sacred.** Sensor, UI, display or storage problems must not block or slow the
  Modbus task. The register block is published atomically, so there are no torn reads. Never more
  than 4 sockets; idle sockets time out; the oldest is evicted.
- **R5. Reliability config stays on.** Task and interrupt watchdogs, panic → print + reboot, brownout
  detector, coredump to flash, custom partition table (guard-checked). App rollback stays off until
  OTA exists.
- **R6. Settings can't brick the probe.** Versioned struct + CRC; anything invalid → defaults. NVS is
  written only on an explicit save. Holding OK at boot is a factory reset that always works.
- **R7. Heat is handled before it does damage.** Warn and critical thresholds with hysteresis and N
  consecutive samples. Critical → message, then deep sleep with timer + button wake, then resume.
  Thresholds are tuned only by real tests next to the roaster (#27).
- **R8. Boot fast and predictably.** Splash < 300 ms, network up in ~2 s, same behaviour on every boot.
  No step in boot may wait on a peripheral without a timeout.

## When a change touches these areas

- Say in the PR which invariants it affects and how they are preserved.
- Add host tests for the logic side (fault mapping, stale timing, hysteresis, CRC/version handling).
- List the hardware checks still needed (e.g. "unplug the probe and watch Artisan") and make sure a
  `TYLER` issue covers them, usually #13, #26 or #27.
