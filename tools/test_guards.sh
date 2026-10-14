#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Local simulation of the two guards (tools/guard_protected.sh,
# tools/guard_stale.sh) — the check to run after touching the guards, the zone
# lists or .github/CODEOWNERS.
#
# Everything happens in a temporary directory:
#
#   * a bare repository is used as "origin", with a main branch, the current
#     working copies of the guard scripts/workflows, a same-repo PR branch, a
#     fork-style refs/pull/<n>/head and a branch based on an older main;
#   * every scenario runs the guard exactly like the workflows do (same env
#     variables) and asserts on the GITHUB_OUTPUT values.
#
# Usage:  bash tools/test_guards.sh          # exit 0 = all scenarios pass
#         bash tools/test_guards.sh --keep   # keep the temp directory
# ---------------------------------------------------------------------------
set -uo pipefail

REPO_ROOT=$(git -C "$(dirname "$0")/.." rev-parse --show-toplevel)
KEEP=0
[ "${1:-}" = "--keep" ] && KEEP=1

TMP=$(mktemp -d)
cleanup() { [ "$KEEP" = "1" ] || rm -rf "$TMP"; }
trap cleanup EXIT

ORIGIN="$TMP/origin.git"
WORK="$TMP/work"
PASS=0
FAIL=0

say() { printf '%s\n' "$*"; }
ok() { PASS=$((PASS + 1)); printf '  PASS  %s\n' "$1"; }
bad() { FAIL=$((FAIL + 1)); printf '  FAIL  %s\n' "$1"; }

# ---------------------------------------------------------------- fixtures
git init -q --bare "$ORIGIN"
git -C "$ORIGIN" symbolic-ref HEAD refs/heads/main
git -C "$REPO_ROOT" push -q "$ORIGIN" HEAD:refs/heads/main
git clone -q "$ORIGIN" "$WORK"
git -C "$WORK" config user.email "guards@example.invalid"
git -C "$WORK" config user.name "guard test"

# main = real history + the guard files as they exist in the working tree
cp -f "$REPO_ROOT/tools/guard_protected.sh" "$WORK/tools/guard_protected.sh"
cp -f "$REPO_ROOT/tools/guard_stale.sh" "$WORK/tools/guard_stale.sh"
cp -f "$REPO_ROOT/.github/workflows/guard-protected.yml" "$WORK/.github/workflows/guard-protected.yml"
cp -f "$REPO_ROOT/.github/workflows/guard-stale.yml" "$WORK/.github/workflows/guard-stale.yml"
git -C "$WORK" add -A
git -C "$WORK" commit -qm "main: guard files under test"
git -C "$WORK" push -q origin main
MAIN=$(git -C "$WORK" rev-parse HEAD)

# older main: the merge of PR #13, i.e. before the protected #14 work landed
OLD_MAIN=$(git -C "$WORK" log --format=%H --grep='Merge pull request #13' -1 --merges)
[ -n "$OLD_MAIN" ] || OLD_MAIN=$(git -C "$WORK" rev-parse HEAD~5)

# same-repo PR touching a protected file (src/rules.c)
git -C "$WORK" checkout -q -b pr-protected "$MAIN"
printf '\n// guard test\n' >>"$WORK/src/rules.c"
git -C "$WORK" commit -qam "touch protected rules"
PROTECTED_HEAD=$(git -C "$WORK" rev-parse HEAD)

# same-repo PR touching nothing protected
git -C "$WORK" checkout -q -b pr-clean "$MAIN"
printf 'guard test\n' >"$WORK/NOTES_guard_test.md"
git -C "$WORK" add NOTES_guard_test.md
git -C "$WORK" commit -qm "docs: unrelated note"
CLEAN_HEAD=$(git -C "$WORK" rev-parse HEAD)

# same-repo PR touching the guard configuration
git -C "$WORK" checkout -q -b pr-config "$MAIN"
printf '\n# guard test\n' >>"$WORK/.github/workflows/build.yml"
git -C "$WORK" commit -qam "touch a workflow"
CONFIG_HEAD=$(git -C "$WORK" rev-parse HEAD)

# PR based on an old main: stale
git -C "$WORK" checkout -q -b pr-stale "$OLD_MAIN"
printf 'guard test\n' >"$WORK/NOTES_guard_stale.md"
git -C "$WORK" add NOTES_guard_stale.md
git -C "$WORK" commit -qm "old base"
STALE_HEAD=$(git -C "$WORK" rev-parse HEAD)

# fork-style: the same commits published as refs/pull/<n>/head on origin
FORK_PROTECTED=101
FORK_STALE=102
git -C "$WORK" push -q origin "$PROTECTED_HEAD:refs/pull/$FORK_PROTECTED/head"
git -C "$WORK" push -q origin "$STALE_HEAD:refs/pull/$FORK_STALE/head"

cd "$WORK" || exit 1
git checkout -q "$MAIN" >/dev/null 2>&1 || true
export REMOTE=origin

# ---------------------------------------------------------------- helpers
run_guard_protected() { # <out> <pr|-> <base> <head> <author> <event> <head-repo>
  # PR_AUTHOR is the only input that decides the owner bypass; the workflow does
  # not pass github.actor at all, so who started the run cannot matter.
  local out=$1 pr=$2 base=$3 head=$4 author=$5 event=$6 head_repo=$7
  : >"$out"
  GITHUB_OUTPUT="$out" \
    BASE_SHA="$base" HEAD_SHA="$head" PR_NUMBER="$pr" \
    PR_AUTHOR="$author" OWNER_LOGIN=Aquaslopper \
    EVENT_NAME="$event" BASE_REPO=Aquaslopper/Xeno-koi HEAD_REPO="$head_repo" \
    bash tools/guard_protected.sh >/dev/null 2>&1
}

