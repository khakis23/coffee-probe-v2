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

## Bring-up diagnostics (#2/#3)

A temporary firmware mode that tells you what hardware is on the bench. When it's on, `app_main` runs the diagnostics and never starts the normal app.

**Turn it on** (simplest): `idf.py menuconfig` → **CoffeeProbe bring-up** → tick *Run bring-up diagnostics instead of the app* → save (`S`) and quit (`Q`). Then build, flash and monitor as usual.

Alternative without the menu: put `CONFIG_PROBE_BRINGUP_DIAG=y` in a file such as `/tmp/diag.defaults`, delete `sdkconfig` (existing values win over defaults), and run
`idf.py -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;/tmp/diag.defaults" build`.

**What to expect** in the monitor (tag `bringup`):

1. A banner with the pin map (XIAO plan: I2C SDA=6 SCL=7; thermocouple SCK=8 MISO=9 CS=10).
2. `I2C scan`: one line per responding address. An OLED is normally `0x3C` or `0x3D` (marked "likely OLED"). Nothing found = check SDA/SCL wiring and power.
3. If an OLED answered, a test pattern: 1-px border, an X corner to corner, and a small filled block top-left.
   A clean border means **SSD1306**. A border shifted ~2 px, or a garbage column at the right edge, means **SH1106**.
4. Every second: `TC raw 0x........`, then the frame decoded as MAX31855 and as MAX6675 (°C and °F), and a one-line guess.
   A MAX31855's internal (cold-junction) temperature reads close to room temperature. A MAX6675 usually leaves the low 16 bits `0000`. All `0`s or all `F`s = wiring or power problem.
   Touch the thermocouple tip and watch the TC reading rise.

**Turn it off again** before normal work: untick it in menuconfig (or delete `sdkconfig` and rebuild without the extra defaults file). Never commit it into `sdkconfig.defaults`.

## Board notes

- Every ESP32-C3 board works with the same firmware image. Pin assignments differ per board and live in one header (see the build plan). Never hardcode a GPIO elsewhere.
- Native-USB boards (XIAO, SuperMini): if a flash attempt can't connect, hold **BOOT**, tap **RESET**, release **BOOT**, and retry.
