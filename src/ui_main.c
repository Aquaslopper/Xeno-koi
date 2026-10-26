#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include "raylib.h"
#include "engine.h"
#include "ai.h"
#include "storage.h"
#include "ui_theme.h"
#include "ui_board.h"
#include "ui_card_tooltip.h"
#include "ui_capture_info.h"
#include "ui_anim.h"
#include "ui_motion.h"
#include "ui_audio.h"
#include "ui_screens.h"
#include "ui_hub.h"
#include "ui_plexus.h"
#include "ui_input.h"
#include "ui_combos.h"
#include "campaign.h"
#include "ui_taunt.h"
#include "ui_speed.h"
#include "ui_action.h"
#include "replay.h"
#include "ui_replay.h"

#ifndef IsFontReady
#define IsFontReady(f) ((f).texture.id > 0)
#endif

Font font_title;  // Audiowide for titles, headers, and button labels
Font font_body;   // Play for data, options, HUD, and supporting text
Font small_font;  // Legacy alias for modules that still use the old name
GameSettings settings;

#define TRANS_OUT_DUR    1.80f
#define TRANS_HOLD_DUR   2.20f
#define TRANS_IN_DUR     1.20f
#define ROUND_BANNER_DUR 4.00f
#define HUB_BLINK_DUR    0.20f
#define HUB_WIPE_DUR     0.16f

static float transition_alpha = 0.0f, transition_timer = 0.0f, round_banner_timer = 0.0f;
static float cached_bloom = -1.0f, cached_crt = -1.0f, cached_gamma = -1.0f;
static float saved_timer = 0.0f, log_timer = 0.0f, overlay_alpha = 0.0f, deus_banner_timer = 0.0f, speedcoin_gameover_timer = 0.0f;
static float event_beat_timer = 0.0f;
static int transition_dir = 0, transition_action = 0, transition_track = 1, pending_result_sfx = 0, loc_bloom, loc_crt, loc_gamma;
static int is_paused = 0, confirm_restart = 0, confirm_reset = 0, campaign_overlay_open = 0;
static int hub_trans_active = 0, hub_from_campaign = 0, hub_reset_opt_page = 0, hub_btn_fsz = 18;
static float hub_trans_timer = 0.0f;
static ScreenState hub_from_screen = SCREEN_START, options_return_screen = SCREEN_START;
static ScreenState stats_return_screen = SCREEN_START, atrium_return_screen = SCREEN_START, match_origin = SCREEN_START;
static Rectangle hub_btn_rect = { 0, 0, 0, 0 };
static const char *hub_btn_label = "";
static int combo_player = -1, combo_page = 0;
static int last_replay_round = 0;   /* replay only: last round the deal was shown for */
static float replay_beat = 0.0f;    /* replay only: pause after a card lands */
static int last_overlay_paused = 0, hovered_idx = -1, frame_count = 0, human_turn_started = 0, shot_frame = -1, quit_requested = 0;
static ScreenState transition_target_screen = SCREEN_START, screen = SCREEN_START;
static GamePhase last_overlay_phase = PHASE_NONE, prev_round_phase = PHASE_NONE;
static AudioAssets audio;
static Sound sfx_banner, sfx_card_slam;
static int has_sfx_banner = 0, has_sfx_card_slam = 0;
static RenderTexture2D canvas, overlay_canvas;
static Shader post_shader;
static BoardAssets assets;
static GameStats stats;
static MatchHistory history;
static GameState g;
static AnimState anim;
static DragState drag;
static uint32_t ai_rng = 1006;

static float smooth01(float x) {
    float t = fminf(1.0f, fmaxf(0.0f, x));
    return t * t * (3.0f - 2.0f * t);
}

static void start_transition(ScreenState target, int action, int track) {
    hub_trans_active = 0;
    if (hub_reset_opt_page) { screens_set_options_page(0); hub_reset_opt_page = 0; }
    transition_dir = 1; transition_timer = 0.0f;
    transition_target_screen = target; transition_action = action; transition_track = track;
}

static void start_hub_transition(ScreenState next_screen, int next_campaign,
                                 Rectangle btn_r, const char *btn_lbl, int btn_fsz) {
    if (next_screen != SCREEN_GAME && next_screen != SCREEN_MATCH_RESULT)
        plexus_restore_match_settings(&settings, &assets);
    hub_from_screen = screen;
    hub_from_campaign = campaign_overlay_open;
    screen = next_screen;
    campaign_overlay_open = next_campaign;
    hub_btn_rect = btn_r;
    hub_btn_label = btn_lbl;
    hub_btn_fsz = btn_fsz;
    hub_trans_timer = 0.0f;
    hub_trans_active = 1;
}

static void draw_credits_slogan(void) {
    const char *lhs = "[to_be \xCE\xA8(n)]";
    const char *rhs = "[not_to_be \xCE\xA6(n)]";
    float fsz = 20.0f, sp = 1.4f, r = 6.2f, pad = 9.0f;
    Vector2 ls = MeasureTextEx(font_body, lhs, fsz, sp);
    Vector2 rs = MeasureTextEx(font_body, rhs, fsz, sp);
    float sym_w = (r + pad) * 2.0f;
    float x0 = floorf(640.0f - (ls.x + sym_w + rs.x) * 0.5f);
    float y0 = 488.0f;
    DrawTextEx(font_body, lhs, (Vector2){ x0, y0 }, fsz, sp, COLOR_PHOSPHOR);
    Vector2 oc = { x0 + ls.x + pad + r, y0 + 10.2f };
    float d = (r - 0.8f) * 0.70710678f;
    DrawRing(oc, r - 1.55f, r, 0.0f, 360.0f, 32, COLOR_PHOSPHOR);
    DrawLineEx((Vector2){ oc.x - d, oc.y - d }, (Vector2){ oc.x + d, oc.y + d }, 1.55f, COLOR_PHOSPHOR);
    DrawLineEx((Vector2){ oc.x - d, oc.y + d }, (Vector2){ oc.x + d, oc.y - d }, 1.55f, COLOR_PHOSPHOR);
    DrawTextEx(font_body, rhs, (Vector2){ floorf(x0 + ls.x + sym_w), y0 }, fsz, sp, COLOR_PHOSPHOR);
}

static void draw_hub_screen_state(ScreenState sc, int camp_open, Vector2 mouse_pos) {
    if (sc == SCREEN_START) {
        ui_hub_draw_title(&assets, mouse_pos, saved_timer);
        if (camp_open) campaign_draw_overlay(&assets, &settings, settings.current_opponent, mouse_pos);
    } else if (sc == SCREEN_ATRIUM) {
        plexus_draw(&assets, &stats, mouse_pos);
    } else if (sc == SCREEN_OPTIONS) {
        screens_draw_options(&assets, &settings, mouse_pos);
    } else if (sc == SCREEN_STATS) {
        screens_draw_stats(&assets, &stats, &settings, &history, mouse_pos, confirm_reset, log_timer);
    } else if (sc == SCREEN_CREDITS) {
        screens_draw_credits(&assets, mouse_pos);
        draw_credits_slogan();
    } else if (sc == SCREEN_MATCH_RESULT) {
        if (plexus_speedcoin_result_active())
            plexus_draw_speedcoin_result(&assets, &stats, mouse_pos);
        else if (plexus_training_result_active())
            plexus_draw_training_result(&assets, &g, &stats, mouse_pos);
        else if (plexus_action_card_result_active())
            plexus_draw_action_card_result(&assets, &g, &stats, mouse_pos);
        else {
            screens_draw_match_result(&assets, &g, (plexus_training_is_active() || plexus_action_card_is_active()) ? NULL : &settings, mouse_pos);
            action_card_draw_match_result_extras(&g);
        }
    } else if (sc == SCREEN_HUB_MAIN) {
        ui_hub_draw_main(&assets, &settings, plexus_get_spine_count(), mouse_pos);
    } else if (sc == SCREEN_HUB_CAMPAIGN) {
        ui_hub_draw_campaign(&assets, &settings, mouse_pos);
        if (camp_open) campaign_draw_overlay(&assets, &settings, settings.current_opponent, mouse_pos);
    }
}

static void draw_hub_strobe_button(Rectangle r, const char *txt, int fsz, int flash_on) {
    int from_plex_res = (hub_from_screen == SCREEN_MATCH_RESULT) &&
                        (plexus_speedcoin_result_active() || plexus_training_result_active() || plexus_action_card_result_active());
    int use_title = hub_from_campaign || (hub_from_screen == SCREEN_START) ||
                    (hub_from_screen == SCREEN_ATRIUM) || from_plex_res ||
                    (hub_from_screen == SCREEN_HUB_MAIN) || (hub_from_screen == SCREEN_HUB_CAMPAIGN);
    Font fnt = use_title ? font_title : font_body;
    float sp = use_title ? 1.2f : 1.0f;
    Vector2 ts = MeasureTextEx(fnt, txt, (float)fsz, sp);
    Vector2 pos = { floorf(r.x + (r.width - ts.x) * 0.5f), floorf(r.y + (r.height - (float)fsz) * 0.5f) };
    if (flash_on) {
        Color outer1 = COLOR_PHOSPHOR; outer1.a = 115;
        Color outer2 = COLOR_PHOSPHOR; outer2.a = 225;
        DrawRectangleLinesEx((Rectangle){ r.x - 4.0f, r.y - 4.0f, r.width + 8.0f, r.height + 8.0f }, 1.5f, outer1);
        DrawRectangleLinesEx((Rectangle){ r.x - 2.0f, r.y - 2.0f, r.width + 4.0f, r.height + 4.0f }, 2.0f, outer2);
        DrawRectangleRec(r, COLOR_PHOSPHOR);
        DrawRectangle((int)r.x + 2, (int)r.y + 2, (int)r.width - 4, (int)(r.height * 0.42f), (Color){ 255, 255, 255, 68 });
        DrawTextEx(fnt, txt, pos, (float)fsz, sp, (Color){ 4, 10, 12, 255 });
    } else {
        DrawRectangleRec(r, (Color){ 0, 0, 0, 238 });
        DrawRectangleLinesEx(r, 1.5f, COLOR_PHOSPHOR);
        Color dim = COLOR_PHOSPHOR_DIM; dim.a = 150;
        DrawTextEx(fnt, txt, pos, (float)fsz, sp, dim);
    }
}

static void draw_hub_scan_beam(int split_y, float fade) {
    if (fade <= 0.001f) return;
    Color trail_hi = COLOR_PHOSPHOR; trail_hi.a = (unsigned char)(88.0f * fade);
    Color trail_lo = COLOR_PHOSPHOR; trail_lo.a = 0;
    DrawRectangleGradientV(0, split_y - 66, CANVAS_W, 66, trail_lo, trail_hi);
    Color lead_hi = COLOR_PHOSPHOR; lead_hi.a = (unsigned char)(64.0f * fade);
    DrawRectangleGradientV(0, split_y + 1, CANVAS_W, 7, lead_hi, trail_lo);
    Color beam = COLOR_PHOSPHOR; beam.a = (unsigned char)(235.0f * fade);
    DrawRectangle(0, split_y - 1, CANVAS_W, 2, beam);
    DrawRectangle(0, split_y, CANVAS_W, 1, (Color){ 235, 255, 255, (unsigned char)(195.0f * fade) });
}

static void init_banner_sfx(void) {
    if (!IsAudioDeviceReady()) return;
    enum { SR = 44100, FRAMES = 11466 }; /* 0.26 s subtle low sci-fi notification tone */
    static float pcm[FRAMES * 2];
    for (int i = 0; i < FRAMES; i++) {
        float t = (float)i / (float)SR, sl = 0.0f, sr_v = 0.0f;
        if (t < 0.120f) {
            float att = 0.5f * (1.0f - cosf(3.14159265f * fminf(1.0f, t / 0.007f)));
            float rel = (t > 0.085f) ? cosf(1.5707963f * (t - 0.085f) / 0.035f) : 1.0f;
            float env = att * rel * expf(-t * 14.0f);
            float f1 = 110.0f + 12.0f * expf(-t * 55.0f), fm = 0.32f * expf(-t * 32.0f);
            float pl = 6.2831853f * (f1 - 0.35f) * t, pr = 6.2831853f * (f1 + 0.35f) * t + 0.22f;
            sl   += env * (0.78f * sinf(pl + fm * sinf(2.0f * pl)) + 0.22f * sinf(0.5f * pl));
            sr_v += env * (0.78f * sinf(pr + fm * sinf(2.0f * pr)) + 0.22f * sinf(0.5f * pr));
        }
        if (t >= 0.072f) {
            float u = t - 0.072f;
            float att = 0.5f * (1.0f - cosf(3.14159265f * fminf(1.0f, u / 0.008f)));
            float tail = (t > 0.200f) ? 0.5f * (1.0f + cosf(3.14159265f * (t - 0.200f) / 0.060f)) : 1.0f;
            float env = 1.08f * att * tail * expf(-u * 11.5f), fm = 0.28f * expf(-u * 28.0f);
            float pl = 6.2831853f * 146.43f * u, pr = 6.2831853f * 147.23f * u + 0.30f;
            sl   += env * (0.80f * sinf(pl + fm * sinf(2.0f * pl)) + 0.20f * sinf(0.5f * pl));
            sr_v += env * (0.80f * sinf(pr + fm * sinf(2.0f * pr)) + 0.20f * sinf(0.5f * pr));
        }
        pcm[i * 2 + 0] = sl * 0.27f;
        pcm[i * 2 + 1] = sr_v * 0.27f;
    }
    Wave w = { .frameCount = FRAMES, .sampleRate = SR, .sampleSize = 32, .channels = 2, .data = pcm };
    sfx_banner = LoadSoundFromWave(w);
    has_sfx_banner = (sfx_banner.stream.buffer != NULL);
}

