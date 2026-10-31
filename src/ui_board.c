#include "ui_board.h"
#include "ui_action.h"
#include "ui_card_tooltip.h"
#include "ui_capture_info.h"
#include "board_layout.h"
#include "ui_motion.h"
#include "ui_board_status.h"
#include "ui_combos.h"
#include "ui_audio.h"
#include "ui_speed.h"
#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

extern Font font_title;
extern Font font_body;
extern GameSettings settings;

static void draw_text_ex_snapped(Font font, const char *text, Vector2 pos, float fsz, float sp, Color col) {
    DrawTextEx(font, text, (Vector2){ floorf(pos.x), floorf(pos.y) }, fsz, sp, col);
}

static void draw_centered_text(Font font, const char *text, float cx, float y, float fsz, float sp, Color col) {
    Vector2 ts = MeasureTextEx(font, text, fsz, sp);
    draw_text_ex_snapped(font, text, (Vector2){ cx - ts.x / 2.0f, y }, fsz, sp, col);
}

static void draw_text_in_rect(Font font, const char *text, Rectangle b, float fsz, float sp, Color col) {
    Vector2 ts = MeasureTextEx(font, text, fsz, sp);
    draw_text_ex_snapped(font, text, (Vector2){ b.x + (b.width - ts.x) / 2.0f, b.y + (b.height - fsz) / 2.0f }, fsz, sp, col);
}

#define ASH_COUNT 100
#define ASH_BURST_MAX 260

/* Ember heat on the ash field. Driven by the same capture / Koi-Koi edges that drive the
   reverb envelope, but kept in its own state so it is NOT reset per round outside
   action-card mode (see the round_reset block in board_draw_table). Tune here only.
   Below EMBER_MIN the particles are drawn byte-for-byte as they always were.

   Note EMBER_KICK_CAP is PER CARD, and the smallest capture event takes two cards (the
   played card plus the field card it matched), so one ordinary capture adds twice this.
   A four-card capture adds four times. Calibrate against that, not against "one capture". */
#define EMBER_MIN        0.08f  /* heat below this stays cold dark ash                */
#define EMBER_HOT        0.50f  /* heat at/above this is drawn in the additive pass   */
#define EMBER_KICK_CAP   0.08f  /* heat per captured card: one capture = 0.20         */
#define EMBER_KICK_KOI   0.10f  /* heat added on a Koi-Koi call                       */
#define EMBER_DECAY      0.06f  /* linear fall per second - slow, so heat accumulates  */
#define EMBER_SMOOTH_ATK 0.22f  /* one-pole up, seconds - the glow swells, never steps */
#define EMBER_SMOOTH_REL 0.55f  /* one-pole down, seconds - embers cool slowly         */
#define EMBER_SIZE_BONUS 0.12f  /* extra heat on the largest fragments (8-10 px)      */
/* Bursts are scaled from the field, not offset above it, so a faint glow gives faint
   sparks. An additive offset made even the first capture burst go straight to HOT. */
#define EMBER_BURST_GAIN 1.35f  /* burst heat = field heat * this                     */
#define EMBER_YOUTH_GAIN 1.25f  /* further scale while life/max_life > 0.60           */

typedef struct {
    Vector2 pos;
    float speed_x, speed_y, sway_speed, sway_amp;
    int size_w, size_h;
    int shape;
    unsigned char alpha;
    float phase;
} AshParticle;

typedef struct {
    Vector2 pos;
    Vector2 vel;
    int card_id;
    int size_w, size_h;
    int shape;
    unsigned char base_alpha;
    float follow;
    float phase;
    float sway_speed;
    float life;
    float max_life;
    int active;
} AshBurstParticle;

static AshParticle g_ash[ASH_COUNT];
static AshBurstParticle g_ash_burst[ASH_BURST_MAX];
static int g_ash_initialized = 0;
/* Written in board_draw_table where the capture edges are detected, read by both ash draw
   passes. Both passes run later in the same frame (ash_draw_and_update at the background
   step, ash_burst_draw_and_update after the field cards), so they see this frame's value. */
static float s_ember_heat = 0.0f;
static float s_ember_glow = 0.0f;
static int s_audio_action_triggers = 0;
static float s_audio_peak_synergy = 0.0f;
static float s_audio_full_hold_sec = 0.0f;
static uint64_t s_board_blind_mask = 0ULL;
static int s_board_distraction_high = 0;

int   board_get_audio_action_triggers(void)   { return s_audio_action_triggers; }
float board_get_audio_peak_synergy(void)      { return s_audio_peak_synergy; }
float board_get_audio_full_hold_seconds(void) { return s_audio_full_hold_sec; }
void  board_set_audio_telemetry(int triggers, float peak, float full_hold_sec) {
    s_audio_action_triggers = (triggers > 0) ? triggers : 0;
    s_audio_peak_synergy = (peak >= 0.0f) ? (peak > 1.0f ? 1.0f : peak) : 0.0f;
    s_audio_full_hold_sec = (full_hold_sec >= 0.0f) ? full_hold_sec : 0.0f;
}
void  board_sync_action_card_mask(uint64_t blind_mask, int distraction_high) {
    s_board_blind_mask = blind_mask;
    s_board_distraction_high = distraction_high ? 1 : 0;
}
int   board_is_blind_field_card(int card_id) {
    return ((unsigned)card_id < 48u) ? (int)((s_board_blind_mask >> card_id) & 1ULL) : 0;
}
int   board_is_action_distraction_high(void) {
    return s_board_distraction_high;
}

static void pick_dark_particle_dims(int roll, int *out_w, int *out_h) {
    /* Wide variety of particle sizes: fine dust (1-2px), medium flakes (3-4px),
       large dark embers (5-7px), and occasional big fragments (8-10px). */
    int base;
    if (roll < 38)      base = 1 + (roll & 1);          /* 1..2 px */
    else if (roll < 72) base = 3 + (roll % 2);          /* 3..4 px */
    else if (roll < 92) base = 5 + (roll % 3);          /* 5..7 px */
    else                base = 8 + (roll % 3);          /* 8..10 px */
    *out_w = base;
    *out_h = (base >= 5 && (roll & 1)) ? (base - 1) : base;
}

static void draw_corrupted_shard(int x, int y, int sz, int shape, Color col) {
    if (sz <= 1) {
        DrawRectangle(x, y, 1, 1, col);
        return;
    }
    if (sz == 2) {
        switch (shape & 3) {
            case 0: DrawRectangle(x, y, 2, 1, col); DrawRectangle(x + 1, y + 1, 1, 1, col); break;
            case 1: DrawRectangle(x, y, 1, 2, col); DrawRectangle(x + 1, y, 1, 1, col); break;
            case 2: DrawRectangle(x, y, 1, 1, col); DrawRectangle(x + 1, y + 1, 2, 1, col); break;
            default: DrawRectangle(x + 1, y, 1, 2, col); DrawRectangle(x, y + 1, 1, 1, col); break;
        }
        return;
    }
    int t = (sz >= 7) ? 2 : 1;
    int s1 = (sz * 2 + 1) / 3;
    int s2 = (sz + 1) / 2;
    int d = (sz >= 6) ? 2 : 1;
    switch (shape & 7) {
        case 0: /* Jagged diagonal slash \ */
            DrawRectangle(x, y, s1, t, col);
            DrawRectangle(x + d, y + t, s2 + 1, t + 1, col);
            DrawRectangle(x + 2 * d, y + 2 * t + 1, s1, t, col);
            DrawRectangle(x + d - 1, y + 2 * t, t, t + 1, col);
            break;
        case 1: /* Corrupted L-claw / hook */
            DrawRectangle(x, y, t, sz, col);
            DrawRectangle(x + t, y + 1, s1, t, col);
            DrawRectangle(x - t, y + sz - t - 1, t + 1, t, col);
            DrawRectangle(x + s2, y + 2, t, s2, col);
            break;
        case 2: /* Fractured glitch cluster with hollow notch */
            DrawRectangle(x, y, s1 + 1, t, col);
            DrawRectangle(x + d, y + t + 1, s2, t, col);
            DrawRectangle(x - 1, y + 2 * t + 1, s1, t, col);
            DrawRectangle(x + s1, y - 1, t, t + 1, col);
            break;
        case 3: /* Biomechanical thorn / skewed cross-rib */
            DrawRectangle(x + d, y, t, sz + 1, col);
            DrawRectangle(x, y + d, sz, t, col);
            DrawRectangle(x + sz - t, y + d + t, t, s2, col);
            DrawRectangle(x - 1, y + sz - 1, t + 1, t, col);
            break;
        case 4: /* Jagged reverse slash / */
            DrawRectangle(x + 2 * d, y, s1, t, col);
            DrawRectangle(x + d, y + t, s2 + 1, t + 1, col);
            DrawRectangle(x, y + 2 * t + 1, s1, t, col);
            DrawRectangle(x + s1 + d, y + t + 1, t, t + 1, col);
            break;
        case 5: /* Torn C-husk shard */
            DrawRectangle(x + 1, y, s1, t, col);
            DrawRectangle(x, y + t, t, s1, col);
            DrawRectangle(x + t, y + t + s1 - 1, s2 + 1, t, col);
            DrawRectangle(x + s1 + 1, y + 1, t, t + 1, col);
            break;
        case 6: /* Stepped Z-splinter */
            DrawRectangle(x, y, s2 + 1, t, col);
            DrawRectangle(x + s2, y + t, t, s2 + 1, col);
            DrawRectangle(x + s2 + t, y + s2 + t, s2 + 1, t, col);
            DrawRectangle(x + 1, y + t + 1, t, t, col);
            break;
        default: /* Shattered triad with orbiting speck */
            DrawRectangle(x + 1, y + 1, s1, t + 1, col);
            DrawRectangle(x + d, y + t + 2, t + 1, s2, col);
            DrawRectangle(x - 1, y - 1, t, t + 1, col);
            DrawRectangle(x + s1 + 1, y + s2 + 1, t + 1, t, col);
            break;
    }
}

/* Dark ash -> dull red -> orange -> near-white, as heat goes 0 -> 1. */
static Color ember_ramp_color(float h) {
    if (h <= 0.0f) return (Color){ 5, 5, 5, 255 };
    if (h >= 1.0f) return (Color){ 255, 224, 168, 255 };
    static const Color stop[4] = {
        {   5,   5,   5, 255 },   /* cold - identical to the ash that was always drawn */
        { 118,  30,  10, 255 },   /* deep ember red                                    */
        { 236, 112,  34, 255 },   /* orange fire                                         */
        { 255, 224, 168, 255 },   /* hot core                                            */
    };
    float t = h * 3.0f;
    int i = (int)t;
    if (i > 2) i = 2;
    float f = t - (float)i;
    Color a = stop[i], b = stop[i + 1];
    return (Color){ (unsigned char)(a.r + (b.r - a.r) * f),
                    (unsigned char)(a.g + (b.g - a.g) * f),
                    (unsigned char)(a.b + (b.b - a.b) * f), 255 };
}

/* heat < EMBER_MIN reproduces the original dark particle exactly: a drop-shadow halo at
   95/255 of alpha offset by 1px, then the near-black core. At or above EMBER_HOT the halo
   is promoted to a bloom and the caller draws this in an additive pass, so overlapping
   embers stack toward white instead of occluding each other. */
