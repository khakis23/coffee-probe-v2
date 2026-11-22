# net

SoftAP setup and the Modbus TCP socket server that Artisan talks to (the PDU/MBAP codec lives in `core`). See [docs/architecture.md](../../docs/architecture.md) → *Code layout*. This is an empty
stub created by #4 so the layout builds; the M3 Wi-Fi + Modbus tickets fill it in. Pins come from `board/board.hpp`,
never from literals here. Hardware-independent logic (parsing, state machines, policies) belongs
in `components/core` so it can be host-tested; this component should stay a thin IDF wrapper.
