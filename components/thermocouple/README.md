# thermocouple

SPI driver (`spi_master`) for the MAX31855 / MAX6675 thermocouple amplifier: reads raw frames and
hands them to `core` for decoding (`core::decode_max31855` / `core::decode_max6675`). Filtering,
fault debounce, recovery and staleness live in `core::SensorPipeline`; the sensor task in
`main/tasks.cpp` wires them together every 250 ms. See [docs/architecture.md](../../docs/architecture.md)
→ *Reliability design*.

- API: `configure(chip, grounded)` (from `app_main`, before the sensor task starts), `init()` (safe
  to retry), `read()` (one decoded frame; `BusError` on any SPI problem), `chip()`,
  `grounded_probe()`, `chip_name()`.
- Chip and grounded-probe mode come from the settings menu and apply at the next restart. The Kconfig
  options (`menuconfig` → CoffeeProbe thermocouple) are the factory default, used when nothing is
  saved (D-012). Default: MAX31855, not grounded.
- Every wait is finite (queue 20 ms, result 50 ms). Reads are spaced at least one worst-case
  conversion apart. A failed init is retried every 5 s. A transfer stuck for 5 s panics (R3).
- Pins come from `board/board.hpp`, never from literals here.
