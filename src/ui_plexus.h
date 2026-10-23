#ifndef UI_PLEXUS_H
#define UI_PLEXUS_H

#include "raylib.h"
#include "ui_board.h"
#include "ui_screens.h"

/* Draw the Atrium Plex hub screen over assets/plexus.png */
void plexus_draw(const BoardAssets *ba, Vector2 vm);

/* Handle mouse clicks inside the Atrium Plex screen.
   Returns ACT_SAVE when SAVE & BACK is clicked (caller saves + transitions to SCREEN_START),
   ACT_START_GAME / ACT_NONE for internal node/month/button clicks (sets *out_clicked = 1 if any control was hit). */
ScreenAction plexus_handle_click(Vector2 vm, int *out_clicked);

/* Unload lazy-loaded Atrium Plex background textures on shutdown */
void plexus_unload_assets(void);

#endif /* UI_PLEXUS_H */