static void play_banner_sfx(void) {
    if (!has_sfx_banner || audio.sfx_volume <= 0.0f) return;
    SetSoundVolume(sfx_banner, audio.sfx_volume);
    PlaySound(sfx_banner);
}

static void init_card_slam_sfx(void) {
    if (!IsAudioDeviceReady()) return;
    const char *dir = GetApplicationDirectory() ? GetApplicationDirectory() : "";
    const char *cands[4] = {
        "assets/sfx/card_slam.wav", TextFormat("%s/assets/sfx/card_slam.wav", dir),
        "assets/sfx/card_slam.ogg", TextFormat("%s/assets/sfx/card_slam.ogg", dir)
    };
    for (int i = 0; i < 4; i++) {
        if (FileExists(cands[i])) {
            sfx_card_slam = LoadSound(cands[i]);
            has_sfx_card_slam = (sfx_card_slam.stream.buffer != NULL);
            if (has_sfx_card_slam) return;
        }
    }
    enum { SR = 44100, FRAMES = 4851 }; /* 0.11 s fallback impact until assets/sfx/card_slam.wav is added */
    static float pcm[FRAMES * 2];
    for (int i = 0; i < FRAMES; i++) {
        float t = (float)i / (float)SR;
        float thud = expf(-t * 38.0f) * sinf(6.2831853f * (165.0f * expf(-t * 26.0f) + 58.0f) * t);
        float snap = expf(-t * 170.0f) * (0.55f * sinf(6.2831853f * 1420.0f * t) + 0.45f * sinf(6.2831853f * 2680.0f * t));
        float s = (0.72f * thud + 0.34f * snap) * 0.42f;
        pcm[i * 2 + 0] = s;
        pcm[i * 2 + 1] = s;
    }
    Wave w = { .frameCount = FRAMES, .sampleRate = SR, .sampleSize = 32, .channels = 2, .data = pcm };
    sfx_card_slam = LoadSoundFromWave(w);
    has_sfx_card_slam = (sfx_card_slam.stream.buffer != NULL);
}

static void play_card_slam_sfx(void) {
    if (!has_sfx_card_slam || audio.sfx_volume <= 0.0f) return;
    float v = fminf(1.0f, fmaxf(0.40f, audio.sfx_volume * 1.90f));
    SetSoundVolume(sfx_card_slam, v);
    PlaySound(sfx_card_slam);
}

static void play_match_capture_sfx(void) {
    play_card_slam_sfx();
    audio_play_capture(&audio);
}

static int pick_match_track(int next) {
    int n = (audio.game_track_count > 0) ? audio.game_track_count : 1;
    if (next && n > 1 && audio.current_game_track >= 0)
        return ((audio.current_game_track + 1 + (rand() % (n - 1))) % n) + 1;
    return (rand() % n) + 1;
}

static void begin_new_match(int trk) {
    plexus_sync_match_settings(&settings, &assets);
    if (frame_count > 0 && !plexus_tutorial_is_active()) plexus_save();
    if (!plexus_tutorial_is_active() && !plexus_speedcoin_is_active() && settings.difficulty == DIFFICULTY_DEUS) {
        if (!plexus_action_card_is_active()) settings.ruleset = RULESET_YAKUZA;
        settings.background = 5; assets.current_bg = 5; theme_set(5);
    }
    replay_rec_abort();
    anim_init(&anim); motion_init(); speed_bar_reset(); action_card_reset_match(); memset(&drag, 0, sizeof(drag));
    hovered_idx = -1; is_paused = 0; confirm_restart = 0; combo_player = -1; combo_page = 0;
    human_turn_started = 0; deus_banner_timer = 0.0f; speedcoin_gameover_timer = 0.0f; round_banner_timer = 0.0f;
    event_beat_timer = 0.0f; prev_round_phase = PHASE_NONE;
    overlay_alpha = 0.0f; last_overlay_phase = PHASE_NONE; last_overlay_paused = 0;
    uint32_t match_seed = (uint32_t)time(NULL) ^ (uint32_t)(frame_count * 2654435761u);
    int init_rounds = (plexus_tutorial_is_active() && plexus_action_card_is_active()) ? 3 : settings.rounds;
    engine_init(&g, plexus_action_card_is_active() ? RULESET_KK_ADD_10 : settings.ruleset, init_rounds, match_seed);
    if (plexus_speedcoin_is_active()) {
        g.rng_state = match_seed ? match_seed : 1u;
        plexus_speedcoin_setup_wave(&g, 1);
        motion_deal_round(&g, 0.45f);
        cinematic_start_countdown(1);
    } else if (plexus_tutorial_is_active()) {
        plexus_tutorial_setup_round(&g);
        motion_deal_round(&g, 0.45f);
        cinematic_start(g.current_round);
    } else if (!plexus_training_is_active() && !plexus_action_card_is_active()) {
#ifndef __EMSCRIPTEN__
        /* From here to the match end every decision of both players is written down,
           so the finished match can be replayed (see docs/reference/modes.md). */
        replay_rec_begin(match_seed, ai_rng, settings.ruleset, settings.rounds, settings.difficulty,
                         settings.master_timer, settings.current_opponent);
#endif
    }
    ai_set_opponent(settings.current_opponent); taunt_init();
    if (!plexus_tutorial_is_active() && !plexus_training_is_active() && !plexus_speedcoin_is_active() && !plexus_action_card_is_active())
        taunt_trigger(campaign_get_opponent(settings.current_opponent)->taunt_text, 4.5f);
    if (trk > 0) audio_start_match_music(&audio, trk);
    else audio_set_reverb(0.0f);
}

static float get_flight_duration(int s) { return (s == 0) ? 0.65f : (s == 1) ? 0.50f : 0.40f; }
static float get_think_duration(int s)  { return (s == 0) ? 0.40f : (s == 1) ? 0.25f : 0.15f; }

static void trigger_event_beat(float base_s) {
    float mul = 0.0f;
    if (plexus_speedcoin_is_active()) mul *= 0.55f;
    float d = base_s * mul;
    if (d > event_beat_timer) event_beat_timer = d;
}

/* Minimal CPU inter-animation beat (~90-120 ms, skipped on captures where
   g.hitlag_timer = 150 ms already runs) so CPU steps never collapse into a
   single frame while keeping Action Card reverb_energy decay negligible. */
static void trigger_cpu_step_beat(float sec) {
    if (!plexus_speedcoin_is_active() && sec > event_beat_timer) event_beat_timer = sec;
}

/* --- replay pacing -------------------------------------------------------
   A live turn is paced by the player thinking and the CPU thinking; a replay
   has neither, so it needs its own beat or the cards run together. These three
   numbers are the whole speed of a replay - raise them to slow it down. */
#define REPLAY_FLIGHT_SCALE 1.40f   /* replays fly slower than a live turn */
#define REPLAY_BEAT         0.45f   /* pause once a card has landed */
static float replay_flight_duration(void) {
    return get_flight_duration(settings.anim_speed) * REPLAY_FLIGHT_SCALE;
}

/* --- replay flight geometry ---------------------------------------------
   A replay has no drag and no CPU think step, so the take-off and landing
   points a live match gets for free have to be worked out from the board
   layout: the slot the card sits in now, and the slot the engine will put it
   in once the step is applied. board_get_field_card_rect_of() already answers
   the second question for a card that is not on the field yet. */

/* Slot of a card still in its owner's hand. Returns x = -1 when the card is
   not in that hand, which means the record has drifted: no flight is shown
   and the engine call below reports the divergence as usual. */
static Vector2 replay_hand_card_pos(const GameState *gs, int player, int card) {
    for (int i = 0; i < gs->hand_count[player]; i++) {
        if (gs->hand[player][i] != card) continue;
        if (player == 0) {
            Rectangle r = board_get_hand_card_rect(i, gs->hand_count[0], 0);
            return (Vector2){ r.x, r.y };
        }
        float step = 62.0f;                                     /* same layout as board_draw_table */
        float w = (gs->hand_count[1] - 1) * step + CPU_CARD_W;
        return (Vector2){ 710.0f - w * 0.5f + i * step, 12.0f };
    }
    return (Vector2){ -1.0f, -1.0f };
}

/* Where a played card comes to rest: on the field card it takes, or in the
   slot the field will give it. */
static Vector2 replay_play_landing(const GameState *gs, int card, int target) {
    if (target >= 0) {
        Vector2 p = board_get_card_field_pos(gs, target);
        if (p.x > 0.0f || p.y > 0.0f) return p;
    }
    Rectangle r = board_get_field_card_rect_of(gs, card);
    if (r.width > 0.0f) return (Vector2){ r.x, r.y };
    return (Vector2){ 710.0f, 240.0f };
}

/* Where a revealed stock card comes to rest: on the field card it takes, or in
   the slot the field will give it. */
static Vector2 replay_draw_landing(const GameState *gs, int card) {
    int match_target = -1;
    for (int i = 0; i < gs->field_count; i++) {
        if (CARDS[gs->field[i]].month == CARDS[card].month) { match_target = gs->field[i]; break; }
    }
    Rectangle land = board_get_field_card_rect_of(gs, (match_target >= 0) ? match_target : card);
    if (land.width > 0.0f) return (Vector2){ land.x, land.y };
    return (Vector2){ 710.0f, 240.0f };
}

static void draw_hover_action_label(const char *label, Rectangle bounds) {
    Vector2 ts = MeasureTextEx(font_title, label, 18.0f, 1.2f);
    DrawTextEx(font_title, label, (Vector2){ floorf(bounds.x + (bounds.width - ts.x) / 2.0f), floorf(bounds.y + (bounds.height - 18.0f) / 2.0f) }, 18.0f, 1.2f, COLOR_PHOSPHOR);
}

static void draw_loading_card(const BoardAssets *ba, float u) {
    float vis = smooth01(u / 0.20f) * smooth01((1.0f - u) / 0.20f);
    if (vis <= 0.005f) return;
    float wv = sinf(fminf(1.0f, fmaxf(0.0f, u)) * 3.14159265f);
    float glow = fminf(1.0f, powf(wv, 1.8f) * 1.15f) * vis;
    Rectangle cr = { 608.0f, 308.0f, 64.0f, 104.0f };
    Rectangle src = { 0, 0, (float)ba->card_back.width, (float)ba->card_back.height };
    Color p = COLOR_PHOSPHOR, zero = { p.r, p.g, p.b, 0 };
    if (glow > 0.005f) {
        static const float radii[4]  = { 128.0f, 92.0f, 66.0f, 46.0f };
        static const float alphas[4] = {   4.0f,  6.0f,  8.0f, 10.0f };
        BeginBlendMode(BLEND_ADDITIVE);
        for (int i = 0; i < 4; i++)
            DrawCircleGradient(640, 360, radii[i], (Color){ p.r, p.g, p.b, (unsigned char)(alphas[i] * glow) }, zero);
        EndBlendMode();
    }
    if (ba->card_back.id > 0)
        DrawTexturePro(ba->card_back, src, cr, (Vector2){ 0, 0 }, 0.0f, (Color){ 255, 255, 255, (unsigned char)((200.0f + 55.0f * glow) * vis) });
    else DrawRectangleRec(cr, (Color){ 10, 14, 18, (unsigned char)(255.0f * vis) });
    Color vc = { 0, 0, 0, (unsigned char)(255.0f * vis * (0.94f - 0.16f * glow)) }, vz = { 0, 0, 0, 0 };
    DrawRectangleGradientV((int)cr.x, (int)cr.y, (int)cr.width, 15, vc, vz);
    DrawRectangleGradientV((int)cr.x, (int)(cr.y + cr.height - 15), (int)cr.width, 15, vz, vc);
    DrawRectangleGradientH((int)cr.x, (int)cr.y, 13, (int)cr.height, vc, vz);
    DrawRectangleGradientH((int)(cr.x + cr.width - 6), (int)cr.y, 6, (int)cr.height, vz, vc);
    BeginBlendMode(BLEND_ADDITIVE);
    DrawCircleGradient(644, 362, 32.0f, (Color){ p.r, p.g, p.b, (unsigned char)((8.0f + 16.0f * glow) * vis) }, zero);
    if (ba->card_back.id > 0 && glow > 0.005f) {
        Rectangle lsrc = { src.width * 0.82f, src.height * 0.59f, src.width * 0.09f, src.height * 0.36f };
        Rectangle ldst = { cr.x + cr.width * 0.82f, cr.y + cr.height * 0.59f, cr.width * 0.09f, cr.height * 0.36f };
        Color boost = { p.r, p.g, p.b, (unsigned char)(200.0f * glow) };
        Color hot   = { 255, 255, 255, (unsigned char)(115.0f * glow * glow) };
        Color bleed = { p.r, p.g, p.b, (unsigned char)(55.0f * glow * glow) };
        DrawCircleGradient(663, 388, 24.0f, (Color){ p.r, p.g, p.b, (unsigned char)(34.0f * glow) }, zero);
        DrawCircleGradient(663, 388, 12.0f, (Color){ p.r, p.g, p.b, (unsigned char)(30.0f * glow * glow) }, zero);
        DrawTexturePro(ba->card_back, lsrc, ldst, (Vector2){ 0, 0 }, 0.0f, boost);
        DrawTexturePro(ba->card_back, lsrc, ldst, (Vector2){ 0, 0 }, 0.0f, hot);
        DrawTexturePro(ba->card_back, lsrc, (Rectangle){ ldst.x - 0.8f, ldst.y, ldst.width, ldst.height }, (Vector2){ 0, 0 }, 0.0f, bleed);
        DrawTexturePro(ba->card_back, lsrc, (Rectangle){ ldst.x + 0.8f, ldst.y, ldst.width, ldst.height }, (Vector2){ 0, 0 }, 0.0f, bleed);
        DrawTexturePro(ba->card_back, lsrc, (Rectangle){ ldst.x, ldst.y - 0.8f, ldst.width, ldst.height }, (Vector2){ 0, 0 }, 0.0f, bleed);
        DrawTexturePro(ba->card_back, lsrc, (Rectangle){ ldst.x, ldst.y + 0.8f, ldst.width, ldst.height }, (Vector2){ 0, 0 }, 0.0f, bleed);
    }
    EndBlendMode();
}

