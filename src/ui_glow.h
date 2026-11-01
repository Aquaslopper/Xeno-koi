#ifndef UI_GLOW_H
#define UI_GLOW_H

#include "raylib.h"

/* Ambient light over the UI screens. UI_GLOW.md is the specification - where it
   draws, what it may never touch, and what every constant below does.
   One call site: src/ui_main.c, after the canvas blit, UI screens only. */

void ui_glow_draw_ui_pass(Texture2D frame, Rectangle dest);

#endif
