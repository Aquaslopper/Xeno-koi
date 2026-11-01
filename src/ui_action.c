#include "ui_action.h"
#include "ui_theme.h"
#include "ui_motion.h"
#include "ui_card_tooltip.h"
#include "ui_speed.h"
#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

extern Font font_title;
extern Font font_body;
extern GameSettings settings;

#define CEIL_VFX_MOTES   56
#define CEIL_VFX_DUR     1.15f

#define BADGE_FORM_DUR   0.38f
#define BADGE_HOLD_DUR   1.30f
#define BADGE_EFFECT_T   1.68f
#define BADGE_VAPOR_DUR  0.60f
#define BADGE_TOTAL_DUR  2.28f
#define BADGE_VFX_MOTES  96

typedef struct {
    Vector2 pos, vel;
    float ox, oy, delay, phase, freq, swirl;
    int spawned;
} VaporMote;

static unsigned int s_music_meter_raw = 0u;
static unsigned int s_music_meter_epoch = 0u;
static int s_music_tap_attached = 0;
static float s_music_meter_vis = 0.0f;

static int s_ac_symbols_fired = 0;
static int s_ac_lifetime_time = 0;
static int s_action_card_mode = 0;
static int s_action_card_paused = 0;
static float s_action_music_level = 0.0f;
static float s_action_peak_hold = 0.0f;
static float s_action_peak_timer = 0.0f;
static float s_action_added_hold = 0.0f;
static float s_action_added_timer = 0.0f;
static float s_action_full_boost = 0.0f;
static float s_action_ceil_accum = 0.0f;
static int s_action_ceil_started = 0;
static int s_action_match_reset_req = 0;
static int s_action_symbol_idx = 0;
static int s_action_symbol_armed = 0;
static int s_action_pending_sym = -1;
static int s_action_above_90 = 0;
static float s_action_beat_cd = 0.0f;
static float s_action_symbol_flash = 0.0f;
static float s_action_last_synergy = 0.0f;
static uint64_t s_blind_field_mask = 0ULL;
static float s_blind_flip[48];
static int s_action_round_time_num[2] = { 0, 0 };
static int s_action_round_time_pts[2] = { 0, 0 };
static int s_action_match_time_num[2] = { 0, 0 };
static int s_action_match_time_pts[2] = { 0, 0 };

static int s_ceil_vfx_active = 0;
static int s_ceil_vfx_val = 0;
static int s_ceil_vfx_who = 0;
static float s_ceil_vfx_t = 0.0f;
static VaporMote s_ceil_motes[CEIL_VFX_MOTES];

static int s_badge_evt_active = 0;
static int s_badge_evt_sym = 0;
static int s_badge_evt_who = 0;
static int s_badge_evt_applied = 0;
static float s_badge_evt_t = 0.0f;
static VaporMote s_badge_motes[BADGE_VFX_MOTES];

static float smooth01_ac(float x) {
    float t = fminf(1.0f, fmaxf(0.0f, x));
    return t * t * (3.0f - 2.0f * t);
}

static void sync_board_mirror(void) {
    uint64_t mask = s_action_card_mode ? s_blind_field_mask : 0ULL;
    int high = (s_action_card_mode && s_action_last_synergy >= 0.75f) ? 1 : 0;
    board_sync_action_card_mask(mask, high);
}

static void draw_centered_snapped(Font font, const char *text, float cx, float y, float fsz, float sp, Color col) {
    Vector2 ts = MeasureTextEx(font, text, fsz, sp);
    DrawTextEx(font, text, (Vector2){ floorf(cx - ts.x * 0.5f), floorf(y) }, fsz, sp, col);
}

static void music_meter_processor(void *buffer, unsigned int frames) {
    const float *pcm = (const float *)buffer;
    if (!pcm || frames == 0) return;
    static float fast_ms = 0.0f, lo_rms = 0.12f, hi_rms = 0.24f;
    static unsigned int last_ep = 0;
    unsigned int ep = __atomic_load_n(&s_music_meter_epoch, __ATOMIC_RELAXED);
    if (ep != last_ep) { fast_ms = 0.0f; lo_rms = 0.12f; hi_rms = 0.24f; last_ep = ep; }
    for (unsigned int f = 0; f < frames; f++) {
        float l = pcm[f * 2 + 0], r = pcm[f * 2 + 1], p2 = 0.5f * (l * l + r * r);
        if (!(p2 >= 0.0f) || p2 > 4.0f) p2 = 0.0f;
        fast_ms += (p2 - fast_ms) * 0.00085f;
    }
    if (fast_ms < 1e-12f) fast_ms = 0.0f;
    float rms = sqrtf(fast_ms), c_sec = (float)frames / 44100.0f;
    if (rms > hi_rms) hi_rms += (rms - hi_rms) * (1.0f - expf(-c_sec / 0.05f));
    else              hi_rms += (rms - hi_rms) * (1.0f - expf(-c_sec / 0.90f));
    if (rms < lo_rms) lo_rms += (rms - lo_rms) * (1.0f - expf(-c_sec / 0.06f));
    else              lo_rms += (rms - lo_rms) * (1.0f - expf(-c_sec / 0.90f));
    float gate = (hi_rms > 0.015f) ? fminf(1.0f, (hi_rms - 0.015f) / 0.075f) : 0.0f;
    float floor_r = lo_rms * 0.88f, span = fmaxf(0.025f, hi_rms - floor_r);
    float beat = fminf(1.0f, fmaxf(0.0f, (rms - floor_r) / span));
    float norm = gate * (0.08f + 0.92f * powf(beat, 1.20f));
    __atomic_store_n(&s_music_meter_raw, (unsigned int)(norm * 1000000.0f), __ATOMIC_RELAXED);
}

static void sync_music_meter_tap(AudioAssets *audio, int attach) {
    if (!audio || attach == s_music_tap_attached) return;
    __atomic_add_fetch(&s_music_meter_epoch, 1u, __ATOMIC_RELAXED);
    __atomic_store_n(&s_music_meter_raw, 0u, __ATOMIC_RELAXED);
    for (int i = 0; i < AUDIO_MAX_TRACKS; i++) {
        if (!audio->has_music_game[i]) continue;
        if (attach) AttachAudioStreamProcessor(audio->music_game[i].stream, music_meter_processor);
        else        DetachAudioStreamProcessor(audio->music_game[i].stream, music_meter_processor);
    }
    s_music_tap_attached = attach;
    if (!attach) s_music_meter_vis = 0.0f;
}

void action_card_sync_music(AudioAssets *audio, int in_action_table, int paused,
                            float transition_alpha, float dt) {
    /* The tap follows the action table alone. It used to be attached on every screen with
       live music so the menu could drive glyph flares from the beat; with the motes back to
       hover-only that listener has no consumer off the table, so it does not run there. */
    int music_live = (audio && audio->current_music == 2);
    sync_music_meter_tap(audio, in_action_table ? 1 : 0);
    int freeze_music = paused || s_badge_evt_active;
    if (!freeze_music && audio && dt > 0.0f) {
        float tgt = 0.0f;
        if (music_live) {
            float raw = __atomic_load_n(&s_music_meter_raw, __ATOMIC_RELAXED) / 1000000.0f;
            tgt = raw * (1.0f - transition_alpha);
        }
        s_music_meter_vis += (tgt - s_music_meter_vis) *
                             (1.0f - expf(-dt / ((tgt > s_music_meter_vis) ? 0.035f : 0.120f)));
    }
    action_card_set_mode(in_action_table ? (paused ? 2 : 1) : 0, s_music_meter_vis);
}

void action_card_detach_music(AudioAssets *audio) {
    sync_music_meter_tap(audio, 0);
}

