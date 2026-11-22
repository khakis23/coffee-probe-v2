---
name: tyler-test
description: Hand a physical task or hardware test to Tyler (the client) - write a TYLER issue or an in-chat request as numbered non-technical steps, and turn his report into results, bugs and next steps. Use for wiring, bench tests, roaster tests, Artisan setup, or anything needing hands on the hardware.
---

# Handing a test to Tyler

Tyler is the customer. He's comfortable with the roaster, Artisan and a breadboard, but he doesn't
read code or use the terminal. User-facing material (the quick-start card, the Artisan guide, screen
messages) is also for his non-technical staff: test it against "could a new staff member follow this
alone?". Every request costs him time, so batch them and make them impossible
to misread.

## In chat (quick physical step)

Use exactly this block:

```
=== TYLER ACTION REQUIRED ===
Why: <one line, plain words>
Steps:
  1. ...
  2. ...
Duration: <honest estimate>
Report back: <exactly what you need: a word, a number, a photo>
=============================
```

Before asking, do everything you can yourself: build and flash it first (`device` skill) if the board
is plugged in, and start the log capture so he only does the physical part.

## As a TYLER issue (a real test session)

Title `[TYLER] <outcome>`, label `TYLER`, milestone set. Body:

```markdown
## Why this matters
<one or two sentences about what it proves for the roaster>

## You'll need
- <hardware, cables, a glass of ice water, the roaster...>
- About <N> minutes

## Steps
1. <one action per step, with what you should see: "The screen shows 72.4°F (± a few degrees)">
2. ...

## Report back (copy this into a comment and fill it in)
- Step 3 reading: ___
- Did Artisan show −1 when unplugged? yes / no
- Photo of: ___
- Anything odd: ___

**Depends on:** #N (Claude makes sure these are merged and flashed first)
```

Rules for the steps:
- No terminal commands, no menuconfig, no CLion. If a command is unavoidable, it's one line to paste,
  and you've already run everything else.
- Say what success looks like at each step, so he knows if something's wrong without asking.
- Safety first: unplug before rewiring; heat tests have a stop condition.
- Prefer timed, scripted protocols ("unplug for 10 s, plug back in") over judgement calls.

## Before handing it over

- The firmware it tests is merged, and you've flashed it or `scripts/flash.sh` is the only step left.
- Rewrite the existing issue body if it's still in the old developer format (CLion, hints).
- Update `docs/STATUS.md` → "Waiting on Tyler".

## When he reports back

- Thank him briefly, restate the result in one line, and tick what passed.
- Every failure becomes a `bug` issue (his words quoted, the log or photo attached), or a fix in the
  current PR if it's small.
- Record measured values (thresholds, timings, chip identity) in the issue and, if they change the
  design, in `docs/DECISIONS.md` and `docs/architecture.md`.
- Close the TYLER issue only when every acceptance item passed or has its own bug issue.
