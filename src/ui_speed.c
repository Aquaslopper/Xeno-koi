#include "ui_speed.h"
#include "ui_theme.h"
#include <math.h>
#include <string.h>

extern Font font_body;

#define HIT_POPUP_MAX  4
#define HIT_POPUP_HOLD 0.50f
#define HIT_POPUP_FLY  0.65f
#define HIT_POPUP_DUR  (HIT_POPUP_HOLD + HIT_POPUP_FLY)

typedef struct {
    int   active;
    float x, y, elapsed;
    int   mp, acc;
} HitPopup;

static HitPopup s_popups[HIT_POPUP_MAX];
static int s_vis_streak;
static int s_pending_popup_ready, s_pending_popup_mp, s_pending_popup_acc;

#define SPEED_BAR_SECONDS 1.0f
#define SPEED_BAR_RECT    (Rectangle){ 1160.0f, 662.0f, 112.0f, 40.0f }
#define SPEED_FLASH_TIME  0.30f
#define SPEED_PULSE_TIME  0.55f
#define SPEED_GHOST_TIME  0.42f

static float s_remaining;   /* seconds left on this turn's bar */
static float s_frac;        /* visible fill fraction (frozen at drop for the flash) */
static int   s_armed;       /* bar is running */
static int   s_started;     /* bar already armed for this turn */
static float s_flash;       /* bar flash timer after an award */
static float s_pulse;       /* gold numbers golden flash timer after an award */
static float s_ghost, s_chain, s_chain_vis, s_time; /* burst echo + unbroken chain glow/size */
static int   s_total;       /* minipoints this match */
static int   s_streak;      /* consecutive perfect catches */
static int   s_last_round;
static int   s_bonus_applied;

/* Session telemetry for Atrium Plex Logfile stats */
static int   s_stat_drops;
static float s_stat_time_sum;
static float s_stat_cover_sum;
static int   s_stat_best_streak;
static int   s_stat_lifetime_mp;

/* Solo Speedcoin mode state (180s countdown + hidden per-wave coin math) */
#define SPEEDCOIN_START_SECONDS 180.0f
#define SPEEDCOIN_STEP_SECONDS  10.0f
#define SPEEDCOIN_CLOCK_FLASH   1.15f

static int   s_sc_active;
static float s_sc_clock;
static int   s_sc_clock_live;
static int   s_sc_wave;
static int   s_sc_wave_start_mp;
static int   s_sc_wave_ten_steps;
static int   s_sc_hand2_dealt;
static int   s_sc_turn_drained;
static int   s_sc_miss_hits;
static int   s_sc_reverse_deck;
static int   s_sc_run_best_streak;
static int   s_sc_run_drops;
static float s_sc_run_time_sum;
static float s_sc_run_cover_sum;
static float s_sc_clock_flash;
static int   s_sc_last_delta;

static void speed_mode_adjust_clock(float delta_sec) {
    if (!s_sc_active) return;
    s_sc_clock += delta_sec;
    if (s_sc_clock < 0.0f) s_sc_clock = 0.0f;
    s_sc_last_delta = (int)lroundf(delta_sec);
    s_sc_clock_flash = SPEEDCOIN_CLOCK_FLASH;
}

void speed_bar_reset(void) {
    s_remaining = 0.0f; s_frac = 0.0f;
    s_armed = 0; s_started = 0;
    s_flash = 0.0f; s_pulse = 0.0f; s_ghost = 0.0f;
    s_chain = 0.0f; s_chain_vis = 0.0f; s_time = 0.0f;
    s_total = 0; s_streak = 0; s_vis_streak = 0;
    s_last_round = -1; s_bonus_applied = 0;
    s_sc_turn_drained = 0;
    s_pending_popup_ready = 0;
    memset(s_popups, 0, sizeof(s_popups));
}

