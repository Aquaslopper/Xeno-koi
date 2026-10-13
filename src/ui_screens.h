#ifndef UI_SCREENS_H
#define UI_SCREENS_H

#include "raylib.h"
#include "engine.h"
#include "storage.h"
#include "ui_board.h"
#include "ui_audio.h"

typedef enum {
    SCREEN_START = 0,
    SCREEN_OPTIONS,
    SCREEN_STATS,
    SCREEN_CREDITS,
    SCREEN_GAME,
    SCREEN_MATCH_RESULT
} ScreenState;

typedef enum {
    ACT_NONE = 0,
    ACT_START_GAME,
    ACT_GOTO_OPTIONS,
    ACT_GOTO_STATS,
    ACT_GOTO_CREDITS,
    ACT_GOTO_MENU,
    ACT_SAVE,
    ACT_QUIT,
    ACT_RESUME,
    ACT_RESTART_MATCH,
    ACT_ABANDON_MATCH,
    ACT_RESET_STATS,
    ACT_SAVE_LOG,
    ACT_NEXT_ROUND
} ScreenAction;

/* Start Screen */
void screens_draw_start(const BoardAssets *ba, const GameStats *st, Vector2 vm, float saved_timer);
ScreenAction screens_handle_start_click(Vector2 vm);

/* Options Screen */
void screens_draw_options(const BoardAssets *ba, GameSettings *s, Vector2 vm);
ScreenAction screens_handle_options_click(GameSettings *s, BoardAssets *ba, AudioAssets *audio, Vector2 vm);

/* Stats Screen */
void screens_draw_stats(const BoardAssets *ba, const GameStats *st, const MatchHistory *h, Vector2 vm, int confirm_reset, float log_timer);
ScreenAction screens_handle_stats_click(Vector2 vm, int *confirm_reset);

/* Credits Screen */
void screens_draw_credits(const BoardAssets *ba, Vector2 vm);
ScreenAction screens_handle_credits_click(Vector2 vm);

/* Pause Screen */
void screens_draw_pause(Vector2 vm, int confirm_restart);
ScreenAction screens_handle_pause_click(Vector2 vm, int *confirm_restart);

/* Match Result Screen */
void screens_draw_match_result(const BoardAssets *ba, const GameState *g, const GameSettings *s, Vector2 vm);
ScreenAction screens_handle_match_result_click(Vector2 vm);

/* In-game Choice / Decision / Round Overlays */
void screens_draw_choice_overlay(const BoardAssets *ba, const GameState *g, Vector2 vm, int is_hand);
int screens_check_choice_click(const GameState *g, Vector2 vm, int is_hand, int *out_target_id);

void screens_draw_decide_overlay(const BoardAssets *ba, const GameState *g, Vector2 vm);
int screens_check_decide_click(Vector2 vm, DecisionType *out_decision);

void screens_draw_round_result(const BoardAssets *ba, const GameState *g, Vector2 vm);
int screens_check_round_result_click(Vector2 vm);

void screens_draw_banner(const char *text);

#endif /* UI_SCREENS_H */
