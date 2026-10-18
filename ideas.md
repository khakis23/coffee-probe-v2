# User

## Functional
- The non-technical user should be able to connect to the ESP coffee probe from Artisan Roasting software from their computer.
- The Probe should display useful information (debugging, connection, etc.) on the tiny OLED when not reading temperature.
- The tiny OLED should always show a temperature readout in ºF when actively reading temperature.
- The user should be able to adjust settings on the tiny OLED using simple 4-button interface.


## Non-functional
- MOST IMPORTANT REQUIREMENT: **Must be super reliable for non-technical users!!!**
- Users should not be required to use the ESP interface (besides booting) unless they are debugging or adjusting settings (mainly for advanced users only).


# System

## Contrains
- No WiFi Router Access!
  - Must transmit wirelessly to Artisan Coffee software
- It is preferable that no other software is required on the user’s computer other than the latest public version of Artisan.

## Requirements
- Potentially (as a later feature), a Seeed Studio Xiao ESP32C3 would be used for battery support which battery percentage should be displayed on the tiny old.
- The system should be built using C++, ESP-IDF for on-device building.
- The system will physically be in a warm environment, overheating protection should be enabled, and a warning should be displayed on the tiny OLED (if possible) when the ESP is getting close to shutting off to protect the SoC.

### Hardware
- Main board: ESP32C3-dev (should work with all C3 board types!)
- MAX6675 / MAX31855
  - K-type probe
- Tiny OLED: E096QJ-H30-40 (I THINK 128x64)
- Buttons/wires/caps/res/etc.

