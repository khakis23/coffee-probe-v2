#!/usr/bin/env python3
"""Capture the board's serial log for a fixed time, without an interactive terminal.

`idf.py monitor` needs a real TTY, which agents don't have. This reads the port for N seconds,
prints each line with a timestamp (seconds since start), and optionally saves it to a file.

    scripts/serial_capture.py --seconds 15 --reset        # auto-detect port, reboot board first
    scripts/serial_capture.py --port /dev/cu.usbmodem1101 --seconds 60 --out log.txt
    scripts/serial_capture.py --until "Project build complete|Guru Meditation" --seconds 30

Panic backtraces are NOT decoded here. Decode with the build folder's ELF (see the device skill):
    riscv32-esp-elf-addr2line -pfiaC -e <build>/coffee_probe.elf 0x4200abcd 0x4200ef01

Needs pyserial, which every ESP-IDF Python env has: run it with the IDF Python, e.g.
    ~/.espressif/python_env/idf5.3_py3.14_env/bin/python scripts/serial_capture.py ...
"""
import argparse
import glob
import re
import sys
import time

try:
    import serial  # pyserial
except ImportError:
    sys.exit("pyserial missing: run with the ESP-IDF Python (see the docstring).")


def find_port():
    ports = []
    for pattern in ("/dev/cu.usbmodem*", "/dev/cu.usbserial-*", "/dev/cu.SLAB_USBtoUART*",
                    "/dev/cu.wchusbserial*"):
        ports += glob.glob(pattern)
    if not ports:
        sys.exit("NO BOARD: no USB serial port found. Ask Tyler to plug the board in.")
    if len(ports) > 1:
        sys.exit(f"Several serial ports, pass --port: {' '.join(ports)}")
    return ports[0]


def reset_board(ser):
    # Same line sequence esptool uses for a normal (non-download) reset: EN low, then high with
    # IO9/BOOT high. Works on native USB-Serial-JTAG (XIAO) and on DevKits with auto-reset circuits.
    ser.dtr = False
    ser.rts = True
    time.sleep(0.1)
    ser.rts = False


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--port")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--seconds", type=float, default=15)
    ap.add_argument("--reset", action="store_true", help="reboot the board before capturing")
    ap.add_argument("--until", help="stop early when a line matches this regex")
    ap.add_argument("--out", help="also write the log to this file")
    args = ap.parse_args()

    port = args.port or find_port()
    until = re.compile(args.until) if args.until else None
    out = open(args.out, "w", encoding="utf-8") if args.out else None

    # The native USB port disappears for a moment when the chip resets; retry opening it.
    deadline = time.monotonic() + 5
    while True:
        try:
            ser = serial.Serial(port, args.baud, timeout=0.2)
            break
        except serial.SerialException as e:
            if time.monotonic() > deadline:
                sys.exit(f"Cannot open {port}: {e}")
            time.sleep(0.3)

    if args.reset:
        reset_board(ser)

    start = time.monotonic()
    buf = b""
    matched = False
    try:
        while time.monotonic() - start < args.seconds and not matched:
            try:
                chunk = ser.read(1024)
            except serial.SerialException:
                time.sleep(0.3)  # port re-enumerating after a reset
                try:
                    ser.close()
                    ser = serial.Serial(port, args.baud, timeout=0.2)
                except serial.SerialException:
                    pass
                continue
            buf += chunk
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                line = raw.decode("utf-8", errors="replace").rstrip("\r")
                line = re.sub(r"\x1b\[[0-9;]*m", "", line)  # strip ESP_LOG colour codes
                stamped = f"[{time.monotonic() - start:7.2f}] {line}"
                print(stamped, flush=True)
                if out:
                    out.write(stamped + "\n")
                if until and until.search(line):
                    matched = True
                    break
    finally:
        ser.close()
        if out:
            out.close()
    if until and not matched:
        print(f"(--until pattern not seen in {args.seconds:g} s)")
        sys.exit(1)


if __name__ == "__main__":
    main()
