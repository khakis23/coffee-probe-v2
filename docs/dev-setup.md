# Dev setup

Firmware is a standard ESP-IDF CMake project targeting the **ESP32-C3**.

| Tool     | Version on this machine | Location                          |
|----------|-------------------------|-----------------------------------|
| ESP-IDF  | v5.3.1                  | `~/esp/esp-idf`                   |
| Toolchain| riscv32-esp-elf GCC 13.2| `~/.espressif/tools`              |
| CLion    | 2025.3                  | `/Applications/CLion.app`         |

## Command line

```bash
. ~/esp/esp-idf/export.sh        # once per shell
idf.py build                     # target comes from sdkconfig.defaults (esp32c3), board defaults to xiao
idf.py -p /dev/cu.usbmodem* flash monitor   # native USB boards (XIAO, SuperMini)
idf.py -p /dev/cu.usbserial-* flash monitor # boards with a USB-UART chip (DevKitM-1)
```

Exit the monitor with `Ctrl+]`. If you can't find the port, run `ls /dev/cu.*` before and after plugging in the board.

If you change settings in `idf.py menuconfig`, copy the ones that matter into `sdkconfig.defaults` (or `sdkconfig.defaults.<board>` if they only apply to one board). `sdkconfig` is generated and not committed.

## Board profiles

| `PROBE_BOARD` | Board | Console | Kconfig symbol |
|---|---|---|---|
| `xiao` (default) | Seeed Studio XIAO ESP32C3 | USB-Serial-JTAG only (frees GPIO20 for BACK) | `CONFIG_PROBE_BOARD_XIAO_C3` |
| `generic` | Any ESP32-C3 DevKit (DevKitM-1, SuperMini, ...) | UART0, plus USB-Serial-JTAG as secondary | `CONFIG_PROBE_BOARD_GENERIC_C3` |

How it works:

1. The top-level `CMakeLists.txt` reads the CMake cache variable `PROBE_BOARD` (default `xiao`) and, **before** `project()`, sets
   `SDKCONFIG_DEFAULTS` to `sdkconfig.defaults;sdkconfig.defaults.<board>`. The board file is applied on top of the shared one.
2. `sdkconfig.defaults.<board>` sets the console options **and** the matching `CONFIG_PROBE_BOARD_*` choice from
   `components/board/Kconfig`, so the pin map and the console can't disagree.
3. `components/board/include/board/board.hpp` picks its `constexpr` pin table with `#if CONFIG_PROBE_BOARD_...`
   (Kconfig symbols become `#define`s in the generated `build/config/sdkconfig.h`). It's the only file allowed to name GPIOs.

**Switching boards: delete `sdkconfig` first.** Defaults files are only read when `sdkconfig` is generated; an existing
`sdkconfig` wins. Deleting `build/` too avoids a full-rebuild surprise in CLion. If you forget, CMake stops with
"PROBE_BOARD=... but sdkconfig is for another board".

```bash
rm -rf sdkconfig build
idf.py -DPROBE_BOARD=generic build    # or -DPROBE_BOARD=xiao
```

`idf.py` remembers the value in `build/CMakeCache.txt`, so later `idf.py build` / `flash` calls keep the same board until you switch again.

Adding a board: a new `config PROBE_BOARD_...` in `components/board/Kconfig`, a new `#elif` table in `board.hpp`,
a new `sdkconfig.defaults.<name>`, and the name in the `PROBE_BOARD` check in the top-level `CMakeLists.txt`.

## CLion (one-time setup)

1. **Open** this folder in CLion (the folder with the top-level `CMakeLists.txt`).
2. **Settings → Build, Execution, Deployment → Toolchains**: click `+` → *System*, name it `ESP-IDF`.
   Click **Add environment → From file** and pick `~/esp/esp-idf/export.sh`.
   CLion sources it before every CMake run, so it finds `IDF_PATH`, the RISC-V compiler, and Python.
   Move this toolchain to the top of the list, or select it in the CMake profile below.