void speed_mode_begin(void) {
    speed_bar_reset();
    s_sc_active = 1;
    s_sc_clock = SPEEDCOIN_START_SECONDS;
    s_sc_clock_live = 0;
    s_sc_wave = 1;
    s_sc_wave_start_mp = 0;
    s_sc_wave_ten_steps = 0;
    s_sc_hand2_dealt = 0;
    s_sc_turn_drained = 0;
    s_sc_miss_hits = 0;
    s_sc_reverse_deck = 0;
    s_sc_run_best_streak = 0;
    s_sc_run_drops = 0;
    s_sc_run_time_sum = 0.0f;
    s_sc_run_cover_sum = 0.0f;
    s_sc_clock_flash = 0.0f;
    s_sc_last_delta = 0;
}

void speed_mode_begin_wave(int wave_num) {
    s_remaining = 0.0f; s_frac = 0.0f;
    s_armed = 0; s_started = 0;
    s_streak = 0; s_vis_streak = 0; s_chain = 0.0f;
    s_pending_popup_ready = 0;
    memset(s_popups, 0, sizeof(s_popups));
    s_sc_clock_live = 0;
    s_sc_wave = (wave_num > 0) ? wave_num : 1;
    s_sc_wave_start_mp = s_total;
    s_sc_wave_ten_steps = 0;
    s_sc_hand2_dealt = 0;
    s_sc_turn_drained = 0;
    s_sc_miss_hits = 0;
    s_sc_reverse_deck = 0;
    s_last_round = s_sc_wave;
}

void speed_mode_stop(void) {
    s_sc_active = 0;
    s_sc_clock_live = 0;
    s_sc_hand2_dealt = 0;
    s_sc_turn_drained = 0;
    s_sc_miss_hits = 0;
    s_sc_reverse_deck = 0;
    s_sc_clock_flash = 0.0f;
    s_sc_last_delta = 0;
}

int   speed_mode_is_active(void) { return s_sc_active; }
float speed_mode_clock(void) { return s_sc_clock; }
int   speed_mode_wave(void) { return (s_sc_wave > 0) ? s_sc_wave : 1; }
int   speed_mode_reverse_deck(void) { return s_sc_active ? s_sc_reverse_deck : 0; }
int   speed_mode_run_best_streak(void) { return s_sc_run_best_streak; }

void speed_mode_get_run_stats(int *out_drops, float *out_avg_sec, float *out_avg_cov_pct) {
    if (out_drops) *out_drops = s_sc_run_drops;
    if (out_avg_sec) *out_avg_sec = (s_sc_run_drops > 0) ? (s_sc_run_time_sum / (float)s_sc_run_drops) : 0.0f;
    if (out_avg_cov_pct) *out_avg_cov_pct = (s_sc_run_drops > 0) ? ((s_sc_run_cover_sum / (float)s_sc_run_drops) * 100.0f) : 0.0f;
}

int speed_mode_deck_glow(void) {
    return (s_sc_active && !s_sc_hand2_dealt && (s_total - s_sc_wave_start_mp) >= SPEEDCOIN_HAND2_MP) ? 1 : 0;
}

int speed_mode_can_deal_hand2(void) {
    return speed_mode_deck_glow();
}

void speed_mode_mark_hand2_dealt(void) {
    s_sc_hand2_dealt = 1;
    s_armed = 0;
    s_started = 0;
    s_sc_turn_drained = 0;
}

float speed_mode_clock_flash(int *out_last_delta) {
    if (out_last_delta) *out_last_delta = s_sc_last_delta;
    return (s_sc_clock_flash > 0.0f) ? (s_sc_clock_flash / SPEEDCOIN_CLOCK_FLASH) : 0.0f;
}

/* Award minipoints for one hand-card capture. A perfect catch (10) starts or
   continues a serie: every perfect catch in a serie of two or more carries a
   +10 bonus point, granted retroactively to the serie's first catch too, so a
   perfect 8-card deal reads 80 + 80 = 160 minipoints = 16 extra points. */
