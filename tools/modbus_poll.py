#!/usr/bin/env python3
"""Poll a CoffeeProbe Modbus TCP server and print register readings.

Usage:
    python3 tools/modbus_poll.py [--host 192.168.4.1] [--port 502] [--unit 1]
                                 [--interval 1.0] [--count 0] [--clients 1]

Dependencies:
    pip install pymodbus
"""

import argparse
from datetime import datetime
import sys
import time

from pymodbus.client import ModbusTcpClient

STATUS_BITS = [
    (0, "probe open"),
    (1, "probe short"),
    (2, "stale"),
    (3, "overheat warn"),
    (4, "overheat crit"),
    (5, "low battery"),
    (6, "bus error"),
]


def fmt_temp(val: int, unit: str = "") -> str:
    """Format 16-bit temperature register (x10) or special status."""
    if val == 0x8000:
        return "n/a"
    signed = val - 0x10000 if val >= 0x8000 else val
    if signed == -10:
        return "FAULT"
    suffix = f" {unit}" if unit else ""
    return f"{signed / 10.0:.1f}{suffix}"


def format_status(raw: int) -> str:
    """Format status register bitfield."""
    flags = [name for bit, name in STATUS_BITS if raw & (1 << bit)]
    return ", ".join(flags) if flags else "ok"


def read_registers(client: ModbusTcpClient, unit: int):
    """Read 9 input registers starting at address 0, handling pymodbus API change."""
    try:
        return client.read_input_registers(address=0, count=9, device_id=unit)
    except TypeError:
        return client.read_input_registers(address=0, count=9, slave=unit)


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--host", default="192.168.4.1", help="server IP (default: 192.168.4.1)")
    parser.add_argument("--port", type=int, default=502, help="server port (default: 502)")
    parser.add_argument("--unit", type=int, default=1, help="unit/slave ID (default: 1)")
    parser.add_argument("--interval", type=float, default=1.0, help="seconds between polls (default: 1.0)")
    parser.add_argument("--count", type=int, default=0, help="number of polls, 0=forever (default: 0)")
    parser.add_argument(
        "--clients",
        type=int,
        default=1,
        metavar="N",
        help="number of simultaneous connections to poll round-robin (default: 1)",
    )
    args = parser.parse_args()

    clients = [ModbusTcpClient(args.host, port=args.port) for _ in range(max(1, args.clients))]
    client_idx = 0
    poll_count = 0

    try:
        while True:
            now = datetime.now().strftime("%H:%M:%S")
            client = clients[client_idx]

            try:
                if not client.connected:
                    if not client.connect():
                        print(f"{now} c{client_idx} error: cannot connect to {args.host}:{args.port}")
                        client.close()
                if client.connected:
                    result = read_registers(client, args.unit)
                    if result is None or (hasattr(result, "isError") and result.isError()):
                        print(f"{now} c{client_idx} error: {result}")
                        client.close()
                    elif not hasattr(result, "registers") or len(result.registers) < 9:
                        print(f"{now} c{client_idx} error: incomplete response ({result})")
                        client.close()
                    else:
                        regs = result.registers
                        bt_f = fmt_temp(regs[0], "F")
                        bt_c = fmt_temp(regs[1], "C")
                        cj = fmt_temp(regs[4], "C")
                        status_str = format_status(regs[2])
                        uptime = (regs[6] << 16) | regs[7]
                        major = (regs[8] >> 8) & 0xFF
                        minor = regs[8] & 0xFF

                        items = [
                            f"BT {bt_f} / {bt_c}",
                            f"CJ {cj}",
                        ]
                        if regs[3] != 0x8000:
                            items.append(f"die {fmt_temp(regs[3], 'C')}")
                        if regs[5] != 0xFFFF:
                            items.append(f"bat {regs[5]}%")
                        items.extend([
                            f"up {uptime} s",
                            f"fw {major}.{minor}",
                            f"status: {status_str}",
                        ])

                        print(f"{now} c{client_idx} {'  '.join(items)}")
            except Exception as err:
                print(f"{now} c{client_idx} error: {err}")
                try:
                    client.close()
                except Exception:
                    pass

            poll_count += 1
            if args.count > 0 and poll_count >= args.count:
                break

            time.sleep(args.interval)
            client_idx = (client_idx + 1) % len(clients)

    except KeyboardInterrupt:
        pass
    finally:
        for c in clients:
            try:
                c.close()
            except Exception:
                pass
    sys.exit(0)


if __name__ == "__main__":
    main()
