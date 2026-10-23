#!/bin/sh
# Enforce owner lock on tests/: no new test files may be added to tests/
# without explicit approval from @Aquaslopper.
#
#   ./tools/tests_guard.sh            verify no unapproved files exist in tests/
#   ./tools/tests_guard.sh --record   update tools/tests_guard.manifest after approval
set -eu
cd "$(dirname "$0")/.."

MANIFEST=tools/tests_guard.manifest

list_current() {
    LC_ALL=C find tests -type f | LC_ALL=C sort
}

if [ "${1:-check}" = "--record" ]; then
    list_current > "$MANIFEST"
    echo "Updated $MANIFEST ($(wc -l < "$MANIFEST") approved files)."
    exit 0
fi

[ -f "$MANIFEST" ] || { echo "FAIL: $MANIFEST is missing"; exit 1; }

tmp_cur=$(mktemp /tmp/xenokoi-tests-cur.XXXXXX)
tmp_exp=$(mktemp /tmp/xenokoi-tests-exp.XXXXXX)
trap 'rm -f "$tmp_cur" "$tmp_exp"' EXIT

list_current > "$tmp_cur"
LC_ALL=C sed '/^[[:space:]]*#/d;/^[[:space:]]*$/d' "$MANIFEST" | LC_ALL=C sort > "$tmp_exp"

added=$(LC_ALL=C comm -23 "$tmp_cur" "$tmp_exp")
if [ -n "$added" ]; then
    echo "FAIL: unapproved file(s) in tests/ (owner approval required):"
    printf '  + %s\n' $added
    echo "Remove unapproved files (run scratch checks in /tmp), or run"
    echo "  ./tools/tests_guard.sh --record"
    echo "only after explicit approval from @Aquaslopper."
    exit 1
fi

echo "tests/ lock ok ($(wc -l < "$tmp_cur" | tr -d ' ') approved files, 0 unapproved)."