static void draw_round_end_banner(const GameState *gs, float timer) {
    float alpha = smooth01((ROUND_BANNER_DUR - timer) / 0.35f) * smooth01(timer / 0.45f);
    if (alpha <= 0.005f) return;
    int match_over = (gs->current_round >= gs->total_rounds), p0 = gs->total_score[0], p1 = gs->total_score[1];
    int w = match_over ? ((p0 > p1) ? 0 : (p0 < p1) ? 1 : -1) : gs->current_round_record.winner;
    const char *title = match_over ? ((w == 0) ? "MATCH WINNER: YOU" : (w == 1) ? "MATCH WINNER: CPU" : "MATCH DRAW")
                                   : ((w == 0) ? "ROUND WINNER: YOU" : (w == 1) ? "ROUND WINNER: CPU" : "ROUND DRAW");
    Color tcol = (w == 0) ? COLOR_PHOSPHOR : (w == 1) ? COLOR_RED : COLOR_PHOSPHOR_DIM;
    char sub[80], gold[64] = "", tline[128] = "";
    if (match_over || w < 0) snprintf(sub, sizeof(sub), "%sSCORE:  YOU %d  -  CPU %d", match_over ? "FINAL " : "", p0, p1);
    else snprintf(sub, sizeof(sub), "+%d PTS   //   SCORE: YOU %d - CPU %d", gs->current_round_record.award, p0, p1);
    int show_gold = (gs->ruleset == RULESET_KK_ADD_10) && (speed_bar_total() > 0);
    if (show_gold) {
        int mp = speed_bar_total();
        if (match_over) snprintf(gold, sizeof(gold), "GOLD MINIPOINTS:  %d  =  +%d PTS", mp, mp / 10);
        else snprintf(gold, sizeof(gold), "GOLD MINIPOINTS:  %d", mp);
    }
    int yt = 0, yp = 0, ct = 0, cp = 0, mt = 0;
    int show_time = plexus_action_card_is_active() &&
                    (match_over ? action_card_get_match_time_stats(&yt, &yp, &ct, &cp, &mt)
                                : action_card_get_round_time_stats(&yt, &yp, &ct, &cp, &mt));
    if (show_time) action_card_format_time_line(tline, sizeof(tline), yt, yp, ct, cp, mt);
    Vector2 ts = MeasureTextEx(font_title, title, 34.0f, 1.5f), ss = MeasureTextEx(font_body, sub, 20.0f, 1.2f);
    Vector2 gs_sz = show_gold ? MeasureTextEx(font_body, gold, 20.0f, 1.2f) : (Vector2){ 0, 0 };
    Vector2 tm_sz = show_time ? MeasureTextEx(font_body, tline, 19.0f, 1.2f) : (Vector2){ 0, 0 };
    float max_w = fmaxf(ts.x, fmaxf(ss.x, fmaxf(gs_sz.x, tm_sz.x)));
    float bw = fmaxf(480.0f, max_w + 88.0f);
    float bh = 104.0f + (show_gold ? 32.0f : 0.0f) + (show_time ? 30.0f : 0.0f);
    Rectangle br = { floorf((CANVAS_W - bw) * 0.5f), floorf(350.0f - bh * 0.5f), ceilf(bw), bh };
    DrawRectangleRec(br, (Color){ 0, 0, 0, (unsigned char)(232.0f * alpha) });
    DrawRectangleLinesEx(br, 2.0f, (Color){ tcol.r, tcol.g, tcol.b, (unsigned char)(255.0f * alpha) });
    ui_theme_set_panel_flare_glow(br, 0.75f * alpha);
    ui_theme_draw_panel_flares(br);
    DrawTextEx(font_title, title, (Vector2){ floorf((CANVAS_W - ts.x) * 0.5f), br.y + 18.0f }, 34.0f, 1.5f,
               (Color){ tcol.r, tcol.g, tcol.b, (unsigned char)(255.0f * alpha) });
    Color info = COLOR_TEXT_INFO; info.a = (unsigned char)(240.0f * alpha);
    DrawTextEx(font_body, sub, (Vector2){ floorf((CANVAS_W - ss.x) * 0.5f), br.y + 64.0f }, 20.0f, 1.2f, info);
    float row_y = br.y + 96.0f;
    if (show_gold) {
        Color gc = COLOR_AMBER; gc.a = (unsigned char)(255.0f * alpha);
        DrawTextEx(font_body, gold, (Vector2){ floorf((CANVAS_W - gs_sz.x) * 0.5f), row_y }, 20.0f, 1.2f, gc);
        row_y += 30.0f;
    }
    if (show_time) {
        Color tc = { 236, 150, 160, (unsigned char)(255.0f * alpha) };
        DrawTextEx(font_body, tline, (Vector2){ floorf((CANVAS_W - tm_sz.x) * 0.5f), row_y }, 19.0f, 1.2f, tc);
    }
}