static void award(int mp) {
    int before = s_total;
    if (mp >= 10) {
        s_streak++;
        s_total += 10;
        if (s_streak >= 2) s_total += 10;
        if (s_streak == 2) s_total += 10;
    } else {
        s_streak = 0;
        s_total += mp;
    }
    if (s_total > before) {
        s_stat_lifetime_mp += (s_total - before);
        if (s_streak > s_stat_best_streak) s_stat_best_streak = s_streak;
        if (s_sc_active && s_streak > s_sc_run_best_streak) s_sc_run_best_streak = s_streak;
        s_flash = SPEED_FLASH_TIME; s_pulse = SPEED_PULSE_TIME; s_ghost = SPEED_GHOST_TIME;
        s_chain = (s_chain < 1.0f) ? 1.0f : fminf(5.0f, s_chain + ((mp >= 10) ? 1.0f : 0.75f));
        if (s_sc_active) {
            int wave_coins = s_total - s_sc_wave_start_mp;
            int steps = (wave_coins > 0) ? (wave_coins / 10) : 0;
            if (steps > s_sc_wave_ten_steps) {
                int gained = steps - s_sc_wave_ten_steps;
                s_sc_wave_ten_steps = steps;
                speed_mode_adjust_clock((float)gained * SPEEDCOIN_STEP_SECONDS);
            }
        }
    } else s_chain = 0.0f;
}

void speed_bar_update(const GameState *g, float dt, int input_unlocked) {
    if (!g) return;
    if (g->phase == PHASE_DEALER_SELECT) { speed_bar_reset(); return; }
    if (g->current_round != s_last_round) { s_last_round = g->current_round; s_streak = 0; s_vis_streak = 0; s_chain = 0.0f; }
    s_time += dt;
    if (s_flash > 0.0f) { s_flash -= dt; if (s_flash < 0.0f) s_flash = 0.0f; }
    if (s_pulse > 0.0f) { s_pulse -= dt; if (s_pulse < 0.0f) s_pulse = 0.0f; }
    if (s_ghost > 0.0f) { s_ghost -= dt; if (s_ghost < 0.0f) s_ghost = 0.0f; }
    if (s_sc_clock_flash > 0.0f) { s_sc_clock_flash -= dt; if (s_sc_clock_flash < 0.0f) s_sc_clock_flash = 0.0f; }
    s_chain_vis += (s_chain - s_chain_vis) * (1.0f - expf(-dt * 8.0f));
    if (s_chain == 0.0f && s_chain_vis < 0.01f) s_chain_vis = 0.0f;

    int turn_live = (g->current_player == 0 && g->phase == PHASE_TURN_HAND);
    if (s_sc_active) {
        if (g->phase == PHASE_ROUND_END || g->phase == PHASE_MATCH_END) {
            s_sc_clock_live = 0;
            s_armed = 0;
            s_started = 0;
        } else if (turn_live && input_unlocked) {
            s_sc_clock_live = 1;
        }
        if (s_sc_clock_live) {
            s_sc_clock -= dt;
            if (s_sc_clock < 0.0f) s_sc_clock = 0.0f;
        }
    }
    if (turn_live && input_unlocked) {
        if (!s_started) {
            s_started = 1; s_armed = 1; s_sc_turn_drained = 0;
            s_remaining = SPEED_BAR_SECONDS; s_frac = 1.0f;
        }
        if (s_armed) {
            s_remaining -= dt;
            if (s_remaining <= 0.0f) {
                s_remaining = 0.0f;
                if (s_sc_active && !s_sc_turn_drained) {
                    s_sc_turn_drained = 1;
                    speed_mode_adjust_clock(-SPEEDCOIN_STEP_SECONDS);
                }
            }
            s_frac = s_remaining / SPEED_BAR_SECONDS;
        }
    } else if (!turn_live && s_armed) {
        /* The turn ended without a graded drop (e.g. a timeout autoplay):
           no award and the serie breaks. */
        speed_bar_consume();
        s_started = 0;
    }
}

