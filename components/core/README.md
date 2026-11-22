# core

Pure, hardware-independent C++: temperature types, thermocouple frame decode, filters, fault
model, settings model + validation, menu state machine, Modbus PDU/MBAP codec, register map,
thermal policy, debouncer (see [docs/architecture.md](../../docs/architecture.md) → *Code layout*).
**No IDF headers or REQUIRES**: everything here must also compile natively on the host, where the
doctest project from #7 tests it. Created as an empty stub by #4; the M1–M6 tickets that own each
piece of logic add it here, with host tests.
