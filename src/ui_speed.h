#ifndef UI_SPEED_H
#define UI_SPEED_H

#include "engine.h"

/* KK-ADD-10 only: the bottom-right speed minigame. A short timer bar arms at
   the start of each human hand turn and drains over 300 ms; it is gone the
   moment the hand card is dropped (the stock flip never counts). Dropping the
   hand card onto a matching card while the bar still runs scores minipoints
   from speed (how much of the bar is left) times accuracy (how much of the
   target card the drop covers). Minipoints show in gold over the bar and stay
   after the bar is gone; every 10 of them become 1 extra score point at the
   end of the match. Human only - the CPU is never graded. */

void speed_bar_reset(void);                       /* new match: clear everything */
void speed_bar_update(const GameState *g, float dt, int input_unlocked);
void speed_bar_consume(void);                     /* hand play resolved with no grade */
void speed_bar_grade(float cover);                /* capture graded: cover is 0..1 */
int  speed_bar_apply_match_bonus(GameState *g);   /* +total/10 to the human; returns the bonus */
int  speed_bar_total(void);                       /* minipoints earned this match */
void speed_bar_draw(void);                        /* bar behind, gold numbers over it */

/* Telemetry getters and persistence hooks for Atrium Plex Logfile stats */
int   speed_bar_graded_drops(void);
float speed_bar_avg_drop_seconds(void);
float speed_bar_avg_cover_pct(void);
int   speed_bar_best_streak(void);
int   speed_bar_lifetime_minipoints(void);
void  speed_bar_get_telemetry(int *drops, float *time_sum, float *cover_sum, int *best_streak, int *lifetime_mp);
void  speed_bar_set_telemetry(int drops, float time_sum, float cover_sum, int best_streak, int lifetime_mp);

/* Solo Atrium-Plex Speedcoin mode (180s countdown, per-wave hidden 30-MP & 10-MP math) */
#define SPEEDCOIN_HAND2_MP 30
void  speed_mode_begin(void);
void  speed_mode_begin_wave(int wave_num);
void  speed_mode_stop(void);
int   speed_mode_is_active(void);
float speed_mode_clock(void);
int   speed_mode_wave(void);
int   speed_mode_deck_glow(void);
int   speed_mode_can_deal_hand2(void);
void  speed_mode_mark_hand2_dealt(void);
int   speed_mode_run_best_streak(void);
float speed_mode_clock_flash(int *out_last_delta);

#endif /* UI_SPEED_H */