static void start_ceil_claim_vfx(int val, int who) {
    if (val <= 0) return;
    s_ceil_vfx_active = 1;
    s_ceil_vfx_val = val;
    s_ceil_vfx_who = (who == 1) ? 1 : 0;
    s_ceil_vfx_t = 0.0f;
    for (int i = 0; i < CEIL_VFX_MOTES; i++) {
        unsigned int h = (unsigned int)(i * 747796405u + (unsigned int)val * 2891336453u + 1013904223u);
        float rx = ((float)(h & 1023u) / 1023.0f) - 0.5f;
        float ry = ((float)((h >> 10) & 1023u) / 1023.0f) - 0.5f;
        float rd = (float)((h >> 20) & 255u) / 255.0f;
        s_ceil_motes[i] = (VaporMote){
            .pos = { 1089.0f, 152.0f }, .vel = { 0.0f, 0.0f },
            .ox = rx, .oy = ry,
            .delay = 0.25f + 0.29f * rd,
            .phase = 6.2831853f * (((float)((h >> 5) & 255u)) / 255.0f),
            .freq = 9.0f + 8.0f * (((float)((h >> 13) & 255u)) / 255.0f),
            .swirl = (rx >= 0.0f) ? (0.4f + rd) : -(0.4f + rd),
            .spawned = 0
        };
    }
}

void action_card_start_badge_event(int sym, int who, AudioAssets *audio) {
    s_badge_evt_active = 1;
    s_badge_evt_sym = ((sym % ACTION_SYM_COUNT) + ACTION_SYM_COUNT) % ACTION_SYM_COUNT;
    s_badge_evt_who = (who == 1) ? 1 : 0;
    s_badge_evt_applied = 0;
    s_badge_evt_t = 0.0f;
    for (int i = 0; i < BADGE_VFX_MOTES; i++) {
        unsigned int h = (unsigned int)(i * 747796405u + (unsigned int)(s_badge_evt_sym + 3) * 2891336453u + 1013904223u);
        float rx = ((float)(h & 1023u) / 1023.0f) - 0.5f;
        float ry = ((float)((h >> 10) & 1023u) / 1023.0f) - 0.5f;
        float rd = (float)((h >> 20) & 255u) / 255.0f;
        s_badge_motes[i] = (VaporMote){
            .pos = { (float)FIELD_X + (float)FIELD_W * 0.5f, (float)FIELD_Y + (float)FIELD_H * 0.5f },
            .vel = { 0.0f, 0.0f },
            .ox = rx, .oy = ry,
            .delay = 0.24f * rd,
            .phase = 6.2831853f * (((float)((h >> 5) & 255u)) / 255.0f),
            .freq = 8.0f + 9.0f * (((float)((h >> 13) & 255u)) / 255.0f),
            .swirl = (rx >= 0.0f) ? (0.5f + rd) : -(0.5f + rd),
            .spawned = 0
        };
    }
    if (audio) audio_play_click(audio);
}

int action_card_is_event_freezing(void) {
    return (s_action_card_mode && s_badge_evt_active) ? 1 : 0;
}

int action_card_update_badge_event(float dt, int *out_sym_to_apply) {
    if (out_sym_to_apply) *out_sym_to_apply = -1;
    if (!s_action_card_mode || !s_badge_evt_active) return 0;
    if (s_action_card_paused) return 0;

    s_badge_evt_t += dt;
    int fire_now = 0;
    if (!s_badge_evt_applied && s_badge_evt_t >= BADGE_EFFECT_T) {
        s_badge_evt_applied = 1;
        if (out_sym_to_apply) *out_sym_to_apply = s_badge_evt_sym;
        fire_now = 1;
    }
    if (s_badge_evt_t >= BADGE_TOTAL_DUR) {
        s_badge_evt_active = 0;
    }
    return fire_now;
}

int action_card_is_mode_active(void) { return s_action_card_mode; }
int action_card_is_paused(void) {
    return (s_action_card_mode && (s_action_card_paused || s_badge_evt_active)) ? 1 : 0;
}

void action_card_set_mode(int active, float music_level) {
    if (!active) {
        s_action_last_synergy = 0.0f;
        s_blind_field_mask = 0ULL;
        s_badge_evt_active = 0;
        s_badge_evt_applied = 0;
        ui_theme_set_global_flare_boost(0.0f, 1.0f);
    }
    s_action_card_mode = (active > 0) ? 1 : 0;
    s_action_card_paused = (active == 2) ? 1 : 0;
    s_action_music_level = (music_level >= 0.0f) ? (music_level > 1.0f ? 1.0f : music_level) : 0.0f;
    sync_board_mirror();
}

int  action_card_get_symbols_fired(void) { return s_ac_symbols_fired; }
int  action_card_get_lifetime_time(void) { return s_ac_lifetime_time; }
void action_card_set_telemetry(int symbols_fired, int lifetime_time) {
    s_ac_symbols_fired = (symbols_fired > 0) ? symbols_fired : 0;
    s_ac_lifetime_time = (lifetime_time > 0) ? lifetime_time : 0;
}

void action_card_advance_round(void) {
    s_action_round_time_num[0] = s_action_round_time_num[1] = 0;
    s_action_round_time_pts[0] = s_action_round_time_pts[1] = 0;
    s_badge_evt_active = 0;
    s_badge_evt_applied = 0;
    s_badge_evt_t = 0.0f;
}

void action_card_reset_match(void) {
    s_action_peak_hold = s_action_peak_timer = 0.0f;
    s_action_added_hold = s_action_added_timer = s_action_full_boost = 0.0f;
    s_action_ceil_accum = 0.0f;
    s_action_ceil_started = 0;
    s_action_symbol_idx = 0;
    s_action_symbol_armed = 0;
    s_action_pending_sym = -1;
    s_action_above_90 = 0;
    s_action_beat_cd = 0.0f;
    s_action_symbol_flash = 0.0f;
    s_action_last_synergy = 0.0f;
    s_blind_field_mask = 0ULL;
    memset(s_blind_flip, 0, sizeof(s_blind_flip));
    action_card_advance_round();
    s_action_match_time_num[0] = s_action_match_time_num[1] = 0;
    s_action_match_time_pts[0] = s_action_match_time_pts[1] = 0;
    s_ceil_vfx_active = 0;
    s_action_match_reset_req = 1;
    sync_board_mirror();
}

int action_card_take_match_reset_req(void) {
    int r = s_action_match_reset_req;
    s_action_match_reset_req = 0;
    return r;
}

int action_card_get_ceiling_count(void) {
    return s_action_ceil_started ? (1 + (int)floorf(s_action_ceil_accum)) : 0;
}

int action_card_claim_ceiling_count(int who) {
    int c = action_card_get_ceiling_count();
    s_action_ceil_accum = 0.0f;
    s_action_ceil_started = 0;
    if (c > 0) {
        int w = (who == 1) ? 1 : 0;
        int pts = (c + 5) / 10;
        s_action_round_time_num[w] += c;
        s_action_round_time_pts[w] += pts;
        s_action_match_time_num[w] += c;
        s_action_match_time_pts[w] += pts;
        if (w == 0) s_ac_lifetime_time += c;
        start_ceil_claim_vfx(c, w);
    }
    return c;
}

int action_card_consume_ceiling_count(void) {
    return action_card_claim_ceiling_count(0);
}

int action_card_get_round_time_stats(int *out_you, int *out_you_pts,
                                     int *out_cpu, int *out_cpu_pts, int *out_meter) {
    int y = s_action_round_time_num[0], yp = s_action_round_time_pts[0];
    int c = s_action_round_time_num[1], cp = s_action_round_time_pts[1];
    int m = action_card_get_ceiling_count();
    if (out_you) *out_you = y;
    if (out_you_pts) *out_you_pts = yp;
    if (out_cpu) *out_cpu = c;
    if (out_cpu_pts) *out_cpu_pts = cp;
    if (out_meter) *out_meter = m;
    return (y > 0 || c > 0 || m > 0) ? 1 : 0;
}

