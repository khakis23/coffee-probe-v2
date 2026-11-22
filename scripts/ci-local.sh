#!/usr/bin/env bash
# Local CI: the project's only CI gate (D-009), run on this Mac. Same jobs as the manual-only
# .github/workflows/ci.yml.
#
#   scripts/ci-local.sh            # everything
#   scripts/ci-local.sh --clean    # wipe the CI build folder first (slow, but like a fresh CI run)
#   scripts/ci-local.sh host xiao  # only jobs whose name contains "host" or "xiao"
#
# Jobs:
#   guards                scripts/guards.sh: mechanical checks of the hard rules (AGENTS.md)
#   host                  host unit tests (test/host, doctest) with the Mac's own compiler
#   <board>-<idf>[-diag]  firmware build in its own <out>/<job>/ folder with its own sdkconfig,
#                         checked after each build; the normal build/ and sdkconfig are untouched
#
# Output goes to ~/Library/Caches/coffee-probe/ci/<checkout-hash> (override with CI_OUT=...).
# Keep builds outside iCloud Drive, which makes "file 2" conflict copies in busy folders.
#
# IDF v6 jobs are "allowed to fail" (reported, but don't fail the run), matching continue-on-error
# in the GitHub workflow, until v6 support lands. Logs: <out>/logs/<job>.log.
#
# Exit code: 0 if every required job passed, 1 on job failure, 2 on invalid arguments or output setup.
set -uo pipefail

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
    "generic v5.3.1 false"
    "xiao v5.3.1 false"
    "generic v6.0.3 false"
    "xiao v6.0.3 false"
    "generic v5.3.1 true"
    "generic v6.0.3 true"
)

clean=false
help=false
filters=()
for arg in "$@"; do
    case "$arg" in
        --clean) clean=true ;;
        -h|--help) help=true ;;
        --*) echo "Unknown option $arg" >&2; exit 2 ;;
        *) filters+=("$arg") ;;
    esac
done

job_names=(guards host)
for job in "${FIRMWARE_JOBS[@]}"; do
    read -r board idf diag <<<"$job"
    name="$board-$idf"; [ "$diag" = true ] && name="$name-diag"
    job_names+=("$name")
done
for f in "${filters[@]+"${filters[@]}"}"; do
    matched=false
    for name in "${job_names[@]}"; do
        if [[ "$name" == *"$f"* ]]; then
            matched=true
            break
        fi
    done
    $matched || { printf "No job matches '%s'\n" "$f" >&2; exit 2; }
done
if $help; then
    sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'
    exit 0
fi

selected() {
    [ ${#filters[@]} -eq 0 ] && return 0
    local f
    for f in "${filters[@]}"; do [[ "$1" == *"$f"* ]] && return 0; done
    return 1
}

ROOT="$(cd "$(dirname "$0")/.." && pwd -P)" || exit 2
if [ "${CI_OUT+x}" ]; then
    [ -n "$CI_OUT" ] || { echo "CI_OUT is empty" >&2; exit 2; }
    out_path="$CI_OUT"
else
    checkout="$(printf %s "$ROOT" | shasum)" || exit 2
    out_path="$HOME/Library/Caches/coffee-probe/ci/${checkout:0:10}"
fi
OUT="$(python3 -c 'import os,sys; print(os.path.realpath(sys.argv[1]))' "$out_path")" || exit 2
LOGS="$OUT/logs"

if $clean; then
    home_real="$(cd "$HOME" && pwd -P)" || exit 2
    if [[ "$OUT" != /* || "$OUT" == / || "$OUT" == "$home_real" ||
          "$OUT/" == "$ROOT/"* || "$ROOT/" == "$OUT/"* ]]; then
        echo "Refusing to clean unsafe output directory: $OUT" >&2
        exit 2
    fi
    rm -rf "$OUT" || exit 2
fi
mkdir -p "$LOGS" || exit 2

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
    if [ "$status" != PASS ]; then
        tail -n 15 "$log" | sed 's/^/    | /'
    fi
}

host_tests() {
    cmake -S "$ROOT/test/host" -B "$OUT/host" &&
    cmake --build "$OUT/host" &&
    ctest --test-dir "$OUT/host" --output-on-failure
}

defaults_fingerprint() (
    local diag="$1" dir="$2" file
    export LC_ALL=C
    cd "$ROOT" || return 1
    {
        for file in sdkconfig.defaults*; do
            shasum "$file" || return 1
        done
        if [ "$diag" = true ]; then
            shasum <"$dir/diag.defaults" || return 1
        fi
    } | shasum
)

firmware_build() {   # firmware_build <board> <idf> <diag> <job dir>
    local board="$1" idf="$2" diag="$3" dir="$4" idf_path fingerprint
    local extra=() config_args=()
    idf_path="$(idf_path_for "$idf")" || { echo "Unknown IDF version $idf"; return 1; }
    [ -f "$idf_path/export.sh" ] || { echo "IDF $idf not installed at $idf_path"; return 1; }
    mkdir -p "$dir" || return 1
    if [ "$diag" = true ]; then
        printf 'CONFIG_PROBE_BRINGUP_DIAG=y\n' >"$dir/diag.defaults" || return 1
        extra+=("-DPROBE_EXTRA_DEFAULTS=$dir/diag.defaults")
        config_args+=(--diag)
    fi

    # Defaults only apply when sdkconfig is generated, so compare their contents before building.
    fingerprint="$(defaults_fingerprint "$diag" "$dir")" || return 1
    if [ ! -f "$dir/.defaults-fingerprint" ] || [ "$(<"$dir/.defaults-fingerprint")" != "$fingerprint" ]; then
        rm -f "$dir/sdkconfig" || return 1
    fi

    # Clean environment (like CI's fresh container): an export.sh already sourced in the
    # caller's shell, e.g. for the other IDF version, would otherwise leak its paths and Python env.
    env -i HOME="$HOME" TERM="${TERM:-dumb}" LANG=en_US.UTF-8 \
        PATH="/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin" \
        IDF_PATH="$idf_path" bash -c '
            set -e
            . "$IDF_PATH/export.sh" >/dev/null
            cd "$0"
            idf.py -B "$1" -DSDKCONFIG="$1/sdkconfig" -DPROBE_BOARD="$2" "${@:3}" build
        ' "$ROOT" "$dir" "$board" "${extra[@]+"${extra[@]}"}" || return 1
    printf '%s\n' "$fingerprint" >"$dir/.defaults-fingerprint" || return 1
    "$ROOT/scripts/guards.sh" --sdkconfig "$dir/sdkconfig" "${config_args[@]+"${config_args[@]}"}"
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
if $required_failed; then
    echo "LOCAL CI FAILED (logs in $LOGS/)"; exit 1
else
    echo "LOCAL CI PASSED"
fi