void speed_bar_consume(void) {
    int was_armed = s_armed;
    int was_started = s_started;
    s_frac = s_sc_active ? 0.0f : (s_remaining / SPEED_BAR_SECONDS);
    if (s_sc_active) s_remaining = 0.0f;
    s_armed = 0; s_started = 0;
    s_streak = 0; s_vis_streak = 0; s_chain = 0.0f;
    s_pending_popup_ready = 0;
    if (s_sc_active && was_started) {
        s_sc_miss_hits++;
        if (s_sc_miss_hits >= 3) s_sc_reverse_deck = 1;
    }
    if (s_sc_active && was_armed && !s_sc_turn_drained) {
        s_sc_turn_drained = 1;
        speed_mode_adjust_clock(-SPEEDCOIN_STEP_SECONDS);
    }
}

static int compute_drop_mp(float cover, float *out_clamped_cover) {
    float c = cover < 0.0f ? 0.0f : (cover > 1.0f ? 1.0f : cover);
    if (out_clamped_cover) *out_clamped_cover = c;
    float half = SPEED_BAR_SECONDS * 0.5f;
    float s = (s_remaining >= half) ? 1.0f : s_remaining / half;
    float a = (cover >= 0.80f) ? 1.0f : cover / 0.80f;
    if (s <= 0.0f) a = 0.0f;
    int mp = (int)lroundf(10.0f * s * a);
    return (mp > 10) ? 10 : ((mp < 0) ? 0 : mp);
}

void speed_bar_grade(float cover) {
    float elapsed = SPEED_BAR_SECONDS - s_remaining;
    if (elapsed < 0.0f) elapsed = 0.0f;
    if (elapsed > SPEED_BAR_SECONDS) elapsed = SPEED_BAR_SECONDS;
    float clamped_cover = 0.0f;
    int mp = compute_drop_mp(cover, &clamped_cover);
    if (s_sc_active) {
        if (cover > 0.0f) {
            s_sc_miss_hits = 0;
            s_sc_reverse_deck = 0;
        } else {
            s_sc_miss_hits++;
            if (s_sc_miss_hits >= 3) s_sc_reverse_deck = 1;
        }
        s_sc_run_drops++;
        s_sc_run_time_sum += elapsed;
        s_sc_run_cover_sum += clamped_cover;
    }
    s_stat_drops++;
    s_stat_time_sum += elapsed;
    s_stat_cover_sum += clamped_cover;
    s_frac = s_remaining / SPEED_BAR_SECONDS;
    s_armed = 0; s_started = 0;
    int before = s_total;
    award(mp);
    s_pending_popup_mp = s_total - before;
    s_pending_popup_acc = (int)lroundf(clamped_cover * 100.0f);
    s_pending_popup_ready = 1;
}

void speed_bar_consume_visual(void) {
    s_armed = 0; s_started = 0; s_vis_streak = 0; s_pending_popup_ready = 0;
}

void speed_bar_grade_visual(float cover) {
    float clamped_cover = 0.0f;
    int mp = compute_drop_mp(cover, &clamped_cover);
    int gained = mp;
    if (mp >= 10) {
        s_vis_streak++;
        gained = 10 + ((s_vis_streak >= 2) ? 10 : 0) + ((s_vis_streak == 2) ? 10 : 0);
    } else s_vis_streak = 0;
    s_armed = 0; s_started = 0;
    s_pending_popup_mp = gained;
    s_pending_popup_acc = (int)lroundf(clamped_cover * 100.0f);
    s_pending_popup_ready = 1;
}

void speed_hit_popup_commit(float x, float y) {
    if (!s_pending_popup_ready) return;
    int slot = 0;
    for (int i = 0; i < HIT_POPUP_MAX; i++) {
        if (!s_popups[i].active) { slot = i; break; }
        if (s_popups[i].elapsed > s_popups[slot].elapsed) slot = i;
    }
    if (x < 56.0f) x = 56.0f; else if (x > 1140.0f) x = 1140.0f;
    if (y < 64.0f) y = 64.0f; else if (y > 660.0f) y = 660.0f;
    s_popups[slot] = (HitPopup){ 1, x, y, 0.0f, s_pending_popup_mp, s_pending_popup_acc };
    s_pending_popup_ready = 0;
}

void speed_hit_popup_discard(void) { s_pending_popup_ready = 0; }