static void draw_round_result_extras(const GameState *gs, Vector2 vm) {
    if (!gs) return;
    int match_over = (gs->current_round >= gs->total_rounds);
    if (plexus_tutorial_is_active() && !plexus_speedcoin_is_active() &&
        (!plexus_action_card_is_active() || match_over)) {
        Rectangle btn = { 520.0f, 610.0f, 240.0f, 48.0f };
        int hov = CheckCollisionPointRec(vm, btn);
        DrawRectangleRec(btn, (Color){ 0, 0, 0, 252 });
        if (hov) DrawRectangleRec(btn, COLOR_BTN_HOVER_BG);
        ui_draw_panel_border(btn, hov ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
        Vector2 ts = MeasureTextEx(font_title, "ATRIUM PLEX", 20.0f, 1.2f);
        Vector2 pos = { floorf(btn.x + (btn.width - ts.x) * 0.5f), floorf(btn.y + (btn.height - 20.0f) * 0.5f) };
        DrawTextEx(font_title, "ATRIUM PLEX", pos, 20.0f, 1.2f, COLOR_PHOSPHOR);
        if (hov) ui_theme_draw_text_flares(font_title, "ATRIUM PLEX", pos, 20.0f, 1.2f, COLOR_PHOSPHOR);
    }
    int mp = speed_bar_total();
    int show_sc = (gs->ruleset == RULESET_KK_ADD_10) && (mp > 0);
    int yt = 0, yp = 0, ct = 0, cp = 0, mt = 0;
    int show_tm = plexus_action_card_is_active() &&
                  (match_over ? action_card_get_match_time_stats(&yt, &yp, &ct, &cp, &mt)
                              : action_card_get_round_time_stats(&yt, &yp, &ct, &cp, &mt));
    if (!show_sc && !show_tm) return;
    float y = (show_sc && show_tm) ? 511.0f : 537.0f;
    if (show_sc) {
        const char *sc_txt = match_over ? TextFormat("SPEED MINIPOINTS:  %d  =  +%d PTS", mp, mp / 10)
                                        : TextFormat("SPEED MINIPOINTS:  %d", mp);
        DrawTextEx(font_body, sc_txt, (Vector2){ 230.0f, y }, 19.0f, 1.2f, COLOR_AMBER);
        y += 26.0f;
    }
    if (show_tm) {
        char tline[128];
        action_card_format_time_line(tline, sizeof(tline), yt, yp, ct, cp, mt);
        DrawTextEx(font_body, tline, (Vector2){ 230.0f, y }, 19.0f, 1.2f, (Color){ 236, 150, 160, 255 });
    }
}

static void draw_active_game_overlays(const BoardAssets *ba, const GameState *gs, Vector2 vm, int paused, int c_restart, int replay) {
    /* Only cards in the foreground overlay may be inspected. */
    card_tooltip_clear();
    if (combo_player >= 0) {
        combos_draw_details(ba, gs, combo_player, combo_page);
        return;
    }
    if (speedcoin_gameover_timer > 0.0f || (screen == SCREEN_GAME && plexus_speedcoin_result_active())) {
        if (paused) {
            card_tooltip_clear();
            screens_draw_pause(vm, c_restart, replay);
        }
        return;
    }
    int fg_ready = !action_card_is_event_freezing() && event_beat_timer <= 0.0f &&
                   !motion_is_reflowing(gs) && !anim_is_blocking(&anim);
    if (gs->phase == PHASE_DEALER_SELECT) board_draw_dealer_select(ba, gs, vm);
    else if ((gs->phase == PHASE_HAND_CHOICE && !action_card_is_event_freezing()) ||
             (gs->phase == PHASE_DRAW_CHOICE && fg_ready))
        screens_draw_choice_overlay(ba, gs, vm, gs->phase == PHASE_HAND_CHOICE);
    else if (gs->phase == PHASE_DECIDE && gs->current_player == 0 && fg_ready)
        screens_draw_decide_overlay(ba, gs, vm);
    else if ((gs->phase == PHASE_ROUND_END && prev_round_phase == PHASE_ROUND_END && round_banner_timer <= 0.0f) ||
             gs->phase == PHASE_MATCH_END) {
        screens_draw_round_result(ba, gs, vm);
        draw_round_result_extras(gs, vm);
    }
    if (paused) {
        card_tooltip_clear();
        screens_draw_pause(vm, c_restart, replay);
    }
}

static void handle_campaign_overlay_click(Vector2 vm) {
    int cres = campaign_handle_click(&settings, &settings.current_opponent, vm);
    if (cres == -1) {
        audio_play_click(&audio);
        if (CheckCollisionPointRec(vm, (Rectangle){ 1145, 48, 32, 28 }))
            start_hub_transition(screen, 0, (Rectangle){ 1145, 48, 32, 28 }, "X", 18);
        else
            start_hub_transition(screen, 0, (Rectangle){ 720, 570, 180, 48 }, "BACK", 20);
    } else if (cres == 1) {
        plexus_tutorial_abort();
        plexus_training_stop();
        plexus_speedcoin_abort();
        plexus_action_card_abort();
        match_origin = screen;
        audio_play_click(&audio);
        ai_set_opponent(settings.current_opponent);
        storage_save_settings(&settings);
        start_transition(SCREEN_GAME, 1, pick_match_track(1));
    } else if (cres == 2 || cres == 3) {
        audio_play_click(&audio);
    }
}

static void update_draw_frame(void) {
    frame_count++;
    float dt = fminf(0.05f, GetFrameTime());

    if (transition_dir == 1) {
        transition_timer += dt;
        transition_alpha = smooth01(transition_timer / TRANS_OUT_DUR);
        if (transition_timer >= TRANS_OUT_DUR) {
            transition_alpha = 1.0f; transition_dir = 2; transition_timer = 0.0f;
        }
    } else if (transition_dir == 2) {
        transition_alpha = 1.0f;
        transition_timer += dt;
        if (transition_timer >= TRANS_HOLD_DUR) {
            transition_dir = -1; transition_timer = 0.0f; screen = transition_target_screen;
            if (screen != SCREEN_GAME && speed_mode_is_active()) speed_mode_stop();
            if (screen != SCREEN_MATCH_RESULT) {
                plexus_speedcoin_close_result();
                plexus_training_close_result();
                plexus_action_card_close_result();
            }
            if (screen != SCREEN_GAME && screen != SCREEN_MATCH_RESULT) {
                action_card_reset_match();
                plexus_restore_match_settings(&settings, &assets);
            }
            campaign_overlay_open = 0; combo_player = -1; combo_page = 0;
            round_banner_timer = 0.0f; speedcoin_gameover_timer = 0.0f; event_beat_timer = 0.0f; prev_round_phase = PHASE_NONE;
            if (transition_action == 1) {
                begin_new_match(transition_track);
            } else if (transition_action == 2 || transition_action == 3) {
                is_paused = 0; confirm_restart = 0;
                audio_start_match_music(&audio, pick_match_track(1));
                if (pending_result_sfx > 0) audio_play_win(&audio);
                else if (pending_result_sfx < 0) audio_play_lose(&audio);
                pending_result_sfx = 0;
            }
        }
    } else if (transition_dir < 0) {
        transition_timer += dt;
        transition_alpha = 1.0f - smooth01(transition_timer / TRANS_IN_DUR);
        if (transition_timer >= TRANS_IN_DUR) { transition_alpha = 0.0f; transition_dir = 0; transition_timer = 0.0f; }
    }

    float b_val = (settings.bloom == 2) ? 1.0f : (settings.bloom == 1) ? 0.5f : 0.0f;
    float c_val = (settings.crt_filter == 3) ? 1.0f : (settings.crt_filter == 2) ? 0.65f : (settings.crt_filter == 1) ? 0.35f : 0.0f;
    float g_val = (settings.gamma > 0.1f) ? settings.gamma : 1.0f;
    if (post_shader.id > 0) {
        if (b_val != cached_bloom) { SetShaderValue(post_shader, loc_bloom, &b_val, SHADER_UNIFORM_FLOAT); cached_bloom = b_val; }
        if (c_val != cached_crt)   { SetShaderValue(post_shader, loc_crt,   &c_val, SHADER_UNIFORM_FLOAT); cached_crt = c_val; }
        if (g_val != cached_gamma) { SetShaderValue(post_shader, loc_gamma, &g_val, SHADER_UNIFORM_FLOAT); cached_gamma = g_val; }
    }

    audio_set_bgm_volume(&audio, settings.bgm_volume * (1.0f - transition_alpha));
    audio_update(&audio);
    int in_game = (screen == SCREEN_GAME);
    int audio_test = screen == SCREEN_OPTIONS ? screens_audio_test_state() : -1;
    if (audio_test >= 0) {
        audio_preview_effect(&audio, audio_test);
    } else {
        if (!in_game) audio_set_reverb(0.0f);
        if (audio.current_music != 2) audio_start_match_music(&audio, (in_game && transition_track > 0) ? transition_track : pick_match_track(0));
        else if (audio_is_match_music_finished(&audio)) audio_start_match_music(&audio, pick_match_track(1));
    }
    int ac_table = in_game && plexus_action_card_is_active();
    int ac_paused = is_paused || plexus_tutorial_panel_open();
    action_card_sync_music(&audio, ac_table, ac_paused, transition_alpha, dt);

    if (saved_timer > 0.0f) saved_timer -= dt;
    if (log_timer > 0.0f) log_timer -= dt;
    if (deus_banner_timer > 0.0f) deus_banner_timer -= dt;

    int cur_w = GetScreenWidth(), cur_h = GetScreenHeight();
    if (cur_w < 960 || cur_h < 540) {
        card_tooltip_begin(0, (Vector2){ 0, 0 });
        capture_info_begin(0, (Vector2){ 0, 0 });
        card_tooltip_draw();
        BeginDrawing(); ClearBackground(BLACK);
        DrawTextEx(font_body, "Enlarge the window to play (minimum 960x540)", (Vector2){ 20.0f, 20.0f }, 20.0f, 1.5f, COLOR_PHOSPHOR);
        EndDrawing(); return;
    }

    Letterbox l = input_get_letterbox();
    Vector2 vm = input_vmouse(l);
    ui_theme_set_mouse(vm);

    if (hub_trans_active) {
        hub_trans_timer += dt;
        if (hub_trans_timer >= (HUB_BLINK_DUR + HUB_WIPE_DUR)) {
            hub_trans_active = 0;
            hub_trans_timer = 0.0f;
            if (hub_reset_opt_page) { screens_set_options_page(0); hub_reset_opt_page = 0; }
            if (screen != SCREEN_GAME && speed_mode_is_active()) speed_mode_stop();
            if (screen != SCREEN_MATCH_RESULT) {
                plexus_speedcoin_close_result();
                plexus_training_close_result();
                plexus_action_card_close_result();
            }
            if (screen != SCREEN_GAME && screen != SCREEN_MATCH_RESULT) {
                action_card_reset_match();
            }
        }
    }

    if (transition_dir == 0 && !hub_trans_active) {
        if (screen == SCREEN_START) {
            if (campaign_overlay_open) {
                handle_campaign_overlay_click(vm);
            } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                HubButtonHit hit = { { 0, 0, 0, 0 }, "", 18 };
                HubAction hact = ui_hub_handle_title_click(vm, &hit);
                if (hact != HUB_ACT_NONE) audio_play_click(&audio);
                if (hact == HUB_ACT_TITLE_START_GAME) {
                    start_hub_transition(SCREEN_HUB_MAIN, 0, hit.rect, hit.label, hit.font_size);
                } else if (hact == HUB_ACT_TITLE_OPTIONS) {
                    options_return_screen = SCREEN_START;
                    start_hub_transition(SCREEN_OPTIONS, 0, hit.rect, hit.label, hit.font_size);
                } else if (hact == HUB_ACT_TITLE_SAVE) {
                    storage_save_all(&settings, &stats, &history);
                    plexus_save();
                    saved_timer = 2.0f;
                } else if (hact == HUB_ACT_TITLE_CREDITS) {
                    start_hub_transition(SCREEN_CREDITS, 0, hit.rect, hit.label, hit.font_size);
                } else if (hact == HUB_ACT_TITLE_QUIT) {
                    plexus_save();
                    quit_requested = 1;
                    return;
                }
            }
        } else if (screen == SCREEN_HUB_MAIN) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                HubButtonHit hit = { { 0, 0, 0, 0 }, "", 20 };
                HubAction hact = ui_hub_handle_main_click(vm, &hit);
                if (hact != HUB_ACT_NONE) audio_play_click(&audio);
                if (hact == HUB_ACT_GOTO_CAMPAIGN_HUB) {
                    start_hub_transition(SCREEN_HUB_CAMPAIGN, 0, hit.rect, hit.label, hit.font_size);
                } else if (hact == HUB_ACT_GOTO_ATRIUM) {
                    atrium_return_screen = SCREEN_HUB_MAIN;
                    start_transition(SCREEN_ATRIUM, 3, 0);
                } else if (hact == HUB_ACT_BACK_TO_TITLE) {
                    start_hub_transition(SCREEN_START, 0, hit.rect, hit.label, hit.font_size);
                }
            }
        } else if (screen == SCREEN_HUB_CAMPAIGN) {
            if (campaign_overlay_open) {
                handle_campaign_overlay_click(vm);
            } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                HubButtonHit hit = { { 0, 0, 0, 0 }, "", 20 };
                HubAction hact = ui_hub_handle_campaign_click(vm, &hit);
                if (hact != HUB_ACT_NONE) audio_play_click(&audio);
                if (hact == HUB_ACT_OPEN_CAMPAIGN_OVERLAY) {
                    start_hub_transition(SCREEN_HUB_CAMPAIGN, 1, hit.rect, hit.label, hit.font_size);
                } else if (hact == HUB_ACT_GOTO_STATS) {
                    stats_return_screen = SCREEN_HUB_CAMPAIGN;
                    start_hub_transition(SCREEN_STATS, 0, hit.rect, hit.label, hit.font_size);
                } else if (hact == HUB_ACT_BACK_TO_MAIN_HUB) {
                    start_hub_transition(SCREEN_HUB_MAIN, 0, hit.rect, hit.label, hit.font_size);
                }
            }
        } else if (screen == SCREEN_ATRIUM) {
            if (plexus_intro_blocking()) {
                if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                    if (plexus_intro_accept_key()) audio_play_click(&audio);
                } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    int p_clicked = 0;
                    plexus_handle_click(vm, &p_clicked);
                    if (p_clicked) audio_play_click(&audio);
                }
            } else if (IsKeyPressed(KEY_ESCAPE)) {
                audio_play_click(&audio);
                if (!plexus_close_subpanel()) {
                    plexus_save();
                    storage_save_all(&settings, &stats, &history);
                    start_transition(atrium_return_screen, 3, 0);
                }
            } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                int p_clicked = 0;
                ScreenAction act = plexus_handle_click(vm, &p_clicked);
                if (p_clicked) audio_play_click(&audio);
                if (act == ACT_GOTO_MENU) {
                    plexus_save();
                    storage_save_all(&settings, &stats, &history);
                    start_transition(atrium_return_screen, 3, 0);
                } else if (act == ACT_GOTO_OPTIONS) {
                    options_return_screen = SCREEN_ATRIUM;
                    start_hub_transition(SCREEN_OPTIONS, 0, plexus_get_option_btn_rect(), "OPTION", 16);
                } else if (act == ACT_START_GAME) {
                    start_transition(SCREEN_GAME, 1, pick_match_track(1));
                }
            }
        } else if (screen == SCREEN_CREDITS && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (screens_handle_credits_click(vm) == ACT_GOTO_MENU) {
                audio_play_click(&audio);
                start_hub_transition(SCREEN_START, 0, (Rectangle){ 520, 530, 240, 46 }, "BACK", 22);
            }
        } else if (screen == SCREEN_OPTIONS) {
            int prev_opt_page = screens_get_options_page();
            if (screens_handle_options_click(&settings, &assets, &audio, vm) == ACT_GOTO_MENU) {
                screens_set_options_page(prev_opt_page);
                hub_reset_opt_page = 1;
                start_hub_transition(options_return_screen, 0, (Rectangle){ 300, 656, 260, 38 }, "BACK (SAVE)", 20);
            }
        } else if (screen == SCREEN_STATS) {
#ifndef __EMSCRIPTEN__
            /* replays/ only exists next to a desktop executable, so the browser build
               gets no replay controls (see docs/reference/modes.md). */
            ui_replay_keys(&history);
#endif
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                int replay_used = 0;
                ScreenAction act = ACT_NONE;
#ifndef __EMSCRIPTEN__
                if (!confirm_reset && ui_replay_click(&history, vm)) { audio_play_click(&audio); replay_used = 1; }
#endif
                if (!replay_used) {
                    act = screens_handle_stats_click(vm, &confirm_reset);
                    if (act != ACT_NONE) audio_play_click(&audio);
                    if (act == ACT_GOTO_MENU) start_hub_transition(stats_return_screen, 0, (Rectangle){ 860, 610, 200, 44 }, "BACK", 20);
                    else if (act == ACT_SAVE_LOG) { storage_write_report(&stats, &history); log_timer = 2.0f; }
                    else if (act == ACT_RESET_STATS) { storage_reset_stats(&stats); memset(&history, 0, sizeof(history)); }
                }
            }
#ifndef __EMSCRIPTEN__
            if (ui_replay_take_start(&g)) {
                anim_init(&anim); motion_init(); memset(&drag, 0, sizeof(drag)); hovered_idx = -1;
                taunt_init(); deus_banner_timer = 0.0f;
                round_banner_timer = 0.0f; prev_round_phase = PHASE_NONE;
                is_paused = 0; confirm_restart = 0;
                motion_deal_round(&g, 0.45f);           /* the deal is shown, not snapped */
                cinematic_start(g.current_round);
                last_replay_round = g.current_round;
                replay_beat = 0.0f;
                screen = SCREEN_GAME;
            }