static void draw_dark_particle(int x, int y, int w, int h, int shape, unsigned char alpha, float heat) {
    if (alpha == 0) return;
    int sz = (w > h) ? w : h;
    if (heat < EMBER_MIN) {
        if (sz >= 4) {
            unsigned char halo_a = (unsigned char)((unsigned int)alpha * 95u / 255u);
            if (halo_a > 0) {
                draw_corrupted_shard(x + 1, y + 1, sz + 1, shape, (Color){ 4, 4, 4, halo_a });
            }
        }
        draw_corrupted_shard(x, y, sz, shape, (Color){ 5, 5, 5, alpha });
        return;
    }
    if (heat > 1.0f) heat = 1.0f;
    Color core = ember_ramp_color(heat);
    core.a = alpha;
    if (sz >= 2) {
        Color bloom = core;
        bloom.r = (unsigned char)(bloom.r * 58u / 100u);
        bloom.g = (unsigned char)(bloom.g * 32u / 100u);
        bloom.b = (unsigned char)(bloom.b * 20u / 100u);
        bloom.a = (unsigned char)((unsigned int)alpha * (62u + (unsigned int)(76.0f * heat)) / 255u);
        draw_corrupted_shard(x - 1, y - 1, sz + 2, shape, bloom);
        if (heat >= EMBER_HOT) draw_corrupted_shard(x, y, sz + 1, shape, bloom);
    }
    draw_corrupted_shard(x, y, sz, shape, core);
}

static void ash_init(void) {
    for (int i = 0; i < ASH_COUNT; i++) {
        int roll = rand() % 100;
        pick_dark_particle_dims(roll, &g_ash[i].size_w, &g_ash[i].size_h);
        g_ash[i].shape = rand() & 7;
        g_ash[i].pos = (Vector2){ (float)(rand() % 1280), (float)(rand() % 720) };
        g_ash[i].speed_y = 11.0f + (float)(rand() % 28) + (float)g_ash[i].size_w * 1.4f;
        g_ash[i].speed_x = ((float)(rand() % 41) - 20.0f) * 0.65f;
        g_ash[i].sway_speed = 0.7f + (float)(rand() % 110) / 70.0f;
        g_ash[i].sway_amp = 9.0f + (float)(rand() % 20);
        float a = 0.24f + (float)(rand() % 38) / 100.0f;
        g_ash[i].alpha = (unsigned char)(255.0f * a);
        g_ash[i].phase = (float)(rand() % 628) / 100.0f;
    }
    for (int i = 0; i < ASH_BURST_MAX; i++) g_ash_burst[i].active = 0;
    g_ash_initialized = 1;
}

static void ash_spawn_capture_burst(const GameState *g, int streak) {
    if (!g_ash_initialized) ash_init();
    if (!g || g->last_captured_count <= 0) return;
    if (streak < 1) streak = 1;
    if (streak > 5) streak = 5;

    Rectangle table_rect = { 640.0f, 240.0f, FIELD_CARD_W, FIELD_CARD_H };
    for (int k = g->last_captured_count - 1; k >= 0; k--) {
        Rectangle tr = motion_get_rect(g->last_captured[k]);
        if (tr.width > 0.0f && tr.y >= 60.0f && tr.y <= 500.0f) {
            table_rect = tr;
            break;
        }
    }

    int per_card = 28 + (streak - 1) * 24;
    float spd_scale = 1.0f + 0.16f * (float)(streak - 1);
    int size_bonus = (streak >= 3) ? 2 : (streak >= 2) ? 1 : 0;

    int slot = 0;
    for (int k = 0; k < g->last_captured_count; k++) {
        int cid = g->last_captured[k];
        Rectangle r = motion_get_rect(cid);
        if (r.width <= 0.0f || k == 0 || r.y > 460.0f || r.y < 60.0f) r = table_rect;
        int rw = (int)fmaxf(1.0f, r.width);
        int rh = (int)fmaxf(1.0f, r.height);

        for (int n = 0; n < per_card; n++) {
            AshBurstParticle *p = NULL;
            for (int i = 0; i < ASH_BURST_MAX; i++) {
                int idx = (slot + i) % ASH_BURST_MAX;
                if (!g_ash_burst[idx].active) { p = &g_ash_burst[idx]; slot = idx + 1; break; }
            }
            if (!p) { p = &g_ash_burst[slot % ASH_BURST_MAX]; slot++; }

            int roll = rand() % 100;
            pick_dark_particle_dims(roll, &p->size_w, &p->size_h);
            if (roll >= 85) { p->size_w += 1 + size_bonus; p->size_h += 1 + size_bonus; }
            else if (roll >= 55 && size_bonus > 0) { p->size_w += 1; p->size_h += 1; }
            p->shape = rand() & 7;

            float ang = (float)(rand() % 628) / 100.0f;
            float spd = (34.0f + (float)(rand() % 108)) * spd_scale;
            p->pos = (Vector2){ r.x + (float)(rand() % rw), r.y + (float)(rand() % rh) };
            p->vel = (Vector2){ cosf(ang) * spd, sinf(ang) * spd - 14.0f };
            p->card_id = cid;
            p->base_alpha = (unsigned char)(200 + (rand() % 52));
            p->follow = 0.22f + (float)(rand() % 22) / 100.0f;
            p->phase = (float)(rand() % 628) / 100.0f;
            p->sway_speed = 2.2f + (float)(rand() % 30) / 10.0f;
            p->max_life = 0.95f + (float)(rand() % 55) / 100.0f + 0.08f * (float)(streak - 1);
            p->life = p->max_life;
            p->active = 1;
        }
    }
}

static void ash_draw_and_update(float dt) {
    if (!g_ash_initialized) ash_init();
    float cur_time = (float)GetTime();
    /* Slow gusting wind: three incommensurate waves so the ash field slides
       right for a while, then left, then crosses quicker - wind, not a
       straight-up fall line. Vertical gusts only breathe the rise rate. */
    float wind_x = sinf(cur_time * 0.110f) * 12.0f
                 + sinf(cur_time * 0.043f + 1.7f) * 16.0f
                 + sinf(cur_time * 0.270f + 4.1f) * 4.0f + 30.0f;
    float wind_y = sinf(cur_time * 0.090f + 2.2f) * 3.5f;
    for (int i = 0; i < ASH_COUNT; i++) {
        AshParticle *p = &g_ash[i];
        float wind_catch = 0.55f + 0.45f * sinf(p->phase);
        p->pos.y -= (p->speed_y + wind_y * wind_catch) * dt;
        p->pos.x += (p->speed_x + wind_x * wind_catch) * dt;
        if (p->pos.y < -14.0f) {
            p->pos.y = 730.0f + (float)(rand() % 30);
            p->pos.x = (float)(rand() % 1280);
        }
        if (p->pos.x < -20.0f) p->pos.x += 1320.0f;
        else if (p->pos.x > 1300.0f) p->pos.x -= 1320.0f;
        if (p->alpha == 0) continue;
        float sway_x = p->pos.x + sinf(p->phase + cur_time * p->sway_speed) * p->sway_amp;
        int rot_shape = (p->shape + (int)(cur_time * 1.8f + p->phase)) & 7;
        /* The big fragments (8-10 px) catch more heat than the fine dust, so the field
           does not light up uniformly. */
        float heat = s_ember_glow + ((p->size_w >= 8) ? EMBER_SIZE_BONUS : 0.0f);
        if (heat > 1.0f) heat = 1.0f;
        if (heat >= EMBER_HOT) continue;   /* drawn in the additive pass below */
        draw_dark_particle((int)sway_x, (int)p->pos.y, p->size_w, p->size_h, rot_shape, p->alpha, heat);
    }
    /* One blend switch per frame for the whole hot set, rather than two per particle. */
    if (s_ember_glow >= EMBER_MIN) {
        BeginBlendMode(BLEND_ADDITIVE);
        for (int i = 0; i < ASH_COUNT; i++) {
            AshParticle *p = &g_ash[i];
            if (p->alpha == 0) continue;
            float heat = s_ember_glow + ((p->size_w >= 8) ? EMBER_SIZE_BONUS : 0.0f);
            if (heat > 1.0f) heat = 1.0f;
            if (heat < EMBER_HOT) continue;
            float sway_x = p->pos.x + sinf(p->phase + cur_time * p->sway_speed) * p->sway_amp;
            int rot_shape = (p->shape + (int)(cur_time * 1.8f + p->phase)) & 7;
            draw_dark_particle((int)sway_x, (int)p->pos.y, p->size_w, p->size_h, rot_shape, p->alpha, heat);
        }
        EndBlendMode();
    }
}

/* A burst particle's heat for this frame. Pass 1 runs the physics and draws the cold
   particles; pass 2 redraws only the hot ones additively. Heat depends on nothing pass 2
   changes (life and size are already final), so both passes compute the same value. */
static float ash_burst_heat(const AshBurstParticle *p, float frac) {
    if (s_ember_glow < EMBER_MIN) return 0.0f;
    /* Multiplicative, not additive: sparks are only as hot as the fire that threw them.
       With an additive boost the very first capture already cleared EMBER_HOT and every
       burst went to the additive pass, which is what made the effect look over-driven. */
    float heat = s_ember_glow * EMBER_BURST_GAIN;
    if (frac > 0.60f) heat *= EMBER_YOUTH_GAIN;    /* youngest sparks are the hottest */
    if (p->size_w >= 8) heat += EMBER_SIZE_BONUS;
    return (heat > 1.0f) ? 1.0f : heat;
}

static void ash_burst_draw_and_update(float dt) {
    if (!g_ash_initialized) return;
    float cur_time = (float)GetTime();
    float damp = expf(-3.4f * dt);
    for (int i = 0; i < ASH_BURST_MAX; i++) {
        AshBurstParticle *p = &g_ash_burst[i];
        if (!p->active) continue;
        p->life -= dt;
        if (p->life <= 0.0f) { p->active = 0; continue; }

        p->vel.x *= damp;
        p->vel.y *= damp;
        Rectangle cr = motion_get_rect(p->card_id);
        if (cr.width > 0.0f) {
            float cx = cr.x + cr.width * 0.5f, cy = cr.y + cr.height * 0.5f;
            p->vel.x += (cx - p->pos.x) * p->follow * 4.5f * dt;
            p->vel.y += (cy - p->pos.y) * p->follow * 4.5f * dt;
        }
        p->pos.x += (p->vel.x + cosf(p->phase + cur_time * p->sway_speed) * 14.0f) * dt;
        p->pos.y += (p->vel.y - 16.0f) * dt;

        float frac = p->life / p->max_life;
        float fade = (frac < 0.35f) ? (frac / 0.35f) : 1.0f;
        unsigned char a = (unsigned char)((float)p->base_alpha * fade);
        int rot_shape = (p->shape + (int)(cur_time * 3.2f + p->phase)) & 7;
        float heat = ash_burst_heat(p, frac);
        if (heat >= EMBER_HOT) continue;   /* drawn in the additive pass below */
        draw_dark_particle((int)p->pos.x, (int)p->pos.y, p->size_w, p->size_h, rot_shape, a, heat);
    }
    if (s_ember_glow >= EMBER_MIN) {
        BeginBlendMode(BLEND_ADDITIVE);
        for (int i = 0; i < ASH_BURST_MAX; i++) {
            AshBurstParticle *p = &g_ash_burst[i];
            if (!p->active) continue;
            float frac = p->life / p->max_life;
            float heat = ash_burst_heat(p, frac);
            if (heat < EMBER_HOT) continue;
            float fade = (frac < 0.35f) ? (frac / 0.35f) : 1.0f;
            unsigned char a = (unsigned char)((float)p->base_alpha * fade);
            int rot_shape = (p->shape + (int)(cur_time * 3.2f + p->phase)) & 7;
            draw_dark_particle((int)p->pos.x, (int)p->pos.y, p->size_w, p->size_h, rot_shape, a, heat);
        }
        EndBlendMode();
    }
}