int action_card_get_match_time_stats(int *out_you, int *out_you_pts,
                                     int *out_cpu, int *out_cpu_pts, int *out_meter) {
    int y = s_action_match_time_num[0], yp = s_action_match_time_pts[0];
    int c = s_action_match_time_num[1], cp = s_action_match_time_pts[1];
    int m = action_card_get_ceiling_count();
    if (out_you) *out_you = y;
    if (out_you_pts) *out_you_pts = yp;
    if (out_cpu) *out_cpu = c;
    if (out_cpu_pts) *out_cpu_pts = cp;
    if (out_meter) *out_meter = m;
    return (y > 0 || c > 0 || m > 0) ? 1 : 0;
}

void action_card_format_time_line(char *buf, size_t cap, int yt, int yp, int ct, int cp, int mt) {
    if (yt > 0 && ct > 0) {
        if (mt > 0) snprintf(buf, cap, "TIME NUMBERS:  YOU %d (+%d)  |  CPU %d (+%d)  |  METER %d", yt, yp, ct, cp, mt);
        else snprintf(buf, cap, "TIME NUMBERS:  YOU %d (+%d PTS)  |  CPU %d (+%d PTS)", yt, yp, ct, cp);
    } else if (yt > 0) {
        if (mt > 0) snprintf(buf, cap, "TIME NUMBERS:  YOU %d (+%d PTS)  |  METER %d", yt, yp, mt);
        else snprintf(buf, cap, "TIME NUMBERS:  YOU %d  =  +%d PTS", yt, yp);
    } else if (ct > 0) {
        if (mt > 0) snprintf(buf, cap, "TIME NUMBERS:  CPU %d (+%d PTS)  |  METER %d", ct, cp, mt);
        else snprintf(buf, cap, "TIME NUMBERS:  CPU %d  =  +%d PTS", ct, cp);
    } else {
        snprintf(buf, cap, "TIME NUMBERS:  %d  (ON METER)", mt);
    }
}

void action_card_draw_match_result_extras(const GameState *gs) {
    int yt = 0, yp = 0, ct = 0, cp = 0, mt = 0;
    if (!gs || gs->ruleset != RULESET_KK_ADD_10 || !action_card_get_match_time_stats(&yt, &yp, &ct, &cp, &mt)) return;
    char tline[128];
    action_card_format_time_line(tline, sizeof(tline), yt, yp, ct, cp, mt);
    Vector2 ts = MeasureTextEx(font_body, tline, 19.0f, 1.2f);
    float bw = fmaxf(480.0f, ceilf(ts.x + 44.0f));
    float by = (speed_bar_total() > 0) ? 658.0f : 604.0f;
    float bh = (speed_bar_total() > 0) ? 46.0f : 48.0f;
    Rectangle tbox = { floorf((CANVAS_W - bw) * 0.5f), by, bw, bh };
    DrawRectangleRec(tbox, COLOR_PANEL_BG);
    ui_draw_panel_border(tbox, (Color){ 182, 70, 82, 255 });
    float glint = 0.84f + 0.16f * sinf((float)GetTime() * 2.2f);
    Color tcol = { 236, 150, 160, (unsigned char)(255.0f * glint) };
    DrawTextEx(font_body, tline,
               (Vector2){ floorf(640.0f - ts.x * 0.5f), floorf(tbox.y + (tbox.height - 19.0f) * 0.5f) },
               19.0f, 1.2f, tcol);
}

int action_card_get_symbol(int *out_armed) {
    if (out_armed) *out_armed = s_action_symbol_armed;
    return s_action_symbol_idx;
}

int action_card_take_pending_symbol(void) {
    int sym = s_action_pending_sym;
    s_action_pending_sym = -1;
    return sym;
}

void action_card_trigger_blind_cards(const GameState *g, uint32_t *rng_state, int count) {
    if (!g || count <= 0) return;
    int cand[48], cn = 0;
    for (int i = 0; i < g->field_count; i++) {
        int cid = g->field[i];
        if ((unsigned)cid < 48u && !((s_blind_field_mask >> cid) & 1ULL)) cand[cn++] = cid;
    }
    for (int k = 0; k < count && cn > 0; k++) {
        int pick = rng_state ? (int)(engine_rand(rng_state) % (uint32_t)cn) : (rand() % cn);
        int cid = cand[pick];
        cand[pick] = cand[--cn];
        s_blind_field_mask |= (1ULL << cid);
        s_blind_flip[cid] = 0.001f;
    }
    sync_board_mirror();
}

static void shuffle_stock_zone(GameState *gs) {
    for (int i = gs->stock_count - 1; i > 0; i--) {
        int j = (int)(engine_rand(&gs->rng_state) % (uint32_t)(i + 1));
        int tmp = gs->stock[i]; gs->stock[i] = gs->stock[j]; gs->stock[j] = tmp;
    }
}

void action_card_apply_symbol(int sym, GameState *g, DragState *drag, AnimState *anim,
                              AudioAssets *audio, int *hovered_idx, int *human_turn_started) {
    if (!g) return;
    switch (sym) {
        case ACTION_SYM_SKULL: {
            int ret = g->field_count / 2;
            for (int k = 0; k < ret && g->field_count > 0 && g->stock_count < 48; k++) {
                int pick = (int)(engine_rand(&g->rng_state) % (uint32_t)g->field_count);
                int cid = g->field[pick];
                for (int j = pick; j < g->field_count - 1; j++) g->field[j] = g->field[j + 1];
                g->field_count--;
                g->stock[g->stock_count++] = cid;
                motion_set_target(cid, (Vector2){ 330.0f, 262.0f }, (Vector2){ FIELD_CARD_W, FIELD_CARD_H }, 0.38f);
            }
            if (ret > 0) {
                shuffle_stock_zone(g);
                if (drag) drag->selected = 0;
                if (audio) audio_play_flip(audio);
            }
            break;
        }
        case ACTION_SYM_HAND: {
            if (drag) { drag->on = 0; drag->idx = -1; drag->moved = 0; drag->selected = 0; }
            if (hovered_idx) *hovered_idx = -1;
            if (human_turn_started) *human_turn_started = 0;
            if (anim && anim->active && (anim->type == ANIM_CPU_THINK || anim->type == ANIM_GLIDE_RETURN))
                anim->active = 0;
            if (g->hand_count[0] > 0 && g->hand_count[1] > 0) {
                int cur = (g->current_player == 1) ? 1 : 0, opp = 1 - cur;
                int tmp_hand[48], tmp_cnt = g->hand_count[0];
                memcpy(tmp_hand, g->hand[0], sizeof(tmp_hand));
                memcpy(g->hand[0], g->hand[1], sizeof(tmp_hand));
                memcpy(g->hand[1], tmp_hand, sizeof(tmp_hand));
                g->hand_count[0] = g->hand_count[1];
                g->hand_count[1] = tmp_cnt;
                while (g->hand_count[opp] > g->hand_count[cur] && g->hand_count[cur] < 48) {
                    int pick = (int)(engine_rand(&g->rng_state) % (uint32_t)g->hand_count[opp]);
                    int cid = g->hand[opp][pick];
                    for (int j = pick; j < g->hand_count[opp] - 1; j++) g->hand[opp][j] = g->hand[opp][j + 1];
                    g->hand_count[opp]--;
                    g->hand[cur][g->hand_count[cur]++] = cid;
                }
                if (audio) audio_play_flip(audio);
            } else {
                if (audio) audio_play_click(audio);
            }
            break;
        }
        case ACTION_SYM_REV_ARROW: {
            int moved = 0;
            for (int p = 0; p < 2; p++) {
                int ret = g->captured_count[p] / 2;
                for (int k = 0; k < ret && g->captured_count[p] > 0 && g->stock_count < 48; k++) {
                    int pick = (int)(engine_rand(&g->rng_state) % (uint32_t)g->captured_count[p]);
                    int cid = g->captured[p][pick];
                    for (int j = pick; j < g->captured_count[p] - 1; j++) g->captured[p][j] = g->captured[p][j + 1];
                    g->captured_count[p]--;
                    g->stock[g->stock_count++] = cid;
                    motion_set_target(cid, (Vector2){ 330.0f, 262.0f }, (Vector2){ FIELD_CARD_W, FIELD_CARD_H }, 0.38f);
                    moved++;
                }
                yaku_snapshot(g->ruleset, g->captured[p], g->captured_count[p], g->last_yaku_points[p]);
                g->last_base[p] = score_captures_base(g->ruleset, g->captured[p], g->captured_count[p]);
            }
            if (moved > 0) {
                shuffle_stock_zone(g);
                if (audio) audio_play_flip(audio);
            }
            break;
        }
        case ACTION_SYM_ONE_EYE:
            action_card_trigger_blind_cards(g, &g->rng_state, 2);
            if (drag) drag->selected = 0;
            if (audio) audio_play_flip(audio);
            break;
        case ACTION_SYM_GOLD_COIN: {
            int who = (g->current_player == 1) ? 1 : 0;
            int cnt = action_card_claim_ceiling_count(who);
            int pts = (cnt + 5) / 10;
            if (pts > 0) {
                g->total_score[who] += pts;
                if (audio) audio_play_capture(audio);
            } else if (audio) {
                audio_play_click(audio);
            }
            break;
        }
        default:
            break;
    }
    sync_board_mirror();
}

