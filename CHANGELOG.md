# Changelog

All notable changes. Each release lists what changed for the probe, in plain words.

## Unreleased

### For the roaster

- Reads the thermocouple 4 times a second, filters glitches, reports an unplugged or shorted probe within 1 s, and never shows an old reading as current (#12)
- Its own Wi-Fi network CoffeeProbe-XXXX, password coffeeprobe (#14)
- Serves bean temperature to Artisan over Modbus TCP at 192.168.4.1, port 502; Artisan shows -1 when there is no trustworthy reading; up to 4 connections (#16)
- Screen: big °F reading, PROBE? with the likely cause, a top line showing whether Artisan is connected or how to join the Wi-Fi (#18, #19, #20)
- Overheat protection: BOX TOO HOT: MOVE IT warning; TOO HOT and a 1-minute cooling sleep when critical (#24)
- Settings menu (hold OK): probe chip, grounded tip, offset, Wi-Fi channel and power, brightness, flip, show password, diagnostics, factory reset; hold OK at power-on for 5 s to reset (#23, #22)
- Diagnostics pages: firmware version, uptime, temperatures, restart reasons and counts, power dips, crash record, address, Artisan connections, memory (#20, #25)
- Check USB cable/power warning and lower Wi-Fi power after repeated power dips, clearing by itself (#25)
- Restarts itself after a freeze or crash (#6, #25)
- Screen burn-in protection: one-pixel shift every 3 minutes, idle screens dim after 10 minutes (#20)
- Artisan setup guide and staff quick-start card, drafts (#17, #28)

### Under the hood

- Pure logic in a host-tested core library; local CI builds every change on ESP-IDF v5.3.1 and v6.0.3 for the generic DevKit and Seeed XIAO (#7, D-009)
- Watchdogs, panic reboot, brownout detector and crash dumps always on, checked by guards (#5)
- Settings stored with version and checksum; damaged settings fall back to defaults field by field (#22)
- Bring-up diagnostics firmware for identifying the hardware (#2)
- Fault-test builds that freeze or crash on purpose for the fault-injection tests, refused in CI and releases (#26)
- Release script that builds one flashable file per board, and a flashing guide (#28)

Nothing has been tested on a wired probe yet; hardware results will be noted here.

