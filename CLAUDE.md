# CoffeeProbe — notes for Claude

- ESP-IDF v5.3.1 at `~/esp/esp-idf`; target esp32c3. Source `~/esp/esp-idf/export.sh` before any `idf.py` call.
- Build: `idf.py build`. The user flashes hardware themselves; don't assume a board is attached.
- C++ (gnu++2b via IDF). No exceptions or RTTI. Avoid heap allocation after init.
- Settings live in `sdkconfig.defaults` (committed); `sdkconfig` is generated and ignored.
- Reliability for non-technical users is the top requirement. Prefer simple, recoverable designs over clever ones.
- The user is relearning C++ and wants to write parts themselves. Explain the non-obvious ESP-IDF/FreeRTOS choices in PRs and commit messages.

## Workflow (see CONTRIBUTING.md)
- Work is tracked in GitHub issues on `khakis23/coffee-probe-v2`, labelled `HUMAN` or `CLAUDE`. Architecture: `docs/architecture.md`.
- Only implement `CLAUDE` tickets. For `HUMAN` tickets, review, hint and answer questions, but don't write the solution unless the user explicitly asks.
- One branch per ticket (`claude/<issue#>-slug`) off up-to-date `main`; PR body says `Closes #N`. Never push to `main`.
- Before starting a ticket, check its "Depends on" issues are closed; comment on the issue when starting.
