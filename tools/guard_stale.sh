#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Guard: stale protected revert.
#
# Fails a pull request whose base predates protected changes that landed on
# main. Such a branch is dangerous: when the contributor later has to resolve
# conflicts (or copies files around), the newer rules/scoring/cards work can be
# silently dropped again. The guard lists the protected files that changed on
# main since the branch's merge base and tells the contributor to update.
#
# Used by .github/workflows/guard-stale.yml, which runs in two contexts:
#   * pull_request_target — the base-branch copy of the workflow and this script
#     (trusted) with a read/write token; produces the required check "stale".
#   * pull_request — the PR's own copy; reports "stale-preview" (informational).
#
# Only git objects are touched: no PR code is checked out or executed.
#
# Inputs (environment):
#   PR_NUMBER            pull request number (refs/pull/<n>/head)
#   MAIN_REF             what "main" is locally (default origin/main)
#   PR_AUTHOR, BASE_REPO, HEAD_REPO, EVENT_NAME  (for comment routing)
#   REMOTE               git remote to fetch from (default origin)
#
# Outputs (GITHUB_OUTPUT): stale, comment, comment_header, body, files
#
# This script never exits non-zero: the workflow posts the sticky comment and
# then turns outputs.stale into the red check.
# ---------------------------------------------------------------------------
set -uo pipefail

REMOTE="${REMOTE:-origin}"
MAIN_REF="${MAIN_REF:-origin/main}"
OWNER_LOGIN="${OWNER_LOGIN:-Aquaslopper}"
PR_NUMBER="${PR_NUMBER:-}"
PR_AUTHOR="${PR_AUTHOR:-unknown}"
EVENT_NAME="${EVENT_NAME:-unknown}"
BASE_REPO="${BASE_REPO:-}"
HEAD_REPO="${HEAD_REPO:-}"
TRUSTED_EVENT="pull_request_target"
BODY_PATH="${BODY_PATH:-${RUNNER_TEMP:-/tmp}/guard-stale-body.md}"
SUMMARY_PATH="${GITHUB_STEP_SUMMARY:-}"

