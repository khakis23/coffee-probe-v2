#!/usr/bin/env bash
# Local CI: the same checks as .github/workflows/ci.yml, run on this Mac.
#
#   scripts/ci-local.sh            # everything
#   scripts/ci-local.sh --clean    # wipe the CI build folder first (slow, but like a fresh CI run)
#   scripts/ci-local.sh host xiao  # only jobs whose name contains "host" or "xiao"
#
# Jobs:
#   guards                scripts/guards.sh: mechanical checks of the hard rules (AGENTS.md)
#   host                  host unit tests (test/host, doctest) with the Mac's own compiler
#   <board>-<idf>[-diag]  firmware build in its own <out>/<job>/ folder with its own sdkconfig,
#                         so the normal build/ and sdkconfig are never touched
#
# Output goes to ~/Library/Caches/coffee-probe/ci (override with CI_OUT=...), NOT the repo: the repo
# is in iCloud Drive, which makes "file 2" conflict copies inside busy build folders.
#
# IDF v6 jobs are "allowed to fail" (reported, but don't fail the run), matching continue-on-error
# in the GitHub workflow, until v6 support lands. Logs: <out>/logs/<job>.log.
#
# Exit code: 0 if every required job passed, 1 otherwise.
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${CI_OUT:-$HOME/Library/Caches/coffee-probe/ci}"
LOGS="$OUT/logs"

# IDF version -> install location on this machine (see docs/dev-setup.md).
idf_path_for() {
    case "$1" in
        v5.3.1) echo "$HOME/esp/esp-idf" ;;
        v6.0.3) echo "$HOME/esp/esp-idf-v6.0" ;;
        *) return 1 ;;
    esac
}

# Keep in sync with the firmware matrix in .github/workflows/ci.yml.
FIRMWARE_JOBS=(
    "xiao v5.3.1 false"
    "generic v5.3.1 false"
    "xiao v6.0.3 false"
    "generic v6.0.3 false"
    "xiao v5.3.1 true"
)

clean=false
filters=()
for arg in "$@"; do
    case "$arg" in
        --clean) clean=true ;;
        -h|--help) sed -n '2,22p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) filters+=("$arg") ;;
    esac
done

selected() {
    [ ${#filters[@]} -eq 0 ] && return 0
    local f
    for f in "${filters[@]}"; do [[ "$1" == *"$f"* ]] && return 0; done
    return 1
}

$clean && rm -rf "$OUT"
mkdir -p "$LOGS"

results=()   # "name|PASS/FAIL/ALLOWED-FAIL|seconds"
required_failed=false

run_job() {   # run_job <name> <allowed_to_fail:true/false> <command...>
    local name="$1" allowed="$2"; shift 2
    local log="$LOGS/$name.log" start=$SECONDS status
    printf '%-24s ' "$name"
    if "$@" >"$log" 2>&1; then
        status=PASS
    elif $allowed; then
        status=ALLOWED-FAIL
    else
        status=FAIL
        required_failed=true
    fi
    local secs=$((SECONDS - start))
    printf '%-13s %4ss\n' "$status" "$secs"
    [ "$status" != PASS ] && tail -n 15 "$log" | sed 's/^/    | /'
    results+=("$name|$status|$secs")
}

host_tests() {
    cmake -S "$ROOT/test/host" -B "$OUT/host" &&
    cmake --build "$OUT/host" &&
    ctest --test-dir "$OUT/host" --output-on-failure
}

firmware_build() {   # firmware_build <board> <idf> <diag> <job dir>
    local board="$1" idf="$2" diag="$3" dir="$4" idf_path extra=""
    idf_path="$(idf_path_for "$idf")" || { echo "Unknown IDF version $idf"; return 1; }
    [ -f "$idf_path/export.sh" ] || { echo "IDF $idf not installed at $idf_path"; return 1; }
    mkdir -p "$dir"

    # sdkconfig.defaults only apply when sdkconfig is generated, so regenerate it whenever a
    # defaults file is newer (otherwise an edited default would silently be ignored here).
    if [ -f "$dir/sdkconfig" ] && [ -n "$(find "$ROOT" -maxdepth 1 -name 'sdkconfig.defaults*' -newer "$dir/sdkconfig")" ]; then
        rm -f "$dir/sdkconfig"
    fi
    if [ "$diag" = true ]; then
        printf 'CONFIG_PROBE_BRINGUP_DIAG=y\n' >"$dir/diag.defaults"
        extra="-DPROBE_EXTRA_DEFAULTS=$dir/diag.defaults"
    fi

    # Clean environment (like CI's fresh container): an export.sh already sourced in the
    # caller's shell, e.g. for the other IDF version, would otherwise leak its paths and Python env.
    env -i HOME="$HOME" TERM="${TERM:-dumb}" LANG=en_US.UTF-8 \
        PATH="/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin" \
        IDF_PATH="$idf_path" bash -c '
            set -e
            . "$IDF_PATH/export.sh" >/dev/null
            cd "$0"
            idf.py -B "$1" -DSDKCONFIG="$1/sdkconfig" -DPROBE_BOARD="$2" $3 build
        ' "$ROOT" "$dir" "$board" "$extra"
}

echo "Local CI in $ROOT (output: $OUT)"
echo "Commit: $(git -C "$ROOT" rev-parse --short HEAD)$(git -C "$ROOT" diff --quiet HEAD -- || echo ' + uncommitted changes')"
echo

selected guards && run_job guards false "$ROOT/scripts/guards.sh"
selected host && run_job host false host_tests

for job in "${FIRMWARE_JOBS[@]}"; do
    read -r board idf diag <<<"$job"
    name="$board-$idf"; [ "$diag" = true ] && name="$name-diag"
    selected "$name" || continue
    allowed=false; [[ "$idf" == v6* ]] && allowed=true
    run_job "$name" "$allowed" firmware_build "$board" "$idf" "$diag" "$OUT/$name"
done

echo
if [ ${#results[@]} -eq 0 ]; then
    echo "No jobs matched: ${filters[*]}"; exit 1
elif $required_failed; then
    echo "LOCAL CI FAILED (logs in $LOGS/)"; exit 1
else
    echo "LOCAL CI PASSED"
fi