static char path_buf[512];
static int s_table_selected = 0;   /* settings.table_design, refreshed per table frame */

/* Mirror of BoardAssets::table_skins[].ready, one flag per slot: the draw paths take a
   const BoardAssets, and the headless UI tests link them without loading any assets. */
static int s_table_ready[BOARD_TABLE_SLOTS] = { 0 };
static Texture2D s_table_bezel[BOARD_TABLE_SLOTS];
static Shader s_bezel_shine_shader = { 0 };
static int s_bezel_shine_phase_loc = -1;
static float s_bezel_phase_acc[2] = { 0.0f, 0.0f };
static float s_bezel_phase_stamp = -1.0f;
static void (*s_draw_bezel_shine_fn)(Rectangle dest, float pulse_alpha) = NULL;

static int s_table_skin_ready(void) {
    if (s_table_selected < 0 || s_table_selected >= BOARD_TABLE_SLOTS) return 0;
    return s_table_ready[s_table_selected];
}

static int s_bezel_muted = 0;      /* presentation draws (capture-info preview) opt out */

/* One map folder per slot, indexed by the TABLE_DESIGN_* ids in storage.h. Slot 0 is the
   CRT table and has no files. The order here MUST match storage.h. */
static const char *const k_table_map_dirs[BOARD_TABLE_SLOTS] = {
    NULL, "city", "castle", "hole", "arctic", "harvest", "face", "realm"
};
typedef char board_table_slots_match_storage_h[(TABLE_DESIGN_COUNT <= BOARD_TABLE_SLOTS) ? 1 : -1];

/* Transparent card-bevel cavity of each map's bezel.png, in TEXTURE px: centre x, centre
   y, height (tools/table_art.py measures it from the file's alpha by flooding from the
   centre). One row per slot; the current frames use the same cavity values, while each
   map keeps its own art. Re-measure a row when a replacement bezel changes that opening. */
static const float k_bezel_cav[BOARD_TABLE_SLOTS][3] = {
    {   0.0f,   0.0f,   0.0f },   /* CRT   - no bezel */
    {  49.5f,  67.0f, 122.0f },   /* CITY    */
    {  49.5f,  67.0f, 122.0f },   /* CASTLE  */
    {  49.5f,  67.0f, 122.0f },   /* HOLE    */
    {  49.5f,  67.0f, 122.0f },   /* ARCTIC  */
    {  49.5f,  67.0f, 122.0f },   /* HARVEST */
    {  49.5f,  67.0f, 122.0f },   /* FACE    */
    {  49.5f,  67.0f, 122.0f }    /* REALM   */
};

static const char* get_path(const char* sub) {
    const char *base = GetApplicationDirectory();
    if (base && base[0] != '\0') {
        snprintf(path_buf, sizeof(path_buf), "%s%s", base, sub);
        if (FileExists(path_buf)) return path_buf;
    }
    snprintf(path_buf, sizeof(path_buf), "./%s", sub);
    return path_buf;
}

#define BEZEL_SHINE_BODY(TEX_FN, OUT_FRAG) \
    "float bval(vec3 v) { return 0.45 * dot(v, vec3(0.299, 0.587, 0.114)) + 0.55 * max(max(v.r, v.g), v.b); }\n" \
    "void main() {\n" \
    "    vec4 c = " TEX_FN "(texture0, fragTexCoord);\n" \
    "    vec4 cl = " TEX_FN "(texture0, fragTexCoord - vec2(u_phase.z, 0.0));\n" \
    "    vec4 cr = " TEX_FN "(texture0, fragTexCoord + vec2(u_phase.z, 0.0));\n" \
    "    vec4 cu = " TEX_FN "(texture0, fragTexCoord - vec2(0.0, u_phase.w));\n" \
    "    vec4 cd = " TEX_FN "(texture0, fragTexCoord + vec2(0.0, u_phase.w));\n" \
    "    float diag_a = min(min(" TEX_FN "(texture0, fragTexCoord - u_phase.zw).a,\n" \
    "                           " TEX_FN "(texture0, fragTexCoord + vec2(u_phase.z, -u_phase.w)).a),\n" \
    "                       min(" TEX_FN "(texture0, fragTexCoord + vec2(-u_phase.z, u_phase.w)).a,\n" \
    "                           " TEX_FN "(texture0, fragTexCoord + u_phase.zw).a));\n" \
    "    float min8 = min(min(min(cl.a, cr.a), min(cu.a, cd.a)), diag_a);\n" \
    "    float solid = smoothstep(0.91, 0.98, c.a) * smoothstep(0.85, 0.95, min8);\n" \
    "    float hc = bval(c.rgb);\n" \
    "    float hl = bval(cl.rgb), hr = bval(cr.rgb), hu = bval(cu.rgb), hd = bval(cd.rgb);\n" \
    "    float peak = max(max(max(hl, hr), max(hu, hd)), 0.12);\n" \
    "    float mask = solid * smoothstep(0.08, 0.18, hc) * smoothstep(0.42, 0.72, hc / peak);\n" \
    "    vec2 g = vec2(hl - hr, hu - hd) / peak;\n" \
    "    vec3 n = normalize(vec3(g * 2.8, 1.0));\n" \
    "    vec2 t1 = (vec2(0.5 + 0.44 * cos(u_phase.x), 0.5 + 0.45 * sin(u_phase.x)) - fragTexCoord) * vec2(0.70, 1.0);\n" \
    "    float d1 = dot(t1, t1);\n" \
    "    vec3 h1 = normalize(normalize(vec3(t1, 0.30)) + vec3(0.0, 0.0, 1.0));\n" \
    "    float nd1 = max(dot(n, h1), 0.0);\n" \
    "    vec2 t2 = (vec2(0.5 + 0.44 * cos(u_phase.y), 0.5 + 0.45 * sin(u_phase.y * 2.0)) - fragTexCoord) * vec2(0.70, 1.0);\n" \
    "    float d2 = dot(t2, t2);\n" \
    "    vec3 h2 = normalize(normalize(vec3(t2, 0.30)) + vec3(0.0, 0.0, 1.0));\n" \
    "    float nd2 = max(dot(n, h2), 0.0);\n" \
    "    float str = fragColor.a * colDiffuse.a;\n" \
    "    float s1 = 0.30 + 0.70 * pow(nd1, 4.5);\n" \
    "    float s2 = 0.30 + 0.70 * pow(nd2, 4.5);\n" \
    "    float core = mask * (exp(-d1 / 0.022) * s1 + exp(-d2 / 0.015) * s2) * 3.40;\n" \
    "    float bloom = mask * (exp(-d1 / 0.038) * (0.35 + 0.65 * nd1 * nd1) + exp(-d2 / 0.028) * (0.35 + 0.65 * nd2 * nd2)) * (0.60 + 0.65 * str);\n" \
    "    float raw = (core + bloom * 0.80) * str;\n" \
    "    float a = 0.88 * (1.0 - exp(-raw * 1.65));\n" \
    "    " OUT_FRAG " = vec4(fragColor.rgb, a);\n" \
    "}\n"

#ifdef __EMSCRIPTEN__
static const char *const k_bezel_shine_fs =
    "#version 100\n"
    "precision mediump float;\n"
    "varying vec2 fragTexCoord;\n"
    "varying vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "uniform vec4 u_phase;\n"
    BEZEL_SHINE_BODY("texture2D", "gl_FragColor");
#else
static const char *const k_bezel_shine_fs =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "uniform vec4 u_phase;\n"
    "out vec4 finalColor;\n"
    BEZEL_SHINE_BODY("texture", "finalColor");
#endif
#undef BEZEL_SHINE_BODY

static void board_draw_table_bezel_shine(Rectangle dest, float pulse_alpha);

