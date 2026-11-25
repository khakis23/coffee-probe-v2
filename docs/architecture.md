# CoffeeProbe v2 — Architecture

## Context

v1 (repo `khakis23/CoffeeProbe`, left as is) was an Arduino BLE sketch plus a custom Python desktop app. v2 has two hard requirements:

1. **Stock Artisan is the only software on the laptop.** Artisan has no generic BLE input, so BLE is out.
2. **Super reliable for a non-technical user**, in a warm roasting room, with **no router**.

Scope: a few units used by Tyler and his non-technical staff in his roastery; one channel (Bean Temp, no ET in v1). Firmware, docs and a wiring guide only: the enclosure is Tyler's. The laptop may lose Wi-Fi internet while roasting (Ethernet still works).

Claude is the sole developer; Tyler is the customer and does the hands-on hardware tests. Work is tracked as GitHub issues labelled `CLAUDE` or `TYLER`; see [CONTRIBUTING.md](../CONTRIBUTING.md).

## Key decisions

| Topic | Decision | Why |
|---|---|---|
| Link to Artisan | **The probe runs its own Wi-Fi network (SoftAP) + Modbus TCP**, which Artisan polls at fixed `192.168.4.1:502`. | Built into stock Artisan, and the most widely used DIY path. Artisan's WebSocket device needs JSON node setup that's hard for a non-technical user. MQTT (Artisan 4.2) needs a broker. |
| Modbus server | **Our own small read-only server** (function codes 3 and 4), not `esp-modbus` | About 200–300 lines; we control socket timeouts, keepalive and connection slots; no dependency risk on IDF v6; fully host-testable. |
| ESP-IDF | **v6.0.x** (v5.3 reaches end of life around Jan 2027); fall back to v5.5.x if needed | Latest stable for a new project. Use only the new driver APIs: `i2c_master`, `spi_master`, `temperature_sensor`, `adc_oneshot`. |
| Display | Direct `i2c_master` driver (no `esp_lcd`, D-011) + **our own 1 KB framebuffer and fonts**; SSD1306 and SH1106 both supported | The OLED part number is unverified (#3 identifies it). Small, testable code. |
| Language rules | C++ (gnu++2b), no exceptions/RTTI, `std::expected<T, Error>` for errors, no heap use in our code after init | Predictable failure paths. |
| Logic vs hardware | All logic lives in `components/core` (pure C++, **no IDF headers**) and is unit-tested on the host | Most bugs get caught without flashing. |

## Architecture

```
 MAX31855/6675 ──SPI──► [sensor task 4 Hz] ──┐
                                             ▼
 internal temp sensor ─► [health task 1 Hz] ─► StateStore (mutex, copy-out snapshot)
                                             │            │              │
 4 buttons ─► [ui task: debounce 10 ms,      │            ▼              ▼
              menu FSM, render 10 Hz] ◄──────┘     [modbus task]   OLED (I2C)
                                                   publishes snapshot → register
                                                   block atomically; serves
                                                   up to 4 sockets on :502
                                                         ▲
                                       Wi-Fi SoftAP ◄────┘  ◄── laptop running Artisan
```

### Code layout

```
main/app_main.cpp        wiring only: init order, task creation
components/
  core/                  pure C++, host-tested: temperature types, thermocouple frame decode,
                         filters, fault model, settings model + validation, menu state machine,
                         Modbus PDU/MBAP codec, register map, thermal policy, debouncer
  board/                 Kconfig board choice → board.hpp (the only place GPIOs appear)
  thermocouple/          spi_master driver for MAX31855 / MAX6675
  display/               i2c_master OLED driver (SSD1306/SH1106); screens and fonts live in core
  buttons/               GPIO polling → event queue
  net/                   SoftAP setup, Modbus TCP socket server
  storage/               NVS settings (versioned, CRC, validated)
  health/                thermal monitor, task liveness, reset/boot diagnostics
test/host/               doctest + CMake; runs natively on the Mac and in CI
artisan/                 Artisan setup guide (screenshots) + optional .aset
.github/workflows/       guards + host tests + firmware builds (mirrored by scripts/ci-local.sh)
scripts/                 ci-local.sh, guards.sh, flash.sh, serial_capture.py
.agents/                 agent rules, skills and hooks (entry point: AGENTS.md)
```

### Register map

Input registers via function code 4; function code 3 returns the same values. All writes are rejected.

| Reg | Meaning |
|---|---|
| 0 | BT °F × 10 (sInt16). **Fault → raw −10**, which Artisan's ÷10 divider turns into −1, its "no reading" value |
| 1 | BT °C × 10 (same fault rule) |
| 2 | Status bits: 0 probe-open, 1 probe-short, 2 stale, 3 overheat-warn, 4 overheat-crit, 5 low-battery, 6 bus-error |
| 3 | Chip die temp °C × 10 |
| 4 | Cold-junction temp °C × 10 (MAX6675: "not available" = 0x8000) |
| 5 | Battery % (0xFFFF until the battery feature lands) |
| 6–7 | Uptime seconds (uint32, reg 6 = high word) |
| 8 | Firmware version, (major << 8) \| minor |

Defined in `components/core/include/core/registers.hpp`; the codec (`core/modbus.hpp`) accepts any unit id and
answers function codes 3 and 4 only. Everything else gets a Modbus exception; malformed framing closes the
connection.

### Screens (MVP)

Drawn by `core::ui::render(Inputs, Framebuffer)` (pure, host-tested); the `ui` task renders every 100 ms
and the driver sends only the 128-byte pages that changed.

| Probe state | Screen |
|---|---|
| Valid reading (no fault, ≤ 2 s old, checked with `bt_valid(state, now_ms)`) | **Live**: whole °F in 46 px 7-segment digits, right-aligned; `HI`/`LO` outside −99…999 °F |
| No valid reading in the first 3 s after boot | **Starting** |
| Open probe | **PROBE?** + "Probe unplugged?" / "Check the plug" |
| Short to GND or VCC | **PROBE?** + "Probe wire short" / "Check the wires" |
| Bus error (no answer from the chip) | **PROBE?** + "No probe signal" / "Check probe board" |
| No fault but no fresh reading (sensor job stalled) | **PROBE?** + "No fresh reading" / "Please wait" |

A one-line top bar on every screen: "Artisan connected" (a Modbus client asked within 5 s), "Waiting for
Artisan" (a device joined the Wi-Fi), or, with nobody connected, "Join CoffeeProbe-XXXX" alternating with
"Password: coffeeprobe" every 3 s (the password only if the "Show password" setting is on). Overheat
(#24): Warn turns the top bar into an inverted "BOX TOO HOT: MOVE IT"; Critical shows a full-screen TOO HOT
before the protective sleep.

**Burn-in protection** (#20, `core/burn_in.hpp`): every 3 min the whole picture moves one pixel around a
loop of six positions (never up; host tests check no screen loses text at the edges), except in the menu.
PROBE? and Starting dim to the lowest contrast after 10 min with no reading and no button press; Live,
the menu and TOO HOT never dim, and a reading or a press brightens it at once.

### Settings menu (#23)

`core::menu::Menu` (pure, host-tested) driven by button presses; the `ui` task polls the buttons every
10 ms, redraws every 100 ms or at once after a press, and carries out what the menu returns.

- **Hold OK** (0.8 s) opens it, so a stray tap never hides the temperature mid-roast. UP/DOWN move
  (wrapping), OK opens an item, BACK closes. 30 s without a press closes it and drops an unsaved edit.
  TOO HOT closes it.
- Items: Probe chip, Grounded tip, Offset (±10.0 °F, 0.1 per press, 1.0 per hold), Wi-Fi channel (1/6/11),
  Wi-Fi power (Auto/Low/Medium/High, capped at the board's limit), Brightness (1–4), Flip screen, Show
  password, Diagnostics (three pages, UP/DOWN switch: firmware, uptime, box and chip temperatures, Wi-Fi
  devices; last reset, restarts, crashes, power dips, crash record saved; address, lowest free memory,
  smallest stack headroom), Factory reset.
- Editor: full screen, the value large, "OK save BACK cancel". Offset, brightness, flip and show password
  apply at once; probe chip, grounded tip and both Wi-Fi settings are set up at boot, so saving them asks
  "Restart to apply this change?" (a held OK restarts, so a double tap can't; BACK keeps it for the
  next power-on). Diagnostics: OK or BACK go back.
- Factory reset: from the menu (confirm with a held OK), or by holding OK from power-on for 5 s with an
  on-screen countdown (letting go cancels). Both erase the saved settings; the build options are the
  factory default for the probe chip and grounded mode (D-012).

Known limit: if the OLED's I2C bus dies, the panel keeps showing whatever it last received (nothing can
blank it without the bus). Artisan is unaffected. After any bus error the panel is switched off and only
switched on again once a whole current frame has been written, so an old number never flashes back.

## Reliability design

Failure mode → how we handle it:

- **Probe unplugged or broken**: MAX fault bits → fault value to Artisan + big `PROBE?` on the OLED within 1 s. Recovers by itself when fixed.
- **Grounded-tip probe** (common on drum roasters; makes the MAX31855 report a short-to-GND fault nonstop): a "Grounded probe" option accepts short-to-GND only (short-to-VCC and open still fault). It is a setting ("Grounded tip"); the Kconfig option `PROBE_TC_GROUNDED` is its factory default, off. In this mode a crushed cable shorting to ground can't be detected. Hardware: 10 nF across T+/T−.
- **Noise spikes**: median-of-3, then a 50 °C/s rate-of-change limit (`core::SensorPipeline`). Faults are debounced over 3 samples (one glitch never shows; an unplugged probe shows 500 ms after the first bad sample). A reading older than 2 s is stale; consumers check age themselves with `bt_valid(state, now_ms)`, so a frozen sensor task can't leave an old reading valid.
- **Replug bounce**: at boot, after a fault and after a gap longer than 2 s, nothing is published until three consecutive good samples agree (each step within the rate limit, 5 °C noise floor). Any fault during recovery restarts it.
- **Impossible frames**: MAX31855 values outside the K-type range (−270…+1372 °C) and MAX6675 `0x0000` (data line stuck low) count as bus errors. An SPI transfer stuck for 5 s panics (coredump, reboot).
- **MAX6675 timing**: reads ≥ 220 ms apart, paced with `vTaskDelayUntil` at 250 ms; the driver also waits out the rest of a worst-case conversion if a read comes early (MAX6675 230 ms, MAX31855 110 ms), since pulling CS low aborts a conversion. SPI clock ≤ 4 MHz, mode 0.
- **Wi-Fi blips**: AP mode only (never AP+STA, which would cause channel hops), fixed channel 1/6/11, country code set, WPA2-PSK, max 4 clients, TX power set per board.
- **Captive-portal trap**: no DNS server on the probe, so lookups fail fast and the OS says "no internet" instead of popping up a captive-portal sheet whose Cancel button disconnects.
- **Stale TCP connections**: 4 socket slots, 10 s idle timeout, lwIP keepalive (~11 s to detect a vanished laptop), oldest evicted when full. Evicting needs `accept()` first, so a 5th socket exists only until the oldest is closed in the same call.
- **Net task never blocks**: it runs every 20 ms (`vTaskDelayUntil`), `select()` polls with zero timeout, recv/send are non-blocking, at most 2 accepts per round and existing clients are served first. The idle task always gets CPU on the single core.
- **Torn reads**: every 250 ms the net task copies a StateStore snapshot into the register block; the server (same task) never reads StateStore directly. If the state lock times out, the fault value is published rather than a held one.
- **Crash or hang**: task watchdog on our tasks; the health task also checks that the sensor and net tasks keep counting their rounds (10 s without one → panic); panic → reboot, coredump to flash. The last reset's cause, restarts and crashes since power-on are on the Diagnostics screen (page 2) and in the boot log (#25).
- **Power dips during Wi-Fi TX bursts (~350 mA)**: brownout detector, power-dip count in RTC memory (`core::BootRecord`, D-013). After 2 dips since power-on (or 1, if RTC memory turns out not to survive a brownout), boot with Wi-Fi at 8 dBm and alternate the top line with an inverted "Check USB cable/power". After 30 min without a dip both clear and full power returns (R2). Hardware: 220–470 µF bulk capacitor.
- **OLED glitch from ESD or a brownout**: I2C timeouts + bus reset, and the setup sequence is resent every 30 s with a full redraw. Pixel shift and idle dimming against burn-in: see *Screens*.
- **Corrupted settings**: versioned struct + CRC; invalid → defaults. NVS only written on an explicit save. Factory reset by holding OK at boot.
- **Boot time**: splash < 300 ms, network up in ~2 s.

### Overheat

- Ambient comes from the MAX31855 cold junction: warn 50 °C, critical 65 °C (lower once a LiPo is fitted: charge 0–45 °C, discharge ≤ 60 °C).
- The C3 die sensor is a backstop: warn 90 °C, critical 105 °C. It runs several °C above ambient with Wi-Fi on. The MAX6675 has no cold-junction output, so with that chip only the die sensor is used.
- Thresholds use hysteresis + N consecutive samples.
- Warn → banner + status bit. Critical → write "TOO HOT, cooling" to the OLED, then deep sleep with a 60 s timer and button wake; auto-resume once cooled. Lowering TX power or CPU speed hardly helps, because the heat comes from the roaster.
- Typical SSD1306 panels are rated to about 70 °C, so the display is likely the weakest part. **All thresholds get tuned by testing next to the real roaster (#27).**

## Hardware / pin plan

**Generic ESP32-C3 DevKit (default profile)**: DevKitM-1, SuperMini and similar. Keeps the UART console
(GPIO20/21) for boards with a USB-UART chip, plus USB-Serial-JTAG (GPIO18/19) for native-USB boards.

| GPIO | Use | Note |
|---|---|---|
| 0 / 1 | I2C SDA / SCL | OLED. 32 kHz crystal pins, unused on DevKits |
| 6 | TC SCK | |
| 5 | TC MISO | |
| 10 | TC CS | **10 kΩ pull-up** keeps the MAX quiet during boot |
| 3 / 4 | UP / DOWN | GPIO 0–5 can wake from deep sleep |
| 7 | OK | |
| 9 | BACK | strapping pin = the onboard BOOT button, which doubles as BACK |

Off limits: GPIO11–17 (SPI flash), 18/19 (native USB), 20/21 (UART console). GPIO2/8 are strapping pins
and GPIO8 drives the RGB LED on many DevKits, so they stay unused.

**Seeed XIAO ESP32C3 (second profile, for battery power later)**: console on USB-Serial-JTAG only.

| XIAO | GPIO | Use | Note |
|---|---|---|---|
| D0 | 2 | Battery divider (later) | ADC1; strapping pin, so confirm the divider doesn't affect boot |
| D1 / D2 / D3 | 3 / 4 / 5 | UP / DOWN / OK | GPIO 0–5 can wake from deep sleep |
| D7 | 20 | BACK | the console is USB-only, which frees UART0 RX |
| D4 / D5 | 6 / 7 | I2C SDA / SCL | OLED |
| D8 | 8 | TC SCK | strapping pin; fine as an output after boot; must not be pulled low at reset |
| D6 | 21 | TC MISO | UART0 TX (unused). The boot ROM may drive it at reset; harmless while CS is high |
| D10 | 10 | TC CS | **10 kΩ pull-up** keeps the MAX quiet during boot |
| D9 | 9 | (unused) | strapping pin + BOOT button. Kept free so flashing always works (review of #35) |

The board is chosen by a Kconfig option + `sdkconfig.defaults.<board>` (see `docs/dev-setup.md` → *Board profiles*).

## Phases

| Milestone | Outcome |
|---|---|
| M0 Toolchain & bring-up | IDF v6 toolchain, build/flash loop, hardware identified |
| M1 Foundation | Component layout, reliability config, StateStore, host tests, CI |
| M2 Thermocouple | Accurate, fault-aware BT readings |
| M3 Wi-Fi + Modbus + Artisan | **Artisan plots live BT**. Critical path, proven before any UI work |
| M4 Display | OLED screens; °F always visible while reading |
| M5 Buttons, menu, settings | 4-button menu, persistent settings, factory reset |
| M6 Health | Thermal protection, watchdogs, diagnostics |
| M7 Hardening & v1.0 | Fault injection, soak + heat tests, release |

**MVP by 2026-11-28 (D-008)**: M0–M3 plus a minimal M4 (OLED: big °F, Wi-Fi/status, PROBE?). M4's full
screen set, M5, M6 and M7 come after the MVP. GitHub labels `MVP` / `post-MVP` mark each ticket.

Backlog: XIAO battery %, Artisan WebSocket mode with CHARGE/DROP buttons, browser flashing via ESP Web Tools.

## Verification

- **Every PR**: local CI (`scripts/ci-local.sh`) builds the firmware for each board profile and run the host tests.
- **Each milestone**: the on-device acceptance criteria in its issues.
- **Fault injection (#26)**: probe unplug/replug, TC short to ground, probe power cycle mid-recording, laptop sleep/wake, a second Wi-Fi client, laptop Wi-Fi toggle, heat gun to warn/critical, corrupt NVS, factory reset, power loss mid-save.
- **Soak (#27)**: 4 h of continuous Artisan recording with zero resets, stable minimum free heap, and no gaps in the curve.
