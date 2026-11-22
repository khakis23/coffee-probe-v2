# Testing rules

## Layers

| Layer | What | Where | Runs |
|---|---|---|---|
| Guards | Hard rules (pins, heap, core purity, reliability config, tracked files) | `scripts/guards.sh` | Local CI + GitHub |
| Host tests | All logic in `components/core` | `test/host/test_*.cpp` (doctest) | Local CI + GitHub |
| Firmware builds | generic + xiao × IDF v5.3.1 + v6.0.3, plus diagnostics builds (generic, both IDFs) | `idf.py` via `ci-local.sh` | Local CI + GitHub |
| Bench checks | Driver behaviour on a real board | `scripts/flash.sh` + `serial_capture.py` | Agent, when a board is plugged in |
| Field tests | Artisan, roaster, faults, soak | `TYLER` issues (#3, #13, #17, #26, #27) | Tyler, via the `tyler-test` skill |

## Host tests

- Every function in `core` with logic has tests. New `core` code without tests is not done.
- One file per unit: `test/host/test_<unit>.cpp`. New files are picked up automatically (glob).
- Name tests for the behaviour they prove: `TEST_CASE("MAX31855 open-circuit frame reports probe-open")`.
- **Expected values come from the datasheet or an independent calculation, never from running the
  code under test.** Put the source in a comment (datasheet table, page, worked example).
- Cover the edges, not just the happy path: fault bits, negative temperatures, min/max of each field,
  rollover, stale timing boundaries, invalid input.
- Time is a parameter in `core`, so tests drive it explicitly. No sleeps in tests.
- Host build flags are `-Wall -Wextra -Werror`. Fix warnings; don't silence them.

## CI

- `scripts/ci-local.sh` before every push; the full run before opening or merging a PR.
- `scripts/ci-local.sh` and `.github/workflows/ci.yml` run the same jobs. Change both together.
- IDF v6 jobs are allowed to fail until #2 lands; after that, remove `continue-on-error` and the
  allowed-failure line in `ci-local.sh` in the same PR.
- Never delete, skip or weaken a failing test or guard to get green. If a test is wrong, say why in
  the PR and fix the test.

## Hardware verification

- A driver PR says exactly what was checked on hardware and how, or says "not verified on hardware"
  and names the `TYLER` issue that will cover it.
- When a board is plugged in, use the `device` skill: flash, capture the log, and paste the relevant
  lines into the PR.
- Never write "works" for something only compiled.
