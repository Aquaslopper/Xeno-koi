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

/* Speedcoin Minigame event flags returned by plexus_speedcoin_update() */
enum {
    PLEXUS_SC_EVT_NONE        = 0,
    PLEXUS_SC_EVT_CLICK       = (1 << 0),
    PLEXUS_SC_EVT_DROP        = (1 << 1),
    PLEXUS_SC_EVT_CAPTURE     = (1 << 2),
    PLEXUS_SC_EVT_FLIP        = (1 << 3),
    PLEXUS_SC_EVT_SYNTH       = (1 << 4),
    PLEXUS_SC_EVT_EXIT_ATRIUM = (1 << 5)
};

/* Speedcoin Minigame hooks (Month 1 Card 2 & bottom SPEEDCOIN tab) */
int  plexus_speedcoin_is_active(void);
void plexus_speedcoin_abort(void);
void plexus_speedcoin_setup_match(GameState *g, uint32_t seed);
int  plexus_speedcoin_combos_hit(Vector2 vm);
int  plexus_speedcoin_update(GameState *g, void *drag_ptr, AnimState *anim,
                             Vector2 vm, int *hovered_idx, float dt);
void plexus_speedcoin_draw(const BoardAssets *ba, const GameState *g, const AnimState *anim,
                           const void *drag_ptr, int hovered_hand_idx, Vector2 vm);

/* Unload lazy-loaded Atrium Plex background textures on shutdown */
void plexus_unload_assets(void);

#endif /* UI_PLEXUS_H */
