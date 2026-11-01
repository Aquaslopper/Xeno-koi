#ifndef UI_GLOW_H
#define UI_GLOW_H

#include "raylib.h"

/* Light over the table casing overlay - the transparent overlay art a map is wearing,
   whichever one that is: the map's own overlay, or the CRT pair when the casing
   setting is on. Not the card bezels, not the menus.
   UI_GLOW.md is the specification. One call site: src/ui_board.c, immediately after
   the overlay is drawn. */

void ui_glow_draw_table_overlay(Texture2D overlay, Rectangle dest);

#endif
