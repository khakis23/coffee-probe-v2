# thermocouple

SPI driver (`spi_master`) for the MAX31855 / MAX6675 thermocouple amplifier: reads raw frames and
hands them to `core` for decoding (`core::decode_max31855` / `core::decode_max6675`). Filtering,
fault debounce, recovery and staleness live in `core::SensorPipeline`; the sensor task in
`main/tasks.cpp` wires them together every 250 ms. See [docs/architecture.md](../../docs/architecture.md)
→ *Reliability design*.

- API: `init()` (safe to retry), `read()` (one decoded frame; `BusError` on any SPI problem),
  `chip_name()`.
- Chip and grounded-probe mode are build-time Kconfig options (`menuconfig` → CoffeeProbe
  thermocouple) until settings exist (D-008). Default: MAX31855, not grounded.
- Every wait is finite (queue 20 ms, result 50 ms). Reads are spaced at least one worst-case
  conversion apart. A failed init is retried every 5 s. A transfer stuck for 5 s panics (R3).
- Pins come from `board/board.hpp`, never from literals here.
