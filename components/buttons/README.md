# buttons

Polls the four button GPIOs and turns them into events on a queue (the debouncer itself lives in `core`). See [docs/architecture.md](../../docs/architecture.md) → *Code layout*. This is an empty
stub created by #4 so the layout builds; the M5 Buttons/menu tickets fill it in. Pins come from `board/board.hpp`,
never from literals here. Hardware-independent logic (parsing, state machines, policies) belongs
in `components/core` so it can be host-tested; this component should stay a thin IDF wrapper.
