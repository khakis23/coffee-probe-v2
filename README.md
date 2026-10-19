# CoffeeProbe

Wireless K-type thermocouple probe for coffee roasting. An ESP32-C3 reads a MAX31855/MAX6675 and streams temperature to [Artisan](https://artisan-scope.org/) with no router and no extra software on the roaster's computer. A small OLED and four buttons handle status and settings.

- Requirements: [`ideas.md`](ideas.md)
- Architecture & plan: [`docs/architecture.md`](docs/architecture.md)
- Dev environment (CLI + CLion): [`docs/dev-setup.md`](docs/dev-setup.md)
- How we work (HUMAN/CLAUDE tickets, branches, PRs): [`CONTRIBUTING.md`](CONTRIBUTING.md)

```bash
. ~/esp/esp-idf/export.sh
idf.py build
idf.py -p /dev/cu.usbmodem* flash monitor
```
