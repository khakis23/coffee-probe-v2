---
name: device
description: Work with a real CoffeeProbe board plugged into the Mac - detect it, build and flash firmware or bring-up diagnostics, capture the serial log without a TTY, decode panic backtraces, read core dumps. Use whenever a task needs the real hardware.
---

# Device runbook (ESP32-C3: XIAO or generic DevKit)

`scripts/flash.sh` builds under `~/Library/Caches/coffee-probe/flash/` (one folder per checkout and
board/IDF/diag combination), never in the repo, and prints the exact **build folder** at the end.
Use that folder wherever this runbook says `<build>`.

Set this once per shell (the IDF Python has pyserial):

```bash
PY=~/.espressif/python_env/idf5.3_py3.14_env/bin/python
```

## Is a board there?

```bash
ls /dev/cu.usbmodem* /dev/cu.usbserial-* 2>/dev/null
```

- `usbmodem…` = native USB board (XIAO, SuperMini). `usbserial-…` = DevKit with a USB-UART chip.
- Nothing listed → no board. Ask Tyler with the `tyler-test` skill format ("Plug the probe's board
  into the Mac with a data USB-C cable", 1 minute, report back "plugged in"). Batch it with any other
  physical step you'll need.

## Flash

```bash
scripts/flash.sh                       # XIAO profile, app firmware, auto port
scripts/flash.sh --diag                # bring-up diagnostics (#2/#3) instead of the app
scripts/flash.sh --board generic --port /dev/cu.usbserial-0001
scripts/flash.sh --idf v6.0.3          # build with IDF v6
```

Exit code 3 = no board. If flashing can't connect on a native-USB board, the chip may be stuck: ask
Tyler to hold **BOOT**, tap **RESET**, release **BOOT**, then retry. After flashing in that mode,
a **RESET** tap is needed to start the app.

## Read the serial log

```bash
$PY scripts/serial_capture.py --reset --seconds 15                  # boot log
$PY scripts/serial_capture.py --seconds 60 --out .scratch/serial.txt
$PY scripts/serial_capture.py --reset --until "Guru Meditation|alive" --seconds 20
```

Lines are prefixed with seconds since capture start. Exit 1 = `--until` pattern never seen;
exit 2 = the board disconnected and didn't come back.

What to look for: `rst:` reason on the ROM banner, `coffee_probe` / `bringup` tags, `E (` error lines,
`Guru Meditation`, `task_wdt`, `Brownout`. `docs/dev-setup.md` → *Crashes and core dumps* explains
each crash signature.

## Decode a panic backtrace

Use the ELF from the same build folder that was flashed (and the matching IDF's `export.sh`):

```bash
. ~/esp/esp-idf/export.sh >/dev/null
riscv32-esp-elf-addr2line -pfiaC -e <build>/coffee_probe.elf <PC addresses from the Backtrace line>
```

## Core dump (crash already happened)

```bash
. ~/esp/esp-idf/export.sh >/dev/null
idf.py -B <build> -DSDKCONFIG=<build>/sdkconfig -p <port> coredump-info
```

Needs the ELF of the firmware that crashed. Erase afterwards (same shell, after `export.sh`):
`parttool.py -p <port> erase_partition --partition-name=coredump`.

## Bring-up diagnostics output

With `--diag` the log shows the pin map, an I2C scan (OLED usually 0x3C/0x3D), and every second a raw
thermocouple frame decoded both as MAX31855 and MAX6675. Interpretation is in `docs/dev-setup.md` →
*Bring-up diagnostics*. All `0`s or all `F`s = wiring or power problem.

## Rules

- Paste the relevant log lines into the PR or issue as evidence. Say which board and firmware.
- Physical actions (touching the probe, unplugging wires, heat) are Tyler's. Script them with the
  `tyler-test` skill and capture the log while he does them.
- Never flash with bring-up diagnostics and leave it there at the end of a session without saying so
  in `docs/STATUS.md`. Reflash the app firmware when done.
