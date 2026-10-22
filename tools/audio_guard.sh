#!/bin/sh
# Detect unintended changes to the audio DSP file.
#
# Records a SHA-256 of src/ui_audio.c next to the commit it was recorded at,
# then proves on demand that the file on disk is byte-for-byte the file you
# released. Any edit - an editor, a patch, a script, a build step - changes
# the hash and this check fails.
#
#   ./tools/audio_guard.sh            check (exit 0 = untouched, 1 = changed)
#   ./tools/audio_guard.sh --record   re-baseline after an intended change
#   ./tools/audio_guard.sh --history  also list who committed the file recently
#
# Needs only git and sha256sum. --history additionally needs gh (optional).
set -eu
cd "$(dirname "$0")/.."

FILE=src/ui_audio.c
RECORD=tools/audio_guard.sha256
MODE="${1:-check}"

[ -f "$FILE" ] || { echo "FAIL: $FILE is missing"; exit 1; }

hash_file() { sha256sum "$FILE" | cut -d' ' -f1; }

hash_recorded() {
    [ -f "$RECORD" ] && sed -n 's/^\([0-9a-f]\{64\}\).*/\1/p' "$RECORD"
}

short() { cut -c1-16; }

if [ "$MODE" = "--record" ]; then
    head_sha=$(git rev-parse --short HEAD 2>/dev/null || echo unknown)
    h=$(hash_file)
    printf '%s  %s  recorded on %s at %s\n' "$h" "$FILE" \
        "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$head_sha" > "$RECORD"
    echo "recorded $FILE = $(echo "$h" | short) at $head_sha"
    exit 0
fi

current=$(hash_file)
recorded=$(hash_recorded || true)
status=0

echo "file    : $FILE"
echo "on disk : $(echo "$current" | short)"
if [ -n "$recorded" ]; then
    echo "record  : $(echo "$recorded" | short)  ($(sed -n 's/.* at //p' "$RECORD"))"
else
    echo "record  : none (run --record once to create a baseline)"
fi

# 1. Untracked edit: does the file on disk still match what was released?
if [ -n "$recorded" ]; then
    if [ "$current" = "$recorded" ]; then
        echo "verdict : UNTOUCHED - matches the recorded release baseline"
    else
        echo "verdict : CHANGED ON DISK - does not match the recorded baseline"
        status=1
    fi
fi

# 2. Working tree vs the index/HEAD, so a modification shows up even without
#    a baseline (uses git's own object hashes, no trust in the working copy).
if git rev-parse --git-dir >/dev/null 2>&1; then
    if ! git diff --quiet -- "$FILE" 2>/dev/null; then
        echo "git     : modified in the working tree (uncommitted)"
        status=1
    elif ! git diff --cached --quiet -- "$FILE" 2>/dev/null; then
        echo "git     : staged but not committed"
        status=1
    else
        echo "git     : clean against HEAD ($(git rev-parse --short HEAD 2>/dev/null || echo unknown))"
    fi
fi

# 3. Upstream: is the released file still what origin/main has?
if git rev-parse --verify origin/main >/dev/null 2>&1; then
    up=$(git show origin/main:"$FILE" 2>/dev/null | sha256sum | cut -d' ' -f1 || true)
    if [ -n "$up" ]; then
        if [ "$up" = "$current" ]; then
            echo "origin  : same as origin/main"
        else
            echo "origin  : DIFFERS from origin/main ($(echo "$up" | short))"
            echo "          -> work is behind/ahead of main, or main moved"
        fi
    fi
fi

if [ "$MODE" = "--history" ] && command -v gh >/dev/null 2>&1; then
    echo
    echo "recent commits touching $FILE:"
    gh api "repos/:owner/:repo/commits?path=$FILE&per_page=10" \
        --jq '.[] | "  \(.sha[0:7])  \(.commit.author.date[0:16])  \(.commit.author.name)  \(.commit.message|split("\n")[0])"' \
        2>/dev/null || echo "  (cannot read history - check gh auth)"
fi

if [ "$status" -ne 0 ]; then
    echo
    echo "If the change was intended: review it, commit it, then run"
    echo "  ./tools/audio_guard.sh --record"
    echo "so the new state becomes the baseline."
fi
exit "$status"
