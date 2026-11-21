# How this project runs

**Claude is the only developer.** Claude writes all the code, tests and docs. **Tyler is the customer
and owner:** he decides what the probe should do, does the hands-on work only a person can do
(wiring, plugging in the board, roasting, Artisan), and follows progress on GitHub. He doesn't need
to read or write code.

The detailed rules for AI agents are in [`AGENTS.md`](AGENTS.md) and [`.agents/`](.agents/). This page
is the short version for people.

## Where to look

- **[`docs/STATUS.md`](docs/STATUS.md)**: where things stand, what's next, and anything waiting on Tyler.
  Updated every session.
- **Pull requests**: every change. Each one starts with an **"In plain English"** summary.
- **Issues**: the plan, grouped into milestones M0–M7 (see [docs/architecture.md](docs/architecture.md)).
- **[`docs/DECISIONS.md`](docs/DECISIONS.md)**: why things are the way they are.

## Labels

| Label | Who | Meaning |
|---|---|---|
| `CLAUDE` | Claude | Code, tests, docs, releases. Almost every ticket. |
| `TYLER` | Tyler | A hands-on task: numbered steps written for a non-developer, and what to report back. No coding. |
| `blocked` | | Waiting on another ticket or on hardware. |
| `bug` | | Something broke. File one in plain words ("the screen went blank after 20 minutes"). |

## How a change lands

1. Claude takes the next unblocked `CLAUDE` ticket and comments that it started.
2. Builds it on a branch, with automatic checks: rule checks, logic tests on the Mac, and a firmware
   build for every board (`scripts/ci-local.sh`). GitHub runs the same checks again.
3. Opens a PR with a plain-English summary. Sensitive areas (Wi-Fi/Artisan link, settings storage,
   watchdogs, temperature faults) get a review from a second AI model.
4. Once everything is green, Claude merges it. **Write "hold" on any PR to stop a merge.**

## What Tyler might be asked for

- **Decisions** the plan doesn't cover. Claude keeps these few.
- **Hardware tests** (`TYLER` issues). If the board is left plugged into the Mac, Claude does the
  flashing and reads its output; Tyler only does the physical part.
- **Results**: what you saw in plain words, a photo, or a pasted screen.

## Talking to Claude

- "Keep going": the next ticket. "Do #15": that ticket.
- "Where are we?": a short status.
- "Explain X": a high-level explanation of part of the system, no code knowledge assumed.
