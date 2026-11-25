#!/usr/bin/env bash
# Mechanical checks for the hard rules in AGENTS.md and .agents/rules/.
# Run by scripts/ci-local.sh (and the manual-only GitHub workflow). Needs no toolchain.
#
# If a guard fires, the code is wrong. Never edit a guard or widen an exception to get past it;
# changing this file needs a DECISIONS.md entry and a second-model review (AGENTS.md).
#
# A single line may opt out of the heap and task-creation guards with a trailing comment
# `// guard-ok: <reason>` (for example a one-time allocation during init). Reviewers check every one.
#
# GUARDS_ROOT can point source checks at a staged snapshot. Tracked paths still use the real index.
# Usage: scripts/guards.sh [--sdkconfig FILE [--diag]]
set -uo pipefail

error() {
    printf 'GUARD ERROR: %s\n' "$1" >&2
    exit 2
}

fail=0
report() {   # report <rule> <matches>
    if [ -n "$2" ]; then
        printf 'GUARD FAILED: %s\n%s\n\n' "$1" "$2"
        fail=1
    fi
}

scan() {
    local status=0
    grep "$@" || status=$?
    case "$status" in
        0|1) return 0 ;;
        *) error "grep $*" ;;
    esac
}

# Reliability settings stay on (reliability rule R5). Removing one needs a DECISIONS.md entry
# and a change here in the same reviewed PR.
required=(
    CONFIG_ESP_TASK_WDT_EN=y
    CONFIG_ESP_TASK_WDT_INIT=y
    CONFIG_ESP_TASK_WDT_PANIC=y
    CONFIG_ESP_INT_WDT=y
    CONFIG_ESP_SYSTEM_PANIC_PRINT_REBOOT=y
    CONFIG_ESP_BROWNOUT_DET=y
    CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y
    CONFIG_PARTITION_TABLE_CUSTOM=y
)
diag_symbol=CONFIG_PROBE_BRINGUP_DIAG

check_required() {
    local file="$1" line matches missing=""
    for line in "${required[@]}"; do
        matches="$(scan -xF -- "$line" "$file")" || exit 2
        [ -n "$matches" ] || missing+="$file lacks: $line"$'\n'
    done
    report "reliability setting removed" "${missing%$'\n'}"
}

if [ "${1:-}" = --sdkconfig ]; then
    config="${2:-}"
    [ -f "$config" ] || error "sdkconfig file missing: ${config:-FILE required}"
    case "$#:${3:-}" in
        2:|3:--diag) ;;
        *) error "usage: scripts/guards.sh --sdkconfig FILE [--diag]" ;;
    esac
    check_required "$config"
    matches="$(scan -nE -- "^${diag_symbol}=" "$config")" || exit 2
    if [ "${3:-}" = --diag ]; then
        enabled="$(scan -xF -- "$diag_symbol=y" "$config")" || exit 2
        [ -n "$enabled" ] || report "diagnostics not enabled" "$config lacks: $diag_symbol=y"
    else
        report "diagnostics enabled or explicitly set without --diag" "$matches"
    fi
    # No CI or release build may be a fault-test build (#26). Checked positively: the normal
    # choice must be selected, so a fault mode added to the Kconfig later is caught too.
    selected="$(scan -xF -- "CONFIG_PROBE_FAULT_TEST_NONE=y" "$config")" || exit 2
    [ -n "$selected" ] || report "fault-test firmware enabled" \
        "$config lacks: CONFIG_PROBE_FAULT_TEST_NONE=y"
    [ "$fail" -ne 0 ] || printf 'guards: sdkconfig OK (%s)\n' "$config"
    exit "$fail"
fi
[ "$#" -eq 0 ] || error "usage: scripts/guards.sh [--sdkconfig FILE [--diag]]"

ROOT="$(cd "$(dirname "$0")/.." && pwd)" || error "cannot locate repo root"
cd "${GUARDS_ROOT:-$ROOT}" || error "cannot open source root: ${GUARDS_ROOT:-$ROOT}"
src_grep() {   # src_grep <extended regex> <source directories...>
    scan -rnE --include='*.c' --include='*.cc' --include='*.cpp' --include='*.h' --include='*.hpp' "$@"
}

# 1. GPIO numbers live only in board.hpp (firmware rule F4).
matches="$(src_grep 'GPIO_NUM_[0-9]|gpio_num_t>[[:space:]]*\([[:space:]]*[0-9]|\(gpio_num_t\)[[:space:]]*[0-9]' components main)" || exit 2
matches="$(scan -vE '^components/board/include/board/board\.hpp:[0-9]+:' <<<"$matches")" || exit 2
report "GPIO number outside board.hpp (use board::kBoard.pins)" "$matches"

