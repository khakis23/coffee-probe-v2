# Loading firmware onto a probe

> **Draft.** Not yet tried on a real probe; checked and corrected the first time it's used (#28).

## Part 1: Load a release file (no developer tools)

Files come from the project's GitHub Releases page.
- For the standard probe (a generic ESP32-C3 DevKit), use the file named `coffee-probe-generic-<version>.bin`.
- `coffee-probe-xiao-<version>.bin` is only for a probe built on the small Seeed XIAO board.
- Ignore the `.elf` files (those are for reading crash reports).

**Important:** loading this file resets the probe's settings (offset, Wi-Fi channel, brightness, etc.) to factory defaults. Write down any settings you changed (Settings menu) so you can enter them again.

### Tool

Espressif's web flasher, https://espressif.github.io/esptool-js/ , in Google Chrome or Microsoft Edge on the Mac (Safari and Firefox can't talk to USB devices).

### Steps

1. Plug the probe into the Mac with a USB cable that carries data (not a charge-only cable).
2. Open the page.
3. Click Connect and pick the probe's port in the list the browser shows (on a DevKit usually named like "USB Single Serial" or "CP210x"/"usbserial"; on native-USB boards "USB JTAG/serial debug unit").
4. Set the flash address to 0x0 (the page may suggest another address; it must be 0x0 for this file).
5. Choose the downloaded .bin file.
6. Click Program and wait until it reports it is finished (about a minute).
7. Press the board's RESET (sometimes labelled EN or RST) button, or unplug and replug the probe.

Labels on the web page may differ slightly from these steps; the address 0x0 is what matters.

### Check

1. The screen shows Starting, then the temperature.
2. If the buttons are fitted: hold OK about a second to open Settings.
3. Go to Diagnostics.
4. Page 1 shows Firmware with the new version.

### If Connect fails

1. Unplug the probe.
2. Hold the BOOT button.
3. Plug the probe back in.
4. Release the BOOT button.
5. Click Connect again.
6. After programming, press RESET to start normally.

## Part 2: For developers

- `scripts/flash.sh` builds and flashes from the source (options `--board generic|xiao`, `--idf v5.3.1|v6.0.3`). It writes only the bootloader, partition table and app, so the probe's settings are kept.
- `scripts/release.sh [--idf v5.3.1|v6.0.3]` builds the release files from a clean commit into `~/Library/Caches/coffee-probe/release/<version>/<idf>/`: one merged `.bin` per board (written at 0x0), the matching `.elf`, and `SHA256SUMS`. Attach all of them to the GitHub release for tag `<version>`. Keep the `.elf`: it is needed to read a crash dump from a probe running that release (`idf.py coredump-info`, see docs/dev-setup.md).
- Why settings reset with the merged file: it covers address 0x0 to the end of the app, which includes the settings area (NVS) and the RF calibration area between them; RF calibration redoes itself on the first boot.
- Never load a fault-test build (`--fault`) or the bring-up diagnostics build (`--diag`) onto a probe staff use; release.sh refuses both.