float action_card_begin_table(const GameState *g, int round_reset, float reverb_synergy) {
    if (round_reset || !s_action_card_mode) {
        s_action_peak_hold = s_action_peak_timer = 0.0f;
        s_action_added_hold = s_action_added_timer = s_action_full_boost = 0.0f;
        s_action_symbol_armed = 0;
        s_action_pending_sym = -1;
        s_action_above_90 = 0;
        s_action_beat_cd = 0.0f;
        s_blind_field_mask = 0ULL;
        memset(s_blind_flip, 0, sizeof(s_blind_flip));
        if (!s_action_card_mode) {
            s_badge_evt_active = 0;
            s_badge_evt_applied = 0;
        }
    }
    if (s_action_card_mode && g) {
        uint64_t fmask = 0ULL;
        for (int i = 0; i < g->field_count; i++) {
            int fcid = g->field[i];
            if ((unsigned)fcid < 48u) fmask |= (1ULL << fcid);
        }
        s_blind_field_mask &= fmask;
    }
    if (s_action_card_mode && !s_action_card_paused && !s_badge_evt_active) {
        s_action_last_synergy = reverb_synergy;
        float g_glow = 0.0f, g_spd = 1.0f;
        if (reverb_synergy >= 0.75f)      { g_glow = 0.90f; g_spd = 2.00f; }
        else if (reverb_synergy >= 0.50f) { g_glow = 0.60f; g_spd = 1.60f; }
        else if (reverb_synergy >= 0.25f) { g_glow = 0.30f; g_spd = 1.00f; }
        ui_theme_set_global_flare_boost(g_glow, g_spd);
    } else {
        if (!s_action_card_mode) s_action_last_synergy = 0.0f;
        ui_theme_set_global_flare_boost(0.0f, 1.0f);
    }
    sync_board_mirror();
    return s_action_card_mode ? reverb_synergy : 0.0f;
}

void action_card_draw_distraction(Rectangle dr, int slot_key, float added_lvl) {
    if (!s_action_card_mode || added_lvl < 0.25f || dr.width <= 0.0f || dr.height <= 0.0f) return;
    float glow = (added_lvl >= 0.75f) ? 1.0f : (added_lvl >= 0.50f) ? 0.65f : 0.32f;
    board_draw_card_glow(dr, glow);
    if (slot_key > 0) ui_theme_draw_panel_flares_keyed(dr, -slot_key);
}

void action_card_draw_stock_returns(const BoardAssets *ba, const GameState *g, float ac_card_lvl) {
    if (!s_action_card_mode || !g || g->stock_count <= 0) return;
    int st_off = g->stock_count >> 2;
    Rectangle top_st = { 330.0f - (float)st_off, 262.0f - (float)st_off, FIELD_CARD_W, FIELD_CARD_H };
    action_card_draw_distraction(top_st, 49, ac_card_lvl);
    for (int i = 0; i < g->stock_count; i++) {
        int scid = g->stock[i];
        if ((unsigned)scid < 48u && motion_is_card_moving(scid)) {
            Rectangle sr = motion_get_rect(scid);
            board_draw_card_shadow(sr, motion_get_card_lift(scid));
            board_draw_card_back(ba, sr);
            action_card_draw_distraction(sr, scid + 1, ac_card_lvl);
        }
    }
}

