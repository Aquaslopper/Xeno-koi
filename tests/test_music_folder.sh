#!/bin/sh
# No graphics, audio device, or installed Raylib required.
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d /tmp/xenokoi-music-test.XXXXXX)
trap 'rm -rf "$work"' EXIT
${CC:-cc} -std=c99 -Wall -Wextra -Werror -O1 -g ${CFLAGS:-} \
    -ffunction-sections -fdata-sections -Itests/raylib_audio_stub \
    tests/test_music_folder.c -Wl,--gc-sections -lm -o "$work/test"
${CC:-cc} -std=c99 -Wall -Wextra -Werror -O1 -g ${CFLAGS:-} -D__EMSCRIPTEN__ \
    -ffunction-sections -fdata-sections -Itests/raylib_audio_stub \
    tests/test_music_folder.c -Wl,--gc-sections -lm -o "$work/test_browser"
headers="${RAYLIB_SRC:-/tmp/raylib/src}"
if [ -f "$headers/raylib.h" ]; then
    ${CC:-cc} -std=c99 -Wall -Wextra -Werror -O1 -g ${CFLAGS:-} \
        -ffunction-sections -fdata-sections -I"$headers" \
        tests/test_music_folder_ui.c -Wl,--gc-sections -lm -o "$work/test_ui"
else
    echo "Music dialog input checks skipped (set RAYLIB_SRC for Raylib headers)."
fi
cd "$work"
mkdir desktop browser
(cd desktop && ../test)
(cd browser && ../test_browser)
if [ -f ./test_ui ]; then ./test_ui; fi
