# storage

NVS-backed settings: versioned, CRC-checked and validated (the settings model lives in `core`). See [docs/architecture.md](../../docs/architecture.md) → *Code layout*. This is an empty
stub created by #4 so the layout builds; the M5 settings tickets fill it in. Pins come from `board/board.hpp`,
never from literals here. Hardware-independent logic (parsing, state machines, policies) belongs
in `components/core` so it can be host-tested; this component should stay a thin IDF wrapper.