static void draw_symbol_glyph_scaled(const BoardAssets *ba, int sym, float cx, float cy,
                                     float sc, Color fg, Color cut, float alpha) {
    fg.a  = (unsigned char)((float)fg.a * alpha);
    cut.a = (unsigned char)((float)cut.a * alpha);
    switch (((sym % ACTION_SYM_COUNT) + ACTION_SYM_COUNT) % ACTION_SYM_COUNT) {
        case ACTION_SYM_SKULL:
            DrawCircleV((Vector2){ cx, cy - 3.0f * sc }, 8.2f * sc, fg);
            DrawRectanglePro((Rectangle){ cx - 7.0f * sc, cy - 3.0f * sc, 14.0f * sc, 7.0f * sc },
                             (Vector2){ 0, 0 }, 0.0f, fg);
            DrawRectanglePro((Rectangle){ cx - 5.0f * sc, cy + 4.0f * sc, 10.0f * sc, 5.0f * sc },
                             (Vector2){ 0, 0 }, 0.0f, fg);
            DrawRectanglePro((Rectangle){ cx - 5.0f * sc, cy - 4.0f * sc, 4.0f * sc, 4.0f * sc },
                             (Vector2){ 0, 0 }, 0.0f, cut);
            DrawRectanglePro((Rectangle){ cx + 1.0f * sc, cy - 4.0f * sc, 4.0f * sc, 4.0f * sc },
                             (Vector2){ 0, 0 }, 0.0f, cut);
            DrawRectanglePro((Rectangle){ cx - 1.0f * sc, cy + 1.0f * sc, 2.0f * sc, 2.0f * sc },
                             (Vector2){ 0, 0 }, 0.0f, cut);
            DrawRectanglePro((Rectangle){ cx - 3.0f * sc, cy + 5.0f * sc, fmaxf(1.0f, 1.0f * sc), 4.0f * sc },
                             (Vector2){ 0, 0 }, 0.0f, cut);
            DrawRectanglePro((Rectangle){ cx,             cy + 5.0f * sc, fmaxf(1.0f, 1.0f * sc), 4.0f * sc },
                             (Vector2){ 0, 0 }, 0.0f, cut);
            DrawRectanglePro((Rectangle){ cx + 3.0f * sc, cy + 5.0f * sc, fmaxf(1.0f, 1.0f * sc), 4.0f * sc },
                             (Vector2){ 0, 0 }, 0.0f, cut);
            break;
        case ACTION_SYM_HAND:
            DrawRectanglePro((Rectangle){ cx - 5.0f * sc, cy, 10.0f * sc, 8.0f * sc },
                             (Vector2){ 0, 0 }, 0.0f, fg);
            DrawRectanglePro((Rectangle){ cx - 4.0f * sc, cy + 7.0f * sc, 8.0f * sc, 3.0f * sc },
                             (Vector2){ 0, 0 }, 0.0f, fg);
            DrawLineEx((Vector2){ cx - 4.4f * sc, cy + 1.0f * sc }, (Vector2){ cx - 4.4f * sc, cy - 8.0f * sc }, 2.2f * sc, fg);
            DrawLineEx((Vector2){ cx - 1.5f * sc, cy + 1.0f * sc }, (Vector2){ cx - 1.5f * sc, cy - 9.6f * sc }, 2.2f * sc, fg);
            DrawLineEx((Vector2){ cx + 1.5f * sc, cy + 1.0f * sc }, (Vector2){ cx + 1.5f * sc, cy - 8.4f * sc }, 2.2f * sc, fg);
            DrawLineEx((Vector2){ cx + 4.4f * sc, cy + 1.0f * sc }, (Vector2){ cx + 4.4f * sc, cy - 6.2f * sc }, 2.2f * sc, fg);
            DrawLineEx((Vector2){ cx - 4.6f * sc, cy + 5.2f * sc }, (Vector2){ cx - 9.2f * sc, cy - 0.6f * sc }, 2.4f * sc, fg);
            break;
        case ACTION_SYM_REV_ARROW:
            DrawLineEx((Vector2){ cx + 4.0f * sc, cy - 5.4f * sc }, (Vector2){ cx - 7.5f * sc, cy - 5.4f * sc }, 2.4f * sc, fg);
            DrawLineEx((Vector2){ cx + 4.0f * sc, cy - 5.4f * sc }, (Vector2){ cx + 8.4f * sc, cy - 2.0f * sc }, 2.4f * sc, fg);
            DrawLineEx((Vector2){ cx + 8.4f * sc, cy - 2.0f * sc }, (Vector2){ cx + 8.4f * sc, cy + 2.0f * sc }, 2.4f * sc, fg);
            DrawLineEx((Vector2){ cx + 8.4f * sc, cy + 2.0f * sc }, (Vector2){ cx + 4.0f * sc, cy + 5.6f * sc }, 2.4f * sc, fg);
            DrawLineEx((Vector2){ cx - 4.8f * sc, cy + 5.6f * sc }, (Vector2){ cx + 4.0f * sc, cy + 5.6f * sc }, 2.4f * sc, fg);
            DrawLineEx((Vector2){ cx - 8.8f * sc, cy - 5.4f * sc }, (Vector2){ cx - 2.8f * sc, cy - 10.0f * sc }, 2.4f * sc, fg);
            DrawLineEx((Vector2){ cx - 8.8f * sc, cy - 5.4f * sc }, (Vector2){ cx - 2.8f * sc, cy - 0.8f * sc }, 2.4f * sc, fg);
            break;
        case ACTION_SYM_ONE_EYE:
            DrawLineEx((Vector2){ cx - 10.8f * sc, cy }, (Vector2){ cx - 4.0f * sc, cy - 6.0f * sc }, 2.2f * sc, fg);
            DrawLineEx((Vector2){ cx - 4.0f * sc, cy - 6.0f * sc }, (Vector2){ cx + 4.0f * sc, cy - 6.0f * sc }, 2.2f * sc, fg);
            DrawLineEx((Vector2){ cx + 4.0f * sc, cy - 6.0f * sc }, (Vector2){ cx + 10.8f * sc, cy }, 2.2f * sc, fg);
            DrawLineEx((Vector2){ cx - 10.8f * sc, cy }, (Vector2){ cx - 4.0f * sc, cy + 6.0f * sc }, 2.2f * sc, fg);
            DrawLineEx((Vector2){ cx - 4.0f * sc, cy + 6.0f * sc }, (Vector2){ cx + 4.0f * sc, cy + 6.0f * sc }, 2.2f * sc, fg);
            DrawLineEx((Vector2){ cx + 4.0f * sc, cy + 6.0f * sc }, (Vector2){ cx + 10.8f * sc, cy }, 2.2f * sc, fg);
            DrawCircleV((Vector2){ cx, cy }, 4.7f * sc, fg);
            DrawRectanglePro((Rectangle){ cx - 1.0f * sc, cy - 4.0f * sc, 2.0f * sc, 8.0f * sc },
                             (Vector2){ 0, 0 }, 0.0f, cut);
            DrawRectanglePro((Rectangle){ cx + 1.0f * sc, cy - 3.0f * sc, 2.0f * sc, 2.0f * sc },
                             (Vector2){ 0, 0 }, 0.0f, fg);
            break;
        default: {
            Color gcol = fg;
            if (ba && ba->coin_gold.id > 0) {
                DrawTexturePro(ba->coin_gold, (Rectangle){ 0, 0, (float)ba->coin_gold.width, (float)ba->coin_gold.height },
                               (Rectangle){ cx - 12.0f * sc, cy - 12.0f * sc, 24.0f * sc, 24.0f * sc },
                               (Vector2){ 0, 0 }, 0.0f, gcol);
            } else {
                DrawCircleV((Vector2){ cx, cy }, 9.4f * sc, gcol);
                DrawCircleV((Vector2){ cx, cy }, 6.4f * sc, cut);
                DrawRectanglePro((Rectangle){ cx - 3.0f * sc, cy - 3.0f * sc, 6.0f * sc, 6.0f * sc },
                                 (Vector2){ 0, 0 }, 0.0f, gcol);
            }
            break;
        }
    }
}