3. **Settings → Build, Execution, Deployment → CMake**: edit the `Debug` profile.
   - Toolchain: `ESP-IDF`
   - Generator: `Ninja`
   - CMake options: `-DIDF_TARGET=esp32c3 -DPROBE_BOARD=xiao` (or `generic`; see [Board profiles](#board-profiles)).
     To switch boards, change this option, delete `sdkconfig` and `build/`, then **Tools → CMake → Reset Cache and Reload Project**.
   - Build directory: `build` (shared with `idf.py`, so the CLI and the IDE don't build everything twice)
4. Reload the CMake project. Indexing should resolve `esp_log.h`, `freertos/...`, etc.
   If headers show red, check the CMake tool window: an empty `IDF_PATH` means step 2 didn't take.

### Build / flash / monitor from CLion

- **Build**: select the `app` target and click the hammer, or build `all`.
- **Flash**: ESP-IDF generates a `flash` CMake target. Create a *CMake Application* run configuration with target `flash`, executable: none. Set the port with env var `ESPPORT=/dev/cu.usbmodemXXXX` in that config.
- **Monitor**: use the built-in Terminal: `idf.py -p $ESPPORT monitor`. (CLion's Serial Monitor plugin also works, at 115200 baud, but it doesn't decode panic backtraces.)
- **Debugging (optional, later)**: C3 boards with native USB expose JTAG directly. Run `idf.py openocd` in a terminal, then add an *Embedded GDB Server* config pointing at `riscv32-esp-elf-gdb`, target `localhost:3333`.

## Bring-up diagnostics (#2/#3)

A temporary firmware mode that tells you what hardware is on the bench. When it's on, `app_main` runs the diagnostics and never starts the normal app.

**Turn it on** (simplest): `idf.py menuconfig` → **CoffeeProbe bring-up** → tick *Run bring-up diagnostics instead of the app* → save (`S`) and quit (`Q`). Then build, flash and monitor as usual.

Alternative without the menu: put `CONFIG_PROBE_BRINGUP_DIAG=y` in a file such as `/tmp/diag.defaults`, delete `sdkconfig` (existing values win over defaults), and run
`idf.py -DPROBE_EXTRA_DEFAULTS=/tmp/diag.defaults build` (the top-level `CMakeLists.txt` layers it after the board defaults).

**What to expect** in the monitor (tag `bringup`):

1. A banner with the board name and pin map (XIAO: I2C SDA=6 SCL=7; thermocouple SCK=8 MISO=9 CS=10).
2. `I2C scan`: one line per responding address. An OLED is normally `0x3C` or `0x3D` (marked "likely OLED"). Nothing found = check SDA/SCL wiring and power.
3. If an OLED answered, a test pattern: 1-px border, an X corner to corner, and a small filled block top-left.
   A clean border means **SSD1306**. A border shifted ~2 px, or a garbage column at the right edge, means **SH1106**.
4. Every second: `TC raw 0x........`, then the frame decoded as MAX31855 and as MAX6675 (°C and °F), and a one-line guess.
   A MAX31855's internal (cold-junction) temperature reads close to room temperature. A MAX6675 usually leaves the low 16 bits `0000`. All `0`s or all `F`s = wiring or power problem.
   Touch the thermocouple tip and watch the TC reading rise.

**Turn it off again** before normal work: untick it in menuconfig (or delete `sdkconfig` and rebuild without the extra defaults file). Never commit it into `sdkconfig.defaults`.

## Board notes

- Each board profile builds its own image (pins and console differ). Pin assignments live only in `components/board/include/board/board.hpp`. Never hardcode a GPIO elsewhere.
- Native-USB boards (XIAO, SuperMini): if a flash attempt can't connect, hold **BOOT**, tap **RESET**, release **BOOT**, and retry.

## Crashes and core dumps

The firmware never halts on a crash: it prints the panic, saves a **core dump** to the `coredump` flash partition, and reboots (see the reliability section of `sdkconfig.defaults` and `partitions.csv`).

### What a crash looks like in the monitor

- **Panic** (null pointer, `abort()`, failed `assert`): a `Guru Meditation Error: Core 0 panic'ed (...)` banner, a register dump and a `Backtrace:` line. `idf.py monitor` decodes the backtrace into file:line for you.
- **Task watchdog** (a task stopped yielding for 5 s):
  ```
  E (12345) task_wdt: Task watchdog got triggered. The following tasks/users did not reset the watchdog in time:
  E (12345) task_wdt:  - IDLE (CPU 0)
  E (12345) task_wdt: Tasks currently running:
  E (12345) task_wdt: CPU 0: main
  E (12345) task_wdt: Aborting.
  ```
  followed by the usual panic output. `IDLE` in the list means some task hogged the CPU; the "currently running" task is usually the culprit.
- **Interrupt watchdog**: `Guru Meditation Error: Core 0 panic'ed (Interrupt wdt timeout on CPU0)`.
- **Stack overflow**: `***ERROR*** A stack overflow in task <name> has been detected.` Give that task a bigger stack.
- **Brownout**: `Brownout detector was triggered`, then a reset. Usually a bad USB cable or a weak supply.

After any of these you'll see `Core dump ... saved` (or similar), a 1 s pause, then the ROM boot banner again (`rst:0xc (RTC_SW_CPU_RST)` after a panic reboot).

### Reading the core dump

With the board plugged in (no need to reproduce the crash):

```bash
idf.py -p $ESPPORT coredump-info    # summary: crashed task, registers, backtrace of every task
idf.py -p $ESPPORT coredump-debug   # same dump, opened in GDB (bt, info threads, p some_var)
```

Both need the **same `build/` ELF** that was running when it crashed. If you've rebuilt since, check out that commit and rebuild first, or the backtrace will point at the wrong lines.

### Erasing the core dump

The dump stays in flash until something overwrites it (the next crash does). To start clean:

```bash
parttool.py -p $ESPPORT erase_partition --partition-name=coredump
```

`idf.py erase-flash` also clears it, but wipes settings (NVS) too.
