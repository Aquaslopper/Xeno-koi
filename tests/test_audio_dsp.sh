#!/bin/sh
# Headless check for the capture-reverb DSP. Needs only a C compiler.
set -e
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
OUT="$(mktemp -t test_audio_dsp.XXXXXX)"
trap 'rm -f "$OUT"' EXIT
"$CC" -std=c99 -Wall -Wextra -Werror -O2 \
    -Itests/raylib_audio_stub \
    tests/test_audio_dsp.c -lm -o "$OUT"
"$OUT"
