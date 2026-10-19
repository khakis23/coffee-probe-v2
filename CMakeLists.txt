# CoffeeProbe firmware — ESP-IDF project root.
# Build with `idf.py build` or open this folder in CLion (see docs/dev-setup.md).
cmake_minimum_required(VERSION 3.16)

# Board profile: `idf.py -DPROBE_BOARD=generic build` (default: xiao).
# It picks which sdkconfig.defaults.<board> file is layered on top of the shared sdkconfig.defaults.
# That file sets both the console config and the CONFIG_PROBE_BOARD_* Kconfig choice, so the
# firmware's pin map and the console can't disagree.
#
# Defaults are only applied when `sdkconfig` is first generated. To switch boards, delete
# `sdkconfig` (and ideally `build/`) first, otherwise the old board's settings stay.
set(PROBE_BOARD "xiao" CACHE STRING "Board profile: xiao or generic")
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

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(coffee_probe)

# After project(), every sdkconfig option is also a CMake variable. Catch a stale sdkconfig
# left over from the other board before it produces firmware with the wrong pin map.
if(PROBE_BOARD STREQUAL "xiao" AND NOT CONFIG_PROBE_BOARD_XIAO_C3)
    message(FATAL_ERROR "PROBE_BOARD=xiao but sdkconfig is for another board. "
                        "Delete sdkconfig (and build/) and rebuild.")
elseif(PROBE_BOARD STREQUAL "generic" AND NOT CONFIG_PROBE_BOARD_GENERIC_C3)
    message(FATAL_ERROR "PROBE_BOARD=generic but sdkconfig is for another board. "
                        "Delete sdkconfig (and build/) and rebuild.")
endif()
