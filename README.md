# CoffeeProbe

Wireless K-type thermocouple probe for coffee roasting. An ESP32-C3 reads a MAX31855/MAX6675 and streams temperature to [Artisan](https://artisan-scope.org/) with no router and no extra software on the roaster's computer. A small OLED and four buttons handle status and settings.

- Requirements: [`ideas.md`](ideas.md)
- Architecture & plan: [`docs/architecture.md`](docs/architecture.md)
- Dev environment (CLI + CLion): [`docs/dev-setup.md`](docs/dev-setup.md)
- How we work (HUMAN/CLAUDE tickets, branches, PRs): [`CONTRIBUTING.md`](CONTRIBUTING.md)

```bash
. ~/esp/esp-idf/export.sh
idf.py build                                  # XIAO ESP32C3 (default board)
idf.py -p /dev/cu.usbmodem* flash monitor
```

### Picking a board

The firmware has one profile per board: `xiao` (Seeed XIAO ESP32C3, default) and `generic` (any
ESP32-C3 DevKit, UART console kept). Pins for each live only in
[`components/board/include/board/board.hpp`](components/board/include/board/board.hpp).
Choose with `-DPROBE_BOARD=...`, and **delete `sdkconfig` when switching**, because board
defaults only apply to a freshly generated `sdkconfig`:

```bash
rm -rf sdkconfig build
idf.py -DPROBE_BOARD=generic build
```

In CLion, add `-DPROBE_BOARD=xiao` (or `generic`) to the CMake options. Details in
[`docs/dev-setup.md`](docs/dev-setup.md#board-profiles).

### Code layout

`main/app_main.cpp` only wires things together; the code lives in `components/` (`core`, `board`,
`thermocouple`, `display`, `buttons`, `net`, `storage`, `health`). Each has a short README, and
[`docs/architecture.md`](docs/architecture.md) → *Code layout* has the overview.
