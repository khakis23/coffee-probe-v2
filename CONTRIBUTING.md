# Contributing

**Claude is the only developer.** Claude writes all the code, tests and docs. **Tyler is the customer and owner:** he decides what the probe should do, does the hands-on work only a person can do (wiring, plugging in boards, roasting), and follows progress through GitHub. He doesn't need to read or write code.

Work is planned in GitHub issues and lands through pull requests, so every change is visible.

## Labels

| Label | Who | What it means |
|---|---|---|
| **`CLAUDE`** | Claude | Code, tests, docs, releases. Almost every ticket. |
| **`TYLER`** | Tyler | Hands-on tasks Claude physically can't do: wiring, bench and roaster tests, trying it in Artisan. Each one has numbered steps written for a non-developer, and says what to report back. No coding. |
| `blocked` | | Waiting on another ticket or on hardware. |
| `bug` | | Something broke. Tyler can file one in plain words ("the screen went blank after 20 minutes"). |
| `[Backlog]` in the title | | Unscheduled idea. |

Milestones `M0`–`M7` are the phases in [docs/architecture.md](docs/architecture.md). Claude works roughly in milestone order and respects each issue's **Depends on** line.

## How Claude works

1. Takes the lowest-numbered unblocked `CLAUDE` ticket (or the one Tyler asks for) and comments on the issue when starting.
2. Branches from an up-to-date `main`: `claude/<issue#>-short-slug`.
3. Small commits that explain *why*.
4. Runs **local CI** before every push: `scripts/ci-local.sh` (see below). Doesn't open or merge a PR while it's red.
5. Opens a PR titled `<what> (#N)` with `Closes #N`. Every PR body starts with a short **plain-English summary**: what changed for the probe, and anything Tyler needs to do.
6. Code PRs get a second review pass (Codex) before merging.
7. When local CI and GitHub CI are both green, Claude **squash-merges its own PR** and deletes the branch. Tyler can comment on any PR or write "hold" to stop a merge.

`main` is never pushed to directly. GitHub can't enforce that on a free repo, so it's a project rule.

## When Tyler is needed

Only for `TYLER` tickets and for decisions. Claude asks for decisions in the issue or the chat, and keeps them few.

- **Hardware tests**: if the board is left plugged into the Mac, Claude can flash it and read its serial output itself. Then Tyler only does the physical part, like touching the probe tip or unplugging a wire.
- **Results**: report what you saw in plain words, a photo, or a pasted screen. Claude turns it into bugs or fixes.

## Asking Claude for work

- "Keep going" / "Pick up the next ticket" → the next unblocked `CLAUDE` ticket.
- "Do #15" → that ticket.
- "Where are we?" → a short status: what's done, what's next, anything waiting on Tyler.
- "Explain X" → a high-level explanation of part of the system, no code knowledge assumed.

## Local CI

`scripts/ci-local.sh` runs the same checks as GitHub Actions ([.github/workflows/ci.yml](.github/workflows/ci.yml)) on this Mac:

- host unit tests (`test/host`, doctest),
- firmware builds for every board profile on IDF v5.3.1 and v6.0.3, plus the bring-up diagnostics build.

Each build gets its own folder under `build-ci/`, so it never touches the normal `build/` or `sdkconfig`. GitHub Actions runs the same checks again on every PR (free, because the repo is public).

## Definition of done

- Acceptance criteria in the issue are all ticked, or the PR says which are left for a `TYLER` test.
- New logic in `components/core` has host tests.
- Local CI green: zero warnings on every board profile.
- Hardware-facing changes: tested on the real board, or a `TYLER` test ticket says exactly how to test them.
