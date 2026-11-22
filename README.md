# CoffeeProbe

Wireless K-type thermocouple probe for coffee roasting. An ESP32-C3 reads a MAX31855/MAX6675 and streams temperature to [Artisan](https://artisan-scope.org/) with no router and no extra software on the roaster's computer. A small OLED and four buttons handle status and settings.

- Requirements: [`ideas.md`](ideas.md)
- Architecture & plan: [`docs/architecture.md`](docs/architecture.md)
- Dev environment (CLI + CLion): [`docs/dev-setup.md`](docs/dev-setup.md)
- How we work (Claude develops, Tyler tests on hardware): [`CONTRIBUTING.md`](CONTRIBUTING.md); current status: [`docs/STATUS.md`](docs/STATUS.md)
- Rules for AI agents: [`AGENTS.md`](AGENTS.md) and [`.agents/`](.agents/)

```bash
. ~/esp/esp-idf/export.sh
idf.py build                                  # generic ESP32-C3 DevKit (default board)
idf.py -p /dev/cu.usbmodem* flash monitor
```

### Picking a board

The firmware has one profile per board: `generic` (any ESP32-C3 DevKit, default) and `xiao` (Seeed
XIAO ESP32C3, for battery power later). Pins for each live only in
[`components/board/include/board/board.hpp`](components/board/include/board/board.hpp).
Choose with `-DPROBE_BOARD=...`, and **delete `sdkconfig` when switching**, because board
defaults only apply to a freshly generated `sdkconfig`:

```bash
rm -rf sdkconfig build
idf.py -DPROBE_BOARD=generic build
```

In CLion, add `-DPROBE_BOARD=generic` (or `xiao`) to the CMake options. Details in
[`docs/dev-setup.md`](docs/dev-setup.md#board-profiles).

### Code layout

`main/app_main.cpp` only wires things together; the code lives in `components/` (`core`, `board`,
`thermocouple`, `display`, `buttons`, `net`, `storage`, `health`). Each has a short README, and
[`docs/architecture.md`](docs/architecture.md) → *Code layout* has the overview.
