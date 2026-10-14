#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Guard: protected mechanics.
#
# Fails a pull request when a non-owner changes files that are protected by
# .github/CODEOWNERS + AGENTS.md, and explains exactly which files (and which
# zone) are involved, so a contributor sees immediately what happened and why.
#
# Used by .github/workflows/guard-protected.yml, which runs in two contexts:
#
#   * pull_request_target — GitHub runs the copy of the workflow that lives on
#     the base branch (trusted) with a read/write token. That run produces the
#     required check name "guard" and is the only one allowed to decide a merge.
#   * pull_request — runs the copy of the workflow that lives in the PR
#     (untrusted) and reports "guard-preview". Informational only; a PR can edit
#     its own copy, so it can never satisfy the required check.
#
# The script never executes PR code. It only runs `git merge-base` / `git diff`
# against git objects, and only ever checks out the base commit.
#
# Inputs (environment):
#   BASE_SHA, HEAD_SHA   commit ids from the pull_request payload
#   PR_NUMBER            pull request number (used to fetch refs/pull/<n>/head)
#   PR_AUTHOR            github.event.pull_request.user.login
#   TRIGGER_ACTOR        github.actor (who started this run)
#   OWNER_LOGIN          the maintainer allowed to bypass (default Aquaslopper)
#   EVENT_NAME           pull_request_target | pull_request
#   BASE_REPO, HEAD_REPO  head.repo.full_name == BASE_REPO means same repository
#   REMOTE               git remote to fetch from (default origin)
#
# Outputs (GITHUB_OUTPUT): protected, fail, comment, comment_header, body,
#                          files, categories
#
# This script never exits non-zero: the workflow's final step turns
# outputs.fail into the red check, *after* the sticky comment has been posted.
# ---------------------------------------------------------------------------
set -uo pipefail

OWNER_LOGIN="${OWNER_LOGIN:-Aquaslopper}"
REMOTE="${REMOTE:-origin}"
BASE_SHA="${BASE_SHA:-}"
HEAD_SHA="${HEAD_SHA:-}"
PR_NUMBER="${PR_NUMBER:-}"
PR_AUTHOR="${PR_AUTHOR:-unknown}"
TRIGGER_ACTOR="${TRIGGER_ACTOR:-unknown}"
EVENT_NAME="${EVENT_NAME:-unknown}"
BASE_REPO="${BASE_REPO:-}"
HEAD_REPO="${HEAD_REPO:-}"
TRUSTED_EVENT="pull_request_target"
BODY_PATH="${BODY_PATH:-${RUNNER_TEMP:-/tmp}/guard-protected-body.md}"
SUMMARY_PATH="${GITHUB_STEP_SUMMARY:-}"

