#!/usr/bin/env bash
# Stop hook: before an agent ends its turn, flag uncommitted work so nothing is silently left
# half-done in the checkout. It blocks once; if the agent stops again (stop_hook_active), it lets
# it, so an agent can still end a turn to ask Tyler a question as long as it says what's uncommitted.
#
# Wired up for Claude Code in .claude/settings.json. Other tools can run it the same way: exit 2 with
# a message on stderr means "don't stop yet".
input="$(cat)"
if printf '%s' "$input" | grep -q '"stop_hook_active"[[:space:]]*:[[:space:]]*true'; then
    exit 0
fi

cd "${CLAUDE_PROJECT_DIR:-$(dirname "$0")/../..}" || exit 0
git rev-parse --is-inside-work-tree >/dev/null 2>&1 || exit 0

changes="$(git status --porcelain --untracked-files=normal 2>/dev/null | head -20)"
[ -z "$changes" ] && exit 0

cat >&2 <<EOF
Uncommitted changes in $(pwd) on branch $(git branch --show-current):
$changes

Before stopping: commit finished work (scripts/ci-local.sh first), or say in your reply exactly what
is left uncommitted and why. Scratch files belong in .scratch/ (git-ignored).
EOF
exit 2
