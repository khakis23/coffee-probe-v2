# CoffeeProbe v2 — Architecture

## Context

v1 (repo `khakis23/CoffeeProbe`, left as is) was an Arduino BLE sketch plus a custom Python desktop app. v2 has two hard requirements:

1. **Stock Artisan is the only software on the laptop.** Artisan has no generic BLE input, so BLE is out.
2. **Super reliable for a non-technical user**, in a warm roasting room, with **no router**.

Scope: one unit, one channel (Bean Temp). The laptop may lose Wi-Fi internet while roasting (Ethernet still works).

Work is tracked as GitHub issues labelled `HUMAN` or `CLAUDE`; see [CONTRIBUTING.md](../CONTRIBUTING.md).

## Key decisions

| Topic | Decision | Why |
|---|---|---|
| Link to Artisan | **The probe runs its own Wi-Fi network (SoftAP) + Modbus TCP**, which Artisan polls at fixed `192.168.4.1:502`. | Built into stock Artisan, and the most widely used DIY path. Artisan's WebSocket device needs JSON node setup that's hard for a non-technical user. MQTT (Artisan 4.2) needs a broker. |
| Modbus server | **Our own small read-only server** (function codes 3 and 4), not `esp-modbus` | About 200–300 lines; we control socket timeouts, keepalive and connection slots; no dependency risk on IDF v6; fully host-testable. |
| ESP-IDF | **v6.0.x** (v5.3 reaches end of life around Jan 2027); fall back to v5.5.x if needed | Latest stable for a new project. Use only the new driver APIs: `i2c_master`, `spi_master`, `temperature_sensor`, `adc_oneshot`. |
| Display | `esp_lcd` panel I/O over `i2c_master` + **our own 1 KB framebuffer and fonts**; SSD1306 and SH1106 both supported | The OLED part number is unverified (#3 identifies it). Small, testable code. |
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
  display/               i2c_master + esp_lcd, framebuffer, fonts, screens
  buttons/               GPIO polling → event queue
  net/                   SoftAP setup, Modbus TCP socket server
  storage/               NVS settings (versioned, CRC, validated)
  health/                thermal monitor, task liveness, reset/boot diagnostics
test/host/               doctest + CMake; runs natively in CLion and in CI
artisan/                 Artisan setup guide (screenshots) + optional .aset
.github/workflows/       firmware build (espressif/idf container) + host tests
```

### Register map

Input registers via function code 4; function code 3 returns the same values. All writes are rejected.

| Reg | Meaning |
|---|---|
| 0 | BT °F × 10 (sInt16). **Fault → raw −10**, which Artisan's ÷10 divider turns into −1, its "no reading" value |
| 1 | BT °C × 10 (same fault rule) |
| 2 | Status bits: probe-open, probe-short, stale, overheat-warn, overheat-crit, low-battery |
| 3 | Chip die temp °C × 10 |
| 4 | Cold-junction temp °C × 10 (MAX6675: "not available" value) |
| 5 | Battery % (0xFFFF until the battery feature lands) |
| 6–7 | Uptime seconds (uint32), firmware version |

## Reliability design

Failure mode → how we handle it:

- **Probe unplugged or broken**: MAX fault bits → fault value to Artisan + big `PROBE?` on the OLED within 1 s. Recovers by itself when fixed.
- **Grounded-tip probe** (common on drum roasters; makes the MAX31855 report a short-to-GND fault nonstop): a "Grounded probe" setting ignores short faults. Hardware: 10 nF across T+/T−.
- **Noise spikes**: median-of-3 + rate-of-change limit. A reading older than 2 s sets the stale flag.
- **MAX6675 timing**: reads ≥ 220 ms apart, paced with `vTaskDelayUntil`. SPI clock ≤ 4 MHz, mode 0.
- **Wi-Fi blips**: AP mode only (never AP+STA, which would cause channel hops), fixed channel 1/6/11, country code set, WPA2-PSK, max 4 clients, TX power set per board.
- **Captive-portal trap**: no DNS server on the probe, so lookups fail fast and the OS says "no internet" instead of popping up a captive-portal sheet whose Cancel button disconnects.
- **Stale TCP connections**: 4 socket slots, ~10 s idle timeout, lwIP keepalive, oldest evicted when full.
- **Torn reads**: the snapshot is copied into the register block under a lock; the server never reads StateStore directly.
- **Crash or hang**: task watchdog on our tasks, liveness counters for the server task, panic → reboot, coredump to flash, reset reason + boot count + last-crash flag on the Diagnostics screen.
- **Power dips during Wi-Fi TX bursts (~350 mA)**: brownout detector, brownout count in RTC memory. After repeated brownouts, boot with low TX power and show "Check USB cable/power". Hardware: 220–470 µF bulk capacitor.
- **OLED glitch from ESD or a brownout**: I2C timeouts + bus reset, and the init sequence is resent every 30 s. Pixel shift and dimming prevent burn-in.
- **Corrupted settings**: versioned struct + CRC; invalid → defaults. NVS only written on an explicit save. Factory reset by holding OK at boot.
- **Boot time**: splash < 300 ms, network up in ~2 s.

### Overheat

- Ambient comes from the MAX31855 cold junction: warn 50 °C, critical 65 °C (lower once a LiPo is fitted: charge 0–45 °C, discharge ≤ 60 °C).
- The C3 die sensor is a backstop: warn 90 °C, critical 105 °C. It runs several °C above ambient with Wi-Fi on. The MAX6675 has no cold-junction output, so with that chip only the die sensor is used.
- Thresholds use hysteresis + N consecutive samples.
- Warn → banner + status bit. Critical → write "TOO HOT, cooling" to the OLED, then deep sleep with a 60 s timer and button wake; auto-resume once cooled. Lowering TX power or CPU speed hardly helps, because the heat comes from the roaster.
- Typical SSD1306 panels are rated to about 70 °C, so the display is likely the weakest part. **All thresholds get tuned by testing next to the real roaster (#27).**

## Hardware / pin plan

XIAO ESP32C3 (default board profile):

| XIAO | GPIO | Use | Note |
|---|---|---|---|
| D0 | 2 | Battery divider (later) | ADC1; strapping pin, so confirm the divider doesn't affect boot |
| D1 / D2 / D3 | 3 / 4 / 5 | UP / DOWN / OK | GPIO 0–5 can wake from deep sleep |
| D7 | 20 | BACK | XIAO console is USB-Serial-JTAG only, which frees UART0 RX |
| D4 / D5 | 6 / 7 | I2C SDA / SCL | OLED |
| D8 | 8 | TC SCK | strapping pin; fine as an output after boot |
| D9 | 9 | TC MISO | strapping pin + BOOT button; MAX output is high-impedance while CS is high |
| D10 | 10 | TC CS | **10 kΩ pull-up** keeps the MAX quiet during boot |
| D6 | 21 | spare | not a button: the boot ROM drives it at reset |

A "Generic C3 DevKit" profile keeps the UART console. The board is chosen by a Kconfig option + `sdkconfig.defaults.<board>`.

## Phases

| Milestone | Outcome |
|---|---|
| M0 Toolchain & bring-up | IDF v6 toolchain, CLion build/flash loop, hardware identified |
| M1 Foundation | Component layout, reliability config, StateStore, host tests, CI |
| M2 Thermocouple | Accurate, fault-aware BT readings |
| M3 Wi-Fi + Modbus + Artisan | **Artisan plots live BT**. Critical path, proven before any UI work |
| M4 Display | OLED screens; °F always visible while reading |
| M5 Buttons, menu, settings | 4-button menu, persistent settings, factory reset |
| M6 Health | Thermal protection, watchdogs, diagnostics |
| M7 Hardening & v1.0 | Fault injection, soak + heat tests, release |

Backlog: XIAO battery %, Artisan WebSocket mode with CHARGE/DROP buttons, browser flashing via ESP Web Tools.

## Verification

- **Every PR**: CI builds the firmware for each board profile and runs the host tests.
- **Each milestone**: the on-device acceptance criteria in its issues.
- **Fault injection (#26)**: probe unplug/replug, TC short to ground, probe power cycle mid-recording, laptop sleep/wake, a second Wi-Fi client, laptop Wi-Fi toggle, heat gun to warn/critical, corrupt NVS, factory reset, power loss mid-save.
- **Soak (#27)**: 4 h of continuous Artisan recording with zero resets, stable minimum free heap, and no gaps in the curve.
