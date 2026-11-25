# net

SoftAP setup (`net.hpp`) and the Modbus TCP server Artisan talks to (`modbus_server.hpp`). The
MBAP/PDU codec, register map, register publishing and connection-slot policy live in `core`
(`modbus.hpp`, `registers.hpp`, `register_publish.hpp`, `conn_table.hpp`) and are host-tested; this
component stays a thin lwIP/IDF wrapper. See [docs/architecture.md](../../docs/architecture.md) →
*Reliability design*.

- SoftAP: `start_softap(channel, tx_power_qdbm)` from `app_main`, with the settings' channel (1, 6 or
  11) and Wi-Fi power capped at the board's limit (`core::effective_tx_power_qdbm`). Both apply at boot.
- Modbus server: port 502, 4 client slots, oldest evicted when full, 10 s idle timeout, TCP
  keepalive. `service(now_ms, 0)` is one non-blocking `select()` round; the net task calls it every
  20 ms and publishes a fresh register block every 250 ms. Nothing in it waits.
- Dev tool: `tools/modbus_poll.py` (needs `pip install pymodbus`) polls the registers from a Mac
  joined to the probe's network. `--clients 5` exercises the slot limit and eviction.
- Pins come from `board/board.hpp`, never from literals here.
