# health

Thermal monitor, task liveness, and reset + boot diagnostics. The policies are in `core`
(`core/thermal.hpp`, `core/boot_record.hpp`, host-tested); this component is the IDF side. See
[docs/architecture.md](../../docs/architecture.md) → *Reliability design*.

- `record_boot()`: called first in `app_main`. Maps `esp_reset_reason()` to a `core::ResetCause` and
  keeps a `core::BootRecord` (boots, crashes, power dips since power-on) in RTC no-init memory, which
  survives restarts, panics, watchdog resets and deep sleep but not a power-off.
- `clear_brownouts()`: the power warning ended (30 min without a dip).
- `init()` / `read_die()`: the C3 die temperature sensor, retried every 10 s after a failure.
- `enter_protective_sleep()`: overheat Critical → 60 s timer deep sleep (#24).
- The health task itself is in `main/tasks.cpp`: thermal policy, liveness of the sensor and net tasks
  (a task that stops running its rounds for 10 s → panic → coredump → reboot), memory margins every
  10 s, and ending the power warning.