#endif
        } else if (screen == SCREEN_MATCH_RESULT && plexus_speedcoin_result_active()) {
            if (IsKeyPressed(KEY_ESCAPE)) {
                audio_play_click(&audio);
                start_hub_transition(SCREEN_ATRIUM, 0, (Rectangle){ 660.0f, 628.0f, 260.0f, 50.0f }, "ATRIUM PLEX", 18);
            } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                int sc_clicked = 0;
                ScreenAction act = plexus_handle_speedcoin_result_click(vm, &sc_clicked);
                if (sc_clicked) audio_play_click(&audio);
                if (act == ACT_START_GAME) start_transition(SCREEN_GAME, 1, pick_match_track(1));
                else if (act == ACT_GOTO_ATRIUM) {
                    Rectangle sc_btn = CheckCollisionPointRec(vm, (Rectangle){ 360.0f, 628.0f, 260.0f, 50.0f })
                                       ? (Rectangle){ 360.0f, 628.0f, 260.0f, 50.0f }
                                       : (Rectangle){ 660.0f, 628.0f, 260.0f, 50.0f };
                    const char *sc_lbl = (sc_btn.x < 500.0f) ? "PLAY AGAIN" : "ATRIUM PLEX";
                    start_hub_transition(SCREEN_ATRIUM, 0, sc_btn, sc_lbl, 18);
                }
            }
        } else if (screen == SCREEN_MATCH_RESULT && plexus_training_result_active()) {
            if (IsKeyPressed(KEY_ESCAPE)) {
                audio_play_click(&audio);
                start_hub_transition(SCREEN_ATRIUM, 0, (Rectangle){ 510.0f, 632.0f, 260.0f, 50.0f }, "ATRIUM PLEX", 18);
            } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                int tr_clicked = 0;
                if (plexus_handle_training_result_click(vm, &tr_clicked) == ACT_GOTO_ATRIUM)
                    start_hub_transition(SCREEN_ATRIUM, 0, (Rectangle){ 510.0f, 632.0f, 260.0f, 50.0f }, "ATRIUM PLEX", 18);
                if (tr_clicked) audio_play_click(&audio);
            }
        } else if (screen == SCREEN_MATCH_RESULT && plexus_action_card_result_active()) {
            if (IsKeyPressed(KEY_ESCAPE)) {
                audio_play_click(&audio);
                start_hub_transition(SCREEN_ATRIUM, 0, (Rectangle){ 660.0f, 632.0f, 260.0f, 50.0f }, "ATRIUM PLEX", 18);
            } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                int ac_clicked = 0;
                ScreenAction act = plexus_handle_action_card_result_click(vm, &ac_clicked);
                if (ac_clicked) audio_play_click(&audio);
                if (act == ACT_START_GAME) start_transition(SCREEN_GAME, 1, pick_match_track(1));
                else if (act == ACT_GOTO_ATRIUM) {
                    Rectangle ac_btn = CheckCollisionPointRec(vm, (Rectangle){ 360.0f, 632.0f, 260.0f, 50.0f })
                                       ? (Rectangle){ 360.0f, 632.0f, 260.0f, 50.0f }
                                       : (Rectangle){ 660.0f, 632.0f, 260.0f, 50.0f };
                    const char *ac_lbl = (ac_btn.x < 500.0f) ? "PLAY AGAIN" : "ATRIUM PLEX";
                    start_hub_transition(SCREEN_ATRIUM, 0, ac_btn, ac_lbl, 18);
                }
            }
        } else if (screen == SCREEN_MATCH_RESULT && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            ScreenAction act = screens_handle_match_result_click(vm);
            if (act != ACT_NONE) audio_play_click(&audio);
            if (act == ACT_START_GAME) start_transition(SCREEN_GAME, 1, pick_match_track(1));
            else if (act == ACT_GOTO_MENU) {
                if (plexus_action_card_is_active()) {
                    plexus_action_card_abort();
                    start_hub_transition(SCREEN_ATRIUM, 0, (Rectangle){ 490, 526, 300, 46 }, "MAIN MENU", 20);
                } else {
                    plexus_training_stop();
                    start_hub_transition(match_origin, 0, (Rectangle){ 490, 526, 300, 46 }, "MAIN MENU", 20);
                }
            }
        } else if (screen == SCREEN_GAME && combo_player >= 0) {
            /* Own the entire frame, including dismissal: no timer, CPU, replay,
               animation, or underlying button can advance through this window. */
            if (combos_handle_details(&g, combo_player, &combo_page, vm)) {
                combo_player = -1; is_paused = 0; audio_play_click(&audio);
            }
        } else if (screen == SCREEN_GAME && !is_paused && !drag.on && speedcoin_gameover_timer <= 0.0f &&
                   !plexus_tutorial_panel_open() &&
                   IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && combos_panel_hit(vm) >= 0 &&
                   g.phase != PHASE_DEALER_SELECT) {
            combo_player = combos_panel_hit(vm); combo_page = 0;
            is_paused = 1; confirm_restart = 0; audio_play_click(&audio);
        } else if (screen == SCREEN_GAME && replay_pb_active()) {
            /* A replay feeds the recorded decisions straight into the engine; there is no
               input and no clock, and nothing here may touch stats or unlocks. Pause and
               STOP work as they do live, the overlay swapping the match actions for
               RESUME / STOP REPLAY.
               Cards still travel the way they do live: replay_pb_want() names the next
               step, the flight is played, and only when it lands is the engine allowed
               to apply it (replay_pb_hold / replay_pb_release). */
            if (IsKeyPressed(KEY_ESCAPE)) { replay_pb_stop(); anim_init(&anim); is_paused = 0; start_transition(SCREEN_STATS, 2, 0); }
            else if (is_paused) {
                /* Paused: the board is frozen and the overlay owns the mouse. */
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    ScreenAction act = screens_handle_pause_click(vm, &confirm_restart, 1);
                    if (act == ACT_RESUME) { is_paused = 0; audio_play_click(&audio); }
                    else if (act == ACT_STOP_REPLAY) {
                        audio_play_click(&audio);
                        replay_pb_stop(); anim_init(&anim); is_paused = 0;
                        start_transition(SCREEN_STATS, 2, 0);
                    } else if (!CheckCollisionPointRec(vm, (Rectangle){ 440, 180, 400, 360 })) is_paused = 0;
                }
            }
            else if (IsKeyPressed(KEY_P) ||
                     (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(vm, (Rectangle){ 1160, 560, 112, 40 }))) {
                is_paused = 1; confirm_restart = 0; audio_play_click(&audio);
            }
            else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(vm, (Rectangle){ 1160, 610, 112, 40 })) {
                audio_play_click(&audio);
                replay_pb_stop(); anim_init(&anim);
                start_transition(SCREEN_STATS, 2, 0);
            }
            else {
                motion_update(dt, -1);
                cinematic_update(dt);
                anim_update(&anim, dt);
                if (replay_beat > 0.0f) replay_beat -= dt;

                /* 1. A flight that has just landed frees the step it was holding.
                      The step is applied in this very frame (replay_pb_release()
                      clears the pacing pause), so the card is never drawn back in
                      the hand it has just left - that read as the flight undoing
                      itself and the card teleporting to the table afterwards. */
                if (anim.active && (anim.type == ANIM_CPU_FLY || anim.type == ANIM_STOCK_FLY) &&
                    anim.elapsed >= anim.duration) {
                    int card = (anim.type == ANIM_CPU_FLY) ? anim.cpu_card : anim.card_id;
                    Vector2 landed = anim.target_pos;
                    anim.active = 0;
                    audio_play_drop(&audio);
                    motion_set_instant(card, landed, (Vector2){ FIELD_CARD_W, FIELD_CARD_H });
                    replay_pb_release();
                    replay_beat = REPLAY_BEAT;   /* a beat to read the board before the next card */
                }

                /* 2. Nothing in the air: take the next step. A round deal is covered
                      by the round-start cinematic, which outlasts the deal itself. */
                if (!anim.active && !cinematic_is_active()) {
                    int prev_caps = g.captured_count[0] + g.captured_count[1];
                    int pb_state = replay_pb_tick(&g, dt);
                    if (g.captured_count[0] + g.captured_count[1] > prev_caps) play_match_capture_sfx();
                    if (pb_state == -1) ui_replay_set_status(replay_pb_error());
                    if (pb_state != 0) { replay_pb_stop(); anim_init(&anim); start_transition(SCREEN_STATS, 2, 0); }
                }

                /* 3. The next step wants a flight: start it and hold the engine. The
                      table has to be settled first, so a card never leaves a hand
                      that is still being dealt or slides out of a row that is still
                      reflowing. */
                if (!anim.active && !cinematic_is_active() && !motion_is_reflowing(&g) && replay_beat <= 0.0f) {
                    int wp = -1, wc = -1, wt = -1;
                    PbWant want = replay_pb_want(&g, &wp, &wc, &wt);
                    if (want == PB_WANT_PLAY) {
                        Vector2 from = replay_hand_card_pos(&g, wp, wc);
                        if (from.x >= 0.0f) {
                            anim_start_cpu_fly(&anim, wc, wt, from, replay_play_landing(&g, wc, wt),
                                               replay_flight_duration());
                            replay_pb_hold();
                        }
                    } else if (want == PB_WANT_DRAW) {
                        audio_play_flip(&audio);
                        anim_start_stock_fly(&anim, wc, (Vector2){ 330.0f, 262.0f },
                                             replay_draw_landing(&g, wc),
                                             replay_flight_duration());
                        replay_pb_hold();
                    }
                }

                /* 4. A new round was dealt: show it the way a live match does. */
                if (g.current_round != last_replay_round) {
                    last_replay_round = g.current_round;
                    motion_deal_round(&g, 0.45f);
                    cinematic_start(g.current_round);
                }
            }
        } else if (screen == SCREEN_GAME) {
            int pause_toggled = 0, restarted_now = 0;
            if (((IsKeyPressed(KEY_ESCAPE) && (!drag.selected || is_paused)) || IsKeyPressed(KEY_P)) && !drag.on) { is_paused = !is_paused; confirm_restart = 0; pause_toggled = 1; }
            else if (!is_paused && IsKeyPressed(KEY_R) && !drag.on) { audio_play_click(&audio); begin_new_match(0); restarted_now = 1; }
            else if (!is_paused && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !drag.on) {
                if (CheckCollisionPointRec(vm, (Rectangle){ 1160, 560, 112, 40 })) { audio_play_click(&audio); is_paused = 1; confirm_restart = 0; pause_toggled = 1; }
                else if (CheckCollisionPointRec(vm, (Rectangle){ 1160, 610, 112, 40 })) { audio_play_click(&audio); begin_new_match(0); restarted_now = 1; }
            }

            if (is_paused && !pause_toggled) {
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    ScreenAction act = screens_handle_pause_click(vm, &confirm_restart, 0);
                    if (act != ACT_NONE) audio_play_click(&audio);
                    if (act == ACT_RESUME) { is_paused = 0; confirm_restart = 0; }
                    else if (act == ACT_RESTART_MATCH) begin_new_match(0);
                    else if (act == ACT_ABANDON_MATCH) {
                        replay_rec_abort(); confirm_restart = 0;
                        if (plexus_tutorial_is_active()) {
                            speedcoin_gameover_timer = 0.0f;
                            plexus_tutorial_abort();
                            start_transition(SCREEN_ATRIUM, 3, 0);
                        } else if (plexus_speedcoin_is_active()) {
                            speedcoin_gameover_timer = 0.0f;
                            plexus_speedcoin_abort();
                            plexus_save();
                            start_transition(SCREEN_ATRIUM, 3, 0);
                        } else if (plexus_action_card_is_active()) {
                            plexus_action_card_abort();
                            plexus_save();
                            start_transition(SCREEN_ATRIUM, 3, 0);
                        } else if (plexus_training_is_active()) {
                            plexus_training_stop();
                            plexus_save();
                            start_transition(SCREEN_ATRIUM, 3, 0);
                        } else {
                            plexus_save();
                            start_transition(match_origin, 3, 0);
                        }
                    }
                    else if (!CheckCollisionPointRec(vm, (Rectangle){ 440, 180, 400, 360 })) { is_paused = 0; confirm_restart = 0; }
                }
            } else if (!is_paused && !pause_toggled && !restarted_now) {
                speed_hit_popups_update(dt);
                int tut_settled = !anim_is_blocking(&anim) && !cinematic_is_active() && !motion_is_reflowing(&g);
                int tut_clicked = 0;
                int tut_frozen = plexus_tutorial_update_and_click(&g, &anim, tut_settled, overlay_alpha,
                                                                  vm, IsMouseButtonPressed(MOUSE_BUTTON_LEFT), &tut_clicked);
                if (tut_clicked) audio_play_click(&audio);

                if (tut_frozen) {
                    hovered_idx = -1;
                } else if (speedcoin_gameover_timer > 0.0f) {
                    hovered_idx = -1;
                    speedcoin_gameover_timer -= dt;
                    if (speedcoin_gameover_timer <= 0.0f) {
                        speedcoin_gameover_timer = 0.0f;
                        int run_waves = speed_mode_wave();
                        int run_coins = speed_bar_total();
                        int run_streak = speed_mode_run_best_streak();
                        int was_sc_tut = plexus_tutorial_is_active();
                        plexus_speedcoin_bank_wave(&g);
                        int bonus_pts = speed_bar_apply_match_bonus(&g);
                        plexus_speedcoin_finish_run(g.total_score[0], run_waves, run_coins, bonus_pts, run_streak);
                        if (was_sc_tut) {
                            pending_result_sfx = 1;
                            start_transition(SCREEN_ATRIUM, 2, 0);
                        } else {
                            pending_result_sfx = (g.total_score[0] > 0) ? 1 : -1;
                            start_transition(SCREEN_MATCH_RESULT, 2, 0);
                        }
                    }
                } else if (round_banner_timer > 0.0f) {
                    round_banner_timer -= dt;
                    if (round_banner_timer < 0.0f) round_banner_timer = 0.0f;
                } else if (g.hitlag_timer > 0.0f) {
                    g.hitlag_timer -= dt;
                    if (g.hitlag_timer < 0.0f) g.hitlag_timer = 0.0f;
                } else {
                    if (ac_table && action_card_is_event_freezing()) {
                        int apply_sym = -1;
                        if (action_card_update_badge_event(dt, &apply_sym) && apply_sym >= 0) {
                            action_card_apply_symbol(apply_sym, &g, &drag, &anim, &audio, &hovered_idx, &human_turn_started);
                        }
                    }

                    int badge_freezing = ac_table && action_card_is_event_freezing();
                    if (!(badge_freezing && anim.active && anim.type == ANIM_CPU_THINK)) {
                        anim_update(&anim, dt);
                    }
                    motion_update(dt, (drag.on && drag.idx >= 0 && drag.idx < g.hand_count[0]) ? g.hand[0][drag.idx] : -1);
                    if (!badge_freezing && event_beat_timer > 0.0f) {
                        event_beat_timer -= dt;
                        if (event_beat_timer < 0.0f) event_beat_timer = 0.0f;
                    }
                    float cd_prev = cinematic_countdown_elapsed();
                    cinematic_update(dt);
                    float cd_now = cinematic_countdown_elapsed();
                    if (cd_prev >= 0.0f && cd_now > cd_prev) {
                        for (int fi = 0; fi < g.hand_count[0]; fi++) {
                            float ft = 2.0f + (float)fi * 0.085f;
                            if (cd_prev < ft && cd_now >= ft) { audio_play_flip(&audio); break; }
                        }
                    }

                    if (!badge_freezing && ac_table && !replay_pb_active() &&
                        g.phase == PHASE_TURN_HAND && !g.has_pending &&
                        (!anim.active || anim.type == ANIM_CPU_THINK || anim.type == ANIM_BANNER) &&
                        !cinematic_is_active() && !motion_is_reflowing(&g) && event_beat_timer <= 0.0f) {
                        int sym = action_card_take_pending_symbol();
                        if (sym >= 0) {
                            if (drag.on && drag.moved && drag.idx >= 0 && drag.idx < g.hand_count[0] && !anim.active) {
                                Rectangle slot = board_get_hand_card_rect(drag.idx, g.hand_count[0], 0);
                                anim_start_glide(&anim, g.hand[0][drag.idx], drag.pos, (Vector2){ slot.x, slot.y }, 0.22f);
                            }
                            drag.on = 0; drag.idx = -1; drag.moved = 0; drag.selected = 0; hovered_idx = -1;
                            action_card_start_badge_event(sym, g.current_player, &audio);
                            badge_freezing = 1;
                        }
                    }

                    int board_busy = badge_freezing || (event_beat_timer > 0.0f) ||
                                     motion_is_reflowing(&g) || anim_is_blocking(&anim);
                    int input_unlocked = !board_busy && !cinematic_is_active();
                    Difficulty active_diff = plexus_tutorial_is_active() ? DIFFICULTY_EASY : settings.difficulty;

                    /* KK-ADD-10 speed minigame: the bottom-right timer bar */
                    if (!badge_freezing) speed_bar_update(&g, dt, input_unlocked);
                    if (plexus_speedcoin_is_active() && speed_mode_clock() <= 0.0f) {
                        drag.on = 0; drag.idx = -1; drag.selected = 0; human_turn_started = 0;
                        hovered_idx = -1; combo_player = -1; prev_round_phase = PHASE_NONE; event_beat_timer = 0.0f;
                        g.phase = PHASE_MATCH_END;
                        anim_init(&anim);
                        play_banner_sfx();
                        speedcoin_gameover_timer = 3.0f;
                    }

                    if (speedcoin_gameover_timer <= 0.0f && g.phase == PHASE_DEALER_SELECT) {
                        if (g.oya_player_choice < 0) {
                            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                                int pick = board_check_dealer_select_click(&g, vm);
                                if (pick >= 0) {
                                    g.oya_player_choice = pick; g.oya_timer = 0.0f;
                                    g.oya_winner = engine_oya_winner(g.oya_cards[pick], g.oya_cards[1 - pick]);
                                    g.dealer = g.oya_winner; audio_play_flip(&audio);
                                    replay_rec_oya(pick);
                                }
                            }
                        } else {
                            float prev_t = g.oya_timer; g.oya_timer += dt;
                            if (prev_t < 0.45f && g.oya_timer >= 0.45f) audio_play_flip(&audio);
                            if (prev_t < 0.90f && g.oya_timer >= 0.90f) audio_play_click(&audio);
                            if (g.oya_timer >= 3.4f) {
                                engine_start_round(&g); motion_deal_round(&g, 0.45f); cinematic_start(g.current_round);
                                if (audio.current_music != 2) audio_start_match_music(&audio, transition_track);
                            }
                        }
                    }

                    if (g.current_player == 1 && g.phase == PHASE_TURN_HAND && !cinematic_is_active()) {
                        if (anim.active && anim.type == ANIM_CPU_FLY && anim.elapsed >= anim.duration) {
                            int card = anim.cpu_card, target = anim.cpu_target;
                            Vector2 landed = anim.target_pos;
                            anim.active = 0; audio_play_drop(&audio);
                            motion_set_instant(card, landed, (Vector2){ FIELD_CARD_W, FIELD_CARD_H });
                            engine_play_hand(&g, card, target);
                            replay_rec_play(1, card, target);
                            if (g.last_captured_count > 0) play_match_capture_sfx();
                            else trigger_cpu_step_beat(0.09f);
                            trigger_event_beat((g.last_captured_count > 0) ? 0.36f : 0.30f);
                        } else if (!badge_freezing && !motion_is_reflowing(&g) && event_beat_timer <= 0.0f) {
                            if (anim.active && anim.type == ANIM_CPU_THINK && anim.elapsed >= anim.duration) {
                                anim.active = 0; int card, target;
                                ai_choose_hand(&g, active_diff, &ai_rng, &card, &target);
                                Vector2 from = { 710.0f, 12.0f };
                                Rectangle land = (target < 0) ? board_get_field_card_rect_of(&g, card) : (Rectangle){ 0, 0, 0, 0 };
                                Vector2 to = (target >= 0) ? board_get_card_field_pos(&g, target)
                                           : (land.width > 0.0f ? (Vector2){ land.x, land.y } : (Vector2){ 710.0f, 240.0f });
                                anim_start_cpu_fly(&anim, card, target, from, to, get_flight_duration(settings.anim_speed));
                            } else if (!anim.active) {
                                anim_start_cpu_think(&anim, get_think_duration(settings.anim_speed));
                            }
                        }
                    }

                    if (g.phase == PHASE_TURN_DRAW && !cinematic_is_active()) {
                        if (anim.active && anim.type == ANIM_STOCK_FLY && anim.elapsed >= anim.duration) {
                            int drawn_cid = anim.card_id;
                            int prev_player = g.current_player;
                            Vector2 landed = anim.target_pos;
                            int rev_land = (plexus_speedcoin_is_active() && landed.x == 330.0f && landed.y == 262.0f);
                            anim.active = 0; audio_play_drop(&audio);
                            motion_set_instant(drawn_cid, landed, (Vector2){ FIELD_CARD_W, FIELD_CARD_H });
                            if (rev_land) {
                                plexus_speedcoin_reverse_draw_finish(&g);
                                trigger_event_beat(0.24f);
                            } else {
                                engine_reveal_stock(&g);
                                if (g.last_captured_count > 0) play_match_capture_sfx();
                                else if (prev_player == 1 || g.current_player == 1) trigger_cpu_step_beat(0.09f);
                                trigger_event_beat((g.last_captured_count > 0) ? 0.38f : 0.30f);
                            }
                        } else if (!anim.active && !board_busy) {
                            if (plexus_speedcoin_is_active() && speed_mode_reverse_deck()) {
                                int rev_cid = -1;
                                Vector2 from_pos = { 710.0f, 240.0f };
                                if (plexus_speedcoin_reverse_draw_start(&g, &rev_cid, &from_pos)) {
                                    audio_play_flip(&audio);
                                    anim_start_stock_fly(&anim, rev_cid, from_pos, (Vector2){ 330.0f, 262.0f },
                                                         get_flight_duration(settings.anim_speed));
                                } else {
                                    plexus_speedcoin_reverse_draw_finish(&g);
                                    trigger_event_beat(0.24f);
                                }
                            } else {
                                int top_stock = (g.stock_count > 0) ? g.stock[g.stock_count - 1] : -1;
                                if (top_stock >= 0) {
                                    audio_play_flip(&audio); int match_target = -1;
                                    for (int i = 0; i < g.field_count; i++) {
                                        if (CARDS[g.field[i]].month == CARDS[top_stock].month) { match_target = g.field[i]; break; }
                                    }
                                    Rectangle land = (match_target >= 0) ? board_get_field_card_rect_of(&g, match_target)
                                                                         : board_get_field_card_rect_of(&g, top_stock);
                                    Vector2 to_pos = (land.width > 0.0f) ? (Vector2){ land.x, land.y } : (Vector2){ 710.0f, 240.0f };
                                    anim_start_stock_fly(&anim, top_stock, (Vector2){ 330.0f, 262.0f }, to_pos, get_flight_duration(settings.anim_speed));
                                } else {
                                    engine_reveal_stock(&g);
                                    trigger_event_beat(0.24f);
                                }
                            }
                        }
                    }

                    if (g.current_player == 1 && g.phase == PHASE_DRAW_CHOICE && input_unlocked) {
                        int target = ai_choose_stock_target(&g, active_diff, &ai_rng);
                        engine_pick_draw_choice(&g, target);
                        replay_rec_pick(1, target);
                        if (g.last_captured_count > 0) play_match_capture_sfx();
                        else trigger_cpu_step_beat(0.09f);
                        trigger_event_beat(0.36f);
                    } else if (g.current_player == 1 && g.phase == PHASE_DECIDE && input_unlocked) {
                        DecisionType dec = ai_choose_decision(&g, active_diff, &ai_rng);
                        if (dec == DECISION_KOI_KOI) {
                            int cpu_streak = koi_caller_multiplier(g.koi_called[1] + 1); /* streak after this call */
                            audio_play_koi(&audio); anim_start_banner(&anim, TextFormat("CPU: KOI-KOI  x%d", cpu_streak), 1.5f);
                            if (!plexus_tutorial_is_active() && !plexus_training_is_active() && !plexus_action_card_is_active())
                                taunt_trigger(campaign_get_opponent(settings.current_opponent)->koi_taunt, 4.0f);
                        }
                        engine_decide(&g, dec);
                        replay_rec_koi(1, dec == DECISION_KOI_KOI);
                        trigger_cpu_step_beat(0.11f);
                        trigger_event_beat((dec == DECISION_KOI_KOI) ? 0.55f : 0.36f);
                    }

                    if (g.current_player != 0 || (g.phase != PHASE_TURN_HAND && g.phase != PHASE_HAND_CHOICE)) human_turn_started = 0;

                    int drag_live = drag.on && !badge_freezing && g.phase == PHASE_TURN_HAND &&
                                    !anim_is_blocking(&anim) && !cinematic_is_active();
                    if (g.current_player == 0 && (input_unlocked || drag_live || (g.phase == PHASE_HAND_CHOICE && !badge_freezing))) {
                        if (g.phase == PHASE_TURN_HAND) {
                            if (input_unlocked && !plexus_tutorial_is_active() && !plexus_speedcoin_is_active() &&
                                (settings.difficulty == DIFFICULTY_MASTER || settings.difficulty == DIFFICULTY_DEUS)) {
                                if (!human_turn_started) {
                                    g.player_timer = (settings.difficulty == DIFFICULTY_DEUS || settings.master_timer == 15) ? 15.0f : 30.0f;
                                    human_turn_started = 1;
                                }
                                g.player_timer -= dt;
                                if (g.player_timer <= 0.0f) {
                                    if (settings.difficulty == DIFFICULTY_DEUS) {
                                        /* DEUS SUPREMUS only: timer 0 = GAME OVER. No points, no win,
                                           no stats, no record - straight back to the origin screen. */
                                        int plex_match = plexus_action_card_is_active() || plexus_training_is_active();
                                        if (plexus_action_card_is_active()) plexus_action_card_abort();
                                        if (plexus_training_is_active()) plexus_training_stop();
                                        g.player_timer = 0.0f; drag.on = 0; drag.idx = -1; drag.selected = 0; human_turn_started = 0;
                                        audio_play_lose(&audio);
                                        deus_banner_timer = TRANS_OUT_DUR;
                                        start_transition(plex_match ? SCREEN_ATRIUM : match_origin, 3, 0);
                                    } else {
                                        g.player_timer = 0.0f; drag.on = 0; drag.idx = -1; drag.selected = 0;
                                        engine_timeout_autoplay(&g);
                                        replay_rec_timeout(0);
                                        if (g.last_captured_count > 0) play_match_capture_sfx();
                                        else { audio_play_drop(&audio); trigger_cpu_step_beat(0.12f); }
                                        anim_start_banner(&anim, "! TIMEOUT !", 1.0f); human_turn_started = 0;
                                        trigger_event_beat(0.36f);
                                    }
                                }
                            }
                            if (g.phase == PHASE_TURN_HAND) {
                                int hand_before = g.hand_count[0];
                                int allowed_cid = plexus_tutorial_allowed_hand_card(&g);
                                int allow_input = 1;
                                if (allowed_cid >= 0 && !drag.on) {
                                    int allowed_idx = -1;
                                    for (int hi = 0; hi < g.hand_count[0]; hi++) {
                                        if (g.hand[0][hi] == allowed_cid) { allowed_idx = hi; break; }
                                    }
                                    if (drag.selected > 0 && drag.selected - 1 != allowed_idx) drag.selected = 0;
                                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && allowed_idx >= 0) {
                                        Rectangle ar = board_get_hand_card_rect(allowed_idx, g.hand_count[0], 0);
                                        int hit_allowed = CheckCollisionPointRec(vm, ar);
                                        int hit_field_sel = (drag.selected == allowed_idx + 1) &&
                                                            CheckCollisionPointRec(vm, board_get_field_rect());
                                        if (!hit_allowed && !hit_field_sel) allow_input = 0;
                                    }
                                }
                                if (allow_input) {
                                    input_handle_mouse(&g, &drag, &anim, l, vm, &hovered_idx, input_unlocked);
                                }
                                if (allowed_cid >= 0) {
                                    if (hovered_idx >= 0 && g.hand[0][hovered_idx] != allowed_cid) hovered_idx = -1;
                                    if (drag.on && g.hand[0][drag.idx] != allowed_cid) { drag.on = 0; drag.idx = -1; }
                                    if (drag.selected > 0 && g.hand[0][drag.selected - 1] != allowed_cid) drag.selected = 0;
                                }
                                if (g.hand_count[0] < hand_before) {
                                    if (g.last_captured_count > 0) play_match_capture_sfx();
                                    else audio_play_drop(&audio);
                                    trigger_event_beat((g.last_captured_count > 0) ? 0.32f : 0.28f);
                                } else if (g.phase == PHASE_HAND_CHOICE) {
                                    audio_play_drop(&audio);
                                }
                            }
                        }
                        else if (g.phase == PHASE_HAND_CHOICE && !badge_freezing && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                            int tid; int res = screens_check_choice_click(&g, vm, 1, &tid);
                            if (res == 1) {
                                Vector2 hp = board_get_card_field_pos(&g, tid);
                                audio_play_drop(&audio); engine_pick_hand_choice(&g, tid); replay_rec_pick(0, tid);
                                if (g.last_captured_count > 0) { play_match_capture_sfx(); speed_hit_popup_commit(hp.x + 38.0f, hp.y); }
                                trigger_event_beat(0.32f);
                            } else if (res == 2) {
                                audio_play_click(&audio);
                                if (g.ruleset == RULESET_KK_ADD_10) speed_bar_consume(); else speed_bar_consume_visual();
                                engine_cancel_hand_choice(&g); replay_rec_cancel(0);
                            }
                        } else if (g.phase == PHASE_DRAW_CHOICE && input_unlocked && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                            int tid; if (screens_check_choice_click(&g, vm, 0, &tid) == 1) {
                                audio_play_drop(&audio); engine_pick_draw_choice(&g, tid); replay_rec_pick(0, tid);
                                if (g.last_captured_count > 0) play_match_capture_sfx();
                                trigger_event_beat(0.34f);
                            }
                        } else if (g.phase == PHASE_DECIDE && input_unlocked && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                            DecisionType dec;
                            if (screens_check_decide_click(vm, &dec)) {
                                audio_play_click(&audio);
                                if (dec == DECISION_KOI_KOI) {
                                    int you_streak = koi_caller_multiplier(g.koi_called[0] + 1);
                                    audio_play_koi(&audio);
                                    anim_start_banner(&anim, TextFormat("KOI-KOI  x%d", you_streak), 1.25f);
                                }
                                engine_decide(&g, dec); replay_rec_koi(0, dec == DECISION_KOI_KOI);
                                trigger_event_beat((dec == DECISION_KOI_KOI) ? 0.42f : 0.36f);
                            }
                        }
                    }

                    board_busy = (ac_table && action_card_is_event_freezing()) || (event_beat_timer > 0.0f) ||
                                 motion_is_reflowing(&g) || anim_is_blocking(&anim);
                    if (plexus_speedcoin_is_active()) {
                        if (g.phase == PHASE_DECIDE) {
                            engine_decide(&g, DECISION_KOI_KOI);
                            g.koi_called[0] = 0;
                            g.match_koi_calls[0] = 0;
                        }
                        if (g.phase == PHASE_ROUND_END && speed_mode_clock() > 0.0f && !board_busy) {
                            if (plexus_speedcoin_deal_hand2(&g)) {
                                audio_play_flip(&audio);
                                anim_start_banner(&anim, "30 MP // BONUS HAND DEALT", 1.10f);
                                prev_round_phase = PHASE_NONE;
                                trigger_event_beat(0.30f);
                            } else if (prev_round_phase != PHASE_ROUND_END) {
                                plexus_speedcoin_bank_wave(&g);
                                prev_round_phase = PHASE_ROUND_END;
                                round_banner_timer = 0.0f;
                                play_banner_sfx();
                                if (anim.active && anim.type == ANIM_BANNER) anim.active = 0;
                            }
                        } else if (g.phase != PHASE_ROUND_END) {
                            prev_round_phase = g.phase;
                        }
                    } else if (g.phase == PHASE_ROUND_END && prev_round_phase != PHASE_ROUND_END &&
                               !cinematic_is_active() && !board_busy) {
                        prev_round_phase = PHASE_ROUND_END;
                        round_banner_timer = ROUND_BANNER_DUR;
                        play_banner_sfx();
                        if (anim.active && anim.type == ANIM_BANNER) anim.active = 0;
                        if (g.current_round >= g.total_rounds) speed_bar_apply_match_bonus(&g);
                    } else if (g.phase != PHASE_ROUND_END) {
                        prev_round_phase = g.phase;
                    }

                    if (g.phase == PHASE_ROUND_END && prev_round_phase == PHASE_ROUND_END &&
                        round_banner_timer <= 0.0f && overlay_alpha >= 0.65f &&
                        input_unlocked && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        if (screens_check_round_result_click(vm)) {
                            audio_play_click(&audio);
                            event_beat_timer = 0.0f;
                            if (plexus_speedcoin_is_active()) {
                                int next_w = speed_mode_wave() + 1;
                                anim_init(&anim); prev_round_phase = PHASE_NONE;
                                plexus_speedcoin_setup_wave(&g, next_w);
                                motion_deal_round(&g, 0.45f);
                                audio_play_flip(&audio);
                                cinematic_start_countdown(next_w);
                            } else if (plexus_tutorial_is_active() && !plexus_action_card_is_active()) {
                                anim_init(&anim);
                                plexus_tutorial_finish_match();
                                pending_result_sfx = 1;
                                start_transition(SCREEN_ATRIUM, 2, 0);
                            } else if (g.current_round >= g.total_rounds) {
                                int was_ac_tut = plexus_tutorial_is_active() && plexus_action_card_is_active();
                                anim_init(&anim);
                                speed_bar_apply_match_bonus(&g); /* KK-ADD-10: minipoints -> score */
                                engine_next_round(&g);
                                if (plexus_action_card_is_active()) {
                                    plexus_action_card_finish_match(&g);
                                } else if (plexus_training_is_active()) {
                                    plexus_training_finish(&g);
                                } else {
                                    storage_record_match(&g, &settings, &stats, &history);
#ifndef __EMSCRIPTEN__
                                    ui_replay_commit_auto(&history);
#endif
                                }
                                plexus_save();
                                if (was_ac_tut) {
                                    pending_result_sfx = 1;
                                    start_transition(SCREEN_ATRIUM, 2, 0);
                                } else {
                                    if (!plexus_training_result_active() && g.total_score[0] > g.total_score[1]) {
                                        pending_result_sfx = 1;
                                        if (!plexus_training_is_active() && !plexus_action_card_is_active() &&
                                            !plexus_action_card_result_active()) {
                                            bool meets_clearance = (settings.difficulty == DIFFICULTY_MASTER && settings.rounds >= 3) ||
                                                                   (settings.difficulty == DIFFICULTY_HARD   && settings.rounds >= 6) ||
                                                                   (settings.difficulty == DIFFICULTY_EASY   && settings.rounds >= 9);
                                            /* Award clearance coin for this opponent on the difficulty played (sticky, no cascade) */
                                            if (meets_clearance) {
                                                int bit = settings.current_opponent * 3;
                                                if (settings.difficulty == DIFFICULTY_HARD) bit += 1;
                                                else if (settings.difficulty == DIFFICULTY_MASTER) bit += 2;
                                                /* EASY -> +0 */
                                                settings.cleared_mask |= (1ULL << bit);
                                            }
                                            campaign_update_unlocks(&settings);
                                            storage_save_settings(&settings);
                                        }
                                    } else if (!plexus_training_result_active() && g.total_score[0] < g.total_score[1]) pending_result_sfx = -1;
                                    start_transition(SCREEN_MATCH_RESULT, 2, 0);
                                }
                            } else {
                                anim_init(&anim); prev_round_phase = PHASE_NONE;
                                action_card_advance_round();
                                engine_next_round(&g); motion_deal_round(&g, 0.45f);
                                cinematic_start(g.current_round);
                                if (!plexus_training_is_active() && !plexus_action_card_is_active())
                                    taunt_trigger(campaign_get_opponent(settings.current_opponent)->taunt_text, 3.5f);
                            }
                        }
                    }
                }
            }
        }
    }

    if (g.phase != PHASE_TURN_HAND || g.current_player != 0) drag.selected = 0;
    if (drag.on) {
        if (screen != SCREEN_GAME || g.phase != PHASE_TURN_HAND || g.current_player != 0 ||
            drag.idx < 0 || drag.idx >= g.hand_count[0]) {
            drag.on = 0; drag.idx = -1; drag.moved = 0;
        } else if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            if (drag.moved && !anim.active) {
                Rectangle slot = board_get_hand_card_rect(drag.idx, g.hand_count[0], 0);
                anim_start_glide(&anim, g.hand[0][drag.idx], drag.pos, (Vector2){ slot.x, slot.y }, 0.30f);
            }
            drag.on = 0; drag.idx = -1; drag.moved = 0;
        }
    }
    int replay_running = replay_pb_active();
    int sc_ending = (speedcoin_gameover_timer > 0.0f) || (screen == SCREEN_GAME && plexus_speedcoin_result_active());
    int fg_settled = !action_card_is_event_freezing() && event_beat_timer <= 0.0f &&
                     !motion_is_reflowing(&g) && !anim_is_blocking(&anim);
    int round_panel_ready = !sc_ending && !plexus_speedcoin_result_active() &&
                            (plexus_speedcoin_is_active()
                             ? (g.phase == PHASE_ROUND_END && prev_round_phase == PHASE_ROUND_END)
                             : ((g.phase == PHASE_ROUND_END && prev_round_phase == PHASE_ROUND_END && round_banner_timer <= 0.0f) ||
                                g.phase == PHASE_MATCH_END));
    int overlay_active = (screen == SCREEN_GAME) &&
                         ((replay_running || sc_ending) ? is_paused :
                          ((g.phase == PHASE_HAND_CHOICE && !action_card_is_event_freezing()) ||
                           (g.phase == PHASE_DRAW_CHOICE && fg_settled) ||
                           (g.phase == PHASE_DECIDE && g.current_player == 0 && fg_settled) ||
                           g.phase == PHASE_DEALER_SELECT || round_panel_ready || is_paused));
    if (overlay_active) {
        if (g.phase != last_overlay_phase || is_paused != last_overlay_paused) {
            overlay_alpha = (g.phase == PHASE_DEALER_SELECT ||
                             (g.phase == PHASE_MATCH_END && last_overlay_phase == PHASE_ROUND_END)) ? 1.0f : 0.0f;
            last_overlay_phase = g.phase; last_overlay_paused = is_paused;
        }
        overlay_alpha = fminf(1.0f, overlay_alpha + dt / 0.85f);
    } else {
        overlay_alpha = 0.0f; last_overlay_phase = g.phase; last_overlay_paused = is_paused;
    }

    if (screen == SCREEN_GAME && !replay_pb_active() && !is_paused) taunt_update(dt);

    card_tooltip_begin(!plexus_tutorial_panel_open() && screen == SCREEN_GAME && !replay_pb_active() &&
                       (plexus_tutorial_is_active() || settings.difficulty == DIFFICULTY_EASY) &&
                       !board_is_action_distraction_high() && !action_card_is_event_freezing() &&
                       !is_paused && !drag.on && !cinematic_is_active() &&
                       transition_dir == 0 && transition_alpha <= 0.0f && round_banner_timer <= 0.0f &&
                       !(anim.active && anim.type == ANIM_BANNER) &&
                       IsCursorOnScreen() && IsWindowFocused(), vm);
    capture_info_begin(!plexus_tutorial_panel_open() && screen == SCREEN_GAME && !overlay_active && !drag.on &&
                       !action_card_is_event_freezing() &&
                       !cinematic_is_active() && transition_dir == 0 && transition_alpha <= 0.0f &&
                       round_banner_timer <= 0.0f && !(anim.active && anim.type == ANIM_BANNER) &&
                       IsCursorOnScreen() && IsWindowFocused(), vm);
    int hub_wipe_stage = hub_trans_active && screen != SCREEN_GAME && (hub_trans_timer >= HUB_BLINK_DUR - 1e-4f);
    if (hub_wipe_stage) {
        plexus_set_intro_ui_texture((Texture2D){ 0 });
        BeginTextureMode(overlay_canvas);
        ClearBackground(BLACK);
        draw_hub_screen_state(hub_from_screen, hub_from_campaign, (Vector2){ -1000.0f, -1000.0f });
        EndTextureMode();
    } else if (screen == SCREEN_ATRIUM && plexus_intro_revealing()) {
        BeginTextureMode(overlay_canvas);
        ClearBackground(BLANK);
        plexus_draw_ui_chrome(&assets, &stats, (Vector2){ -1000.0f, -1000.0f });
        EndTextureMode();
        plexus_set_intro_ui_texture(overlay_canvas.texture);
    } else {
        plexus_set_intro_ui_texture((Texture2D){ 0 });
    }

    BeginTextureMode(canvas);
    if (screen != SCREEN_GAME) {
        if (hub_trans_active) {
            Vector2 off_vm = { -1000.0f, -1000.0f };
            if (!hub_wipe_stage) {
                int flash_on = ((((int)(hub_trans_timer * 60.0f + 0.5f)) / 4) & 1) == 0;
                draw_hub_screen_state(hub_from_screen, hub_from_campaign, off_vm);
                draw_hub_strobe_button(hub_btn_rect, hub_btn_label, hub_btn_fsz, flash_on);
            } else {
                float u = fminf(1.0f, fmaxf(0.0f, (hub_trans_timer - HUB_BLINK_DUR) / HUB_WIPE_DUR));
                float ease = 0.30f * u + 0.70f * smooth01(u);
                int split_y = (int)roundf(ease * (float)CANVAS_H);
                draw_hub_screen_state(screen, campaign_overlay_open, off_vm);
                if (split_y < CANVAS_H) {
                    BeginScissorMode(0, split_y, CANVAS_W, CANVAS_H - split_y);
                    DrawTextureRec(overlay_canvas.texture,
                                   (Rectangle){ 0, 0, (float)CANVAS_W, -(float)CANVAS_H },
                                   (Vector2){ 0, 0 }, WHITE);
                    EndScissorMode();
                }
                float beam_fade = (u > 0.88f) ? ((1.0f - u) / 0.12f) : 1.0f;
                draw_hub_scan_beam(split_y, beam_fade);
            }
        } else {
            draw_hub_screen_state(screen, campaign_overlay_open, vm);
        }
    }
    else if (screen == SCREEN_GAME) {
        const char *status = ""; char cpu_status[64];
        if (replay_pb_active()) {
            status = is_paused ? "REPLAY PAUSED" : "REPLAY - P TO PAUSE   ESC TO STOP";
            if (anim.active && (anim.type == ANIM_CPU_FLY || anim.type == ANIM_STOCK_FLY)) {
                int fc = (anim.type == ANIM_CPU_FLY) ? anim.cpu_card : anim.card_id;
                if (fc >= 0 && fc < 48) {
                    snprintf(cpu_status, sizeof(cpu_status), "REPLAY - %s", CARDS[fc].name);
                    status = cpu_status;
                }
            }
        }
        else if (cinematic_is_active() || action_card_is_event_freezing()) status = "";
        else if (anim.active && anim.type == ANIM_STOCK_FLY) {
            if (plexus_speedcoin_is_active() && anim.target_pos.x == 330.0f && anim.target_pos.y == 262.0f) {
                status = "REVERSE DECK - CARD RETURNED TO STOCK";
            } else {
                snprintf(cpu_status, sizeof(cpu_status), "DRAW STOCK: %s", CARDS[anim.card_id].name); status = cpu_status;
            }
        } else if (g.phase == PHASE_TURN_HAND) {
            status = (g.current_player == 0) ? (drag.selected ? "SELECTED - CLICK A MATCH OR EMPTY FIELD / ESC TO CANCEL" : "YOUR TURN - DRAG OR CLICK A CARD TO SELECT") :
                     (anim.active && anim.type == ANIM_CPU_FLY) ? "CPU PLAYS CARD" : "CPU IS THINKING...";
        } else if (g.phase == PHASE_HAND_CHOICE || g.phase == PHASE_DRAW_CHOICE) status = "CHOOSE ONE OF THE MATCHING CARDS";
        else if (g.phase == PHASE_DECIDE) status = (g.current_player == 0) ? "CALL KOI-KOI TO CONTINUE OR SHOBU TO SCORE" : "";

        ac_paused = is_paused || plexus_tutorial_panel_open();
        action_card_sync_music(&audio, ac_table, ac_paused, transition_alpha, 0.0f);
        board_draw_table(&assets, &g, &anim, hovered_idx, drag.on ? drag.idx : -1, drag.pos,
                         replay_running ? (replay_pb_difficulty() == DIFFICULTY_EASY)
                                        : (plexus_tutorial_is_active() || settings.difficulty == DIFFICULTY_EASY),
                         replay_running, status);
        if (!replay_pb_active()) {
            input_draw_selection(&g, &drag);
            plexus_tutorial_draw_board_guides(&assets, &g);
            taunt_draw();
        }

        if (!is_paused) {
            Rectangle pause_btn = { 1160, 560, 112, 40 }, restart_btn = { 1160, 610, 112, 40 };
            if (CheckCollisionPointRec(vm, pause_btn)) {
                DrawRectangleRec(pause_btn, COLOR_BTN_HOVER_BG); ui_draw_panel_border(pause_btn, COLOR_PHOSPHOR);
                draw_hover_action_label("PAUSE", pause_btn);
            }
            if (CheckCollisionPointRec(vm, restart_btn)) {
                DrawRectangleRec(restart_btn, COLOR_BTN_HOVER_BG); ui_draw_panel_border(restart_btn, COLOR_PHOSPHOR);
                draw_hover_action_label(replay_running ? "STOP" : "RESTART", restart_btn);
            }
        }
        ui_theme_set_global_flare_boost(0.0f, 1.0f);
        if (sc_ending) screens_draw_banner("GAME OVER");
        else if (anim.active && anim.type == ANIM_BANNER) screens_draw_banner(anim.banner_text);
        if (!replay_running && round_banner_timer > 0.0f) draw_round_end_banner(&g, round_banner_timer);
        cinematic_draw();   /* a replay shows the round deal too */
        if (overlay_active && overlay_alpha >= 0.999f) draw_active_game_overlays(&assets, &g, vm, is_paused, confirm_restart, replay_pb_active());
    }
    EndTextureMode();

    if (overlay_active && overlay_alpha > 0.0f && overlay_alpha < 0.999f) {
        float oa = smooth01(overlay_alpha);
        BeginTextureMode(overlay_canvas); ClearBackground(BLANK);
        draw_active_game_overlays(&assets, &g, vm, is_paused, confirm_restart, replay_pb_active());
        EndTextureMode();
        BeginTextureMode(canvas);
        DrawTextureRec(overlay_canvas.texture, (Rectangle){ 0, 0, (float)CANVAS_W, -(float)CANVAS_H }, (Vector2){ 0, 0 }, (Color){ 255, 255, 255, (unsigned char)(oa * 255.0f) });
        EndTextureMode();
    }
    if (!is_paused && screen == SCREEN_GAME && plexus_tutorial_is_active()) {
        BeginTextureMode(canvas);
        plexus_tutorial_draw_panel(&assets, vm);
        EndTextureMode();
    }
    if (transition_alpha > 0.0f) {
        BeginTextureMode(canvas);
        DrawRectangle(0, 0, CANVAS_W, CANVAS_H, (Color){ 0, 0, 0, (unsigned char)(fminf(1.0f, fmaxf(0.0f, transition_alpha)) * 255.0f) });
        if (transition_dir == 2) draw_loading_card(&assets, transition_timer / TRANS_HOLD_DUR);
        EndTextureMode();
    }
    /* DEUS timeout banner: big red transparent text, drawn above everything so it
       hits the match field and rides the fade back to the start screen */
    if (deus_banner_timer > 0.0f) {
        BeginTextureMode(canvas);
        float t = fminf(1.0f, deus_banner_timer / 0.50f);             /* fade-out tail */
        unsigned char a = (unsigned char)(t * 200.0f);                /* ~78% - transparent */
        const char *msg = "/// TIME EXPIRED ///";
        Vector2 ts = MeasureTextEx(font_title, msg, 56.0f, 1.6f);
        DrawTextEx(font_title, msg, (Vector2){ floorf((CANVAS_W - ts.x) / 2.0f), 330.0f }, 56.0f, 1.6f, (Color){ 0xFF, 0x30, 0x30, a });
        EndTextureMode();
    }

    BeginTextureMode(canvas);
    card_tooltip_draw();
    capture_info_draw(&assets);
    EndTextureMode();

    BeginDrawing(); ClearBackground(BLACK);
    int use_post = (post_shader.id > 0) && (b_val > 0.001f || c_val > 0.001f || fabsf(g_val - 1.0f) > 0.005f);
    if (use_post) BeginShaderMode(post_shader);
    DrawTexturePro(canvas.texture, (Rectangle){ 0, 0, (float)CANVAS_W, -(float)CANVAS_H },
                   (Rectangle){ l.ox, l.oy, CANVAS_W * l.scale, CANVAS_H * l.scale }, (Vector2){ 0, 0 }, 0.0f, WHITE);
    if (use_post) EndShaderMode();
    EndDrawing();

    if (shot_frame > 0 && frame_count >= shot_frame) { TakeScreenshot("screenshot.png"); quit_requested = 1; }
}

