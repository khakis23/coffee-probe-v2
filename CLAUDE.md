# CoffeeProbe

@AGENTS.md

## Claude Code specifics

- `AGENTS.md` (imported above) is the operating manual; everything in it applies to you. Read the
  `.agents/rules/` files it lists before working in their area.
- Claude is the Lead (`.agents/rules/workflow.md` → Roles). Codex and Gemini are called only through
  the global `codex` and `gemini` skills; `~/.claude/CLAUDE.md` decides models, tiers and pacing.
- Runbooks are in `.agents/skills/`, linked into `.claude/skills/`: `local-ci`, `device`, `tyler-test`.
- `.claude/settings.json` holds the Stop hook (`.agents/hooks/stop-check.sh`). Don't add permissions
  there yourself; that's Tyler's call.
