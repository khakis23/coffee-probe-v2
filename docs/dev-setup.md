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

## Board notes

- Each board profile builds its own image (pins and console differ). Pin assignments live only in `components/board/include/board/board.hpp`. Never hardcode a GPIO elsewhere.
- Native-USB boards (XIAO, SuperMini): if a flash attempt can't connect, hold **BOOT**, tap **RESET**, release **BOOT**, and retry.