void action_card_draw_symbol_icon(const BoardAssets *ba, int sym, float cx, float cy, int armed, float flash) {
    Rectangle box = { cx - 16.0f, cy - 16.0f, 32.0f, 32.0f };
    DrawRectangleRec(box, COLOR_PANEL_BG);
    if (armed) ui_theme_set_panel_flare_glow(box, fminf(1.0f, 0.72f + 0.28f * flash));
    ui_draw_panel_border(box, armed ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    if (flash > 0.01f) {
        BeginBlendMode(BLEND_ADDITIVE);
        Color fc = COLOR_PHOSPHOR;
        fc.a = (unsigned char)(140.0f * flash);
        DrawRectangleRec(box, fc);
        EndBlendMode();
    }
    int is_coin = ((((sym % ACTION_SYM_COUNT) + ACTION_SYM_COUNT) % ACTION_SYM_COUNT) == ACTION_SYM_GOLD_COIN);
    Color fg = is_coin ? (armed ? (Color){ 255, 224, 118, 255 } : (Color){ 156, 132, 72, 130 })
                       : (armed ? COLOR_TEXT_INFO : (Color){ COLOR_PHOSPHOR_DIM.r, COLOR_PHOSPHOR_DIM.g, COLOR_PHOSPHOR_DIM.b, 115 });
    Color cut = { 8, 10, 10, 245 };
    draw_symbol_glyph_scaled(ba, sym, cx, cy, 1.0f, fg, cut, 1.0f);
}

static void draw_action_vertical_bar(Rectangle bar, float lvl, float grey_lvl,
                                     float *hold, float *timer, float dt,
                                     int wine_right, int marker_quartile) {
    if (lvl >= *hold) {
        *hold = lvl;
        *timer = 0.35f;
    } else if (*timer > 0.0f) {
        *timer -= dt;
    } else {
        *hold = fmaxf(lvl, *hold - 0.48f * dt);
    }
    Color hi_c   = wine_right ? (Color){ 224, 136, 146, 255 } : COLOR_TEXT_INFO;
    Color base_c = wine_right ? (Color){ 182,  70,  82, 255 } : COLOR_PHOSPHOR;
    Color dim_c  = wine_right ? (Color){ 136,  48,  58, 255 } : COLOR_PHOSPHOR_DIM;
    DrawRectangleRec(bar, COLOR_PANEL_BG);
    int is_full = (lvl >= 0.98f);
    int ix = (int)bar.x + 2, iw = (int)bar.width - 4, ih = (int)bar.height - 4, bot = (int)bar.y + 2 + ih;
    int fh = is_full ? ih : (int)lroundf((float)ih * lvl);
    int gh = (grey_lvl >= 0.98f) ? ih : (int)lroundf((float)ih * grey_lvl);
    if (gh > fh) {
        int gy = bot - gh, g_h = gh - fh;
        DrawRectangleGradientV(ix, gy, iw, g_h,
                               (Color){ 130, 134, 140, 180 },
                               (Color){  76,  80,  86, 145 });
        DrawRectangle(ix, gy, iw, (g_h >= 2) ? 2 : 1, (Color){ 162, 166, 172, 210 });
    }
    if (fh > 0) {
        Color top_c = hi_c, bot_c = base_c;
        top_c.a = (unsigned char)(195.0f + 60.0f * lvl);
        bot_c.a = (unsigned char)(135.0f + 95.0f * lvl);
        DrawRectangleGradientV(ix, bot - fh, iw, fh, top_c, bot_c);
        DrawRectangle(ix, bot - fh, iw, (fh >= 2) ? 2 : 1, hi_c);
    }
    if (wine_right) {
        for (int t = 1; t <= 3; t++) {
            int ty = bot - (ih * t) / 4;
            if (t == marker_quartile) {
                Color mc = (ty >= bot - fh)
                    ? (Color){ (unsigned char)(dim_c.r / 3), (unsigned char)(dim_c.g / 3), (unsigned char)(dim_c.b / 3), 210 }
                    : (Color){ hi_c.r, hi_c.g, hi_c.b, 215 };
                DrawRectangle(ix, ty - 1, iw, 3, mc);
            } else {
                Color lc = (ty >= bot - fh)
                    ? (Color){ (unsigned char)(dim_c.r / 3), (unsigned char)(dim_c.g / 3), (unsigned char)(dim_c.b / 3), 165 }
                    : (Color){ base_c.r, base_c.g, base_c.b, 150 };
                DrawLine(ix, ty, ix + iw, ty, lc);
            }
        }
    } else {
        int ty = bot - (ih * 9) / 10;
        Color lc = (ty >= bot - fh)
            ? (Color){ (unsigned char)(dim_c.r / 3), (unsigned char)(dim_c.g / 3), (unsigned char)(dim_c.b / 3), 165 }
            : (Color){ base_c.r, base_c.g, base_c.b, 155 };
        DrawLine(ix, ty, ix + iw, ty, lc);
    }
    if (*hold > 0.02f) {
        int py = bot - (int)lroundf((float)ih * (*hold));
        DrawRectangle(ix, (py < (int)bar.y + 2) ? ((int)bar.y + 2) : py, iw, 2, base_c);
    }
    if (is_full) ui_theme_set_panel_flare_glow(bar, 1.0f);
    ui_draw_panel_border(bar, is_full ? base_c : dim_c);
    if (wine_right) {
        int marker_y = bot - (ih * marker_quartile) / 4;
        Color cap_c = (marker_y >= bot - fh) ? hi_c : base_c;
        DrawRectangle((int)bar.x - 3, marker_y - 1, 5, 3, cap_c);
        DrawRectangle((int)(bar.x + bar.width) - 2, marker_y - 1, 5, 3, cap_c);
    }
}

void action_card_draw_meters(const BoardAssets *ba, const GameState *g,
                             float reverb_synergy, float env_dt, int replay_mode) {
    static float s_peak_buf_sec = 0.0f;
    static int   s_grey_lock    = 0;
    if (!s_action_card_mode) {
        s_peak_buf_sec = 0.0f;
        s_grey_lock    = 0;
        return;
    }
    int live_round = g && g->phase != PHASE_DEALER_SELECT &&
                     g->phase != PHASE_ROUND_END && g->phase != PHASE_MATCH_END;
    if (g && g->phase == PHASE_DEALER_SELECT) {
        s_peak_buf_sec = 0.0f;
        s_grey_lock    = 0;
    }
    if (s_grey_lock) {
        if (reverb_synergy <= 0.50f) {
            s_grey_lock    = 0;
            s_peak_buf_sec = 0.0f;
        }
    } else if (reverb_synergy >= 0.98f) {
        if (live_round && !s_action_card_paused && !s_badge_evt_active && !replay_mode) {
            s_peak_buf_sec += env_dt;
            if (s_peak_buf_sec >= 5.0f) {
                s_grey_lock          = 1;
                s_peak_buf_sec       = 0.0f;
                s_action_added_hold  = 0.50f;
                s_action_added_timer = 0.0f;
            }
        }
    } else {
        s_peak_buf_sec = 0.0f;
    }
    float added_lvl = s_grey_lock ? 0.50f : reverb_synergy;
    float grey_lvl  = s_grey_lock ? reverb_synergy : 0.0f;
    int right_full = (added_lvl >= 0.98f);
    int trigger_quartile = (settings.difficulty == DIFFICULTY_EASY) ? 1 :
                           (settings.difficulty == DIFFICULTY_MASTER) ? 3 : 2;
    int right_fh = right_full ? 260 : (int)lroundf(260.0f * added_lvl);
    int right_above_third = (right_fh > (260 * 3) / 4);
    float b_tc = right_above_third ? 0.04f : 0.14f;
    s_action_full_boost += ((right_above_third ? 1.0f : 0.0f) - s_action_full_boost) * (1.0f - expf(-env_dt / b_tc));
    float base_music = 0.80f * s_action_music_level;
    float full_music = fminf(1.0f, 0.40f + 0.68f * s_action_music_level);
    float music_lvl = base_music + (full_music - base_music) * s_action_full_boost;
    if (s_action_beat_cd > 0.0f) {
        s_action_beat_cd -= env_dt;
        if (s_action_beat_cd < 0.0f) s_action_beat_cd = 0.0f;
    }
    if (s_action_symbol_flash > 0.0f) {
        s_action_symbol_flash -= env_dt * 2.8f;
        if (s_action_symbol_flash < 0.0f) s_action_symbol_flash = 0.0f;
    }
    if (live_round && !s_action_card_paused && !s_badge_evt_active && !replay_mode) {
        if (right_full) {
            s_action_ceil_started = 1;
            s_action_ceil_accum += env_dt * 10.0f;
        }
        if (!s_action_above_90 && music_lvl >= 0.90f && s_action_beat_cd <= 0.0f) {
            s_action_above_90 = 1;
            s_action_beat_cd = 0.12f;
            s_action_symbol_idx = (s_action_symbol_idx + 1) % ACTION_SYM_COUNT;
            s_action_symbol_armed = 1;
            s_action_pending_sym = -1;
            s_action_symbol_flash = 1.0f;
        } else if (s_action_above_90 && music_lvl < 0.84f) {
            s_action_above_90 = 0;
        }
        if (s_action_symbol_armed && right_fh < (260 * trigger_quartile) / 4) {
            s_action_pending_sym = s_action_symbol_idx;
            s_action_symbol_armed = 0;
            s_action_symbol_flash = 1.0f;
            s_ac_symbols_fired++;
        }
    }
    draw_action_vertical_bar((Rectangle){ 414.0f, 170.0f, 14.0f, 264.0f }, music_lvl, 0.0f,
                             &s_action_peak_hold, &s_action_peak_timer, env_dt, 0, 2);
    action_card_draw_symbol_icon(ba, s_action_symbol_idx, 421.0f, 148.0f, s_action_symbol_armed, s_action_symbol_flash);
    draw_action_vertical_bar((Rectangle){ 1082.0f, 170.0f, 14.0f, 264.0f }, added_lvl, grey_lvl,
                             &s_action_added_hold, &s_action_added_timer, env_dt, 1,
                             trigger_quartile);
    if (s_action_ceil_started) {
        int ceil_num = action_card_get_ceiling_count();
        const char *cstr = TextFormat("%d", ceil_num);
        Color ccol = right_full ? (Color){ 238, 156, 166, 255 } : (Color){ 182, 70, 82, 225 };
        if (right_full) {
            BeginBlendMode(BLEND_ADDITIVE);
            draw_centered_snapped(font_title, cstr, 1089.0f, 140.0f, 24.0f, 1.0f, (Color){ 182, 70, 82, 120 });
            EndBlendMode();
        }
        draw_centered_snapped(font_title, cstr, 1089.0f, 140.0f, 24.0f, 1.0f, ccol);
    }
}

int action_card_draw_blind_flip(const BoardAssets *ba, int cid, Rectangle dr, float env_dt, float ac_card_lvl) {
    (void)env_dt;
    if (!s_action_card_mode || (unsigned)cid >= 48u || !((s_blind_field_mask >> cid) & 1ULL)) return 0;
    if (s_blind_flip[cid] <= 0.0f || s_blind_flip[cid] >= 1.0f) return 0;
    float vfx_dt = s_action_card_paused ? 0.0f : fminf(0.05f, GetFrameTime());
    s_blind_flip[cid] = fminf(1.0f, s_blind_flip[cid] + vfx_dt / 0.36f);
    float fp = s_blind_flip[cid];
    float hop = sinf(fp * 3.14159265f) * 10.0f;
    float cv = cosf(fp * 3.14159265f);
    float pw = dr.width * fmaxf(0.06f, fabsf(cv));
    Rectangle pr = { dr.x + (dr.width - pw) * 0.5f, dr.y - hop, pw, dr.height };
    board_draw_card_shadow(pr, fmaxf(motion_is_card_moving(cid) ? motion_get_card_lift(cid) : 0.0f, hop / 10.0f));
    if (cv > 0.0f) {
        card_tooltip_record(-1, pr);
        Rectangle src = { (float)((cid >> 2) * ATLAS_CELL_W), (float)((cid & 3) * ATLAS_CELL_H), (float)ATLAS_CELL_W, (float)ATLAS_CELL_H };
        Texture2D ftex = (settings.difficulty == DIFFICULTY_DEUS && ba->atlas_deus.id > 0) ? ba->atlas_deus : ba->atlas;
        DrawTexturePro(ftex, src, pr, (Vector2){ 0, 0 }, 0.0f, WHITE);
        DrawRectangleLinesEx(pr, 1.0f, (Color){ 20, 40, 25, 200 });
    } else {
        board_draw_card_back(ba, pr);
    }
    action_card_draw_distraction(pr, cid + 1, ac_card_lvl);
    return 1;
}

static void draw_ceil_claim_vfx(float dt) {
    if (!s_ceil_vfx_active) return;
    s_ceil_vfx_t += dt;
    if (s_ceil_vfx_t >= CEIL_VFX_DUR) { s_ceil_vfx_active = 0; return; }

    Vector2 orig = { 1089.0f, 152.0f };
    Vector2 tgt = (s_ceil_vfx_who == 1) ? (Vector2){ 540.0f, 34.0f } : (Vector2){ 540.0f, 650.0f };
    float dx = tgt.x - orig.x, dy = tgt.y - orig.y;
    float dlen = sqrtf(dx * dx + dy * dy);
    Vector2 dir = (dlen > 1e-3f) ? (Vector2){ dx / dlen, dy / dlen } : (Vector2){ -1.0f, 0.0f };
    Vector2 perp = { -dir.y, dir.x };
    const char *txt = TextFormat("%d", s_ceil_vfx_val);

    float u_grow = fminf(1.0f, s_ceil_vfx_t / 0.26f);
    float grow = smooth01_ac(u_grow);
    float fsz = 24.0f + 16.0f * grow;
    float u_diss = (s_ceil_vfx_t > 0.30f) ? fminf(1.0f, (s_ceil_vfx_t - 0.30f) / 0.34f) : 0.0f;
    float d_alpha = 1.0f - smooth01_ac(u_diss);
    float cx = orig.x + dir.x * 18.0f * u_diss, cy = orig.y + dir.y * 18.0f * u_diss;
    Color accent = (s_ceil_vfx_who == 0) ? (Color){ 255, 208, 68, 255 } : (Color){ 244, 96, 114, 255 };

    if (d_alpha > 0.01f) {
        int flash_on = (((int)(s_ceil_vfx_t * 36.0f)) & 1);
        float jx = sinf(s_ceil_vfx_t * 95.0f) * 2.4f * u_diss;
        float jy = cosf(s_ceil_vfx_t * 83.0f) * 2.0f * u_diss;
        Vector2 ts = MeasureTextEx(font_title, txt, fsz, 1.2f);
        Vector2 tpos = { cx - ts.x * 0.5f + jx, cy - fsz * 0.5f + jy };
        BeginBlendMode(BLEND_ADDITIVE);
        DrawTextEx(font_title, txt, tpos, fsz, 1.2f,
                   (Color){ accent.r, accent.g, accent.b, (unsigned char)((flash_on ? 210.0f : 110.0f) * d_alpha) });
        EndBlendMode();
        Color fg = flash_on ? (Color){ 255, 250, 226, (unsigned char)(255.0f * d_alpha) }
                            : (Color){ accent.r, accent.g, accent.b, (unsigned char)(245.0f * d_alpha) };
        DrawTextEx(font_title, txt, tpos, fsz, 1.2f, fg);
    }

    Vector2 bsz = MeasureTextEx(font_title, txt, 40.0f, 1.2f);
    float bw = fmaxf(30.0f, bsz.x);
    BeginBlendMode(BLEND_ADDITIVE);
    for (int i = 0; i < CEIL_VFX_MOTES; i++) {
        VaporMote *m = &s_ceil_motes[i];
        if (s_ceil_vfx_t < m->delay) continue;
        if (!m->spawned) {
            m->pos = (Vector2){ cx + m->ox * bw, cy + m->oy * 30.0f };
            float sp0 = 135.0f + 95.0f * (0.5f + m->ox);
            m->vel = (Vector2){ dir.x * sp0 + perp.x * m->swirl * 58.0f,
                                dir.y * sp0 + perp.y * m->swirl * 58.0f };
            m->spawned = 1;
        }
        float age = s_ceil_vfx_t - m->delay;
        float max_age = fmaxf(0.15f, CEIL_VFX_DUR - m->delay);
        float prog = fminf(1.0f, age / max_age);
        float curl = sinf(age * m->freq + m->phase) * 240.0f;
        m->vel.x += (dir.x * 1450.0f + perp.x * curl) * dt;
        m->vel.y += (dir.y * 1450.0f + perp.y * curl) * dt;
        m->pos.x += m->vel.x * dt;
        m->pos.y += m->vel.y * dt;
        float ma = sinf(prog * 3.14159265f) * (1.0f - 0.35f * prog);
        if (ma <= 0.01f) continue;
        Vector2 tail = { m->pos.x - m->vel.x * 0.022f, m->pos.y - m->vel.y * 0.022f };
        DrawLineEx(tail, m->pos, 1.0f + 0.05f * ma,
                   (Color){ accent.r, accent.g, accent.b, (unsigned char)(195.0f * ma) });
        DrawRectangle((int)lroundf(m->pos.x), (int)lroundf(m->pos.y), 2, 2,
                      (Color){ 255, 250, 232, (unsigned char)(245.0f * ma) });
    }
    EndBlendMode();
}

static Vector2 badge_mote_point(const VaporMote *m, Rectangle tr, float tau) {
    float cx = tr.x + tr.width * 0.5f, cy = tr.y + tr.height * 0.5f;
    float sx = cx + m->ox * tr.width * 0.92f, sy = cy + m->oy * tr.height * 0.88f;
    float rdx = m->ox, rdy = m->oy - 0.14f;
    float rlen = sqrtf(rdx * rdx + rdy * rdy);
    Vector2 out = (rlen > 1e-3f) ? (Vector2){ rdx / rlen, rdy / rlen } : (Vector2){ 0.0f, -1.0f };
    Vector2 perp = { -out.y, out.x };
    float dist = (68.0f + 96.0f * fabsf(m->swirl)) * (0.35f * tau + 0.65f * tau * tau);
    float curl = sinf(tau * m->freq * 0.65f + m->phase) * 20.0f * tau * m->swirl;
    return (Vector2){ sx + out.x * dist + perp.x * curl,
                      sy + out.y * dist + perp.y * curl - 68.0f * tau * tau };
}

static void draw_badge_popup_vapor_vfx(const BoardAssets *ba, float dt) {
    (void)dt;
    if (!s_badge_evt_active) return;

    static const char * const k_titles[ACTION_SYM_COUNT] = {
        "SKULL // FIELD PURGE",
        "SEVERED HAND // HAND SWAP",
        "REVERSE ARROW // CAPTURE RECALL",
        "ONE EYE // FIELD BLINDNESS",
        "GOLD COIN // CEILING CLAIM"
    };
    static const char * const k_subs[ACTION_SYM_COUNT] = {
        "HALF OF FIELD CARDS RETURN TO STOCK",
        "SWAPPING HANDS WITH OPPONENT",
        "HALF OF CAPTURED CARDS RETURN TO STOCK",
        "TWO FIELD CARDS TURNED FACE DOWN",
        "CLAIMING ACCUMULATED TIME NUMBERS"
    };

    float bw = floorf((float)FIELD_W * 0.70f);
    float bh = floorf((float)FIELD_H * 0.70f);
    Rectangle badge_r = {
        floorf((float)FIELD_X + ((float)FIELD_W - bw) * 0.5f),
        floorf((float)FIELD_Y + ((float)FIELD_H - bh) * 0.5f),
        bw, bh
    };
    int forming  = (s_badge_evt_t < BADGE_FORM_DUR);
    int vaporing = (s_badge_evt_t >= BADGE_EFFECT_T);

    float u_in  = (s_badge_evt_t > 0.10f)
                  ? fminf(1.0f, (s_badge_evt_t - 0.10f) / (BADGE_FORM_DUR - 0.10f)) : 0.0f;
    float u_out = vaporing ? fminf(1.0f, (s_badge_evt_t - BADGE_EFFECT_T) / 0.32f) : 0.0f;
    float u_diss = forming ? (1.0f - u_in) : u_out;
    float d_alpha = 1.0f - smooth01_ac(u_diss);
    int is_coin = (s_badge_evt_sym == ACTION_SYM_GOLD_COIN);
    Color accent = is_coin ? (Color){ 255, 214, 92, 255 } : (Color){ 224, 108, 122, 255 };
    Color rim_c  = is_coin ? (Color){ 255, 224, 118, 255 } : COLOR_PHOSPHOR;

    if (d_alpha > 0.01f) {
        float jx = sinf(s_badge_evt_t * 96.0f) * 5.0f * u_diss;
        float jy = cosf(s_badge_evt_t * 82.0f) * 3.0f * u_diss;
        Rectangle box = { badge_r.x + jx, badge_r.y + jy, badge_r.width, badge_r.height };

        DrawRectangleRec(box, (Color){ 6, 10, 10, (unsigned char)(218.0f * d_alpha) });
        Rectangle inner = { box.x + 8.0f, box.y + 8.0f, box.width - 16.0f, box.height - 16.0f };
        DrawRectangleLinesEx(box, 2.0f, (Color){ rim_c.r, rim_c.g, rim_c.b, (unsigned char)(245.0f * d_alpha) });
        DrawRectangleLinesEx(inner, 1.5f, (Color){ accent.r, accent.g, accent.b, (unsigned char)(185.0f * d_alpha) });
        if (u_diss < 0.35f) {
            ui_theme_set_panel_flare_glow(box, 0.92f * d_alpha);
            ui_theme_draw_panel_flares(box);
        }

        /* Subtle additive pulse on reverse-vaporize condensation & effect trigger */
        float lock_p   = 1.0f - fminf(1.0f, fabsf(s_badge_evt_t - BADGE_FORM_DUR) / 0.14f);
        float impact_p = 1.0f - fminf(1.0f, fabsf(s_badge_evt_t - BADGE_EFFECT_T) / 0.16f);
        float pulse    = fmaxf(lock_p * 0.75f, impact_p);
        if (pulse > 0.01f) {
            BeginBlendMode(BLEND_ADDITIVE);
            DrawRectangleRec(box, (Color){ accent.r, accent.g, accent.b, (unsigned char)(70.0f * pulse * d_alpha) });
            EndBlendMode();
        }

        float sc = fminf(box.width / 82.0f, box.height / 64.0f);
        float gcx = box.x + box.width * 0.5f;
        float gcy = box.y + box.height * 0.42f;
        Color fg = is_coin ? (Color){ 255, 224, 118, 255 } : COLOR_TEXT_INFO;
        Color cut = { 8, 12, 12, 245 };

        BeginBlendMode(BLEND_ADDITIVE);
        draw_symbol_glyph_scaled(ba, s_badge_evt_sym, gcx, gcy, sc * 1.05f,
                                 (Color){ accent.r, accent.g, accent.b, 125 }, cut, d_alpha);
        EndBlendMode();
        draw_symbol_glyph_scaled(ba, s_badge_evt_sym, gcx, gcy, sc, fg, cut, d_alpha);

        float max_tw = box.width - 36.0f;
        float t_fsz = 21.0f;
        Vector2 tts = MeasureTextEx(font_title, k_titles[s_badge_evt_sym], t_fsz, 1.1f);
        if (tts.x > max_tw && tts.x > 1.0f) t_fsz = floorf(t_fsz * (max_tw / tts.x));
        float s_fsz = 16.0f;
        Vector2 sts = MeasureTextEx(font_body, k_subs[s_badge_evt_sym], s_fsz, 1.0f);
        if (sts.x > max_tw && sts.x > 1.0f) s_fsz = floorf(s_fsz * (max_tw / sts.x));

        const char *who_tag = (s_badge_evt_who == 1) ? "TARGET: CPU" : "TARGET: YOU";
        draw_centered_snapped(font_body, who_tag, gcx, box.y + 16.0f, 16.0f, 1.2f,
                              (Color){ accent.r, accent.g, accent.b, (unsigned char)(235.0f * d_alpha) });
        draw_centered_snapped(font_title, k_titles[s_badge_evt_sym], gcx, box.y + box.height - 68.0f, t_fsz, 1.1f,
                              (Color){ rim_c.r, rim_c.g, rim_c.b, (unsigned char)(255.0f * d_alpha) });
        draw_centered_snapped(font_body, k_subs[s_badge_evt_sym], gcx, box.y + box.height - 36.0f, s_fsz, 1.0f,
                              (Color){ COLOR_TEXT_INFO.r, COLOR_TEXT_INFO.g, COLOR_TEXT_INFO.b, (unsigned char)(240.0f * d_alpha) });
    }

    if (forming || vaporing) {
        float phase_u = forming ? (1.0f - fminf(1.0f, s_badge_evt_t / BADGE_FORM_DUR))
                                : fminf(1.0f, (s_badge_evt_t - BADGE_EFFECT_T) / BADGE_VAPOR_DUR);
        BeginBlendMode(BLEND_ADDITIVE);
        for (int i = 0; i < BADGE_VFX_MOTES; i++) {
            const VaporMote *m = &s_badge_motes[i];
            if (phase_u < m->delay) continue;
            float tau = fminf(1.0f, (phase_u - m->delay) / fmaxf(0.10f, 1.0f - m->delay));
            float ma = sinf(tau * 3.14159265f) * (1.0f - 0.28f * tau);
            if (ma <= 0.01f) continue;
            float tau_tail = fminf(1.0f, fmaxf(0.0f, tau + (forming ? 0.085f : -0.085f)));
            Vector2 p0 = badge_mote_point(m, badge_r, tau);
            Vector2 pt = badge_mote_point(m, badge_r, tau_tail);
            DrawLineEx(pt, p0, 1.5f + 0.8f * ma,
                       (Color){ accent.r, accent.g, accent.b, (unsigned char)(205.0f * ma) });
            DrawRectangle((int)lroundf(p0.x) - 1, (int)lroundf(p0.y) - 1, 3, 3,
                          (Color){ rim_c.r, rim_c.g, rim_c.b, (unsigned char)(245.0f * ma) });
        }
        EndBlendMode();
    }
}

void action_card_draw_vfx(const BoardAssets *ba, float env_dt) {
    (void)env_dt;
    if (!s_action_card_mode) return;
    float vfx_dt = s_action_card_paused ? 0.0f : fminf(0.05f, GetFrameTime());
    draw_ceil_claim_vfx(vfx_dt);
    draw_badge_popup_vapor_vfx(ba, vfx_dt);
}