int main(int argc, char **argv) {
    uint32_t seed = 7;
    int win_w = 1280, win_h = 720, auto_start = 0, auto_options = 0, auto_dealer = 0, auto_stats = 0, auto_campaign = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) seed = (uint32_t)atoi(argv[++i]);
        else if (strcmp(argv[i], "--shot-frame") == 0 && i + 1 < argc) shot_frame = atoi(argv[++i]);
        else if (strcmp(argv[i], "--window") == 0 && i + 1 < argc) sscanf(argv[++i], "%dx%d", &win_w, &win_h);
        else if (strcmp(argv[i], "--autostart") == 0) auto_start = 1;
        else if (strcmp(argv[i], "--options") == 0) auto_options = 1;
        else if (strcmp(argv[i], "--stats") == 0) auto_stats = 1;
        else if (strcmp(argv[i], "--campaign") == 0) auto_campaign = 1;
        else if (strcmp(argv[i], "--dealer") == 0) auto_dealer = 1;
    }
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(win_w, win_h, "[XENO/KOI] (Giger Biomechanical Edition)");
    if (GetApplicationDirectory() && GetApplicationDirectory()[0] != '\0') ChangeDirectory(GetApplicationDirectory());
    SetWindowMinSize(960, 540); SetTargetFPS(60);

    storage_load_settings(&settings);
    if (campaign_update_unlocks(&settings)) storage_save_settings(&settings);
