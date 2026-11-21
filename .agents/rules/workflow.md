# Workflow rules

How work moves from an issue to merged code, who reviews it, and how Tyler is kept in the loop.

## The work loop

One issue at a time, roughly in milestone order (M0 → M7), one PR per issue.

1. Pick the lowest-numbered open `CLAUDE` issue whose **Depends on** issues are all closed, unless
   Tyler named one. Comment on the issue that work has started.
2. `git fetch` and branch from up-to-date `main`: `git switch -c claude/<issue#>-<short-slug> origin/main`.
   If the issue depends on an unmerged PR, branch from that PR's branch instead (a stacked PR, see Merging).
3. For pure logic in `core`, write the host tests from the acceptance criteria first.
4. Implement the smallest thing that meets the acceptance criteria and follows the rules files.
5. Run `scripts/ci-local.sh guards host` while iterating, and the full `scripts/ci-local.sh` before
   every push.
6. Small commits whose message says *why*. Non-obvious ESP-IDF or FreeRTOS choices are explained in
   the commit or PR so the history teaches the next reader.
7. Push and open the PR (format below). Get a review when the table below requires one.
8. When local CI and GitHub CI are green and review (if required) approves: squash-merge and delete
   the branch. Update `docs/STATUS.md` in the PR or a follow-up commit.
9. Next issue.

A milestone is finished when its `CLAUDE` issues are merged **and** its `TYLER` hardware tests have
passed. Hand those tests over with the `tyler-test` skill.

## Definition of done (every PR)

- `scripts/ci-local.sh` passes locally and GitHub CI is green.
- Every acceptance criterion in the issue is met and covered by a host test or a recorded hardware
  check, or the PR says which ones wait for a named `TYLER` test.
- New logic lives in `components/core` with host tests; drivers only connect hardware to it.
- Docs touched by the change are updated in the same PR (architecture, dev-setup, the component README).
- `docs/STATUS.md` is accurate for whoever comes next.

## PR format

Title: `<what changed> (#N)`. Body, in this order:

1. **In plain English**: 2–4 sentences for Tyler. What changed for the probe or the project, and
   whether he needs to do anything ("Nothing for you to do" is a fine answer). No jargon.
2. **What** / **Why**: the technical record. Explain non-obvious choices.
3. **Verification**: what was run and the result (paste the CI summary). Say plainly what was *not*
   verified, especially anything that needs real hardware.
4. `Closes #N`, then the tool's attribution line if it adds one.

## Roles and review

- **Lead (Claude)**: owns architecture, plans, issue refinement, UI/design and anything needing the
  conversation with Tyler. Writes most code directly.
- **Builder (Codex)**: may implement a well-specified issue from a written plan in its own git
  worktree. The Lead reviews every line before it lands.
- **Reviewers**: Codex reviews code the Lead wrote; the Lead reviews code Codex wrote. Gemini is a
  second, independent reviewer for the areas marked below, and drafts human-facing docs.

Which model, tier and how much usage to spend are machine-level settings (`~/.claude/CLAUDE.md`),
not project rules. Call other models only through the installed `codex` / `gemini` skills.

**A review by a model other than the author is required before merging** changes to:

| Area | Why | Second reviewer (Gemini) too |
|---|---|---|
| `components/net` (SoftAP, Modbus server) | Artisan's only link; socket bugs hang the roast | yes |
| `components/storage` (NVS settings) | Corrupt settings can brick the user experience | yes |
| `components/health`, watchdog or task setup in `main/` | A wrong watchdog = reboot loops or silent hangs | yes |
| Thermocouple fault handling, filtering, stale detection | Wrong temperature shown as real | yes |
| `sdkconfig.defaults*`, `partitions.csv` | Reliability config | |
| `scripts/guards.sh`, CI files, `AGENTS.md`, `.agents/rules/` | They define every other check | |

Other changes may merge on green CI. Reviewers read the diff cold, look for violations of the hard
rules and the reliability contract, give concrete failure scenarios, skip style comments, and end
with APPROVE or REQUEST CHANGES. Review runs are read-only; the Lead runs CI itself on the reviewed
commit.

### Delegating to the Builder

1. Its own worktree: `git worktree add .scratch/wt/<issue> -b claude/<issue#>-<slug> origin/main`.
2. A written plan that leaves no design choices open: files it may edit, exact API (names, types),
   rules that apply, tests to write and where expected values come from, "run
   `scripts/ci-local.sh guards host` until it passes", "no commits, pushes or branch switches", and
   the report format.
3. The Lead runs CI in the worktree itself (never trusts "it passed"), reads all non-test code, and
   spot-checks test vectors against the datasheet. Numbers the Builder computed by running code are
   guesses until recomputed independently.

## Merging

- Squash-merge, delete the branch. Never force-push a branch with an open PR.
- **Stacked PRs**: when an issue needs an unmerged PR, branch from it and open the PR with
  `--base <parent-branch>`. Merge parents first. Before merging a parent with `--delete-branch`,
  retarget its children to `main` (`gh pr edit <child> --base main`), or GitHub closes them.
- Tyler can write **"hold"** on any PR. A held PR is not merged until he lifts it.
- If `gh pr merge` is refused by permissions, don't look for another way: leave the PR green, list it
  in `docs/STATUS.md` under "Waiting on Tyler", and continue stacked on top of it.

## Working with Tyler

Tyler is the customer, not a tester on call. Protect his time.

- Decide engineering questions yourself, using the docs. Ask Tyler only for product decisions the plan
  doesn't cover, anything that costs money or is public, his accounts and settings, and physical actions.
- Physical requests go through the `tyler-test` skill: batched, numbered steps, a time estimate, and
  exactly what to report back.
- Write to him in plain language: what changed for the roaster, what (if anything) he needs to do.
  He wants to understand the project at a high level, not the code.
- **End every session** with a short status: what landed, what's next, anything waiting on him. Keep
  `docs/STATUS.md` matching it.
