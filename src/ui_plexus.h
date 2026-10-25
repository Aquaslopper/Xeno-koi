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
int  plexus_tutorial_panel_open(void);
void plexus_tutorial_abort(void);

/* Non-campaign Atrium Plex Training match state */
int  plexus_training_is_active(void);
void plexus_training_stop(void);

/* Atrium Plex Action Card mode state */
int  plexus_action_card_is_active(void);
void plexus_action_card_abort(void);

/* Atrium Plex Speedcoin solo highscore mode & dedicated result screen */
int          plexus_speedcoin_is_active(void);
void         plexus_speedcoin_abort(void);
void         plexus_speedcoin_setup_wave(GameState *g, int wave_num);
int          plexus_speedcoin_deal_hand2(GameState *g);
int          plexus_speedcoin_bank_wave(GameState *g);
int          plexus_speedcoin_reverse_draw_start(GameState *g, int *out_cid, Vector2 *out_from);
void         plexus_speedcoin_reverse_draw_finish(GameState *g);
void         plexus_speedcoin_finish_run(int final_score, int waves, int run_coins, int run_bonus_pts, int run_best_streak);
int          plexus_speedcoin_result_active(void);
void         plexus_speedcoin_close_result(void);
void         plexus_draw_speedcoin_result(const BoardAssets *ba, const GameStats *st, Vector2 vm);
ScreenAction plexus_handle_speedcoin_result_click(Vector2 vm, int *out_clicked);

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
