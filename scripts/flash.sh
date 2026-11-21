#!/usr/bin/env bash
# Build and flash the firmware to a plugged-in board, without an interactive terminal.
#
#   scripts/flash.sh                    # XIAO profile, normal app, auto-detect the port
#   scripts/flash.sh --board generic    # DevKit profile
#   scripts/flash.sh --diag             # bring-up diagnostics firmware instead of the app
#   scripts/flash.sh --port /dev/cu.usbmodem1101 --idf v6.0.3
#   scripts/flash.sh --build-only       # build, print the folder, don't flash
#
# Builds in ~/Library/Caches/coffee-probe/flash/<board>-<idf>[-diag] (outside iCloud; see
# docs/dev-setup.md) with its own sdkconfig, so it never touches build/ or sdkconfig.
# Then read the serial output with scripts/serial_capture.py.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
board=xiao idf=v5.3.1 diag=false port="" build_only=false
while [ $# -gt 0 ]; do
    case "$1" in
        --board) board="$2"; shift 2 ;;
        --idf) idf="$2"; shift 2 ;;
        --diag) diag=true; shift ;;
        --port) port="$2"; shift 2 ;;
        --build-only) build_only=true; shift ;;
        -h|--help) sed -n '2,14p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "Unknown option $1"; exit 2 ;;
    esac
done

case "$idf" in
    v5.3.1) idf_path="$HOME/esp/esp-idf" ;;
    v6.0.3) idf_path="$HOME/esp/esp-idf-v6.0" ;;
    *) echo "Unknown IDF version $idf (v5.3.1 or v6.0.3)"; exit 2 ;;
esac

if ! $build_only && [ -z "$port" ]; then
    # Native USB boards (XIAO, SuperMini) show up as usbmodem; DevKits with a USB-UART chip as
    # usbserial / SLAB / wchusbserial.
    ports=()
    for p in /dev/cu.usbmodem* /dev/cu.usbserial-* /dev/cu.SLAB_USBtoUART* /dev/cu.wchusbserial*; do
        [ -e "$p" ] && ports+=("$p")
    done
    if [ ${#ports[@]} -eq 0 ]; then
        echo "NO BOARD: no USB serial port found. Ask Tyler to plug the board in (see the device skill)."
        exit 3
    elif [ ${#ports[@]} -gt 1 ]; then
        echo "Several serial ports found, pass --port: ${ports[*]}"
        exit 2
    fi
    port="${ports[0]}"
fi

name="$board-$idf"; $diag && name="$name-diag"
dir="$HOME/Library/Caches/coffee-probe/flash/$name"
mkdir -p "$dir"
# Board defaults only apply to a fresh sdkconfig: regenerate it whenever a defaults file changed.
if [ -f "$dir/sdkconfig" ] && [ -n "$(find "$ROOT" -maxdepth 1 -name 'sdkconfig.defaults*' -newer "$dir/sdkconfig")" ]; then
    rm -f "$dir/sdkconfig"
fi
extra=""
if $diag; then
    printf 'CONFIG_PROBE_BRINGUP_DIAG=y\n' >"$dir/diag.defaults"
    extra="-DPROBE_EXTRA_DEFAULTS=$dir/diag.defaults"
fi
target=flash; $build_only && target=build
port_arg=""; [ -n "$port" ] && port_arg="-p $port"

echo "Building $name${port:+ and flashing $port}"
# Clean environment so an export.sh sourced for the other IDF version can't leak in.
env -i HOME="$HOME" TERM=dumb LANG=en_US.UTF-8 \
    PATH="/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin" \
    IDF_PATH="$idf_path" bash -c '
        set -e
        . "$IDF_PATH/export.sh" >/dev/null
        cd "$0"
        idf.py -B "$1" -DSDKCONFIG="$1/sdkconfig" -DPROBE_BOARD="$2" $3 $4 $5
    ' "$ROOT" "$dir" "$board" "$extra" "$port_arg" "$target"

echo "Build folder: $dir"
$build_only || echo "Flashed $port. Read output: scripts/serial_capture.py --port $port --reset --seconds 15"
