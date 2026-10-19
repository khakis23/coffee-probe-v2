# thermocouple

SPI driver (`spi_master`) for the MAX31855 / MAX6675 thermocouple amplifier: reads raw frames and hands them to `core` for decoding. See [docs/architecture.md](../../docs/architecture.md) → *Code layout*. This is an empty
stub created by #4 so the layout builds; the M2 Thermocouple tickets fill it in. Pins come from `board/board.hpp`,
never from literals here. Hardware-independent logic (parsing, state machines, policies) belongs
in `components/core` so it can be host-tested; this component should stay a thin IDF wrapper.
