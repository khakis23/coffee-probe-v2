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
# Untracked files count too: e.g. a stray version.txt would change the version the firmware reports.
# ESP-IDF also reads a version.txt even if .gitignore hides it, so check for it by name.
if { [ -n "$(git -C "$ROOT" status --porcelain --untracked-files=all)" ] ||
     [ -e "$ROOT/version.txt" ]; } && ! $allow_dirty; then
    echo "Uncommitted or untracked files: a release must be built from a clean commit."
    echo "(--allow-dirty to test the script.)"
    exit 2
fi

idf_env() {   # idf_env <command string> [args...]: run in a clean environment with this IDF
    env -i HOME="$HOME" TERM=dumb LANG=en_US.UTF-8 \
        PATH="/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin" \
        IDF_PATH="$idf_path" bash -c '. "$IDF_PATH/export.sh" >/dev/null && '"$1" "${@:2}"
}
# The files are labelled with the IDF version, so make sure the install really is that version.
actual="$(idf_env 'idf.py --version' 2>/dev/null | tail -1)"
# Exact: not a release candidate, not commits after the tag, not a modified (-dirty) checkout.
if [ "$actual" != "ESP-IDF $idf" ]; then
    echo "Expected ESP-IDF $idf at $idf_path, found: ${actual:-nothing}"
    exit 2
fi

out="$HOME/Library/Caches/coffee-probe/release/$version/$idf"
rm -rf "$out"
mkdir -p "$out"

for board in generic xiao; do
    log="$out/build-$board.log"
    # A fresh build folder and sdkconfig from the committed defaults only: never a developer's
    # cached build, whose sdkconfig may hold settings changed by hand.
    dir="$out/build-$board"
    rm -rf "$dir"
    idf_env 'cd "$0" && idf.py -B "$1" -DSDKCONFIG="$1/sdkconfig" -DPROBE_BOARD="$2" build' \
        "$ROOT" "$dir" "$board" >"$log" 2>&1 || { echo "Build failed for $board, see $log"; exit 1; }
    # Same rules as CI: reliability settings on, no diagnostics, no fault test.
    "$ROOT/scripts/guards.sh" --sdkconfig "$dir/sdkconfig" >>"$log" 2>&1 || {
        echo "Guard failed for $board's sdkconfig, see $log"; exit 1; }
    bin="$out/coffee-probe-$board-$version.bin"
    # flash_args lists each part with its address; merging writes them as one image for 0x0.
    idf_env 'cd "$0" && python -m esptool --chip esp32c3 "$2" -o "$1" @flash_args' \
        "$dir" "$bin" "$merge" >>"$log" 2>&1 || { echo "Merging failed for $board, see $log"; exit 1; }
    cp "$dir/coffee_probe.elf" "$out/coffee-probe-$board-$version.elf"
    echo "Built $bin"
done

(cd "$out" && shasum -a 256 ./*.bin ./*.elf >SHA256SUMS)
echo "Release files ($idf, $version) in $out"
