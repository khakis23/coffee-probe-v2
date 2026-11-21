#!/usr/bin/env bash
# Mechanical checks for the hard rules in AGENTS.md and .agents/rules/.
# Run by scripts/ci-local.sh and by GitHub Actions. Fast (grep only), needs no toolchain.
#
# If a guard fires, the code is wrong. Never edit a guard or widen an exception to get past it;
# changing this file needs a DECISIONS.md entry and a second-model review (AGENTS.md).
#
# A single line may opt out of the heap and task-creation guards with a trailing comment
# `// guard-ok: <reason>` (for example a one-time allocation during init). Reviewers check every one.
set -uo pipefail
cd "$(dirname "$0")/.."

fail=0
report() {   # report <rule> <matches>
    if [ -n "$2" ]; then
        printf 'GUARD FAILED: %s\n%s\n\n' "$1" "$2"
        fail=1
    fi
}

SRC_DIRS=(components main)
src_grep() {   # src_grep <extended regex>   -> matching lines in our C/C++ sources, minus guard-ok lines
    grep -rnE "$1" "${SRC_DIRS[@]}" --include='*.cpp' --include='*.hpp' --include='*.h' --include='*.c' 2>/dev/null |
        grep -v 'guard-ok:' || true
}

# 1. GPIO numbers live only in board.hpp (firmware rule F4).
report "GPIO number outside components/board (use board::kBoard.pins)" \
    "$(src_grep 'GPIO_NUM_[0-9]' | grep -v '^components/board/')"

# 2. core is pure standard C++: no IDF/FreeRTOS headers (firmware rule F2). The host build also enforces this.
report "IDF/FreeRTOS header in components/core" \
    "$(grep -rnE '#include[[:space:]]*[<"](esp_|freertos/|driver/|hal/|soc/|nvs|lwip/|sdkconfig)' components/core 2>/dev/null)"

# 3. No heap allocation in our code (firmware rule F3). Exceptions are marked `// guard-ok: <reason>`.
report "heap allocation (new/malloc/std containers that allocate)" \
    "$(src_grep '(^|[^_[:alnum:]])(new[[:space:]]+[[:alnum:]_:<]|malloc[[:space:]]*\(|calloc[[:space:]]*\(|realloc[[:space:]]*\(|heap_caps_malloc|std::(vector|string|map|unordered_map|set|unordered_set|list|deque|function|shared_ptr)|make_(unique|shared))' | grep -v '^[^:]*:[0-9]*:[[:space:]]*//')"

# 4. Tasks use static stacks (firmware rule F3): xTaskCreateStatic, never xTaskCreate.
report "xTaskCreate / xTaskCreatePinnedToCore (use the ...Static variant)" \
    "$(src_grep '(xTaskCreate|xTaskCreatePinnedToCore)[[:space:]]*\(')"

# 5. Bring-up diagnostics must never be on in committed defaults (it replaces the real app).
report "CONFIG_PROBE_BRINGUP_DIAG enabled in a committed sdkconfig.defaults file" \
    "$(grep -n '^CONFIG_PROBE_BRINGUP_DIAG=y' sdkconfig.defaults* 2>/dev/null)"

# 6. Reliability settings stay on (reliability rule R5). Removing one needs a DECISIONS.md entry
#    and a change here in the same reviewed PR.
required=(
    CONFIG_ESP_TASK_WDT_PANIC=y
    CONFIG_ESP_INT_WDT=y
    CONFIG_ESP_SYSTEM_PANIC_PRINT_REBOOT=y
    CONFIG_ESP_BROWNOUT_DET=y
    CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y
    CONFIG_PARTITION_TABLE_CUSTOM=y
)
missing=""
for line in "${required[@]}"; do
    grep -qx "$line" sdkconfig.defaults || missing+="sdkconfig.defaults lacks: $line"$'\n'
done
report "reliability setting removed" "${missing%$'\n'}"

# 7. No iCloud conflict copies ("name 2.cpp") and no generated files in git.
report "iCloud conflict copy or generated file tracked in git" \
    "$(git ls-files | grep -E '( [0-9]+(\.[^/]*)?$| [0-9]+/)|(^|/)sdkconfig$|(^|/)sdkconfig\.old$|(^|/)build(-[a-z]+)?/')"

if [ "$fail" -eq 0 ]; then
    echo "guards: all passed"
fi
exit "$fail"
