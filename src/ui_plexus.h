#ifndef UI_PLEXUS_H
#define UI_PLEXUS_H

#include "raylib.h"
#include "storage.h"
#include "ui_board.h"
#include "ui_screens.h"

/* Draw the Atrium Plex hub screen (and middle-screen Logfile window when open) */
void plexus_draw(const BoardAssets *ba, const GameStats *st, Vector2 vm);

/* Handle mouse clicks inside the Atrium Plex screen.
   Returns ACT_GOTO_MENU when SAVE & BACK is clicked (caller saves + transitions to SCREEN_START),
   ACT_START_GAME when START launches a playable tutorial match from Month 1,
   ACT_NONE for internal node/month/logfile/button clicks (sets *out_clicked = 1 if any control was hit). */
ScreenAction plexus_handle_click(Vector2 vm, int *out_clicked);

/* Scripted Month 1 Tutorial Match hooks (used by SCREEN_GAME when launched from Atrium Plex) */
int  plexus_tutorial_is_active(void);
void plexus_tutorial_abort(void);
void plexus_tutorial_setup_round(GameState *g);
int  plexus_tutorial_update_and_click(const GameState *g, const AnimState *anim,
                                      int board_settled, float overlay_alpha,
                                      Vector2 vm, int mouse_pressed, int *out_clicked);
int  plexus_tutorial_allowed_hand_card(const GameState *g);
void plexus_tutorial_draw_board_guides(const BoardAssets *ba, const GameState *g);
void plexus_tutorial_draw_panel(const BoardAssets *ba, Vector2 vm);
void plexus_tutorial_finish_match(void);

/* Unload lazy-loaded Atrium Plex background textures on shutdown */
void plexus_unload_assets(void);

/* Load / save Atrium Plex progress and telemetry (plexsave.txt) */
void plexus_load(void);
void plexus_save(void);

#endif /* UI_PLEXUS_H */
