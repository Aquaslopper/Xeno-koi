#!/bin/sh
# Raylib headers only; rendering/input are stubbed, gameplay and geometry are real.
set -eu
cd "$(dirname "$0")/.."
binary=$(mktemp /tmp/xenokoi-table-test.XXXXXX)
trap 'rm -f "$binary"' EXIT
${CC:-gcc} -std=c99 -O1 -g -Wall -Wextra ${CFLAGS:-} \
    -ffunction-sections -fdata-sections -Isrc -I"${RAYLIB_SRC:-/tmp/raylib/src}" \
    tests/test_table_ui.c src/ui_input.c src/ui_combos.c src/ui_screens.c src/ui_board.c \
    src/campaign.c src/board_layout.c src/ui_anim.c src/ui_speed.c src/cards.c src/rules.c src/engine.c \
    -Wl,--gc-sections -Wl,--wrap=board_draw_card -lm -o "$binary"
"$binary"