# --- the protected zones ---------------------------------------------------
# One line per zone: "<zone>: <git pathspecs...>".
# tools/check_protected_zones.py checks this block against .github/CODEOWNERS
# and against the zone list in tools/guard_stale.sh.
zone_specs() {
  cat <<'ZONES'
# BEGIN PROTECTED ZONES (checked by tools/check_protected_zones.py)
rules: src/rules.* src/engine.* src/cards.* tools/wiki_tables.c tools/test_koikoi_trigger.c docs/rulesets/ docs/scoring/
settings: src/campaign.* src/storage.* src/ai.*
cards_atlas: assets/atlas.png assets/atlas_deus.png assets/back.png assets/deus.png assets/*.jpg docs/assets/cards/ docs/assets/deus-bg.webp tools/make_card_images.py
wiki: docs/ mkdocs.yml overrides/
core_mechanic: src/ui_screens.c src/ui_board_status.c src/board_layout.* src/rules.c src/engine.c
protection_config: .github/CODEOWNERS .github/workflows/
# END PROTECTED ZONES
ZONES
}

zone_label() {
  case "$1" in
    rules)             echo "Rules / scoring / engine" ;;
    settings)          echo "Settings / Deus / difficulties" ;;
    cards_atlas)       echo "Cards / atlas / art" ;;
    wiki)              echo "Docs / wiki / rulesets" ;;
    core_mechanic)     echo "Core Koi-Koi mechanics (screen, points, multiplier)" ;;
    protection_config) echo "Guard configuration" ;;
    *)                 echo "$1" ;;
  esac
}

out() { if [ -n "${GITHUB_OUTPUT:-}" ]; then printf '%s\n' "$1" >>"$GITHUB_OUTPUT"; fi; }
say() { printf '%s\n' "$*"; }
warn() { printf '::warning::%s\n' "$*"; }
note() { printf '::notice::%s\n' "$*"; }

# Workflow commands must not be able to break out of an annotation, and a file
# name must not be able to inject extra workflow commands.
sanitize() { printf '%s' "$1" | tr -d '\r\n'; }
escape_prop() { sanitize "$1" | sed -e 's/%/%25/g' -e 's/:/%3A/g' -e 's/,/%2C/g'; }

# ---------------------------------------------------------------- locate HEAD
# The PR head is fetched through refs/pull/<n>/head, which exists for same-repo
# and for fork pull requests. Never uses a raw fork SHA fetch, never checks the
# head out, never runs anything from it.
HEAD=""
HEAD_REF=""
if [ -n "$PR_NUMBER" ]; then
  HEAD_REF="refs/remotes/origin/pr-$PR_NUMBER"
  if temp=$(git fetch --no-tags --force "$REMOTE" "+refs/pull/$PR_NUMBER/head:$HEAD_REF" 2>&1); then
    say "fetched refs/pull/$PR_NUMBER/head"
  else
    warn "could not fetch refs/pull/$PR_NUMBER/head: $(sanitize "$temp")"
    HEAD_REF=""
  fi
fi
if [ -n "$HEAD_REF" ] && git rev-parse --verify -q "$HEAD_REF" >/dev/null 2>&1; then
  HEAD=$(git rev-parse "$HEAD_REF")
  if [ -n "$HEAD_SHA" ] && [ "$HEAD_SHA" != "$HEAD" ]; then
    warn "PR moved while the workflow was starting: auditing $HEAD (event recorded $HEAD_SHA)"
  fi
elif [ -n "$HEAD_SHA" ] && git rev-parse --verify -q "${HEAD_SHA}^{commit}" >/dev/null 2>&1; then
  HEAD=$(git rev-parse "${HEAD_SHA}^{commit}")
fi

BASE=""
if [ -n "$BASE_SHA" ] && git rev-parse --verify -q "${BASE_SHA}^{commit}" >/dev/null 2>&1; then
  BASE=$(git rev-parse "${BASE_SHA}^{commit}")
fi

if [ -z "$HEAD" ] || [ -z "$BASE" ]; then
  say "cannot determine the PR base/head commits (base='$BASE_SHA' head='$HEAD_SHA')"
  out "protected=unknown"
  out "fail=true"
  out "comment=true"
  out "comment_header=$([ "$EVENT_NAME" = "$TRUSTED_EVENT" ] && echo guard-protected || echo guard-protected-preview)"
  {
    say "### ⚠️ Protected-mechanics guard could not run"
    say ""
    say "The guard could not resolve the pull request commits (base \`$(sanitize "$BASE_SHA")\`, head \`$(sanitize "$HEAD_SHA")\`)."
    say "Please merge \`main\` into the branch and push again."
  } >"$BODY_PATH"
  out "body=$BODY_PATH"
  printf '::error::Protected-mechanics guard could not resolve the pull request commits\n'
  [ -n "$SUMMARY_PATH" ] && { printf '### ⚠️ Guard could not resolve commits\n' >>"$SUMMARY_PATH"; } || true
  exit 0
fi

# ------------------------------------------------------------------- compare
# Three-dot equivalent: everything the PR itself changed relative to its own
# base (this is what GitHub shows in "Files changed"). Changes that came from
# merging main into the branch are not attributed to the PR.
MERGE_BASE=$(git merge-base "$BASE" "$HEAD" 2>/dev/null) || MERGE_BASE="$BASE"
say "base=$BASE merge-base=$MERGE_BASE head=$HEAD"

ALL_FILES_TMP=$(mktemp)
: >"$ALL_FILES_TMP"
CATEGORIES=""
ZONE_SECTIONS=""

while IFS= read -r line; do
  case "$line" in ''|'#'*) continue ;; esac
  zone=${line%%:*}
  specs=${line#*:}
  read -r -a spec_arr <<<"$specs"
  files=$(git diff --name-only -z "$MERGE_BASE" "$HEAD" -- "${spec_arr[@]}" 2>/dev/null | tr '\0' '\n' | sed '/^$/d')
  [ -n "$files" ] || continue
  printf '%s\n' "$files" >>"$ALL_FILES_TMP"
  CATEGORIES="${CATEGORIES:+$CATEGORIES,}$zone"
  ZONE_SECTIONS="${ZONE_SECTIONS}**$(zone_label "$zone")**
\`\`\`
$files
\`\`\`
"
done < <(zone_specs)

PROTECTED_FILES=$(sort -u "$ALL_FILES_TMP" | sed '/^$/d')
rm -f "$ALL_FILES_TMP"

if [ -z "$PROTECTED_FILES" ]; then
  say "no protected files changed by this PR"
  out "protected=false"
  out "fail=false"
  out "comment=false"
  out "files="
  out "categories="
  [ -n "$SUMMARY_PATH" ] && { printf '### ✅ Protected mechanics guard\n\nNo protected files changed.\n' >>"$SUMMARY_PATH"; } || true
  exit 0
fi

FILE_COUNT=$(printf '%s\n' "$PROTECTED_FILES" | wc -l | tr -d ' ')
FILES_ONE_LINE=$(printf '%s' "$PROTECTED_FILES" | tr '\n' ' ')
say "protected files changed ($FILE_COUNT): $FILES_ONE_LINE"

# -------------------------------------------------------------- who is this?
# Only a PR that the maintainer opened *and* runs keeps the bypass: a re-run of
# a contributor's PR by the maintainer must stay red until it is reviewed.
OWNER_EXEMPT=false
if [ "$PR_AUTHOR" = "$OWNER_LOGIN" ] && [ "$TRIGGER_ACTOR" = "$OWNER_LOGIN" ]; then
  OWNER_EXEMPT=true
fi

# Comments need a token that can write; fork pull_request runs are read-only.
CAN_COMMENT=false
if [ "$EVENT_NAME" = "$TRUSTED_EVENT" ] || { [ -n "$BASE_REPO" ] && [ "$HEAD_REPO" = "$BASE_REPO" ]; }; then
  CAN_COMMENT=true
fi

if [ "$OWNER_EXEMPT" = "true" ]; then
  COMMENT_HEADER="guard-protected"
  FAIL=false
  COMMENT=false
  note "protected mechanics touched, but this PR is the owner's ($OWNER_LOGIN) — guard bypassed, CODEOWNERS review still applies"
  {
    say "### ✅ Protected mechanics touched — owner PR"
    say ""
    say "The guard is bypassed for the maintainer's own pull requests. The ruleset still"
    say "requires an approving review plus a CODEOWNERS review before merging."
    say ""
    say "$ZONE_SECTIONS"
  } >"$BODY_PATH"
else
  FAIL=true
  COMMENT=$CAN_COMMENT
  if [ "$EVENT_NAME" = "$TRUSTED_EVENT" ]; then
    COMMENT_HEADER="guard-protected"
  else
    COMMENT_HEADER="guard-protected-preview"
  fi
  {
    say "### ⚠️ Protected mechanics changed by a non-owner"
    say ""
    say "This pull request (by @$(sanitize "$PR_AUTHOR")) changes files that are protected by"
    say "\`.github/CODEOWNERS\` and \`AGENTS.md\`. They hold the rules, scoring, Koi-Koi combo"
    say "logic, cards/atlas art, settings and the wiki, and they must be reviewed by"
    say "@$OWNER_LOGIN before they change."
    say ""
    say "$ZONE_SECTIONS"
    say "**What to do**"
    say ""
    say "1. If these changes are not intentional, revert them (keep the files exactly as \`main\` has them)."
    say "2. If they are intentional, describe why in the pull request and wait for @$OWNER_LOGIN's review."
    say "3. Never hand-edit generated assets: regenerate card thumbnails with"
    say "   \`python3 tools/make_card_images.py\` and the wiki tables with \`python3 tools/gen_wiki_tables.py\`."
    say ""
    say "See [protected mechanics](https://github.com/$BASE_REPO/blob/main/docs/reference/protected-mechanics.md) for the full list of zones and for the local check you can run before pushing."
  } >"$BODY_PATH"
  printf '::error::Protected mechanics changed by a non-owner: %s\n' "$(sanitize "$FILES_ONE_LINE")"
  printf '%s\n' "$PROTECTED_FILES" | head -20 | while IFS= read -r f; do
    printf '::error file=%s::protected file changed — owner review required (.github/CODEOWNERS)\n' "$(escape_prop "$f")"
  done
fi

[ -n "$SUMMARY_PATH" ] && {
  if [ "$OWNER_EXEMPT" = "true" ]; then
    printf '### ✅ Protected mechanics touched — owner PR (guard bypassed)\n' >>"$SUMMARY_PATH"
  else
    printf '### ⚠️ Protected mechanics changed by a non-owner\n' >>"$SUMMARY_PATH"
  fi
  printf '\n%s\n' "$ZONE_SECTIONS" >>"$SUMMARY_PATH"
} || true

out "protected=true"
out "fail=$FAIL"
out "comment=$COMMENT"
out "comment_header=$COMMENT_HEADER"
out "body=$BODY_PATH"
out "files=$FILES_ONE_LINE"
out "categories=$CATEGORIES"
exit 0