void board_load_assets(BoardAssets *ba) {
    ba->atlas = LoadTexture(get_path("assets/atlas.png"));
    GenTextureMipmaps(&ba->atlas);
    SetTextureFilter(ba->atlas, TEXTURE_FILTER_TRILINEAR);
    ba->atlas_deus = LoadTexture(get_path("assets/atlas_deus.png"));
    if (ba->atlas_deus.id > 0) {
        GenTextureMipmaps(&ba->atlas_deus);
        SetTextureFilter(ba->atlas_deus, TEXTURE_FILTER_TRILINEAR);
    }

    ba->coin_gold = LoadTexture(get_path("assets/coin_gold.png"));
    if (ba->coin_gold.id > 0) { GenTextureMipmaps(&ba->coin_gold); SetTextureFilter(ba->coin_gold, TEXTURE_FILTER_TRILINEAR); }
    ba->coin_silver = LoadTexture(get_path("assets/coin_silver.png"));
    if (ba->coin_silver.id > 0) { GenTextureMipmaps(&ba->coin_silver); SetTextureFilter(ba->coin_silver, TEXTURE_FILTER_TRILINEAR); }
    ba->coin_bronze = LoadTexture(get_path("assets/coin_bronze.png"));
    if (ba->coin_bronze.id > 0) { GenTextureMipmaps(&ba->coin_bronze); SetTextureFilter(ba->coin_bronze, TEXTURE_FILTER_TRILINEAR); }

    /* Forged card tables: assets/maps/<name>/{overlay,bezel}.png, one folder per slot.
       A slot whose files are missing just stays on the CRT table. */
    for (int i = 1; i < BOARD_TABLE_SLOTS; i++) {
        TableSkin *ts = &ba->table_skins[i];
        char path[256];
        if (!k_table_map_dirs[i]) continue;
        snprintf(path, sizeof(path), "assets/maps/%s/overlay.png", k_table_map_dirs[i]);
        ts->overlay = LoadTexture(get_path(path));
        if (ts->overlay.id > 0) {
            GenTextureMipmaps(&ts->overlay);
            SetTextureFilter(ts->overlay, TEXTURE_FILTER_TRILINEAR);
        }
        snprintf(path, sizeof(path), "assets/maps/%s/bezel.png", k_table_map_dirs[i]);
        ts->bezel = LoadTexture(get_path(path));
        if (ts->bezel.id > 0) {
            GenTextureMipmaps(&ts->bezel);
            SetTextureFilter(ts->bezel, TEXTURE_FILTER_TRILINEAR);
            SetTextureWrap(ts->bezel, TEXTURE_WRAP_CLAMP);
        }
        s_table_bezel[i] = ts->bezel;
        ts->ready = (ts->overlay.id > 0 && ts->bezel.id > 0);
        s_table_ready[i] = ts->ready;
        TraceLog(LOG_INFO, "TABLE SKIN %d (%s): %s", i, k_table_map_dirs[i],
                 ts->ready ? "ready" : "missing assets/maps folder files");
    }
    s_bezel_shine_shader = LoadShaderFromMemory(0, k_bezel_shine_fs);
    s_bezel_shine_phase_loc = (s_bezel_shine_shader.id > 0)
        ? GetShaderLocation(s_bezel_shine_shader, "u_phase") : -1;
    s_draw_bezel_shine_fn = board_draw_table_bezel_shine;

    Image img = LoadImage(get_path("assets/back.png"));
    if (img.data) {
        float crop_x = (img.width - 945) / 2.0f;
        ImageCrop(&img, (Rectangle){ crop_x > 0 ? crop_x : 0, 0, 945, (float)img.height });
        ImageResize(&img, ATLAS_CELL_W, ATLAS_CELL_H);
        ba->card_back = LoadTextureFromImage(img);
        UnloadImage(img);
        GenTextureMipmaps(&ba->card_back);
        SetTextureFilter(ba->card_back, TEXTURE_FILTER_TRILINEAR);
    }

    const char *bg_names[8] = {
        "assets/harvest.jpg", "assets/hole.jpg", "assets/arctic.jpg",
        "assets/castle.jpg",  "assets/city.jpg", "assets/deus.png",
        "assets/face.jpg", "assets/realm.jpg"
    };
    const char *normal_names[8] = {
        "assets/harvest_normal.png", "assets/hole_normal.png", "assets/arctic_normal.png",
        "assets/castle_normal.png", "assets/city_normal.png", "assets/deus_normal.png",
        "assets/face_normal.png", "assets/realm_normal.png"
    };
    for (int i = 0; i < 8; i++) {
        ba->bgs[i] = LoadTexture(get_path(bg_names[i]));
        ba->bg_normals[i].id = 0;
        if (normal_names[i][0] != '\0') {
            ba->bg_normals[i] = LoadTexture(get_path(normal_names[i]));
            if (ba->bg_normals[i].id > 0) {
                SetTextureFilter(ba->bg_normals[i], TEXTURE_FILTER_BILINEAR);
                SetTextureWrap(ba->bg_normals[i], TEXTURE_WRAP_CLAMP);
            }
        }
    }

#ifdef __EMSCRIPTEN__
    ba->fog_shader = LoadShader(0, get_path("assets/depth_fog_web.fs"));
#else
    ba->fog_shader = LoadShader(0, get_path("assets/depth_fog.fs"));
#endif
    ba->has_fog_shader = (ba->fog_shader.id > 0);
    ba->fog_time_loc = ba->light_normal_loc = ba->light_enabled_loc = ba->light_flash_loc = -1;
    if (ba->has_fog_shader) {
        ba->fog_time_loc = GetShaderLocation(ba->fog_shader, "time");
        ba->light_normal_loc = GetShaderLocation(ba->fog_shader, "normalMap");
        ba->light_enabled_loc = GetShaderLocation(ba->fog_shader, "lightEnabled");
        ba->light_flash_loc = GetShaderLocation(ba->fog_shader, "flashAmount");
    }

    ba->current_bg = 0; ba->assets_loaded = 1;
}

void board_unload_assets(BoardAssets *ba) {
    if (!ba->assets_loaded) return;
    UnloadTexture(ba->atlas); UnloadTexture(ba->card_back);
    if (ba->atlas_deus.id > 0) UnloadTexture(ba->atlas_deus);
    if (ba->coin_gold.id > 0) UnloadTexture(ba->coin_gold);
    if (ba->coin_silver.id > 0) UnloadTexture(ba->coin_silver);
    if (ba->coin_bronze.id > 0) UnloadTexture(ba->coin_bronze);
    for (int i = 1; i < BOARD_TABLE_SLOTS; i++) {
        if (ba->table_skins[i].overlay.id > 0) UnloadTexture(ba->table_skins[i].overlay);
        if (ba->table_skins[i].bezel.id > 0) UnloadTexture(ba->table_skins[i].bezel);
        ba->table_skins[i].overlay.id = 0;
        ba->table_skins[i].bezel.id = 0;
        ba->table_skins[i].ready = 0;
        s_table_bezel[i] = (Texture2D){ 0 };
        s_table_ready[i] = 0;
    }
    if (s_bezel_shine_shader.id > 0) {
        UnloadShader(s_bezel_shine_shader);
        s_bezel_shine_shader = (Shader){ 0 };
    }
    s_bezel_shine_phase_loc = -1;
    s_draw_bezel_shine_fn = NULL;
    for (int i = 0; i < 8; i++) {
        UnloadTexture(ba->bgs[i]);
        if (ba->bg_normals[i].id > 0) UnloadTexture(ba->bg_normals[i]);
    }
    if (ba->has_fog_shader) { UnloadShader(ba->fog_shader); ba->has_fog_shader = 0; }
    ba->assets_loaded = 0;
}

int board_table_skin_active(void) {
    /* Reads no globals on purpose: board_draw_table refreshes the selection flag each
       frame, so the headless UI tests can link the sidebar/card paths (which consult
       this) without stubbing the game's settings global. */
    return s_table_skin_ready() && s_table_selected > 0 && s_table_selected < BOARD_TABLE_SLOTS;
}

void board_set_table_bezel_muted(int muted) { s_bezel_muted = muted; }

Rectangle board_get_hand_card_rect(int index, int total_cards, int hovered) {
    float step = 96.0f, total_w = (total_cards > 0) ? ((total_cards - 1) * step + HAND_CARD_W) : 0;
    return (Rectangle){ 710.0f - total_w / 2.0f + index * step, hovered ? 552.0f : 566.0f, HAND_CARD_W, HAND_CARD_H };
}

Rectangle board_get_field_card_rect(int index, int total_cards) {
    float x, y;
    board_field_slot_xy(index, total_cards, &x, &y);
    return (Rectangle){ x, y, FIELD_CARD_W, FIELD_CARD_H };
}

/* Sticky physical table mat:
   - 16 non-overlapping primary slots (2 rows x 8 cols at 81px step inside FIELD_X..FIELD_X+FIELD_W)
     plus 8 fanned backup slots (16..23) for extreme >16-card Speedcoin waves.
   - Initial round deals are sorted cleanly by month into two centered rows (cols 2..5 for 8 cards,
     cols 0..6 for 14 cards), then locked in place so existing table cards never slide around.
   - Capturing a card frees its slot in place; unmatched plays/draws claim an empty slot. */
#define TABLE_COLS          8
#define TABLE_PRIMARY_SLOTS 16
#define TABLE_TOTAL_SLOTS   24

static int     s_slot_card[TABLE_TOTAL_SLOTS];
static int     s_table_seeded  = 0;
static int     s_table_round   = -1;
static int     s_drop_hint_cid = -1;
static Vector2 s_drop_hint_pos = { 0.0f, 0.0f };

static const int k_table_slot_order[TABLE_TOTAL_SLOTS] = {
    2, 3, 4, 5,  10, 11, 12, 13,
    1, 6,         9, 14,
    0, 7,         8, 15,
    16, 17, 18, 19, 20, 21, 22, 23
};

static Rectangle table_slot_rect(int slot) {
    if (slot < 0 || slot >= TABLE_TOTAL_SLOTS) return (Rectangle){ 0, 0, 0, 0 };
    if (slot < TABLE_PRIMARY_SLOTS) {
        int row = slot / TABLE_COLS;
        int col = slot % TABLE_COLS;
        return (Rectangle){ 434.0f + (float)col * 81.0f, (row == 0) ? 170.0f : 310.0f,
                            FIELD_CARD_W, FIELD_CARD_H };
    }
    int ov  = slot - TABLE_PRIMARY_SLOTS;
    int row = (ov >= 4) ? 1 : 0;
    int col = 2 + (ov & 3);
    return (Rectangle){ 434.0f + (float)col * 81.0f + 12.0f, (row == 0) ? 184.0f : 324.0f,
                        FIELD_CARD_W, FIELD_CARD_H };
}

void board_reset_field_slots(const GameState *g) {
    for (int s = 0; s < TABLE_TOTAL_SLOTS; s++) s_slot_card[s] = -1;
    s_drop_hint_cid = -1;
    if (!g) {
        s_table_seeded = 0;
        s_table_round = -1;
        return;
    }
    s_table_seeded = 1;
    s_table_round = g->current_round;
    int n = g->field_count;
    if (n <= 0) return;
    if (n > 48) n = 48;

    int sf[48];
    board_field_sort_slots(g->field, n, sf);

    int pri_n = (n <= TABLE_PRIMARY_SLOTS) ? n : TABLE_PRIMARY_SLOTS;
    int top_n = (pri_n + 1) / 2;
    int bot_n = pri_n - top_n;
    int c0 = (TABLE_COLS - top_n) / 2;
    int c1 = (TABLE_COLS - bot_n) / 2;

    for (int i = 0; i < top_n; i++) s_slot_card[c0 + i] = g->field[sf[i]];
    for (int i = 0; i < bot_n; i++) s_slot_card[TABLE_COLS + c1 + i] = g->field[sf[top_n + i]];
    for (int i = TABLE_PRIMARY_SLOTS; i < n && i < TABLE_TOTAL_SLOTS; i++) {
        s_slot_card[i] = g->field[sf[i]];
    }
}

void board_hint_field_drop(int card_id, Vector2 drop_center) {
    if ((unsigned)card_id >= 48u) return;
    s_drop_hint_cid = card_id;
    s_drop_hint_pos = drop_center;
}

static int pick_open_table_slot(int use_hint, Vector2 hint_center) {
    if (use_hint) {
        int best_s = -1;
        float best_d2 = 1e30f;
        for (int s = 0; s < TABLE_PRIMARY_SLOTS; s++) {
            if (s_slot_card[s] != -1) continue;
            Rectangle r = table_slot_rect(s);
            float dx = (r.x + r.width * 0.5f) - hint_center.x;
            float dy = (r.y + r.height * 0.5f) - hint_center.y;
            float d2 = dx * dx + dy * dy;
            if (d2 < best_d2) { best_d2 = d2; best_s = s; }
        }
        if (best_s >= 0) return best_s;
    }
    for (int i = 0; i < TABLE_TOTAL_SLOTS; i++) {
        int s = k_table_slot_order[i];
        if (s_slot_card[s] == -1) return s;
    }
    return -1;
}

static void sync_field_slots(const GameState *g) {
    if (!g) return;
    if (g->phase == PHASE_DEALER_SELECT) {
        board_reset_field_slots(NULL);
        return;
    }
    if (!s_table_seeded || g->current_round != s_table_round) {
        board_reset_field_slots(g);
        return;
    }

    uint64_t present = 0ULL;
    for (int i = 0; i < g->field_count; i++) {
        int cid = g->field[i];
        if ((unsigned)cid < 48u) present |= (1ULL << cid);
    }
    for (int s = 0; s < TABLE_TOTAL_SLOTS; s++) {
        int cid = s_slot_card[s];
        if (cid >= 0 && !((present >> cid) & 1ULL)) s_slot_card[s] = -1;
    }

    uint64_t assigned = 0ULL;
    for (int s = 0; s < TABLE_TOTAL_SLOTS; s++) {
        int cid = s_slot_card[s];
        if ((unsigned)cid < 48u) assigned |= (1ULL << cid);
    }
    for (int i = 0; i < g->field_count; i++) {
        int cid = g->field[i];
        if ((unsigned)cid >= 48u || ((assigned >> cid) & 1ULL)) continue;
        int use_hint = (cid == s_drop_hint_cid);
        int s = pick_open_table_slot(use_hint, s_drop_hint_pos);
        if (use_hint) s_drop_hint_cid = -1;
        if (s >= 0) {
            s_slot_card[s] = cid;
            assigned |= (1ULL << cid);
        }
    }
}

