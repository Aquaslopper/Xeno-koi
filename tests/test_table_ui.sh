#!/bin/sh
# Raylib headers only; rendering/input are stubbed, gameplay and geometry are real.
set -eu
cd "$(dirname "$0")/.."
binary=$(mktemp /tmp/xenokoi-table-test.XXXXXX)
trap 'rm -f "$binary"' EXIT
${CC:-gcc} -std=c99 -O1 -g -Wall -Wextra ${CFLAGS:-} \
    -ffunction-sections -fdata-sections -Isrc -I"${RAYLIB_SRC:-/tmp/raylib/src}" \
    tests/test_table_ui.c src/ui_input.c src/ui_combos.c src/ui_screens.c src/ui_board.c \
    src/ui_capture_info.c src/campaign.c src/board_layout.c src/ui_anim.c src/ui_speed.c \
    src/cards.c src/rules.c src/engine.c \
    -Wl,--gc-sections -Wl,--wrap=board_draw_card -lm -o "$binary"
"$binary"

hub_bin=$(mktemp /tmp/xenokoi-hub-test.XXXXXX)
trap 'rm -f "$binary" "$hub_bin"' EXIT
${CC:-gcc} -std=c99 -O2 -Wall -Wextra ${CFLAGS:-} \
    -Isrc -I"${RAYLIB_SRC:-/tmp/raylib/src}" \
    tests/test_hub_transition.c \
    src/ai.c src/board_layout.c src/campaign.c src/cards.c src/engine.c src/replay.c \
    src/rules.c src/storage.c src/ui_anim.c src/ui_audio.c src/ui_board.c \
    src/ui_board_status.c src/ui_capture_info.c src/ui_card_tooltip.c src/ui_combos.c \
    src/ui_input.c src/ui_motion.c src/ui_replay.c src/ui_screens.c src/ui_speed.c \
    src/ui_taunt.c src/ui_theme.c \
    -lm -o "$hub_bin"
"$hub_bin"
