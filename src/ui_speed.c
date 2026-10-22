#include "ui_speed.h"
#include "ui_theme.h"
#include <math.h>

extern Font font_body;

#define SPEED_BAR_SECONDS 0.30f
#define SPEED_BAR_RECT    (Rectangle){ 1160.0f, 662.0f, 112.0f, 40.0f }
#define SPEED_FLASH_TIME  0.30f
#define SPEED_PULSE_TIME  0.60f

static float s_remaining;   /* seconds left on this turn's bar */
static float s_frac;        /* visible fill fraction (frozen at drop for the flash) */
static int   s_armed;       /* bar is running */
static int   s_started;     /* bar already armed for this turn */
static float s_flash;       /* bar flash timer after an award */
static float s_pulse;       /* gold numbers pulse timer after an award */
static int   s_total;       /* minipoints this match */
static int   s_streak;      /* consecutive perfect catches */
static int   s_last_round;
static int   s_bonus_applied;

void speed_bar_reset(void) {
    s_remaining = 0.0f; s_frac = 0.0f;
    s_armed = 0; s_started = 0;
    s_flash = 0.0f; s_pulse = 0.0f;
    s_total = 0; s_streak = 0;
    s_last_round = -1; s_bonus_applied = 0;
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
    if (s_total > before) { s_flash = SPEED_FLASH_TIME; s_pulse = SPEED_PULSE_TIME; }
}

void speed_bar_update(const GameState *g, float dt, int input_unlocked) {
    if (!g) return;
    if (g->phase == PHASE_DEALER_SELECT) { speed_bar_reset(); return; }
    if (g->current_round != s_last_round) { s_last_round = g->current_round; s_streak = 0; }
    if (s_flash > 0.0f) { s_flash -= dt; if (s_flash < 0.0f) s_flash = 0.0f; }
    if (s_pulse > 0.0f) { s_pulse -= dt; if (s_pulse < 0.0f) s_pulse = 0.0f; }

    int turn_live = (g->current_player == 0 && g->phase == PHASE_TURN_HAND);
    if (turn_live && input_unlocked) {
        if (!s_started) {
            s_started = 1; s_armed = 1;
            s_remaining = SPEED_BAR_SECONDS; s_frac = 1.0f;
        }
        if (s_armed) {
            s_remaining -= dt;
            if (s_remaining < 0.0f) s_remaining = 0.0f;
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
    s_frac = s_remaining / SPEED_BAR_SECONDS;
    s_armed = 0; s_started = 0;
    s_streak = 0;
}

void speed_bar_grade(float cover) {
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
            float blink = (0.5f + 0.5f * sinf(s_flash * 42.0f)) * (s_flash / SPEED_FLASH_TIME);
            fill = mix_color(COLOR_PHOSPHOR, (Color){ 255, 255, 255, 255 }, blink * 0.85f);
        }
        DrawRectangle((int)(bar.x + bar.width - fill_w), (int)bar.y,
                      (int)fill_w, (int)bar.height, fill);
        ui_draw_panel_border(bar, COLOR_PHOSPHOR);
    }

    /* The gold minipoints over the bar - always the same size and spot, and
       still there after the bar is gone. Pulses when new ones are added. */
    if (s_total > 0) {
        Color gold = COLOR_AMBER;
        if (s_pulse > 0.0f) {
            float p = s_pulse / SPEED_PULSE_TIME;
            float wob = 0.55f + 0.45f * sinf(s_pulse * 36.0f);
            gold = mix_color(COLOR_AMBER, (Color){ 255, 255, 255, 255 }, 0.55f * p * wob);
        }
        const char *txt = TextFormat("%d", s_total);
        Vector2 ts = MeasureTextEx(font_body, txt, 22.0f, 1.2f);
        Vector2 pos = { bar.x + (bar.width - ts.x) / 2.0f, bar.y + (bar.height - ts.y) / 2.0f };
        DrawTextEx(font_body, txt, (Vector2){ floorf(pos.x), floorf(pos.y) }, 22.0f, 1.2f, gold);
    }
}