#ifdef __EMSCRIPTEN__
    /* The browser build does not use raylib's fullscreen mode. On web,
       ToggleBorderlessWindowed() resolves to Module.requestFullscreen(), which
       re-parents the canvas and resizes its CSS box to monitor device pixels,
       leaving it out of step with the backing store raylib sizes to the window:
       the game is drawn offset and clicks land in the wrong place. The web
       shell owns fullscreen instead (see web/shell.html, docs/errata.md), so
       ignore whatever the settings file says. This also self-heals a
       fullscreen=1 saved by an earlier web session. */
    settings.fullscreen = 0;
#else
    if (settings.fullscreen == 1) ToggleBorderlessWindowed();
#endif
    srand((unsigned int)time(NULL) ^ seed);
    theme_set(settings.terminal_color);
    audio_init(&audio, settings.bgm_volume, settings.sfx_volume);
    init_banner_sfx();
    init_card_slam_sfx();
    audio_set_effects(settings.effect_chorus, settings.effect_bass, settings.effect_reverb);
#ifndef __EMSCRIPTEN__
    if (settings.music_folder[0]) audio_set_music_folder(&audio, settings.music_folder);
#endif
    canvas = LoadRenderTexture(CANVAS_W, CANVAS_H); SetTextureFilter(canvas.texture, TEXTURE_FILTER_BILINEAR);
    overlay_canvas = LoadRenderTexture(CANVAS_W, CANVAS_H); SetTextureFilter(overlay_canvas.texture, TEXTURE_FILTER_BILINEAR);

