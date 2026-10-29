#!/bin/sh
# ACTION CARD LOCKDOWN.
#
# The Action Card game mode is frozen by owner decision (AGENTS.md, "Owner decisions").
# No agent may change its code, and no agent may change anything that *influences* it
# without the maintainer asking for that specific change.
#
# This guard is two checks:
#
#   1. FROZEN FILES - a SHA-256 must match tools/action_guard.sha256 byte for byte.
#      Any edit at all - an agent, an editor, a patch, a script - fails the check.
#
#   2. INFLUENCE POINTS - the lines listed in tools/action_guard.manifest must exist
#      exactly once, verbatim. These live in shared files (ui_main.c, ui_board.c, ...)
#      that legitimately change for unrelated reasons, so they cannot be hashed whole.
#      Each one is a line that changes how the mode plays: CPU pacing, the ruleset the
#      mode is forced onto, the meter input, the freeze gates, the difficulty source.
#
#   ./tools/action_guard.sh            check (exit 0 = frozen, 1 = touched)
#   ./tools/action_guard.sh --report   print every influence point with its line number
#   ./tools/action_guard.sh --record   re-baseline the hashes (OWNER ONLY, after approval)
#
# A failure is a REPORT, not an instruction. Do not "fix" it by editing the mode, the
# guard, the manifest or the recorded hash. Tell the maintainer what changed and stop.
set -eu
cd "$(dirname "$0")/.."

RECORD=tools/action_guard.sha256
MANIFEST=tools/action_guard.manifest
MODE="${1:-check}"

FROZEN="src/ui_action.c src/ui_action.h"

for f in $FROZEN; do
    [ -f "$f" ] || { echo "FAIL: frozen file $f is missing"; exit 1; }
done
[ -f "$MANIFEST" ] || { echo "FAIL: $MANIFEST is missing"; exit 1; }

# --- 2. Report mode: list the influence surface for a human audit -------------------
if [ "$MODE" = "--report" ]; then
    echo "Action Card influence points (each must stay verbatim):"
    echo
    awk -F'\t' '
        /^[[:space:]]*#/ || NF < 2 { next }
        { printf "%s\t%s\n", $1, $2 }
    ' "$MANIFEST" | while IFS="$(printf '\t')" read -r path text _note; do
        line=$(grep -Fnx -- "$text" "$path" 2>/dev/null | head -1 | cut -d: -f1)
        if [ -n "$line" ]; then
            printf '  %s:%s\n      %s\n' "$path" "$line" "$text"
        else
            printf '  %s: MISSING\n      %s\n' "$path" "$text"
        fi
    done
    echo
    echo "Frozen files (hash-guarded):"
    for f in $FROZEN; do
        printf '  %s  %s\n' "$(sha256sum "$f" | cut -c1-16)" "$f"
    done
    exit 0
fi

# --- Record mode: owner-only re-baseline -------------------------------------------
if [ "$MODE" = "--record" ]; then
    : > "$RECORD"
    for f in $FROZEN; do
        printf '%s  %s\n' "$(sha256sum "$f" | cut -d' ' -f1)" "$f" >> "$RECORD"
    done
    echo "recorded $(echo "$FROZEN" | tr ' ' '\n' | grep -c .) frozen file(s) in $RECORD"
    echo "This must only be done by the maintainer, after approving the change."
    exit 0
fi

# --- 1. Check frozen files ---------------------------------------------------------
status=0
for f in $FROZEN; do
    cur=$(sha256sum "$f" | cut -d' ' -f1)
    rec=$(awk -v p="$f" '$2 == p { print $1 }' "$RECORD" 2>/dev/null | head -1)
    if [ -z "$rec" ]; then
        echo "FAIL: no recorded hash for $f (run --record once to create a baseline)"
        status=1
    elif [ "$cur" != "$rec" ]; then
        echo "FAIL: $f CHANGED  $(echo "$cur" | cut -c1-16) != recorded $(echo "$rec" | cut -c1-16)"
        status=1
    fi
done

# --- 2. Check influence points ------------------------------------------------------
missing=0
while IFS="$(printf '\t')" read -r path text _note; do
    case "$path" in ''|'#'*) continue ;; esac
    [ -n "$text" ] || continue
    count=$(grep -Fxc -- "$text" "$path" 2>/dev/null || true)
    [ -n "$count" ] || count=0
    if [ "$count" -ne 1 ]; then
        echo "FAIL: influence point changed or duplicated ($count matches, expected 1)"
        echo "      $path"
        echo "      $text"
        missing=$((missing + 1))
        status=1
    fi
done < "$MANIFEST"

if [ "$status" -ne 0 ]; then
    cat <<'MSG'

======================================================================
ACTION CARD MODE IS FROZEN. This is a report, not a to-do.
======================================================================
The Action Card game mode is locked by owner decision. Nothing above may be
"fixed", reverted, tuned or tidied without the maintainer explicitly asking
for that specific change.

DO NOT:
  - edit src/ui_action.c or src/ui_action.h;
  - edit the influence lines listed in tools/action_guard.manifest;
  - run ./tools/action_guard.sh --record to make this pass;
  - edit this script or the manifest;
  - describe any of it as a bug, a leftover or an improvement.

DO:
  - report exactly which lines changed and what the change does to the mode;
  - leave the code as it is and stop there.

Influence means more than the mode's own files: CPU think durations and event
beats in src/ui_main.c, the ruleset the mode is forced onto, the meter input
and freeze gates in src/ui_board.c, the difficulty the mode reads, and the
telemetry keys in src/ui_plexus.c all change how it plays.
======================================================================
MSG
    exit 1
fi

echo "Action Card lockdown ok ($(echo "$FROZEN" | tr ' ' '\n' | grep -c .) frozen files, $(grep -c -v '^[[:space:]]*#\|^[[:space:]]*$' "$MANIFEST" 2>/dev/null || echo 0) influence points intact)."