/* Returns the sticky table slot rectangle of card_id.  If card_id is not on g->field[] yet
   (an incoming stock card or CPU/replay unmatched card), returns the open slot it will claim
   upon landing so flight animations land with zero post-drop sliding. */
Rectangle board_get_field_card_rect_of(const GameState *g, int card_id) {
    if (!g || card_id < 0 || card_id >= 48) return (Rectangle){ 0, 0, 0, 0 };
    sync_field_slots(g);
    for (int s = 0; s < TABLE_TOTAL_SLOTS; s++) {
        if (s_slot_card[s] == card_id) return table_slot_rect(s);
    }
    int preview_s = pick_open_table_slot(card_id == s_drop_hint_cid, s_drop_hint_pos);
    if (preview_s >= 0) return table_slot_rect(preview_s);
    return (Rectangle){ 710.0f, 240.0f, FIELD_CARD_W, FIELD_CARD_H };
}

Vector2 board_get_card_field_pos(const GameState *g, int card_id) {
    if (card_id >= 0 && card_id < 48) {
        Vector2 p = motion_get_pos(card_id);
        if (p.x > 0.0f || p.y > 0.0f) return p;
    }
    Rectangle r = board_get_field_card_rect_of(g, card_id);
    if (r.width > 0.0f) return (Vector2){ r.x, r.y };
    return (Vector2){ 710.0f, 240.0f };
}

Rectangle board_get_field_rect(void) { return (Rectangle){ FIELD_X, FIELD_Y, FIELD_W, FIELD_H }; }
Rectangle board_get_stock_rect(void) { return (Rectangle){ 330.0f, 262.0f, FIELD_CARD_W, FIELD_CARD_H }; }

void board_draw_card_shadow(Rectangle dest, float lift_amount) {
    if (lift_amount <= 0.01f) return;
    if (lift_amount > 1.0f) lift_amount = 1.0f;
    float off_x = 4.0f + lift_amount * 8.0f, off_y = 5.0f + lift_amount * 10.0f;
    DrawRectangleRec((Rectangle){ dest.x + off_x, dest.y + off_y, dest.width, dest.height }, (Color){ 0, 0, 0, (unsigned char)(102.0f * lift_amount) });
}

void board_draw_card_shimmer(Rectangle dest, float shimmer_val) {
    if (shimmer_val <= 0.5f || dest.width <= 0.0f || dest.height <= 0.0f) return;
    unsigned char top_a = (unsigned char)shimmer_val;
    if (top_a > 23) top_a = 23;
    unsigned char bot_a = (unsigned char)(top_a >> 2);
    DrawRectangleGradientV((int)dest.x, (int)dest.y, (int)dest.width, (int)dest.height,
                           (Color){ 255, 255, 255, top_a },
                           (Color){ 255, 255, 255, bot_a });
}

/* Card bezel, one per map (assets/maps/<name>/bezel.png): the OWNER-PROVIDED
   transparent frame tables/assets_castle/castle_card_bezel.png (95x136, measured
   transparent cavity (10, 6, 79, 122) px) - provided art, not manufactured by the
   fitter (WORKFLOW.md trap 17); k_bezel_cav[] mirrors each file's measured cavity.
   Drawn as a SANDWICH - once under the card, once over it - so the card is covered by
   the border on both sides: scaled by card HEIGHT (deal/stock flips squeeze width,
   never height) with the cavity centre on the card centre, and the drawn width follows
   a flip's squeeze so the frame turns with its card. The cavity (79/122 ~ 0.648) is a
   touch wider in aspect than the card (~0.615): at the ratio below the side rails abut
   the card, the top/bottom lip overlaps ~3-4 px at hand size, and the frame silhouette
   stays ~3-8 px proud of the card. Cards under 60 px tall (capture-lane minis) get no
   bezel. */
#define TABLE_BEZEL_RATIO  0.94f      /* cavity height per card height (window for this
                                          texture: >=0.897 silhouette, <=0.950 no side gap) */

static void board_draw_table_bezel(const BoardAssets *ba, Rectangle dest) {
    if (s_bezel_muted || !board_table_skin_active()) return;
    const TableSkin *ts = &ba->table_skins[s_table_selected];
    float cav_cx = k_bezel_cav[s_table_selected][0];
    float cav_cy = k_bezel_cav[s_table_selected][1];
    float cav_h = k_bezel_cav[s_table_selected][2];
    if (!ts->ready || ts->bezel.id <= 0 || cav_h <= 1.0f) return;
    if (dest.width <= 0.0f || dest.height < 60.0f) return;
    float s = (dest.height * TABLE_BEZEL_RATIO) / cav_h;
    float squeeze = dest.width / (dest.height * 0.615f);   /* 1.0 at rest, <1 mid-flip */
    if (squeeze > 1.0f) squeeze = 1.0f;
    if (squeeze < 0.06f) squeeze = 0.06f;
    float cx = dest.x + dest.width * 0.5f, cy = dest.y + dest.height * 0.5f;
    Rectangle bd = { cx - cav_cx * s * squeeze, cy - cav_cy * s,
                     (float)ts->bezel.width * s * squeeze,
                     (float)ts->bezel.height * s };
    DrawTexturePro(ts->bezel,
                   (Rectangle){ 0, 0, (float)ts->bezel.width, (float)ts->bezel.height },
                   bd, (Vector2){ 0, 0 }, 0.0f, WHITE);
}

void board_draw_card(const BoardAssets *ba, int card_id, Rectangle dest) {
    if ((unsigned)card_id >= 48u) return;
    if ((s_board_blind_mask >> card_id) & 1ULL) {
        board_draw_card_back(ba, dest);
        return;
    }
    card_tooltip_record(card_id, dest);
    board_draw_table_bezel(ba, dest);   /* sandwich under-layer (no-op on CRT) */
    Rectangle src = { (float)((card_id >> 2) * ATLAS_CELL_W), (float)((card_id & 3) * ATLAS_CELL_H), (float)ATLAS_CELL_W, (float)ATLAS_CELL_H };
    Texture2D tex = (settings.difficulty == DIFFICULTY_DEUS && ba->atlas_deus.id > 0) ? ba->atlas_deus : ba->atlas;
    DrawTexturePro(tex, src, dest, (Vector2){ 0, 0 }, 0.0f, WHITE);
    board_draw_table_bezel(ba, dest);   /* hollow frame over the face */
    if (!board_table_skin_active()) DrawRectangleLinesEx(dest, 1.0f, (Color){ 20, 40, 25, 200 });
}

void board_draw_card_back(const BoardAssets *ba, Rectangle dest) {
    card_tooltip_record(-1, dest);
    board_draw_table_bezel(ba, dest);   /* sandwich under-layer */
    Rectangle src = { 0, 0, (float)ba->card_back.width, (float)ba->card_back.height };
    DrawTexturePro(ba->card_back, src, dest, (Vector2){ 0, 0 }, 0.0f, WHITE);
    board_draw_table_bezel(ba, dest);
    if (!board_table_skin_active()) {
        Color bo = g_theme.dim; bo.a = 100; Color bi = g_theme.base; bi.a = 180;
        DrawRectangleLinesEx((Rectangle){ dest.x - 1, dest.y - 1, dest.width + 2, dest.height + 2 }, 1.0f, bo);
        DrawRectangleLinesEx(dest, 1.5f, bi);
    }
}

static void board_draw_table_bezel_shine(Rectangle dest, float pulse_alpha) {
    if (dest.width <= 0.0f || pulse_alpha <= 0.001f) return;
    if (dest.height < 60.0f) {
        if (pulse_alpha >= 0.85f)
            DrawRectangleLinesEx(dest, 1.5f, (Color){ 255, 255, 255, 210 });
        return;
    }
    Texture2D bez = s_table_bezel[s_table_selected];
    float cav_cx = k_bezel_cav[s_table_selected][0];
    float cav_cy = k_bezel_cav[s_table_selected][1];
    float cav_h = k_bezel_cav[s_table_selected][2];
    if (bez.id <= 0 || cav_h <= 1.0f || s_bezel_shine_shader.id <= 0) return;
    float s = (dest.height * TABLE_BEZEL_RATIO) / cav_h;
    float squeeze = dest.width / (dest.height * 0.615f);
    if (squeeze > 1.0f) squeeze = 1.0f;
    if (squeeze < 0.06f) squeeze = 0.06f;
    float cx = dest.x + dest.width * 0.5f, cy = dest.y + dest.height * 0.5f;
    Rectangle bd = { cx - cav_cx * s * squeeze, cy - cav_cy * s,
                     (float)bez.width * s * squeeze, (float)bez.height * s };
    float t = (float)GetTime();
    int live_table = (s_bezel_phase_stamp >= 0.0f && fabsf(t - s_bezel_phase_stamp) < 0.25f);
    float p0 = live_table ? s_bezel_phase_acc[0] : (t * 0.52f);
    float p1 = live_table ? s_bezel_phase_acc[1] : (-t * 0.37f);
    float seed = dest.x * 0.019f + dest.y * 0.013f;
    float phase[4] = {
        p0 + seed,
        p1 + seed * 2.3f,
        1.45f / (float)bez.width,
        1.45f / (float)bez.height
    };
    if (s_bezel_shine_phase_loc >= 0)
        SetShaderValue(s_bezel_shine_shader, s_bezel_shine_phase_loc, phase, SHADER_UNIFORM_VEC4);
    float p_clamped = fminf(1.0f, fmaxf(0.0f, pulse_alpha));
    float a = (p_clamped > 0.001f) ? fminf(1.0f, 0.16f + 0.84f * powf(p_clamped, 0.68f)) : 0.0f;
    if (dest.height < 76.0f) a *= (dest.height - 60.0f) / 16.0f;
    Color c = { 250, 246, 238, (unsigned char)(a * 255.0f) };
    BeginBlendMode(BLEND_ADDITIVE);
    BeginShaderMode(s_bezel_shine_shader);
    DrawTexturePro(bez, (Rectangle){ 0, 0, (float)bez.width, (float)bez.height },
                   bd, (Vector2){ 0, 0 }, 0.0f, c);
    EndShaderMode();
    EndBlendMode();
}

void board_draw_card_glow(Rectangle dest, float pulse_alpha) {
    if (!s_bezel_muted && board_table_skin_active()) {
        if (s_draw_bezel_shine_fn) s_draw_bezel_shine_fn(dest, pulse_alpha);
        return;
    }
    Color gc = g_theme.base; gc.a = (unsigned char)(pulse_alpha * 255.0f);
    DrawRectangleLinesEx(dest, 1.5f, gc);
}

