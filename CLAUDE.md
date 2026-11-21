# CoffeeProbe — notes for Claude

- ESP-IDF v5.3.1 at `~/esp/esp-idf` and v6.0.3 at `~/esp/esp-idf-v6.0`; target esp32c3. Source the matching `export.sh` before any `idf.py` call.
- Build: `idf.py build`. Don't assume a board is attached; if one is (`ls /dev/cu.usbmodem*`), Claude may flash and monitor it.
- C++ (gnu++2b via IDF). No exceptions or RTTI. Avoid heap allocation after init.
- Settings live in `sdkconfig.defaults` (committed); `sdkconfig` is generated and ignored.
- Reliability for non-technical users is the top requirement. Prefer simple, recoverable designs over clever ones.

## Roles (see CONTRIBUTING.md)
- **Claude is the sole developer.** Tyler is the customer/owner: he doesn't write or review code. Keep him informed at a high level.
- Every PR body starts with a plain-English summary (what changed for the probe, anything Tyler must do). Technical detail follows for the record.
- `TYLER` tickets are physical tasks only (wiring, bench/roaster tests, Artisan). Write them as numbered steps for a non-developer, with what to report back. Never give Tyler coding work.

## Workflow
- Issues on `khakis23/coffee-probe-v2`, labelled `CLAUDE` or `TYLER`. Architecture: `docs/architecture.md`.
- One branch per ticket (`claude/<issue#>-slug`) off up-to-date `main`; PR body says `Closes #N`. Never push to `main`.
- Before starting a ticket, check its "Depends on" issues are closed; comment on the issue when starting.
- Run `scripts/ci-local.sh` before pushing; don't open or merge a red PR. Squash-merge your own PR once local and GitHub CI are green, unless Tyler wrote "hold".
- End each session with a short status for Tyler: what landed, what's next, anything waiting on him.
