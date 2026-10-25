#ifndef UI_ACTION_H
#define UI_ACTION_H

#include <stddef.h>
#include <stdint.h>
#include "raylib.h"
#include "engine.h"
#include "ui_board.h"
#include "ui_input.h"
#include "ui_anim.h"
#include "ui_audio.h"

typedef enum {
    ACTION_SYM_SKULL = 0,
    ACTION_SYM_HAND,
    ACTION_SYM_REV_ARROW,
    ACTION_SYM_ONE_EYE,
    ACTION_SYM_GOLD_COIN,
    ACTION_SYM_COUNT
} ActionCardSymbol;

/* Music rhythm tap & mode state */
void  action_card_sync_music(AudioAssets *audio, int in_action_table, int paused,
                             float transition_alpha, float dt);
void  action_card_detach_music(AudioAssets *audio);
void  action_card_set_mode(int active, float music_level);
int   action_card_is_mode_active(void);
int   action_card_is_paused(void);
void  action_card_reset_match(void);
void  action_card_advance_round(void);
int   action_card_take_match_reset_req(void);

/* Telemetry & Time-Number readouts */
int   action_card_get_symbols_fired(void);
int   action_card_get_lifetime_time(void);
void  action_card_set_telemetry(int symbols_fired, int lifetime_time);
int   action_card_get_ceiling_count(void);
int   action_card_claim_ceiling_count(int who);
int   action_card_consume_ceiling_count(void);
int   action_card_get_round_time_stats(int *out_you, int *out_you_pts,
                                       int *out_cpu, int *out_cpu_pts, int *out_meter);
int   action_card_get_match_time_stats(int *out_you, int *out_you_pts,
                                       int *out_cpu, int *out_cpu_pts, int *out_meter);
void  action_card_format_time_line(char *buf, size_t cap,
                                   int yt, int yp, int ct, int cp, int mt);
void  action_card_draw_match_result_extras(const GameState *gs);

/* Beat symbols & board effects */
int   action_card_get_symbol(int *out_armed);
int   action_card_take_pending_symbol(void);
void  action_card_trigger_blind_cards(const GameState *g, uint32_t *rng_state, int count);
void  action_card_apply_symbol(int sym, GameState *g, DragState *drag, AnimState *anim,
                               AudioAssets *audio, int *hovered_idx, int *human_turn_started);

/* Table rendering & VFX hooks */
float action_card_begin_table(const GameState *g, int round_reset, float reverb_synergy);
void  action_card_draw_distraction(Rectangle dr, int slot_key, float added_lvl);
void  action_card_draw_stock_returns(const BoardAssets *ba, const GameState *g, float ac_card_lvl);
void  action_card_draw_meters(const BoardAssets *ba, const GameState *g,
                              float reverb_synergy, float env_dt, int replay_mode);
int   action_card_draw_blind_flip(const BoardAssets *ba, int cid, Rectangle dr,
                                  float env_dt, float ac_card_lvl);
void  action_card_draw_vfx(float env_dt);
void  action_card_draw_symbol_icon(const BoardAssets *ba, int sym,
                                   float cx, float cy, int armed, float flash);

#endif /* UI_ACTION_H */