static void draw_capture_lanes(const BoardAssets *ba, const GameState *g, int player, float start_y) {
    static const char *const labels[4] = { "B", "A", "R", "P" };
    float card_w = MINI_CARD_W - 6.0f, card_h = MINI_CARD_H - 6.0f;
    int lanes[4][48], counts[4] = { 0, 0, 0, 0 }, cup = 0, cap_n = g->captured_count[player];
    for (int i = 0; i < cap_n; i++) {
        int cid = g->captured[player][i], t = (int)CARDS[cid].type;
        if (cid == 32) cup = 1;
        if ((unsigned)t < 4u) lanes[t][counts[t]++] = cid;
    }
    for (int t = 0; t < 4; t++) {
        float y = start_y + t * (CAPTURE_PANEL_H + 4.0f), card_y = y + (CAPTURE_PANEL_H - card_h) / 2.0f;
        Rectangle lr = { 12.0f, y, CAPTURE_PANEL_W, CAPTURE_PANEL_H };
        /* Forged table skin: the lane casing comes from the forged overlay - only the
           live label and minis draw over it. */
        if (!board_table_skin_active()) {
            DrawRectangleRec(lr, COLOR_PANEL_BG);
            ui_draw_panel_border(lr, COLOR_PHOSPHOR_DIM);
        }
        int count = counts[t];
        const char *cnt_str = TextFormat((t == 1 && cup) ? "%s %d*" : "%s %d", labels[t], count);
        draw_text_ex_snapped(font_body, cnt_str, (Vector2){ 16.0f, y + 8.5f }, 20.0f, 1.4f, COLOR_TEXT_INFO);
        float step = (count > 1) ? fminf(24.0f, (CAPTURE_PANEL_W - 54.0f - card_w) / (count - 1)) : 0.0f;
        for (int c = 0; c < count; c++) {
            int cid = lanes[t][c];
            motion_set_target(cid, (Vector2){ 54.0f + c * step, card_y }, (Vector2){ card_w, card_h }, 0.35f);
            Rectangle dr = motion_get_rect(cid);
            if (motion_is_card_moving(cid)) {
                board_draw_card_shadow(dr, motion_get_card_lift(cid));
                board_draw_card(ba, cid, dr);
                board_draw_card_glow(dr, board_table_skin_active() ? 0.90f : 0.4f);
            } else {
                board_draw_card(ba, cid, dr);
                capture_info_record(cid, player, c + 1, count, dr);
            }
        }
    }
}

static void draw_speedcoin_clock_panel(void) {
    Rectangle r = { 12.0f, 12.0f, CAPTURE_PANEL_W, 196.0f };
    int last_delta = 0;
    float flash = speed_mode_clock_flash(&last_delta);
    float sec = speed_mode_clock();
    int low = (sec <= 20.0f);
    Color accent = low ? COLOR_RED : COLOR_PHOSPHOR;
    if (flash > 0.01f) accent = (last_delta < 0) ? COLOR_RED : (Color){ 255, 224, 108, 255 };

    DrawRectangleRec(r, COLOR_PANEL_BG);
    if (flash > 0.01f || low) {
        ui_theme_set_panel_flare_glow(r, fmaxf(flash, low ? 0.65f : 0.0f));
    }
    ui_draw_panel_border(r, accent);
    DrawLine((int)(r.x + 12.0f), (int)(r.y + 44.0f), (int)(r.x + r.width - 12.0f), (int)(r.y + 44.0f), COLOR_PHOSPHOR_DIM);

    draw_centered_text(font_title, "TIME REMAINING", r.x + r.width * 0.5f, r.y + 16.0f, 15.0f, 1.0f, COLOR_PHOSPHOR);
    const char *t_str = TextFormat("%.1fs", (double)sec);
    float t_size = (sec >= 1000.0f) ? 34.0f : 40.0f;
    draw_centered_text(font_title, t_str, r.x + r.width * 0.5f, r.y + 72.0f, t_size, 1.2f,
                       low ? COLOR_RED : COLOR_TEXT_INFO);

    if (flash > 0.01f && last_delta != 0) {
        Color dcol = (last_delta > 0) ? (Color){ 255, 224, 108, 255 } : COLOR_RED;
        const char *d_str = (last_delta > 0) ? TextFormat("+%d SEC", last_delta) : TextFormat("%d SEC", last_delta);
        draw_centered_text(font_title, d_str, r.x + r.width * 0.5f, r.y + 138.0f, 20.0f, 1.0f, dcol);
    } else {
        draw_centered_text(font_body, TextFormat("WAVE %d", speed_mode_wave()),
                           r.x + r.width * 0.5f, r.y + 140.0f, 18.0f, 1.0f, COLOR_TEXT_INFO_DIM);
    }
    draw_centered_text(font_body, "-10s DRAIN / MISS   +10s / 10 MP",
                       r.x + r.width * 0.5f, r.y + 170.0f, 13.0f, 0.6f, COLOR_PHOSPHOR_DIM);
}

