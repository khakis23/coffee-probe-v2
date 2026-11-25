---
name: local-ci
description: Run CoffeeProbe's local CI (guards, host tests, every firmware build) and read its failures. Use before every push, before opening or merging a PR, and whenever a build or test fails.
---

# Local CI runbook

`scripts/ci-local.sh` is the only CI gate (GitHub's workflow is manual-only, D-009). It has the same jobs as `.github/workflows/ci.yml`.

## Run it

```bash
scripts/ci-local.sh                  # full run, ~1.5 min incremental; run in the background
scripts/ci-local.sh guards host      # fast loop while working on core logic
scripts/ci-local.sh generic          # any job whose name contains "generic"
scripts/ci-local.sh --clean          # from scratch, like a fresh GitHub runner (slow)
```

Jobs: `guards`, `host`, `generic-v5.3.1`, `xiao-v5.3.1`, `generic-v6.0.3`, `xiao-v6.0.3`,
`generic-v5.3.1-diag`, `generic-v6.0.3-diag`. Every job must pass, v6 included.
A filter that matches no job, or an unknown `--option`, exits 2 instead of silently skipping checks.

Each firmware job also checks the **generated** sdkconfig (`scripts/guards.sh --sdkconfig`): the
reliability settings must really be on in the build, and diagnostics on only in the diag job.

Output and logs: `~/Library/Caches/coffee-probe/ci/<checkout-hash>/` (outside the repo because of
iCloud; one folder per checkout, so git worktrees never share build caches). The first line of the
run prints the exact folder. Each failing job prints its last 15 log lines; the full log is
`<folder>/logs/<job>.log`. `--clean` refuses to delete anything that isn't a dedicated output folder.

## Reading failures

- **guards**: the message names the rule and the offending lines. Fix the code. Never edit
  `scripts/guards.sh` to get past it (that needs a DECISIONS entry and review).
- **host**: `ctest --output-on-failure` output shows the failing `CHECK` with both values. A compile
  error mentioning `esp_` / `freertos` means an IDF header leaked into `core`.
- **firmware**: search the log for `error:` first. If the error mentions an `sdkconfig` symbol after
  `sdkconfig.defaults*` changed, the job's sdkconfig is regenerated automatically; if it still looks
  stale, rerun that job with `--clean`.
- **Only v6 fails**: check whether the API differs in v6 (`ESP_IDF_VERSION` guard in one place) and
  note it on #2. It blocks the merge like any other failure.
- **Weird missing-file or "2" errors**: iCloud conflict copies. `--clean`, and check `git status` for
  files named `<name> 2`.

## Report

Paste the summary table (job, PASS/FAIL, seconds) into the PR's Verification section, with the commit
it ran on. If you ran a subset, say which jobs.

## Keeping it in sync

When the workflow's matrix changes, update `FIRMWARE_JOBS` in `ci-local.sh` in the same PR, and the
job list above.
