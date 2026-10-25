#include "ui_speed.h"
#include "ui_theme.h"
#include <math.h>

extern Font font_body;

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
static int   s_sc_run_best_streak;
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
    s_total = 0; s_streak = 0;
    s_last_round = -1; s_bonus_applied = 0;
    s_sc_turn_drained = 0;
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
    s_sc_run_best_streak = 0;
    s_sc_clock_flash = 0.0f;
    s_sc_last_delta = 0;
}

void speed_mode_begin_wave(int wave_num) {
    s_remaining = 0.0f; s_frac = 0.0f;
    s_armed = 0; s_started = 0;
    s_streak = 0; s_chain = 0.0f;
    s_sc_wave = (wave_num > 0) ? wave_num : 1;
    s_sc_wave_start_mp = s_total;
    s_sc_wave_ten_steps = 0;
    s_sc_hand2_dealt = 0;
    s_sc_turn_drained = 0;
    s_last_round = s_sc_wave;
}

void speed_mode_stop(void) {
    s_sc_active = 0;
    s_sc_clock_live = 0;
    s_sc_hand2_dealt = 0;
    s_sc_turn_drained = 0;
    s_sc_clock_flash = 0.0f;
    s_sc_last_delta = 0;
}

int   speed_mode_is_active(void) { return s_sc_active; }
float speed_mode_clock(void) { return s_sc_clock; }
int   speed_mode_wave(void) { return (s_sc_wave > 0) ? s_sc_wave : 1; }
int   speed_mode_run_best_streak(void) { return s_sc_run_best_streak; }

int speed_mode_deck_glow(void) {
    return (s_sc_active && !s_sc_hand2_dealt && (s_total - s_sc_wave_start_mp) >= 50) ? 1 : 0;
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
    if (g->current_round != s_last_round) { s_last_round = g->current_round; s_streak = 0; s_chain = 0.0f; }
    s_time += dt;
    if (s_flash > 0.0f) { s_flash -= dt; if (s_flash < 0.0f) s_flash = 0.0f; }
    if (s_pulse > 0.0f) { s_pulse -= dt; if (s_pulse < 0.0f) s_pulse = 0.0f; }
    if (s_ghost > 0.0f) { s_ghost -= dt; if (s_ghost < 0.0f) s_ghost = 0.0f; }
    if (s_sc_clock_flash > 0.0f) { s_sc_clock_flash -= dt; if (s_sc_clock_flash < 0.0f) s_sc_clock_flash = 0.0f; }
    s_chain_vis += (s_chain - s_chain_vis) * (1.0f - expf(-dt * 8.0f));
    if (s_chain == 0.0f && s_chain_vis < 0.01f) s_chain_vis = 0.0f;

    int turn_live = (g->current_player == 0 && g->phase == PHASE_TURN_HAND);
    if (s_sc_active) {
        if (turn_live && input_unlocked) s_sc_clock_live = 1;
        if (s_sc_clock_live && g->phase != PHASE_MATCH_END) {
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
    s_frac = s_sc_active ? 0.0f : (s_remaining / SPEED_BAR_SECONDS);
    if (s_sc_active) s_remaining = 0.0f;
    s_armed = 0; s_started = 0;
    s_streak = 0; s_chain = 0.0f;
    if (s_sc_active && was_armed && !s_sc_turn_drained) {
        s_sc_turn_drained = 1;
        speed_mode_adjust_clock(-SPEEDCOIN_STEP_SECONDS);
    }
}

void speed_bar_grade(float cover) {
    float elapsed = SPEED_BAR_SECONDS - s_remaining;
    if (elapsed < 0.0f) elapsed = 0.0f;
    if (elapsed > SPEED_BAR_SECONDS) elapsed = SPEED_BAR_SECONDS;
    float clamped_cover = cover < 0.0f ? 0.0f : (cover > 1.0f ? 1.0f : cover);
    s_stat_drops++;
    s_stat_time_sum += elapsed;
    s_stat_cover_sum += clamped_cover;
    float half = SPEED_BAR_SECONDS * 0.5f;
    float s = (s_remaining >= half) ? 1.0f : s_remaining / half;
    float a = (cover >= 0.80f) ? 1.0f : cover / 0.80f;
    if (s <= 0.0f) a = 0.0f;
    int mp = (int)lroundf(10.0f * s * a);
    if (mp > 10) mp = 10;
    if (mp < 0) mp = 0;
    s_frac = s_remaining / SPEED_BAR_SECONDS;
    s_armed = 0; s_started = 0;
    award(mp);
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
