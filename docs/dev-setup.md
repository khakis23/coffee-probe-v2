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
idf.py build                     # target comes from sdkconfig.defaults (esp32c3)
idf.py -p /dev/cu.usbmodem* flash monitor   # native USB boards (XIAO, SuperMini)
idf.py -p /dev/cu.usbserial-* flash monitor # boards with a USB-UART chip (DevKitM-1)
```

Exit the monitor with `Ctrl+]`. If you can't find the port, run `ls /dev/cu.*` before and after plugging in the board.

If you change settings in `idf.py menuconfig`, copy the ones that matter into `sdkconfig.defaults`. `sdkconfig` is generated and not committed.

## CLion (one-time setup)

1. **Open** this folder in CLion (the folder with the top-level `CMakeLists.txt`).
2. **Settings → Build, Execution, Deployment → Toolchains**: click `+` → *System*, name it `ESP-IDF`.
   Click **Add environment → From file** and pick `~/esp/esp-idf/export.sh`.
   CLion sources it before every CMake run, so it finds `IDF_PATH`, the RISC-V compiler, and Python.
   Move this toolchain to the top of the list, or select it in the CMake profile below.
3. **Settings → Build, Execution, Deployment → CMake**: edit the `Debug` profile.
   - Toolchain: `ESP-IDF`
   - Generator: `Ninja`
   - CMake options: `-DIDF_TARGET=esp32c3`
   - Build directory: `build` (shared with `idf.py`, so the CLI and the IDE don't build everything twice)
4. Reload the CMake project. Indexing should resolve `esp_log.h`, `freertos/...`, etc.
   If headers show red, check the CMake tool window: an empty `IDF_PATH` means step 2 didn't take.

### Build / flash / monitor from CLion

- **Build**: select the `app` target and click the hammer, or build `all`.
- **Flash**: ESP-IDF generates a `flash` CMake target. Create a *CMake Application* run configuration with target `flash`, executable: none. Set the port with env var `ESPPORT=/dev/cu.usbmodemXXXX` in that config.
- **Monitor**: use the built-in Terminal: `idf.py -p $ESPPORT monitor`. (CLion's Serial Monitor plugin also works, at 115200 baud, but it doesn't decode panic backtraces.)
- **Debugging (optional, later)**: C3 boards with native USB expose JTAG directly. Run `idf.py openocd` in a terminal, then add an *Embedded GDB Server* config pointing at `riscv32-esp-elf-gdb`, target `localhost:3333`.

## Board notes

- Every ESP32-C3 board works with the same firmware image. Pin assignments differ per board and live in one header (see the build plan). Never hardcode a GPIO elsewhere.
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