void speed_hit_popups_update(float dt) {
    for (int i = 0; i < HIT_POPUP_MAX; i++) {
        if (!s_popups[i].active) continue;
        s_popups[i].elapsed += dt;
        if (s_popups[i].elapsed >= HIT_POPUP_DUR) s_popups[i].active = 0;
    }
}

int speed_bar_graded_drops(void) { return s_stat_drops; }
float speed_bar_avg_drop_seconds(void) { return (s_stat_drops > 0) ? (s_stat_time_sum / (float)s_stat_drops) : 0.0f; }
float speed_bar_avg_cover_pct(void) { return (s_stat_drops > 0) ? ((s_stat_cover_sum / (float)s_stat_drops) * 100.0f) : 0.0f; }
int speed_bar_best_streak(void) { return s_stat_best_streak; }
int speed_bar_lifetime_minipoints(void) { return s_stat_lifetime_mp; }

void speed_bar_get_telemetry(int *drops, float *time_sum, float *cover_sum, int *best_streak, int *lifetime_mp) {
    if (drops) *drops = s_stat_drops;
    if (time_sum) *time_sum = s_stat_time_sum;
    if (cover_sum) *cover_sum = s_stat_cover_sum;
    if (best_streak) *best_streak = s_stat_best_streak;
    if (lifetime_mp) *lifetime_mp = s_stat_lifetime_mp;
}

void speed_bar_set_telemetry(int drops, float time_sum, float cover_sum, int best_streak, int lifetime_mp) {
    s_stat_drops = (drops > 0) ? drops : 0;
    s_stat_time_sum = (time_sum >= 0.0f) ? time_sum : 0.0f;
    s_stat_cover_sum = (cover_sum >= 0.0f) ? cover_sum : 0.0f;
    s_stat_best_streak = (best_streak > 0) ? best_streak : 0;
    s_stat_lifetime_mp = (lifetime_mp > 0) ? lifetime_mp : 0;
}

int speed_bar_total(void) {
    return s_total;
}

int speed_bar_apply_match_bonus(GameState *g) {
    if (!g || g->ruleset != RULESET_KK_ADD_10 || s_bonus_applied) return 0;
    s_bonus_applied = 1;
    int bonus = s_total / 10;
    if (bonus > 0) g->total_score[0] += bonus;
    return bonus;
}

static Color mix_color(Color a, Color b, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return (Color){ (unsigned char)((float)a.r + ((float)b.r - (float)a.r) * t),
                    (unsigned char)((float)a.g + ((float)b.g - (float)a.g) * t),
                    (unsigned char)((float)a.b + ((float)b.b - (float)a.b) * t),
                    (unsigned char)((float)a.a + ((float)b.a - (float)a.a) * t) };
}