void board_draw_table(const BoardAssets *ba, const GameState *g, const AnimState *anim,
                      int hovered_hand_idx, int dragging_idx, Vector2 drag_pos,
                      int easy_mode, int replay_mode, const char *status_msg) {
    float cur_time = (float)GetTime();
    float frame_dt = fminf(0.05f, GetFrameTime());
    static int prev_round = -1;
    static int prev_captured = -1;
    static int prev_field = -1;
    static int prev_koi = -1;
    static int capture_streak = 0;
    static float thunder_flash = 0.0f;
    static float thunder_hold = 0.0f;
    static float reverb_hold = 0.0f;
    static float reverb_energy = 0.0f;
    static float reverb_mid = 0.0f;
    static float reverb_curve = 0.0f;
    int cur_round = g ? g->current_round : -1;
    int cur_captured = g ? (g->captured_count[0] + g->captured_count[1]) : 0;
    int cur_field = g ? g->field_count : 0;
    int cur_koi = g ? (g->koi_called[0] + g->koi_called[1]) : 0;
    int round_reset = (action_card_take_match_reset_req() || cur_round != prev_round ||
                       (g && g->phase == PHASE_DEALER_SELECT));
    if (round_reset || cur_captured < prev_captured) {
        prev_round = cur_round;
        prev_captured = cur_captured;
        prev_field = cur_field;
        prev_koi = cur_koi;
        capture_streak = 0;
        if (round_reset || !action_card_is_mode_active()) {
            reverb_hold = reverb_energy = reverb_mid = reverb_curve = 0.0f;
        }
    }
    int delta_captured = (prev_captured >= 0 && cur_captured > prev_captured) ? (cur_captured - prev_captured) : 0;
    int capture_edge = (delta_captured > 0);
    int koi_edge = (prev_koi >= 0 && cur_koi > prev_koi);
    if (capture_edge) {
        capture_streak += (delta_captured >= 4) ? 2 : 1;
        if (capture_streak > 5) capture_streak = 5;
        ash_spawn_capture_burst(g, capture_streak);
    } else if (prev_field >= 0 && cur_field > prev_field) {
        capture_streak = 0;
    }
    prev_captured = cur_captured;
    prev_field = cur_field;
    prev_koi = cur_koi;
    if (capture_edge || koi_edge) {
        thunder_flash = 0.25f;
        thunder_hold = 0.50f;
    } else if (thunder_hold > 0.0f) {
        thunder_hold -= frame_dt;
        if (thunder_hold < 0.0f) thunder_hold = 0.0f;
    } else if (thunder_flash > 0.0f) {
        thunder_flash *= expf(-frame_dt / 1.00f);
        if (thunder_flash < 0.002f) thunder_flash = 0.0f;
    }
    /* Ember heat on the ash field: the same edges as the reverb, but its own envelope, and
       deliberately NOT cleared by the round reset above - so it also runs in normal, easy,
       speed and replay play, not only in action-card mode. frame_dt rather than env_dt for
       the same reason: this is a background reaction, not an action-card meter. */
    if (delta_captured > 0) s_ember_heat += (float)delta_captured * EMBER_KICK_CAP;
    if (koi_edge)           s_ember_heat += EMBER_KICK_KOI;
    if (s_ember_heat > 1.0f) s_ember_heat = 1.0f;
    s_ember_heat -= EMBER_DECAY * frame_dt;
    if (s_ember_heat < 0.0f) s_ember_heat = 0.0f;
    float ember_tc = (s_ember_heat > s_ember_glow) ? EMBER_SMOOTH_ATK : EMBER_SMOOTH_REL;
    s_ember_glow += (s_ember_heat - s_ember_glow) * (1.0f - expf(-frame_dt / ember_tc));
    if (s_ember_glow < 0.002f) s_ember_glow = 0.0f;   /* lands exactly on the cold path */

    /* Capture buildup follows a smooth S-curve:
       clearly audible on the 1st capture (~55% reverb, gentle ~17% stereo spread),
       builds on the 2nd capture (~92%), and reaches 100% wide stereo synergy on the 3rd capture. */
    float env_dt = action_card_is_paused() ? 0.0f : frame_dt;
    if (delta_captured > 0) reverb_energy += (float)delta_captured * 0.09f;
    if (koi_edge)           reverb_energy += 0.20f;
    if (reverb_energy > 0.93f) reverb_energy = 0.93f;
    if ((capture_edge || koi_edge) && reverb_energy < 0.85f) {
        reverb_hold = 4.10f;
    } else if (reverb_hold > 0.0f) {
        reverb_hold -= env_dt;
        if (reverb_hold < 0.0f) reverb_hold = 0.0f;
    } else {
        reverb_energy -= 0.4f * env_dt;
        if (reverb_energy < 0.0f) reverb_energy = 0.0f;
    }
    reverb_mid    += (reverb_energy - reverb_mid) * (1.0f - expf(-env_dt / 0.7f));
    reverb_curve  += (reverb_mid - reverb_curve) * (1.0f - expf(-env_dt / 0.14f));
    if (reverb_curve > 1.0f) reverb_curve = 1.0f;
    if (reverb_energy < 0.0005f && reverb_mid < 0.0005f && reverb_curve < 0.0005f) {
        reverb_energy = reverb_mid = reverb_curve = 0.0f;
    }
    float syn_x = reverb_curve / 0.68f;
    if (syn_x < 0.0f) syn_x = 0.0f;
    if (syn_x > 1.0f) syn_x = 1.0f;
    float reverb_synergy = 0.30f * syn_x + 0.70f * (syn_x * syn_x * (3.0f - 2.0f * syn_x));
    /* Limit a full-strength peak independently of the normal capture hold. */
    static float reverb_peak_hold = 0.0f;
    if (round_reset || reverb_synergy <= 0.85f) reverb_peak_hold = 0.0f;
    else if (reverb_peak_hold > 0.0f) {
        reverb_peak_hold -= env_dt;
        if (reverb_peak_hold <= 0.0f) {
            /* Do not raise the idle envelope or its smoothing history. */
            reverb_energy = fminf(reverb_energy, 0.53222625f);
            reverb_mid = fminf(reverb_mid, 0.53222625f);
            reverb_curve = 0.53222625f; /* 85% on the S-curve */
            reverb_synergy = 0.85f;
            reverb_peak_hold = 0.0f;
        }
    } else if (reverb_synergy >= 1.0f) reverb_peak_hold = 4.10f;
    audio_set_reverb(reverb_synergy);
    if (!replay_mode) {
        if (capture_edge || koi_edge) s_audio_action_triggers++;
        if (reverb_synergy > s_audio_peak_synergy) s_audio_peak_synergy = reverb_synergy;
        if (reverb_synergy >= 0.95f) s_audio_full_hold_sec += env_dt;
    }
    float ac_card_lvl = action_card_begin_table(g, round_reset, reverb_synergy);
    {
        float shine_spd = (ac_card_lvl >= 0.75f) ? 2.00f : (ac_card_lvl >= 0.50f) ? 1.60f : 1.00f;
        s_bezel_phase_acc[0] = fmodf(s_bezel_phase_acc[0] + env_dt * 0.52f * shine_spd, 628.31853f);
        s_bezel_phase_acc[1] = fmodf(s_bezel_phase_acc[1] - env_dt * 0.37f * shine_spd, 628.31853f);
        s_bezel_phase_stamp = cur_time;
    }

    s_table_selected = settings.table_design;
    if (s_table_selected < 0 || s_table_selected >= BOARD_TABLE_SLOTS) s_table_selected = 0;

    /* 1. Background with depth fog and a localized normal-map light */
    if (ba->has_fog_shader) {
        int bg = ba->current_bg;
        float light_enabled = (bg >= 0 && bg < 8 && ba->bg_normals[bg].id > 0 &&
                               ba->light_normal_loc >= 0 && ba->light_enabled_loc >= 0) ? 1.0f : 0.0f;
        SetShaderValue(ba->fog_shader, ba->fog_time_loc, &cur_time, SHADER_UNIFORM_FLOAT);
        SetShaderValue(ba->fog_shader, ba->light_enabled_loc, &light_enabled, SHADER_UNIFORM_FLOAT);
        if (ba->light_flash_loc >= 0)
            SetShaderValue(ba->fog_shader, ba->light_flash_loc, &thunder_flash, SHADER_UNIFORM_FLOAT);
        BeginShaderMode(ba->fog_shader);
        if (light_enabled > 0.5f)
            SetShaderValueTexture(ba->fog_shader, ba->light_normal_loc, ba->bg_normals[bg]);
    }
    if (ba->current_bg >= 0 && ba->current_bg < 8 && ba->bgs[ba->current_bg].id > 0) {
        DrawTexturePro(ba->bgs[ba->current_bg], (Rectangle){ 0, 0, 1280, 720 }, (Rectangle){ 0, 0, 1280, 720 }, (Vector2){ 0, 0 }, 0.0f, WHITE);
    } else ClearBackground(BLACK);
    if (ba->has_fog_shader) EndShaderMode();

    DrawRectangle(0, 0, 1280, 720, COLOR_TABLE_DIM);
    DrawRectangleGradientV(0, 380, 1280, 340, (Color){ 5, 5, 5, 0 }, (Color){ 5, 5, 5, 65 });
    ash_draw_and_update(frame_dt);

    /* Castle table skin: the forged casing sits above the dimmed backdrop/ash and
       below every live element - text, counts, minis and cards draw over it, and the
       CRT fills/hairlines they'd otherwise sit in are gated off while it is active. */
    if (board_table_skin_active() && ba->table_skins[s_table_selected].overlay.id > 0) {
        DrawTexture(ba->table_skins[s_table_selected].overlay, 0, 0, WHITE);
    }

    /* 2. Table Chrome, Status, Combos & Left Column (all behind cards) */
    int active_turn = (g->phase != PHASE_ROUND_END && g->phase != PHASE_MATCH_END) ?
                      ((anim && anim->active && anim->type == ANIM_CAPTURE_FLY) ? anim->capture_player : g->current_player) : -1;

    Rectangle cpu_score_box = { 12, 12, CAPTURE_PANEL_W, 30 };
    Rectangle you_score_box = { 12, 678, CAPTURE_PANEL_W, 30 };
    int sc_mode = speed_mode_is_active();

    if (g->current_player == 0 && g->phase == PHASE_TURN_HAND && g->player_timer > 0.0f) {
        char t_str[32];
        if (g->player_timer <= 5.0f) {
            int flash = ((int)(g->player_timer * 6.0f)) & 1;
            Color tcol = flash ? (Color){ 255, 30, 30, 255 } : (Color){ 255, 230, 230, 255 };
            snprintf(t_str, sizeof(t_str), "! TIME: %.1fS !", g->player_timer);
            draw_centered_text(font_body, t_str, 640.0f, 502.0f, 24.0f, 1.2f, tcol);
        } else {
            snprintf(t_str, sizeof(t_str), "TIME: %.1fS", g->player_timer);
            draw_centered_text(font_body, t_str, 640.0f, 505.0f, 22.0f, 1.2f, COLOR_PHOSPHOR);
        }
    }
    board_draw_status_message(status_msg);

    for (int p = 0; p < 2; p++) {
        if (sc_mode && p == 1) continue;
        ScoreResult cs = score_captures(g->ruleset, g->captured[p], g->captured_count[p]);
        if (cs.yaku_count > 0) {
            float cglow = fminf(1.0f, 0.28f * (float)cs.yaku_count);
            ui_theme_set_panel_flare_glow(combos_panel_rect(p), cglow);
        }
    }
    combos_draw_sidebar(g);

    Rectangle pause_btn = { 1160, 560, 112, 40 }, restart_btn = { 1160, 610, 112, 40 };
    int table_skin = board_table_skin_active();   /* button casings are in the overlay */
    if (!table_skin) { DrawRectangleRec(pause_btn, COLOR_PANEL_BG); ui_draw_panel_border(pause_btn, COLOR_PHOSPHOR); }
    draw_text_in_rect(font_title, "PAUSE", pause_btn, 16.0f, 1.0f, COLOR_PHOSPHOR);
    if (!table_skin) { DrawRectangleRec(restart_btn, COLOR_PANEL_BG); ui_draw_panel_border(restart_btn, COLOR_PHOSPHOR); }
    /* A replay cannot be restarted: the second button ends it instead. */
    draw_text_in_rect(font_title, replay_mode ? "STOP" : "RESTART", restart_btn, 16.0f, 1.0f, COLOR_PHOSPHOR);
    if (!replay_mode && g->ruleset == RULESET_KK_ADD_10) speed_bar_draw();
    draw_text_ex_snapped(font_body, TextFormat("%d", g->stock_count), (Vector2){ 356.0f, 392.0f }, 18.0f, 1.2f, COLOR_PHOSPHOR);

    if (sc_mode) {
        draw_speedcoin_clock_panel();
    } else {
        board_draw_info_box(cpu_score_box, TextFormat("CPU   SCORE: %d%s", g->total_score[1], (g->dealer == 1) ? " [DEALER]" : ""), (active_turn == 1));
        draw_capture_lanes(ba, g, 1, 46.0f);
    }
    draw_capture_lanes(ba, g, 0, 508.0f);
    card_tooltip_clear();
    board_draw_info_box(you_score_box, TextFormat("YOU   SCORE: %d%s", g->total_score[0], (!sc_mode && g->dealer == 0) ? " [DEALER]" : ""), (active_turn == 0));

    /* 3. CPU Hand */
    int cpu_count = sc_mode ? 0 : g->hand_count[1];
    float cpu_step = 62.0f, cpu_w = (cpu_count > 0) ? ((cpu_count - 1) * cpu_step + CPU_CARD_W) : 0, cpu_x = 710.0f - cpu_w / 2.0f;
    for (int i = 0; i < cpu_count; i++) {
        int cid = g->hand[1][i];
        if (anim && anim->active && anim->type == ANIM_CPU_FLY && anim->cpu_card == cid) continue;
        Rectangle r = { cpu_x + i * cpu_step, 12.0f, CPU_CARD_W, CPU_CARD_H };
        motion_set_target(cid, (Vector2){ r.x, r.y }, (Vector2){ r.width, r.height }, 0.30f);
        Rectangle dr = motion_get_rect(cid);
        if (motion_is_card_moving(cid)) {
            board_draw_card_shadow(dr, motion_get_card_lift(cid));
            board_draw_card_back(ba, dr);
        } else board_draw_card_back(ba, dr);
        action_card_draw_distraction(dr, cid + 1, ac_card_lvl);
    }

    /* 4. Stock Pile */
    int st_off = g->stock_count >> 2;
    if (g->stock_count > 0) {
        for (int i = 0; i <= st_off; i++) board_draw_card_back(ba, (Rectangle){ 330.0f - i, 262.0f - i, FIELD_CARD_W, FIELD_CARD_H });
    }
    Rectangle top_st = { 330.0f - (float)st_off, 262.0f - (float)st_off, FIELD_CARD_W, FIELD_CARD_H };
    if (sc_mode && speed_mode_deck_glow() && g->stock_count > 0) {
        float sp = 0.72f + 0.28f * (0.5f + 0.5f * sinf(cur_time * 5.2f));
        board_draw_card_glow(top_st, sp);
        if (!board_table_skin_active()) {
            ui_theme_set_panel_flare_glow(top_st, 0.92f);
            ui_theme_draw_panel_flares(top_st);
        }
    }
    action_card_draw_stock_returns(ba, g, ac_card_lvl);
    action_card_draw_meters(ba, g, reverb_synergy, env_dt, replay_mode);

    /* 5. Field Cards (Sticky physical slots: sorted at deal, zero movement once on the felt) */
    sync_field_slots(g);
    uint16_t field_month_mask = 0;
    for (int i = 0; i < g->field_count; i++) {
        int fcid = g->field[i];
        if (!board_is_blind_field_card(fcid)) field_month_mask |= (uint16_t)(1u << CARDS[fcid].month);
    }
    int drag_m = (dragging_idx >= 0 && dragging_idx < g->hand_count[0]) ? CARDS[g->hand[0][dragging_idx]].month : -1;
    float pulse = 0.12f + 0.28f * (0.5f + 0.5f * sinf(cur_time * 4.0f));

    for (int s = 0; s < TABLE_TOTAL_SLOTS; s++) {
        int cid = s_slot_card[s];
        if (cid < 0) continue;
        Rectangle r = table_slot_rect(s);
        motion_set_target(cid, (Vector2){ r.x, r.y }, (Vector2){ r.width, r.height }, 0.22f);
        Rectangle dr = motion_get_rect(cid);
        int is_blind = board_is_blind_field_card(cid);
        if (!action_card_draw_blind_flip(ba, cid, dr, env_dt, ac_card_lvl)) {
            if (motion_is_card_moving(cid)) board_draw_card_shadow(dr, motion_get_card_lift(cid));
            board_draw_card(ba, cid, dr);
            if (!is_blind && ac_card_lvl < 0.75f && easy_mode && drag_m > 0 && CARDS[cid].month == drag_m) {
                board_draw_card_glow(dr, pulse);
                ui_theme_draw_panel_flares_keyed(dr, cid + 1);
            }
            action_card_draw_distraction(dr, cid + 1, ac_card_lvl);
        }
    }
    ash_burst_draw_and_update(frame_dt);

    /* 5b. A hand card waiting on a choice (it matched two field cards): it has
           left the hand and is not on the field yet, so nothing else draws it. */
    if (g->phase == PHASE_HAND_CHOICE && g->played_hand_card >= 0) {
        Rectangle hr = motion_get_rect(g->played_hand_card);
        if (hr.width > 0.0f) {
            board_draw_card(ba, g->played_hand_card, hr);
            action_card_draw_distraction(hr, g->played_hand_card + 1, ac_card_lvl);
        }
    }

    /* 6. Pending Stock Card */
    if (g->has_pending && g->pending_card >= 0 && (!anim->active || anim->type != ANIM_STOCK_FLY)) {
        Rectangle pr = motion_get_rect(g->pending_card);
        Rectangle dp = (pr.width > 0 && (pr.x != 330.0f || pr.y != 262.0f))
                     ? pr : (Rectangle){ 330.0f, 262.0f, FIELD_CARD_W, FIELD_CARD_H };
        board_draw_card(ba, g->pending_card, dp);
        action_card_draw_distraction(dp, g->pending_card + 1, ac_card_lvl);
    }

    /* 7. Player Hand */
    float cd_el = cinematic_countdown_elapsed();
    for (int i = 0; i < g->hand_count[0]; i++) {
        if (i == dragging_idx) continue;
        int cid = g->hand[0][i];
        if (anim && anim->active && anim->type == ANIM_GLIDE_RETURN && anim->card_id == cid) continue;
        /* A replay flies both players' cards, so the human hand has to let go of a
           card that is in the air as well. */
        if (anim && anim->active && anim->type == ANIM_CPU_FLY && anim->cpu_card == cid) continue;
        Rectangle r = board_get_hand_card_rect(i, g->hand_count[0], i == hovered_hand_idx);
        motion_set_target(cid, (Vector2){ r.x, r.y }, (Vector2){ r.width, r.height }, (i == hovered_hand_idx ? 0.15f : 0.30f));
        Rectangle dr = motion_get_rect(cid);
        float fp = (cd_el >= 0.0f) ? fminf(1.0f, fmaxf(0.0f, (cd_el - (2.0f + (float)i * 0.085f)) / 0.36f)) : 1.0f;
        if (fp < 1.0f) {
            float hop = sinf(fp * 3.14159265f) * 14.0f;
            float cv = cosf(fp * 3.14159265f);
            float pw = dr.width * fmaxf(0.06f, fabsf(cv));
            Rectangle pr = { dr.x + (dr.width - pw) * 0.5f, dr.y - hop, pw, dr.height };
            float lift = fmaxf(motion_is_card_moving(cid) ? motion_get_card_lift(cid) : 0.0f, hop / 14.0f);
            if (lift > 0.001f) board_draw_card_shadow(pr, lift);
            if (cv > 0.0f) board_draw_card_back(ba, pr);
            else board_draw_card(ba, cid, pr);
        } else {
            if (motion_is_card_moving(cid)) board_draw_card_shadow(dr, motion_get_card_lift(cid));
            board_draw_card(ba, cid, dr);
            if (ac_card_lvl < 0.75f && easy_mode && g->current_player == 0 && dragging_idx < 0) {
                if ((field_month_mask >> CARDS[cid].month) & 1u) board_draw_card_glow(dr, pulse);
            }
            action_card_draw_distraction(dr, cid + 1, ac_card_lvl);
        }
    }

    /* 8. Dragged Card */
    if (dragging_idx >= 0 && dragging_idx < g->hand_count[0]) {
        int cid = g->hand[0][dragging_idx];
        motion_set_drag_velocity(cid, GetMouseDelta());
        motion_set_instant(cid, drag_pos, (Vector2){ HAND_CARD_W, HAND_CARD_H });
        Rectangle dr = { drag_pos.x, drag_pos.y, HAND_CARD_W, HAND_CARD_H };
        board_draw_card_shadow(dr, 1.0f);
        board_draw_card(ba, cid, dr);
        board_draw_card_shimmer(dr, motion_get_card_shimmer(cid));
        board_draw_card_glow(dr, 1.0f);
        ui_theme_draw_panel_flares_keyed(dr, cid + 1);
        action_card_draw_distraction(dr, cid + 1, ac_card_lvl);
    }

    /* 9. Animated Cards */
    if (anim->active && anim->type == ANIM_GLIDE_RETURN) {
        Rectangle ar = { anim->current_pos.x, anim->current_pos.y, HAND_CARD_W, HAND_CARD_H };
        float lift = sinf((anim->elapsed / anim->duration) * 3.14159265f);
        board_draw_card_shadow(ar, lift);
        board_draw_card(ba, anim->card_id, ar);
    } else if (anim->active && anim->type == ANIM_CPU_FLY) {
        /* A card leaves a hand at hand size and settles at field size, so it
           grows or shrinks over the flight instead of popping on take-off. */
        float t = (anim->duration > 0.0f) ? (anim->elapsed / anim->duration) : 1.0f;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        float from_bottom = (anim->start_pos.y > 300.0f) ? 1.0f : 0.0f;   /* human hand sits low */
        float sw = from_bottom ? HAND_CARD_W : CPU_CARD_W;
        float sh = from_bottom ? HAND_CARD_H : CPU_CARD_H;
        Rectangle ar = { anim->current_pos.x, anim->current_pos.y,
                         sw + (FIELD_CARD_W - sw) * t, sh + (FIELD_CARD_H - sh) * t };
        float lift = sinf((anim->elapsed / anim->duration) * 3.14159265f);
        board_draw_card_shadow(ar, lift);
        board_draw_card(ba, anim->cpu_card, ar);
        board_draw_card_glow(ar, 1.0f);
        ui_theme_draw_panel_flares_keyed(ar, anim->cpu_card + 1);
    } else if (anim->active && (anim->type == ANIM_STOCK_FLY || anim->type == ANIM_STOCK_FLIP)) {
        float t = anim->elapsed / anim->duration;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        float flip_t = (t < 0.70f) ? (t / 0.70f) : 1.0f;
        float hop = sinf(flip_t * 3.14159265f) * 18.0f;
        float cos_val = cosf(flip_t * 3.14159265f), cur_w = FIELD_CARD_W * fmaxf(0.06f, fabsf(cos_val));
        float cur_x = anim->current_pos.x + (FIELD_CARD_W - cur_w) / 2.0f, cur_y = anim->current_pos.y - hop;
        Rectangle cr = { cur_x, cur_y, cur_w, FIELD_CARD_H };
        float lift = hop / 18.0f;
        Rectangle sr = { cur_x + 4.0f + lift * 8.0f, cur_y + 5.0f + lift * 10.0f + hop, cur_w, FIELD_CARD_H };
        DrawRectangleRec(sr, (Color){ 0, 0, 0, 102 });
        int to_deck = (anim->type == ANIM_STOCK_FLY && anim->target_pos.x == 330.0f && anim->target_pos.y == 262.0f);
        if (to_deck ? (cos_val < 0.0f) : (cos_val > 0.0f)) board_draw_card_back(ba, cr);
        else board_draw_card(ba, anim->card_id, cr);
        board_draw_card_glow(cr, 1.0f);
        ui_theme_draw_panel_flares_keyed(cr, anim->card_id + 1);
    }

    if (!replay_mode) speed_hit_popups_draw();
    if (!replay_mode) action_card_draw_vfx(ba, env_dt);
}

