#!/usr/bin/env bash
# Build the release images (#28): for each board profile, one merged .bin that is written at
# address 0x0 (bootloader + partition table + app in one file), plus the matching .elf for reading
# crash dumps, and SHA256SUMS.
#
#   scripts/release.sh                  # IDF v5.3.1
#   scripts/release.sh --idf v6.0.3
#   scripts/release.sh --allow-dirty    # test the script with uncommitted changes (not for a release)
#
# Output: ~/Library/Caches/coffee-probe/release/<version>/<idf>/, where <version> is `git describe`, the
# same string the firmware reports on Diagnostics. Attach those files to the GitHub release.
#
# Note: the merged image covers 0x0 up to the end of the app, so it also blanks the settings (NVS)
# and RF calibration areas in between. A probe loaded with it starts with factory settings; RF
# calibration redoes itself on the first boot. docs/flashing.md says so.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd -P)"
idf=v5.3.1 allow_dirty=false
while [ $# -gt 0 ]; do
    case "$1" in
        --idf) idf="$2"; shift 2 ;;
        --allow-dirty) allow_dirty=true; shift ;;
        -h|--help) sed -n '2,16p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "Unknown option $1"; exit 2 ;;
    esac
done
case "$idf" in
    # esptool 4 (IDF v5.3) spells it merge_bin; esptool 5 (IDF v6) merge-bin.
    v5.3.1) idf_path="$HOME/esp/esp-idf" merge=merge_bin ;;
    v6.0.3) idf_path="$HOME/esp/esp-idf-v6.0" merge=merge-bin ;;
    *) echo "Unknown IDF version $idf (v5.3.1 or v6.0.3)"; exit 2 ;;
esac

version="$(git -C "$ROOT" describe --tags --always --dirty)"
if [[ "$version" == *-dirty ]] && ! $allow_dirty; then
    echo "Uncommitted changes: a release must be built from a commit. (--allow-dirty to test.)"
    exit 2
fi

out="$HOME/Library/Caches/coffee-probe/release/$version/$idf"
rm -rf "$out"
mkdir -p "$out"

for board in generic xiao; do
    log="$out/build-$board.log"
    "$ROOT/scripts/flash.sh" --build-only --board "$board" --idf "$idf" >"$log" 2>&1 || {
        echo "Build failed for $board, see $log"; exit 1; }
    dir="$(sed -n 's/^Build folder: //p' "$log")"
    # Same rules as CI: reliability settings on, no diagnostics, no fault test.
    "$ROOT/scripts/guards.sh" --sdkconfig "$dir/sdkconfig" >>"$log" 2>&1 || {
        echo "Guard failed for $board's sdkconfig, see $log"; exit 1; }
    bin="$out/coffee-probe-$board-$version.bin"
    env -i HOME="$HOME" TERM=dumb LANG=en_US.UTF-8 \
        PATH="/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin" \
        IDF_PATH="$idf_path" bash -c '
            set -e
            . "$IDF_PATH/export.sh" >/dev/null
            cd "$0"
            # flash_args lists each part with its address; merge_bin writes them as one image.
            python -m esptool --chip esp32c3 "$2" -o "$1" @flash_args
        ' "$dir" "$bin" "$merge" >>"$log" 2>&1 || { echo "Merging failed for $board, see $log"; exit 1; }
    cp "$dir/coffee_probe.elf" "$out/coffee-probe-$board-$version.elf"
    echo "Built $bin"
done

(cd "$out" && shasum -a 256 ./*.bin ./*.elf >SHA256SUMS)
echo "Release files ($idf, $version) in $out"