# 2. core is pure standard C++: no IDF/FreeRTOS headers (firmware rule F2). The host build also enforces this.
matches="$(src_grep '^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"](esp_|freertos/|driver/|hal/|soc/|rom/|nvs|lwip/|sdkconfig)' components/core)" || exit 2
report "IDF/FreeRTOS header in components/core" "$matches"

# 3. No heap allocation in our code (firmware rule F3). Exceptions are marked `// guard-ok: <reason>`.
guard_ok='//[[:space:]]*guard-ok:[[:space:]]*[^[:space:]].*$'
heap_re='(^|[^_[:alnum:]])(new[[:space:]]+([[:alpha:]_]|::)|((aligned_alloc|malloc|calloc|realloc|strdup)[[:space:]]*\()|(heap_caps_(malloc|calloc|realloc)|std::(vector|string|map|unordered_map|set|unordered_set|list|deque|function|shared_ptr)|make_(unique|shared))([^_[:alnum:]]|$))'
matches="$(src_grep "$heap_re" components main)" || exit 2
matches="$(scan -vE "^[^:]*:[0-9]+:[[:space:]]*//|$guard_ok" <<<"$matches")" || exit 2
# Drop lines where the only match is inside a trailing // comment ("x = y;  // new reading").
matches="$(
    while IFS= read -r m; do
        [ -n "$m" ] || continue
        code="${m#*:*:}"
        code="${code%%//*}"
        if grep -qE -- "$heap_re" <<<"$code"; then printf '%s\n' "$m"; fi
    done <<<"$matches"
)"
report "heap allocation (new/malloc/std containers that allocate)" "$matches"

# 4. Tasks and other FreeRTOS objects use static storage (firmware rule F3).
matches="$(src_grep '(^|[^_[:alnum:]])(xTaskCreate(PinnedToCore)?|xQueueCreate|xSemaphoreCreate(Mutex|Binary|Counting|RecursiveMutex)|xEventGroupCreate|xTimerCreate|xStreamBufferCreate|xMessageBufferCreate)[[:space:]]*\(' components main)" || exit 2
matches="$(scan -vE "$guard_ok" <<<"$matches")" || exit 2
report "FreeRTOS object creation (use the ...Static variant)" "$matches"

# 5. Bring-up diagnostics must never be on in committed defaults (it replaces the real app).
matches="$(scan -nE "^[[:space:]]*${diag_symbol}[[:space:]]*=[[:space:]]*y" sdkconfig.defaults*)" || exit 2
report "CONFIG_PROBE_BRINGUP_DIAG enabled in a committed sdkconfig.defaults file" "$matches"

# 5b. Fault-test builds freeze or crash on purpose (#26): committed defaults never mention a fault
# mode (any CONFIG_PROBE_FAULT_TEST_ symbol other than the normal _NONE).
matches="$(scan -nE "^[[:space:]]*#?[[:space:]]*CONFIG_PROBE_FAULT_TEST_[A-Z0-9_]*" sdkconfig.defaults*)" || exit 2
# Exempt only lines whose setting is _NONE itself (not a fault line with _NONE in a comment).
matches="$(scan -vE "^[^:]*:[0-9]+:[[:space:]]*#?[[:space:]]*CONFIG_PROBE_FAULT_TEST_NONE([^A-Z0-9_]|$)" <<<"$matches")" || exit 2
report "CONFIG_PROBE_FAULT_TEST_* fault mode set in a committed sdkconfig.defaults file" "$matches"

# 6. Board defaults must preserve the required reliability settings.
check_required sdkconfig.defaults
for file in sdkconfig.defaults.*; do
    [ "$file" != 'sdkconfig.defaults.*' ] || [ -e "$file" ] || continue
    for line in "${required[@]}"; do
        symbol="${line%=y}"
        matches="$(scan -nE "^(${symbol}=|# ${symbol} is not set$)" "$file")" || exit 2
        matches="$(scan -vE "^[0-9]+:${line}$" <<<"$matches")" || exit 2
        report "board defaults override $line" "${matches:+$file:$matches}"
    done
done

# 7. No iCloud conflict copies ("name 2.cpp") and no generated files in git.
matches="$(
    git -C "$ROOT" ls-files -z |
        while IFS= read -r -d '' path; do
            if [[ "$path" =~ (^|/)[^/]*\ [0-9]+(\.[^/]*)?(/|$) ||
                  "$path" =~ (^|/)(sdkconfig|sdkconfig\.old|build|build-host|build-ci|cmake-build-[^/]*|managed_components)(/|$) ]]; then
                printf '%s\n' "$path"
            fi
        done
)" || error "git ls-files failed"
report "iCloud conflict copy or generated file tracked in git" "$matches"

if [ "$fail" -eq 0 ]; then
    echo "guards: all passed"
fi
exit "$fail"