void board_draw_dealer_select(const BoardAssets *ba, const GameState *g, Vector2 vm) {
    DrawRectangle(0, 0, 1280, 720, (Color){ 0, 0, 0, 245 });
    draw_centered_text(font_title, "PICK A CARD TO DETERMINE DEALER (EARLIEST MONTH WINS)", 640.0f, 85.0f, 26.0f, 1.2f, COLOR_PHOSPHOR);
    Rectangle r[2] = { { 400, 180, 176, 286 }, { 704, 180, 176, 286 } };
    if (g->oya_player_choice < 0) {
        for (int i = 0; i < 2; i++) {
            board_draw_card_back(ba, r[i]);
            int hov = CheckCollisionPointRec(vm, r[i]);
            if (hov) {
                board_draw_card_glow(r[i], 1.0f);
                if (!board_table_skin_active()) ui_theme_draw_panel_flares(r[i]);
            }
            draw_centered_text(font_body, "CLICK TO PICK", r[i].x + r[i].width / 2.0f, 485.0f, 18.0f, 1.2f, hov ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
        }
    } else {
        int pc = g->oya_player_choice, cc = 1 - pc, p_cid = g->oya_cards[pc], c_cid = g->oya_cards[cc];

        float pt = fminf(1.0f, g->oya_timer / 0.45f);
        float pw = r[pc].width * fmaxf(0.06f, fabsf(cosf(pt * 3.14159f)));
        Rectangle pr = { r[pc].x + (r[pc].width - pw) / 2.0f, r[pc].y, pw, r[pc].height };
        if (pt < 0.5f) board_draw_card_back(ba, pr); else board_draw_card(ba, p_cid, pr);

        float ct = (g->oya_timer > 0.45f) ? fminf(1.0f, (g->oya_timer - 0.45f) / 0.45f) : 0.0f;
        float cw = r[cc].width * fmaxf(0.06f, fabsf(cosf(ct * 3.14159f)));
        Rectangle cr = { r[cc].x + (r[cc].width - cw) / 2.0f, r[cc].y, cw, r[cc].height };
        if (ct < 0.5f) board_draw_card_back(ba, cr); else board_draw_card(ba, c_cid, cr);

        draw_centered_text(font_body, "YOUR CARD", r[pc].x + r[pc].width / 2.0f, 480.0f, 20.0f, 1.2f, COLOR_PHOSPHOR);
        draw_centered_text(font_body, "CPU'S CARD", r[cc].x + r[cc].width / 2.0f, 480.0f, 20.0f, 1.2f, COLOR_PHOSPHOR_DIM);

        if (g->oya_timer >= 0.9f) {
            draw_centered_text(font_body, TextFormat("Month %d", CARDS[p_cid].month), r[pc].x + r[pc].width / 2.0f, 510.0f, 18.0f, 1.2f, COLOR_PHOSPHOR);
            draw_centered_text(font_body, TextFormat("Month %d", CARDS[c_cid].month), r[cc].x + r[cc].width / 2.0f, 510.0f, 18.0f, 1.2f, COLOR_PHOSPHOR_DIM);

            const char *res = (g->oya_winner == 0) ? "YOU ARE DEALER" : "CPU IS DEALER";
            Color res_col = (g->oya_winner == 0) ? COLOR_PHOSPHOR : COLOR_RED;
            draw_centered_text(font_title, res, 640.0f, 565.0f, 32.0f, 1.5f, res_col);
            Rectangle winner_card = r[(g->oya_winner == 0) ? pc : cc];
            board_draw_card_glow(winner_card, 1.0f);
            if (!board_table_skin_active()) ui_theme_draw_panel_flares(winner_card);
        }
    }
}

int board_check_dealer_select_click(const GameState *g, Vector2 vm) {
    if (g->oya_player_choice >= 0) return -1;
    if (CheckCollisionPointRec(vm, (Rectangle){ 400, 180, 176, 286 })) return 0;
    if (CheckCollisionPointRec(vm, (Rectangle){ 704, 180, 176, 286 })) return 1;
    return -1;
}