void speed_bar_draw(void) {
    Rectangle bar = SPEED_BAR_RECT;

    /* The bar itself: theme-filled box that empties from left to right, and
       flashes briefly when minipoints land. Gone the moment it runs out. */
    int bar_visible = (s_armed && s_frac > 0.0f) || (s_flash > 0.0f && s_frac > 0.0f);
    if (bar_visible) {
        DrawRectangleRec(bar, COLOR_PANEL_BG);
        float fill_w = bar.width * s_frac;
        Color fill = COLOR_PHOSPHOR;
        if (s_flash > 0.0f) {
            float ft = s_flash / SPEED_FLASH_TIME;
            float blink = (0.5f + 0.5f * sinf(s_flash * 42.0f)) * ft;
            fill = mix_color(COLOR_PHOSPHOR, (Color){ 255, 255, 255, 255 }, blink * 0.45f);
            fill.a = (unsigned char)(135.0f + 120.0f * ft);
        }
        DrawRectangle((int)(bar.x + bar.width - fill_w), (int)bar.y,
                      (int)fill_w, (int)bar.height, fill);
        ui_draw_panel_border(bar, COLOR_PHOSPHOR);
    }

    /* Gold minipoints: glyph-shaped glow (stronger on an unbroken serie, no halo),
       slight serie growth, golden flash + short larger transparent echo on gain. */
    if (s_total > 0) {
        const char *txt = TextFormat("%d", s_total);
        float serie = (s_chain_vis > 1.0f) ? fminf(1.0f, (s_chain_vis - 1.0f) * 0.25f) : 0.0f;
        float p = (s_pulse > 0.0f) ? (s_pulse / SPEED_PULSE_TIME) : 0.0f, flash = p * p;
        float grow = fminf(5.0f, s_chain_vis) * 1.10f + flash * 2.2f;
        float fsz = 22.0f + grow, sp = 1.2f + 0.05f * grow;
        float cx = bar.x + bar.width * 0.5f, cy = bar.y + bar.height * 0.5f;
        float shim = 1.0f + 0.12f * serie * sinf(s_time * 6.5f);
        static const float ox[8] = { -1.0f, 1.0f, 0.0f, 0.0f, -0.72f, 0.72f, -0.72f, 0.72f };
        static const float oy[8] = { 0.0f, 0.0f, -1.0f, 1.0f, -0.72f, -0.72f, 0.72f, 0.72f };

        if (s_ghost > 0.0f) {
            float g = s_ghost / SPEED_GHOST_TIME, u = 1.0f - g;
            float scale = 1.48f + 0.62f * (1.0f - (1.0f - u) * (1.0f - u));
            float g_fsz = fsz * scale, g_sp = sp * scale, ga = g * g;
            Vector2 gts = MeasureTextEx(font_body, txt, g_fsz, g_sp);
            Vector2 gpos = { cx - gts.x * 0.5f, cy - 0.529f * g_fsz };
            DrawTextEx(font_body, txt, gpos, g_fsz, g_sp, (Color){ 255, 182, 20, (unsigned char)(110.0f * ga) });
            if (!bar_visible) {
                BeginBlendMode(BLEND_ADDITIVE);
                DrawTextEx(font_body, txt, gpos, g_fsz, g_sp, (Color){ 255, 200, 45, (unsigned char)(80.0f * ga) });
                EndBlendMode();
            }
        }

        Vector2 ts = MeasureTextEx(font_body, txt, fsz, sp);
        Vector2 pos = { cx - ts.x * 0.5f, cy - 0.529f * fsz };
        if (grow < 0.05f) { pos.x = floorf(pos.x); pos.y = floorf(pos.y); }
        if (bar_visible) {
            for (int i = 0; i < 8; i++)
                DrawTextEx(font_body, txt, (Vector2){ pos.x + ox[i] * 1.4f, pos.y + oy[i] * 1.4f }, fsz, sp, (Color){ 16, 10, 0, 210 });
        }

        float r_out = 2.0f + 1.25f * serie + 0.75f * flash, r_in = 1.0f + 0.35f * serie;
        unsigned char a_out = (unsigned char)fminf(255.0f, (14.0f + 30.0f * serie + 28.0f * flash) * shim);
        unsigned char a_in  = (unsigned char)fminf(255.0f, (24.0f + 38.0f * serie + 36.0f * flash) * shim);
        BeginBlendMode(BLEND_ADDITIVE);
        for (int i = 0; i < 8; i++) {
            DrawTextEx(font_body, txt, (Vector2){ pos.x + ox[i] * r_out, pos.y + oy[i] * r_out }, fsz, sp, (Color){ 255, 138, 0, a_out });
            DrawTextEx(font_body, txt, (Vector2){ pos.x + ox[i] * r_in,  pos.y + oy[i] * r_in  }, fsz, sp, (Color){ 255, 168, 16, a_in  });
        }
        EndBlendMode();

        Color base_gold = mix_color((Color){ 255, 188, 18, 255 }, (Color){ 255, 212, 65, 255 }, serie);
        DrawTextEx(font_body, txt, pos, fsz, sp, mix_color(base_gold, (Color){ 255, 232, 110, 255 }, flash));
        if (!bar_visible) {
            BeginBlendMode(BLEND_ADDITIVE);
            DrawTextEx(font_body, txt, pos, fsz, sp, (Color){ 255, 196, 38, (unsigned char)fminf(255.0f, (36.0f + 78.0f * serie + 125.0f * flash) * shim) });
            EndBlendMode();
        }
    }
}