#ifdef __EMSCRIPTEN__
    const char *sh_path = "assets/postprocess_web.fs";
#else
    const char *sh_path = "assets/postprocess.fs";
#endif
    post_shader = LoadShader(0, FileExists(sh_path) ? sh_path : TextFormat("%s/%s", GetApplicationDirectory() ? GetApplicationDirectory() : "", sh_path));
    loc_bloom = GetShaderLocation(post_shader, "bloom_intensity");
    loc_crt = GetShaderLocation(post_shader, "crt_intensity");
    loc_gamma = GetShaderLocation(post_shader, "gamma");

    board_load_assets(&assets); assets.current_bg = settings.background;
    if (settings.difficulty == DIFFICULTY_DEUS) { settings.ruleset = RULESET_YAKUZA; settings.background = 5; assets.current_bg = 5; theme_set(5); }

    font_title = LoadFontEx("assets/audiowide.ttf", 96, NULL, 0);
    if (!IsFontReady(font_title)) font_title = LoadFontEx(TextFormat("%s/assets/audiowide.ttf", GetApplicationDirectory() ? GetApplicationDirectory() : ""), 96, NULL, 0);
    if (!IsFontReady(font_title)) font_title = GetFontDefault();
    else { GenTextureMipmaps(&font_title.texture); SetTextureFilter(font_title.texture, TEXTURE_FILTER_TRILINEAR); }

    int body_cps[97];
    for (int cp = 0; cp < 95; cp++) body_cps[cp] = 32 + cp;
    body_cps[95] = 0x03A6; /* U+03A6 Greek Capital Letter Phi */
    body_cps[96] = 0x03A8; /* U+03A8 Greek Capital Letter Psi */
    font_body = LoadFontEx("assets/play.ttf", 36, body_cps, 97);
    if (!IsFontReady(font_body)) font_body = LoadFontEx(TextFormat("%s/assets/play.ttf", GetApplicationDirectory() ? GetApplicationDirectory() : ""), 36, body_cps, 97);
    if (!IsFontReady(font_body)) font_body = GetFontDefault();
    else { GenTextureMipmaps(&font_body.texture); SetTextureFilter(font_body.texture, TEXTURE_FILTER_BILINEAR); }
    small_font = font_body;

    storage_load_stats(&stats); storage_load_history(&history); plexus_load();
    screen = (auto_start || auto_dealer) ? SCREEN_GAME : auto_options ? SCREEN_OPTIONS : auto_stats ? SCREEN_STATS : SCREEN_START;
    engine_init(&g, settings.ruleset, settings.rounds, seed);
    motion_init(); taunt_init(); ai_set_opponent(settings.current_opponent);
    if (auto_start) {
        engine_start_round(&g); motion_deal_round(&g, 0.45f); cinematic_start(g.current_round);
        audio_start_match_music(&audio, pick_match_track(0));
        taunt_trigger(campaign_get_opponent(settings.current_opponent)->taunt_text, 4.0f);
    }

    anim_init(&anim); memset(&drag, 0, sizeof(drag));
    ai_rng = seed + 999; campaign_overlay_open = auto_campaign;

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(update_draw_frame, 0, 1);
#else
    while (!WindowShouldClose() && !quit_requested) update_draw_frame();
#endif

    plexus_restore_match_settings(&settings, &assets);
    plexus_save();
    action_card_detach_music(&audio);
    if (has_sfx_card_slam) UnloadSound(sfx_card_slam);
    if (has_sfx_banner) UnloadSound(sfx_banner);
    audio_close(&audio);
    if (font_title.texture.id != GetFontDefault().texture.id) UnloadFont(font_title);
    if (font_body.texture.id != GetFontDefault().texture.id) UnloadFont(font_body);
    plexus_unload_assets();
    board_unload_assets(&assets);
    if (post_shader.id > 0) UnloadShader(post_shader);
    UnloadRenderTexture(overlay_canvas); UnloadRenderTexture(canvas);
    CloseWindow();
    return 0;
}
