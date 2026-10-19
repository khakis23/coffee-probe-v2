# Contributing

Two contributors: **Tyler** (HUMAN) and **Claude** (CLAUDE). Work is planned in GitHub issues and lands through pull requests.

## Tickets

- Every issue is labelled **`HUMAN`** (Tyler implements) or **`CLAUDE`** (Claude implements). The title carries the same tag, e.g. `[HUMAN] MAX31855 frame decoder`.
- Milestones `M0`–`M7` are the phases in [docs/architecture.md](docs/architecture.md). Work roughly in milestone order and respect each issue's **Depends on** line.
- `blocked` = waiting on another ticket or on hardware. `bug` = something broke. `[Backlog]` issues are unscheduled ideas.
- HUMAN tickets include *Learning notes / hints* but not solutions. Ask for more hints in the issue comments any time.

## Branches and PRs

1. Branch from an up-to-date `main`: `human/<issue#>-short-slug` or `claude/<issue#>-short-slug`.
2. Keep commits small; explain *why* in the message.
3. Open a PR titled `<what> (#N)` with `Closes #N` in the body.
4. Review:
   - Claude reviews HUMAN PRs with comments, and doesn't push commits unless asked.
   - Tyler reviews and merges CLAUDE PRs.
5. CI must be green. Squash-merge, delete the branch.

`main` is never pushed to directly. GitHub can't enforce that on a free private repo, so it's a team rule.

## Asking Claude for work

- "Pick up the next CLAUDE ticket" → Claude takes the lowest-numbered unblocked CLAUDE issue, comments that it's started, and opens a PR.
- "Do #15" → that specific ticket.
- "Review my PR" / "I'm stuck on #10" → review comments or hints, not a solution, unless you ask for one.

## Definition of done

- Acceptance criteria in the issue are all ticked.
- New logic in `components/core` has host tests.
- Builds with zero warnings on every board profile.
- Hardware-facing changes: tested on the real board (say how in the PR).