run_guard_stale() { # <out> <pr> <event> <head-repo>
  local out=$1 pr=$2 event=$3 head_repo=$4
  : >"$out"
  GITHUB_OUTPUT="$out" \
    PR_NUMBER="$pr" PR_AUTHOR=sneed-and-feed EVENT_NAME="$event" \
    BASE_REPO=Aquaslopper/Xeno-koi HEAD_REPO="$head_repo" \
    MAIN_REF=origin/main \
    bash tools/guard_stale.sh >/dev/null 2>&1
}

value_of() { sed -n "s/^$2=//p" "$1" | tail -1; }

expect() { # <out> <key> <value> <description>
  local got
  got=$(value_of "$1" "$2")
  if [ "$got" = "$3" ]; then ok "$4 ($2=$got)"; else bad "$4 (expected $2=$3, got '$got')"; fi
}

expect_contains() { # <out> <needle> <description>
  if grep -qF -- "$2" "$1"; then ok "$3"; else bad "$3 (missing '$2')"; fi
}

say "temp repository: $TMP"
say "main=$MAIN  old main=$OLD_MAIN"

# ---------------------------------------------------------------- scenarios
say ""
say "guard-protected: non-owner PR touching src/rules.c"
O="$TMP/protected-nonowner.out"
run_guard_protected "$O" 201 "$MAIN" "$PROTECTED_HEAD" sneed-and-feed pull_request_target Aquaslopper/Xeno-koi
expect "$O" protected true "protected zone detected"
expect "$O" fail true "fails for a non-owner"
expect "$O" comment true "posts a sticky comment"
expect "$O" comment_header guard-protected "trusted event uses the required-guard header"
expect_contains "$O" src/rules.c "file list names src/rules.c"

say ""
say "guard-protected: fork PR (read-only token), head only reachable via refs/pull"
O="$TMP/protected-fork.out"
run_guard_protected "$O" "$FORK_PROTECTED" "$MAIN" "" sneed-and-feed pull_request sneed-and-feed/Xeno-koi
expect "$O" fail true "fails for a fork PR"
expect "$O" comment false "does not comment with the read-only fork token"
expect_contains "$O" src/rules.c "file list names src/rules.c"

say ""
say "guard-protected: non-owner PR touching the guard configuration"
O="$TMP/protected-config.out"
run_guard_protected "$O" 202 "$MAIN" "$CONFIG_HEAD" sneed-and-feed pull_request_target Aquaslopper/Xeno-koi
expect "$O" fail true "fails when a non-owner edits .github/workflows"
expect "$O" categories protection_config "reports the protection_config zone"
expect_contains "$O" .github/workflows/build.yml "file list names the workflow"

say ""
say "guard-protected: PR without protected changes"
O="$TMP/protected-clean.out"
run_guard_protected "$O" 205 "$MAIN" "$CLEAN_HEAD" sneed-and-feed pull_request_target sneed-and-feed/Xeno-koi
expect "$O" protected false "nothing protected touched"
expect "$O" fail false "green"

say ""
say "guard-protected: owner PR touching src/rules.c (bypass)"
O="$TMP/protected-owner.out"
run_guard_protected "$O" 203 "$MAIN" "$PROTECTED_HEAD" Aquaslopper pull_request_target Aquaslopper/Xeno-koi
expect "$O" protected true "protected zone detected"
expect "$O" fail false "green for the owner"
expect "$O" comment false "no comment for the owner"

say ""
say "guard-protected: contributor PR stays red however the run was started"
O="$TMP/protected-rerun.out"
run_guard_protected "$O" 204 "$MAIN" "$PROTECTED_HEAD" sneed-and-feed pull_request_target Aquaslopper/Xeno-koi
expect "$O" fail true "a contributor PR never goes green (no actor input exists)"
expect "$O" comment true "and keeps its explaining comment"

say ""
say "guard-protected: same-repo pull_request run is labelled as a preview"
O="$TMP/protected-preview.out"
run_guard_protected "$O" 206 "$MAIN" "$PROTECTED_HEAD" sneed-and-feed pull_request Aquaslopper/Xeno-koi
expect "$O" comment_header guard-protected-preview "preview runs never use the required-guard header"

say ""
say "guard-stale: branch based on an old main (fork, refs/pull/$FORK_STALE/head)"
O="$TMP/stale-old.out"
run_guard_stale "$O" "$FORK_STALE" pull_request_target sneed-and-feed/Xeno-koi
expect "$O" stale true "stale branch is detected"
expect "$O" comment true "posts a sticky comment"
expect_contains "$O" src/rules.c "lists src/rules.c"
expect_contains "$O" src/engine.c "lists src/engine.c"
expect_contains "$O" .github/workflows/ "lists the guard configuration"

say ""
say "guard-stale: up-to-date branch (refs/pull/$FORK_PROTECTED/head)"
O="$TMP/stale-fresh.out"
run_guard_stale "$O" "$FORK_PROTECTED" pull_request_target Aquaslopper/Xeno-koi
expect "$O" stale false "up-to-date branch is not stale"
expect "$O" comment false "no comment"

say ""
say "---------------------------------------------------------------"
say "passed: $PASS   failed: $FAIL"
[ "$FAIL" = "0" ] || exit 1