static void draw_hit_num(const char *txt, float cx, float cy, float fsz, Color core, Color hi, Color glow, Color outl, float flash, float alpha) {
    static const float ox[8] = { -1.0f, 1.0f, 0.0f, 0.0f, -0.72f, 0.72f, -0.72f, 0.72f };
    static const float oy[8] = { 0.0f, 0.0f, -1.0f, 1.0f, -0.72f, -0.72f, 0.72f, 0.72f };
    float sp = 1.2f;
    Vector2 ts = MeasureTextEx(font_body, txt, fsz, sp);
    Vector2 pos = { cx - ts.x * 0.5f, cy - 0.529f * fsz };
    outl.a = (unsigned char)((float)outl.a * alpha);
    for (int i = 0; i < 8; i++)
        DrawTextEx(font_body, txt, (Vector2){ pos.x + ox[i] * 1.6f, pos.y + oy[i] * 1.6f }, fsz, sp, outl);
    BeginBlendMode(BLEND_ADDITIVE);
    Color gcol = glow;
    gcol.a = (unsigned char)fminf(255.0f, (58.0f + 70.0f * flash) * alpha);
    for (int i = 0; i < 8; i++)
        DrawTextEx(font_body, txt, (Vector2){ pos.x + ox[i] * 2.1f, pos.y + oy[i] * 2.1f }, fsz, sp, gcol);
    EndBlendMode();
    Color fill = mix_color(core, hi, flash);
    fill.a = (unsigned char)(255.0f * alpha);
    DrawTextEx(font_body, txt, pos, fsz, sp, fill);
    BeginBlendMode(BLEND_ADDITIVE);
    Color boost = glow;
    boost.a = (unsigned char)fminf(255.0f, (78.0f + 92.0f * flash) * alpha);
    DrawTextEx(font_body, txt, pos, fsz, sp, boost);
    EndBlendMode();
}

void speed_hit_popups_draw(void) {
    for (int i = 0; i < HIT_POPUP_MAX; i++) {
        if (!s_popups[i].active) continue;
        float t = s_popups[i].elapsed;
        if (t < 0.0f || t >= HIT_POPUP_DUR) continue;
        float fu = (t > HIT_POPUP_HOLD) ? ((t - HIT_POPUP_HOLD) / HIT_POPUP_FLY) : 0.0f;
        float rise = 64.0f * fu * fu * (3.0f - 2.0f * fu);
        float pop = (t < 0.11f) ? (1.0f + 0.34f * (1.0f - t / 0.11f) * (1.0f - t / 0.11f)) : 1.0f;
        float flash = (t < 0.18f) ? (1.0f - t / 0.18f) : 0.0f;
        float alpha = (t < 0.03f) ? (t / 0.03f) : ((fu > 0.35f) ? ((1.0f - fu) / 0.65f) : 1.0f);
        alpha = alpha * (2.0f - alpha);
        float cy = s_popups[i].y - 10.0f - rise;
        float mp_sz = ((s_popups[i].mp >= 20) ? 28.0f : 25.0f) * pop;
        draw_hit_num(TextFormat("+%d", s_popups[i].mp), s_popups[i].x, cy - 13.0f, mp_sz,
                     (Color){ 255, 224, 78, 255 }, (Color){ 255, 252, 220, 255 },
                     (Color){ 255, 192, 36, 255 }, (Color){ 18, 10, 0, 236 }, flash, alpha);
        draw_hit_num(TextFormat("%d%%", s_popups[i].acc), s_popups[i].x, cy + 12.0f, 19.5f * pop,
                     (Color){ 255, 96, 232, 255 }, (Color){ 255, 228, 252, 255 },
                     (Color){ 255, 58, 212, 255 }, (Color){ 20, 0, 18, 236 }, flash, alpha);
    }
}
