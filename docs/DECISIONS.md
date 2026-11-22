# Decisions

Append-only log. Each entry: date, decision, why, who decided. Don't edit old entries; add a new one
that supersedes it. The original key decisions (SoftAP + Modbus TCP, own Modbus server, IDF v6,
own display stack, no exceptions/RTTI, pure `core`) are in `docs/architecture.md` → *Key decisions*.
The IDF version there (v6.0.x) is the target; until #2 confirms it, code must build on v5.3.1 and v6.0.3.

## D-001 · 2026-11-21 · Claude is the sole developer (Tyler)

Tyler no longer has time to write code. Claude leads all development and uses Codex and Gemini as
helpers according to Tyler's global routing rules; Tyler is the customer and does physical tasks only.
Former `HUMAN` coding tickets became `CLAUDE`; the label is now `TYLER` for hands-on work.

**Pending Tyler's OK** (proposed by Claude): Claude merges its own PRs once local and GitHub CI are
green, unless Tyler writes "hold". Until he confirms, PRs wait for him to merge.

## D-002 · 2026-11-21 · Agent environment: AGENTS.md + .agents/ (Tyler, Claude)

Same structure as Tyler's other AI-built projects: a short `CLAUDE.md` that imports `AGENTS.md`;
detailed rules in `.agents/rules/`; runbooks in `.agents/skills/` linked into `.claude/skills/`; a
Stop hook that flags uncommitted work. Keeps every model and session consistent.

## D-003 · 2026-11-21 · Mechanical guards (Claude)

`scripts/guards.sh` enforces the rules a grep can check (pins only in `board.hpp`, no heap, static
tasks, pure `core`, reliability sdkconfig present, no tracked generated files) in local and GitHub CI.
Changing a guard needs a new entry here and a second-model review.

## D-004 · 2026-11-21 · Host tests compile as C++23 (Claude)

The architecture's error type is `std::expected`, which is C++23. The host build was C++20, so the
first `core` code using it would have failed the host tests. Host build is now C++23 (no extensions),
checked with Apple clang 21. The firmware already builds as gnu++2b.

## D-005 · 2026-11-21 · Script build output lives outside the repo (Claude)

The repo is in iCloud Drive, which made conflict copies inside CLion's build folder. `ci-local.sh` and
`flash.sh` build in `~/Library/Caches/coffee-probe/`.

## D-006 · 2026-11-21 · Claude merges its own PRs (Tyler)

Tyler: "yes always merge yourself. you're the dev." Supersedes the pending part of D-001. Claude
squash-merges its own PRs once local CI and GitHub CI are green and the required reviews approve.
Tyler can still write "hold" on any PR to stop a merge.
