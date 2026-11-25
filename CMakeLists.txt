# CoffeeProbe firmware — ESP-IDF project root.
# Build with `idf.py build` or open this folder in CLion (see docs/dev-setup.md).
cmake_minimum_required(VERSION 3.16)

# Board profile: generic DevKit by default; `idf.py -DPROBE_BOARD=xiao build` selects XIAO.
# It picks which sdkconfig.defaults.<board> file is layered on top of the shared sdkconfig.defaults.
# That file sets both the console config and the CONFIG_PROBE_BOARD_* Kconfig choice, so the
# firmware's pin map and the console can't disagree.
#
# Defaults are only applied when `sdkconfig` is first generated. To switch boards, delete
# `sdkconfig` (and ideally `build/`) first, otherwise the old board's settings stay.
set(PROBE_BOARD "generic" CACHE STRING "Board profile: xiao or generic")
set_property(CACHE PROBE_BOARD PROPERTY STRINGS xiao generic)
if(NOT PROBE_BOARD MATCHES "^(xiao|generic)$")
    message(FATAL_ERROR "PROBE_BOARD must be 'xiao' or 'generic', got '${PROBE_BOARD}'")
endif()
# Must be set before project(): IDF reads it while generating sdkconfig.
set(SDKCONFIG_DEFAULTS "sdkconfig.defaults;sdkconfig.defaults.${PROBE_BOARD}")
# Optional extra defaults layered last, e.g. a scratch file with CONFIG_PROBE_BRINGUP_DIAG=y:
# `idf.py -DPROBE_EXTRA_DEFAULTS=/tmp/diag.defaults build` (fresh sdkconfig only, as above).
set(PROBE_EXTRA_DEFAULTS "" CACHE STRING "Extra sdkconfig defaults file(s), ;-separated")
if(PROBE_EXTRA_DEFAULTS)
    list(APPEND SDKCONFIG_DEFAULTS ${PROBE_EXTRA_DEFAULTS})
endif()
# A one-off diagnostics overlay must not come back when sdkconfig is regenerated.
unset(PROBE_EXTRA_DEFAULTS CACHE)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Supported ESP-IDF versions (#2): CI builds and tests on v5.3.1 and v6.0.3 only. Another version
# may build but has never been checked against the reliability rules, so refuse it.
if(NOT ((IDF_VERSION_MAJOR EQUAL 5 AND IDF_VERSION_MINOR EQUAL 3) OR
        (IDF_VERSION_MAJOR EQUAL 6 AND IDF_VERSION_MINOR EQUAL 0)))
    message(FATAL_ERROR "ESP-IDF ${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}.${IDF_VERSION_PATCH} "
                        "is not supported. Use v5.3.x or v6.0.x (docs/dev-setup.md).")
endif()

project(coffee_probe)

# After project(), every sdkconfig option is also a CMake variable. Catch a stale sdkconfig
# before it pairs a board's pin map with the wrong console.
if(PROBE_BOARD STREQUAL "xiao" AND
   (NOT CONFIG_PROBE_BOARD_XIAO_C3 OR NOT CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG))
    message(FATAL_ERROR "PROBE_BOARD=xiao but sdkconfig is for another board or console. "
                        "Delete sdkconfig (and build/) and rebuild.")
elseif(PROBE_BOARD STREQUAL "generic" AND
       (NOT CONFIG_PROBE_BOARD_GENERIC_C3 OR NOT CONFIG_ESP_CONSOLE_UART_DEFAULT))
    message(FATAL_ERROR "PROBE_BOARD=generic but sdkconfig is for another board or console. "
                        "Delete sdkconfig (and build/) and rebuild.")
endif()

if(CONFIG_PROBE_FAULT_TEST_HANG OR CONFIG_PROBE_FAULT_TEST_CRASH)
    message(WARNING "Building a FAULT TEST firmware: it freezes or crashes on purpose 30 s after "
                    "power-on (#26). Never give it to staff. To go back: delete sdkconfig and build "
                    "without PROBE_EXTRA_DEFAULTS.")
endif()

if(CONFIG_PROBE_BRINGUP_DIAG)
    message(WARNING "Building BRING-UP DIAGNOSTICS firmware, not the CoffeeProbe app. "
                    "To go back: delete sdkconfig and build without PROBE_EXTRA_DEFAULTS.")
endif()