# --- protected zones -------------------------------------------------------
# Superset of every zone in tools/guard_protected.sh: anything the owner
# protects must also be flagged here when main moves ahead of the branch.
# tools/check_protected_zones.py keeps this list in sync with
# .github/CODEOWNERS and with the zone list in tools/guard_protected.sh.
zone_specs() {
  cat <<'ZONES'
# BEGIN PROTECTED ZONES (checked by tools/check_protected_zones.py)
protected: src/rules.* src/engine.* src/cards.* src/campaign.* src/storage.* src/ai.* src/ui_screens.c src/ui_board_status.c src/board_layout.* tools/wiki_tables.c tools/test_koikoi_trigger.c tools/make_card_images.py assets/atlas.png assets/atlas_deus.png assets/back.png assets/deus.png assets/*.jpg docs/ mkdocs.yml overrides/ .github/CODEOWNERS .github/workflows/
# END PROTECTED ZONES
ZONES
}

out() { if [ -n "${GITHUB_OUTPUT:-}" ]; then printf '%s\n' "$1" >>"$GITHUB_OUTPUT"; fi; }
say() { printf '%s\n' "$*"; }
warn() { printf '::warning::%s\n' "$*"; }

sanitize() { printf '%s' "$1" | tr -d '\r\n'; }
escape_prop() { sanitize "$1" | sed -e 's/%/%25/g' -e 's/:/%3A/g' -e 's/,/%2C/g'; }

meta() { # meta <stale> <comment> <header> ; body file must exist
  out "stale=$1"
  out "comment=$2"
  out "comment_header=$3"
  out "body=$BODY_PATH"
  out "files=${4:-}"
}

# Comments need a token that can write; fork pull_request runs are read-only.
CAN_COMMENT=false
if [ "$EVENT_NAME" = "$TRUSTED_EVENT" ] || { [ -n "$BASE_REPO" ] && [ "$HEAD_REPO" = "$BASE_REPO" ]; }; then
  CAN_COMMENT=true
fi
if [ "$EVENT_NAME" = "$TRUSTED_EVENT" ]; then
  COMMENT_HEADER="guard-stale"
else
  COMMENT_HEADER="guard-stale-preview"
fi

# ------------------------------------------------------------------- fetch
git fetch --no-tags --force "$REMOTE" "+refs/heads/main:refs/remotes/origin/main" >/dev/null 2>&1 \
  || warn "could not refresh $MAIN_REF"

MAIN=$(git rev-parse --verify -q "$MAIN_REF^{commit}" 2>/dev/null)
HEAD_REF="refs/remotes/origin/pr-$PR_NUMBER"
if [ -n "$PR_NUMBER" ] && git fetch --no-tags --force "$REMOTE" "+refs/pull/$PR_NUMBER/head:$HEAD_REF" >/dev/null 2>&1; then
  HEAD=$(git rev-parse --verify -q "$HEAD_REF^{commit}" 2>/dev/null)
else
  HEAD=""
fi

if [ -z "$MAIN" ] || [ -z "$HEAD" ]; then
  say "cannot determine main ('$MAIN_REF') or the PR head (refs/pull/$PR_NUMBER/head)"
  out "stale=false"
  out "comment=false"
  out "comment_header=$COMMENT_HEADER"
  out "body="
  out "files="
  printf '::warning::Stale guard could not resolve main or the PR head; skipping the staleness check\n'
  [ -n "$SUMMARY_PATH" ] && printf '### ⚠️ Stale guard could not resolve refs\n' >>"$SUMMARY_PATH" || true
  exit 0
fi

say "main=$MAIN head=$HEAD"

# ------------------------------------------------------------------ compare
# Everything protected that changed on main since the branch's merge base.
# Empty base (= no common ancestor) is treated as stale, so the branch is
# updated before it can be merged.
BASE=$(git merge-base "$HEAD" "$MAIN" 2>/dev/null) || BASE=""
if [ -z "$BASE" ]; then
  {
    say "### ⚠️ Stale branch — no common history with \`main\`"
    say ""
    say "The branch does not share history with \`main\`, so the guard cannot tell what it"
    say "would revert. Merge or rebase \`main\` into the branch and push again."
  } >"$BODY_PATH"
  say "no merge base between $HEAD and $MAIN"
  printf '::error::Stale branch: no common history with main\n'
  meta true "$CAN_COMMENT" "$COMMENT_HEADER" ""
  [ -n "$SUMMARY_PATH" ] && printf '### ⚠️ Stale branch: no common history with main\n' >>"$SUMMARY_PATH" || true
  exit 0
fi

read -r -a spec_arr <<<"$(zone_specs | sed -n 's/^protected:[[:space:]]*//p')"
CHANGED=$(git diff --name-only -z "$BASE" "$MAIN" -- "${spec_arr[@]}" 2>/dev/null | tr '\0' '\n' | sort -u | sed '/^$/d')

if [ -z "$CHANGED" ]; then
  say "branch is up to date: no protected file changed on main since $BASE"
  out "stale=false"
  out "comment=false"
  out "comment_header=$COMMENT_HEADER"
  out "body="
  out "files="
  [ -n "$SUMMARY_PATH" ] && printf '### ✅ Stale guard\n\nUp to date: no protected file changed on `main` since the branch base (`%s`).\n' "$BASE" >>"$SUMMARY_PATH" || true
  exit 0
fi

FILE_COUNT=$(printf '%s\n' "$CHANGED" | wc -l | tr -d ' ')
FILES_ONE_LINE=$(printf '%s' "$CHANGED" | tr '\n' ' ')
say "protected files changed on main since $BASE ($FILE_COUNT): $FILES_ONE_LINE"

{
  say "### ⚠️ Stale branch — would revert protected mechanics"
  say ""
  say "This branch is based on \`$(sanitize "$BASE")\`. Protected files changed on \`main\` after that,"
  say "so merging as-is risks dropping newer rules, scoring, combo triggers, card art or wiki work."
  say ""
  say "\`\`\`"
  printf '%s\n' "$CHANGED"
  say "\`\`\`"
  say ""
  say "**What to do**"
  say ""
  say "\`\`\`bash"
  say "git fetch origin main && git merge origin/main   # or: git rebase origin/main"
  say "git diff --name-only \$(git merge-base HEAD origin/main)..origin/main -- <protected paths>   # should be empty afterwards"
  say "\`\`\`"
  say ""
  say "Resolve conflicts *in favour of \`main\`* for these files unless the pull request is"
  say "deliberately changing them, and wait for @$OWNER_LOGIN's review."
} >"$BODY_PATH"

printf '::error::Stale branch: %s protected file(s) changed on main since %s\n' "$FILE_COUNT" "$(sanitize "$BASE")"
printf '%s\n' "$CHANGED" | head -30 | while IFS= read -r f; do
  printf '::error file=%s::protected file changed on main since this branch was based on it\n' "$(escape_prop "$f")"
done

[ -n "$SUMMARY_PATH" ] && {
  printf '### ⚠️ Stale branch — would revert protected mechanics\n\n' >>"$SUMMARY_PATH"
  printf '```\n%s\n```\n' "$CHANGED" >>"$SUMMARY_PATH"
} || true

meta true "$CAN_COMMENT" "$COMMENT_HEADER" "$FILES_ONE_LINE"
exit 0
