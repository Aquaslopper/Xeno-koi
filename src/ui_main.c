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
#include "ui_anim.h"
#include "ui_motion.h"
#include "ui_audio.h"
#include "ui_screens.h"
#include "ui_input.h"
#include "ui_combos.h"
#include "campaign.h"
#include "ui_taunt.h"
#include "replay.h"
#include "ui_replay.h"

#ifndef IsFontReady
#define IsFontReady(f) ((f).texture.id > 0)
#endif

Font font_title;  // Audiowide for titles, headers, and button labels
Font font_body;   // Play for data, options, HUD, and supporting text
Font small_font;  // Legacy alias for modules that still use the old name
GameSettings settings;

static float transition_alpha = 0.0f, cached_bloom = -1.0f, cached_crt = -1.0f, cached_gamma = -1.0f;
static float saved_timer = 0.0f, log_timer = 0.0f, overlay_alpha = 0.0f, deus_banner_timer = 0.0f;
static int transition_dir = 0, transition_action = 0, transition_track = 1, loc_bloom, loc_crt, loc_gamma;
static int is_paused = 0, confirm_restart = 0, confirm_reset = 0, campaign_overlay_open = 0;
static int combo_player = -1, combo_page = 0;
static int last_replay_round = 0;   /* replay only: last round the deal was shown for */
static float replay_beat = 0.0f;    /* replay only: pause after a card lands */
static int last_overlay_paused = 0, hovered_idx = -1, frame_count = 0, human_turn_started = 0, shot_frame = -1, quit_requested = 0;
static ScreenState transition_target_screen = SCREEN_START, screen = SCREEN_START;
static GamePhase last_overlay_phase = PHASE_NONE;
static AudioAssets audio;
static RenderTexture2D canvas, overlay_canvas;
static Shader post_shader;
static BoardAssets assets;
static GameStats stats;
static MatchHistory history;
static GameState g;
static AnimState anim;
static DragState drag;
static uint32_t ai_rng = 1006;

static void start_transition(ScreenState target, int action, int track) {
    transition_dir = 1; transition_target_screen = target; transition_action = action; transition_track = track;
}

static float get_flight_duration(int s) { return (s == 0) ? 0.65f : (s == 1) ? 0.50f : 0.40f; }
static float get_think_duration(int s)  { return (s == 0) ? 0.40f : (s == 1) ? 0.25f : 0.15f; }

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

static void draw_active_game_overlays(const BoardAssets *ba, const GameState *gs, Vector2 vm, int paused, int c_restart, int replay) {
    /* Only cards in the foreground overlay may be inspected. */
    card_tooltip_clear();
    if (combo_player >= 0) {
        combos_draw_details(ba, gs, combo_player, combo_page);
        return;
    }
    if (gs->phase == PHASE_DEALER_SELECT) board_draw_dealer_select(ba, gs, vm);
    else if (gs->phase == PHASE_HAND_CHOICE || gs->phase == PHASE_DRAW_CHOICE) screens_draw_choice_overlay(ba, gs, vm, gs->phase == PHASE_HAND_CHOICE);
    else if (gs->phase == PHASE_DECIDE && gs->current_player == 0) screens_draw_decide_overlay(ba, gs, vm);
    else if (gs->phase == PHASE_ROUND_END) screens_draw_round_result(ba, gs, vm);
    if (paused) {
        card_tooltip_clear();
        screens_draw_pause(vm, c_restart, replay);
    }
}

static void update_draw_frame(void) {
    frame_count++;
    float dt = fminf(0.05f, GetFrameTime());

    if (transition_dir > 0) {
        transition_alpha += dt * 2.5f;
        if (transition_alpha >= 1.0f) {
            transition_alpha = 1.0f; transition_dir = -1; screen = transition_target_screen;
            combo_player = -1; combo_page = 0;
            if (transition_action == 1) {
                if (settings.difficulty == DIFFICULTY_DEUS) { settings.ruleset = RULESET_YAKUZA; settings.background = 5; assets.current_bg = 5; theme_set(5); }
                anim_init(&anim); memset(&drag, 0, sizeof(drag)); hovered_idx = -1; is_paused = 0; confirm_restart = 0; human_turn_started = 0; deus_banner_timer = 0.0f;
                uint32_t match_seed = (uint32_t)time(NULL);
                engine_init(&g, settings.ruleset, settings.rounds, match_seed);
#ifndef __EMSCRIPTEN__
                /* From here to the match end every decision of both players is written down,
                   so the finished match can be replayed (see docs/reference/modes.md). */
                replay_rec_begin(match_seed, ai_rng, settings.ruleset, settings.rounds, settings.difficulty,
                                 settings.master_timer, settings.current_opponent);
#endif
                ai_set_opponent(settings.current_opponent); taunt_init();
                taunt_trigger(campaign_get_opponent(settings.current_opponent)->taunt_text, 4.0f);
                audio_start_match_music(&audio, transition_track);
            } else if (transition_action == 2 || transition_action == 3) audio_play_menu_music(&audio);
        }
    } else if (transition_dir < 0) {
        transition_alpha -= dt * 2.5f;
        if (transition_alpha <= 0.0f) { transition_alpha = 0.0f; transition_dir = 0; }
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
    int in_game = (screen == SCREEN_GAME) || (transition_dir != 0 && transition_target_screen == SCREEN_GAME);
    int audio_test = screen == SCREEN_OPTIONS ? screens_audio_test_state() : -1;
    if (audio_test >= 0) {
        audio_preview_effect(&audio, audio_test);
    } else if (in_game) {
        if (audio.current_music != 2) audio_start_match_music(&audio, transition_track > 0 ? transition_track : ((rand() % 10) + 1));
        else if (audio_is_match_music_finished(&audio)) audio_start_match_music(&audio, ((audio.current_game_track + 1 + (rand() % 9)) % 10) + 1);
    } else {
        audio_set_reverb(0.0f);
        audio_play_menu_music(&audio);
    }

    if (saved_timer > 0.0f) saved_timer -= dt;
    if (log_timer > 0.0f) log_timer -= dt;
    if (deus_banner_timer > 0.0f) deus_banner_timer -= dt;

    int cur_w = GetScreenWidth(), cur_h = GetScreenHeight();
    if (cur_w < 960 || cur_h < 540) {
        card_tooltip_begin(0, (Vector2){ 0, 0 });
        card_tooltip_draw();
        BeginDrawing(); ClearBackground(BLACK);
        DrawTextEx(font_body, "Enlarge the window to play (minimum 960x540)", (Vector2){ 20.0f, 20.0f }, 20.0f, 1.5f, COLOR_PHOSPHOR);
        EndDrawing(); return;
    }

    Letterbox l = input_get_letterbox();
    Vector2 vm = input_vmouse(l);
    ui_theme_set_mouse(vm);

    if (transition_dir == 0) {
        if (screen == SCREEN_START) {
            if (campaign_overlay_open) {
                int cres = campaign_handle_click(&settings, &settings.current_opponent, vm);
                if (cres == -1) { campaign_overlay_open = 0; audio_play_click(&audio); }
                else if (cres == 1) {
                    campaign_overlay_open = 0; ai_set_opponent(settings.current_opponent); storage_save_settings(&settings);
                    int trk = (rand() % 10) + 1; audio_start_match_music(&audio, trk); start_transition(SCREEN_GAME, 1, trk);
                } else if (cres == 2 || cres == 3) audio_play_click(&audio);
            } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                ScreenAction act = screens_handle_start_click(vm);
                if (act != ACT_NONE) audio_play_click(&audio);
                if (act == ACT_START_GAME) campaign_overlay_open = 1;
                else if (act == ACT_GOTO_OPTIONS) screen = SCREEN_OPTIONS;
                else if (act == ACT_GOTO_STATS) screen = SCREEN_STATS;
                else if (act == ACT_GOTO_CREDITS) screen = SCREEN_CREDITS;
                else if (act == ACT_SAVE) { storage_save_all(&settings, &stats, &history); saved_timer = 2.0f; }
                else if (act == ACT_QUIT) { quit_requested = 1; return; }
            }
        } else if (screen == SCREEN_CREDITS && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (screens_handle_credits_click(vm) == ACT_GOTO_MENU) { audio_play_click(&audio); screen = SCREEN_START; }
        } else if (screen == SCREEN_OPTIONS) {
            if (screens_handle_options_click(&settings, &assets, &audio, vm) == ACT_GOTO_MENU) screen = SCREEN_START;
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
                    if (act == ACT_GOTO_MENU) screen = SCREEN_START;
                    else if (act == ACT_SAVE_LOG) { storage_write_report(&stats, &history); log_timer = 2.0f; }
                    else if (act == ACT_RESET_STATS) storage_reset_stats(&stats);
                }
            }
#ifndef __EMSCRIPTEN__
            if (ui_replay_take_start(&g)) {
                anim_init(&anim); motion_init(); memset(&drag, 0, sizeof(drag)); hovered_idx = -1;
                taunt_init(); deus_banner_timer = 0.0f;
                is_paused = 0; confirm_restart = 0;
                motion_deal_round(&g, 0.45f);           /* the deal is shown, not snapped */
                cinematic_start(g.current_round);
                last_replay_round = g.current_round;
                replay_beat = 0.0f;
                screen = SCREEN_GAME;
            }
#endif
        } else if (screen == SCREEN_MATCH_RESULT && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            ScreenAction act = screens_handle_match_result_click(vm);
            if (act != ACT_NONE) audio_play_click(&audio);
            if (act == ACT_START_GAME) { int trk = (rand() % 10) + 1; audio_start_match_music(&audio, trk); start_transition(SCREEN_GAME, 1, trk); }
            else if (act == ACT_GOTO_MENU) start_transition(SCREEN_START, 3, 0);
        } else if (screen == SCREEN_GAME && combo_player >= 0) {
            /* Own the entire frame, including dismissal: no timer, CPU, replay,
               animation, or underlying button can advance through this window. */
            if (combos_handle_details(&g, combo_player, &combo_page, vm)) {
                combo_player = -1; is_paused = 0; audio_play_click(&audio);
            }
        } else if (screen == SCREEN_GAME && !is_paused && !drag.on &&
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
                    int pb_state = replay_pb_tick(&g, dt);
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
            int pause_toggled = 0;
            if (((IsKeyPressed(KEY_ESCAPE) && (!drag.selected || is_paused)) || IsKeyPressed(KEY_P)) && !drag.on) { is_paused = !is_paused; confirm_restart = 0; pause_toggled = 1; }
            else if (IsKeyPressed(KEY_R) && !drag.on) { is_paused = 1; confirm_restart = 1; pause_toggled = 1; }
            else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !drag.on) {
                if (CheckCollisionPointRec(vm, (Rectangle){ 1160, 560, 112, 40 })) { audio_play_click(&audio); is_paused = !is_paused; confirm_restart = 0; pause_toggled = 1; }
                else if (CheckCollisionPointRec(vm, (Rectangle){ 1160, 610, 112, 40 })) { audio_play_click(&audio); is_paused = 1; confirm_restart = 1; pause_toggled = 1; }
            }

            if (is_paused && !pause_toggled) {
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    ScreenAction act = screens_handle_pause_click(vm, &confirm_restart, 0);
                    if (act != ACT_NONE) audio_play_click(&audio);
                    if (act == ACT_RESUME) { is_paused = 0; confirm_restart = 0; }
                    else if (act == ACT_RESTART_MATCH) {
                        int trk = (rand() % 10) + 1; audio_start_match_music(&audio, trk);
                        is_paused = 0; confirm_restart = 0; start_transition(SCREEN_GAME, 1, trk);
                    }
                    else if (act == ACT_ABANDON_MATCH) { replay_rec_abort(); is_paused = 0; confirm_restart = 0; start_transition(SCREEN_START, 3, 0); }
                    else if (!CheckCollisionPointRec(vm, (Rectangle){ 440, 180, 400, 360 })) { is_paused = 0; confirm_restart = 0; }
                }
            } else if (!is_paused && !pause_toggled) {
                if (g.hitlag_timer > 0.0f) {
                    g.hitlag_timer -= dt;
                    if (g.hitlag_timer < 0.0f) g.hitlag_timer = 0.0f;
                } else {
                    anim_update(&anim, dt);
                    motion_update(dt, (drag.on && drag.idx >= 0 && drag.idx < g.hand_count[0]) ? g.hand[0][drag.idx] : -1);
                    cinematic_update(dt);

                    int input_unlocked = !anim_is_blocking(&anim) && !cinematic_is_active() && !motion_is_reflowing(&g);

                    if (g.phase == PHASE_DEALER_SELECT) {
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

                    if (g.current_player == 1 && g.phase == PHASE_TURN_HAND && !cinematic_is_active() && !motion_is_reflowing(&g)) {
                        if (anim.active && anim.type == ANIM_CPU_FLY && anim.elapsed >= anim.duration) {
                            int card = anim.cpu_card, target = anim.cpu_target;
                            Vector2 landed = anim.target_pos;
                            anim.active = 0; audio_play_drop(&audio);
                            motion_set_instant(card, landed, (Vector2){ 76.0f, 116.0f });
                            engine_play_hand(&g, card, target);
                            replay_rec_play(1, card, target);
                            if (g.last_captured_count > 0) audio_play_capture(&audio);
                        } else if (anim.active && anim.type == ANIM_CPU_THINK && anim.elapsed >= anim.duration) {
                            anim.active = 0; int card, target;
                            ai_choose_hand(&g, settings.difficulty, &ai_rng, &card, &target);
                            Vector2 from = { 710.0f, 12.0f };
                            Vector2 to = (target >= 0) ? board_get_card_field_pos(&g, target) : (Vector2){ 710.0f, 240.0f };
                            anim_start_cpu_fly(&anim, card, target, from, to, get_flight_duration(settings.anim_speed));
                        } else if (!anim.active) {
                            anim_start_cpu_think(&anim, get_think_duration(settings.anim_speed));
                        }
                    }

                    if (g.phase == PHASE_TURN_DRAW && !cinematic_is_active() && !motion_is_reflowing(&g)) {
                        if (anim.active && anim.type == ANIM_STOCK_FLY && anim.elapsed >= anim.duration) {
                            int drawn_cid = anim.card_id;
                            Vector2 landed = anim.target_pos;
                            anim.active = 0; audio_play_drop(&audio);
                            motion_set_instant(drawn_cid, landed, (Vector2){ 76.0f, 116.0f });
                            engine_reveal_stock(&g);
                            if (g.last_captured_count > 0) audio_play_capture(&audio);
                        } else if (!anim.active) {
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
                            } else { engine_reveal_stock(&g); }
                        }
                    }

                    if (g.current_player == 1 && g.phase == PHASE_DRAW_CHOICE && input_unlocked) {
                        int target = ai_choose_stock_target(&g, settings.difficulty, &ai_rng);
                        engine_pick_draw_choice(&g, target);
                        replay_rec_pick(1, target);
                        if (g.last_captured_count > 0) audio_play_capture(&audio);
                    }

                    if (g.current_player == 1 && g.phase == PHASE_DECIDE && input_unlocked) {
                        DecisionType dec = ai_choose_decision(&g, settings.difficulty, &ai_rng);
                        if (dec == DECISION_KOI_KOI) {
                            int cpu_streak = koi_caller_multiplier(g.koi_called[1] + 1); /* streak after this call */
                            audio_play_koi(&audio); anim_start_banner(&anim, TextFormat("CPU: KOI-KOI  x%d", cpu_streak), 1.5f);
                            taunt_trigger(campaign_get_opponent(settings.current_opponent)->koi_taunt, 4.0f);
                        }
                        engine_decide(&g, dec);
                        replay_rec_koi(1, dec == DECISION_KOI_KOI);
                    }

                    if (g.current_player != 0 || (g.phase != PHASE_TURN_HAND && g.phase != PHASE_HAND_CHOICE)) human_turn_started = 0;

                    if (g.current_player == 0 && input_unlocked) {
                        if (g.phase == PHASE_TURN_HAND) {
                            if (settings.difficulty == DIFFICULTY_MASTER || settings.difficulty == DIFFICULTY_DEUS) {
                                if (!human_turn_started) {
                                    g.player_timer = (settings.difficulty == DIFFICULTY_DEUS || settings.master_timer == 15) ? 15.0f : 30.0f;
                                    human_turn_started = 1;
                                }
                                g.player_timer -= dt;
                                if (g.player_timer <= 0.0f) {
                                    if (settings.difficulty == DIFFICULTY_DEUS) {
                                        /* DEUS SUPREMUS only: timer 0 = GAME OVER. No points, no win,
                                           no stats, no record - straight back to the start screen. */
                                        g.player_timer = 0.0f; drag.on = 0; drag.idx = -1; drag.selected = 0; human_turn_started = 0;
                                        audio_play_lose(&audio);
                                        deus_banner_timer = 3.0f;
                                        start_transition(SCREEN_START, 3, 0);
                                    } else {
                                        g.player_timer = 0.0f; drag.on = 0; drag.idx = -1; drag.selected = 0;
                                        engine_timeout_autoplay(&g);
                                        replay_rec_timeout(0);
                                        if (g.last_captured_count > 0) audio_play_capture(&audio); else audio_play_drop(&audio);
                                        anim_start_banner(&anim, "! TIMEOUT !", 1.0f); human_turn_started = 0;
                                    }
                                }
                            }
                            if (g.phase == PHASE_TURN_HAND) {
                                int hand_before = g.hand_count[0];
                                input_handle_mouse(&g, &drag, &anim, l, vm, &hovered_idx, 1);
                                if (g.hand_count[0] < hand_before) {
                                    if (g.last_captured_count > 0) audio_play_capture(&audio);
                                    else audio_play_drop(&audio);
                                } else if (g.phase == PHASE_HAND_CHOICE) {
                                    audio_play_drop(&audio);
                                }
                            }
                        }
                        else if (g.phase == PHASE_HAND_CHOICE && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                            int tid; int res = screens_check_choice_click(&g, vm, 1, &tid);
                            if (res == 1) { audio_play_drop(&audio); engine_pick_hand_choice(&g, tid); replay_rec_pick(0, tid); if (g.last_captured_count > 0) audio_play_capture(&audio); }
                            else if (res == 2) { audio_play_click(&audio); engine_cancel_hand_choice(&g); replay_rec_cancel(0); }
                        } else if (g.phase == PHASE_DRAW_CHOICE && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                            int tid; if (screens_check_choice_click(&g, vm, 0, &tid) == 1) {
                                audio_play_drop(&audio); engine_pick_draw_choice(&g, tid); replay_rec_pick(0, tid); if (g.last_captured_count > 0) audio_play_capture(&audio);
                            }
                        } else if (g.phase == PHASE_DECIDE && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                            DecisionType dec;
                            if (screens_check_decide_click(vm, &dec)) {
                                audio_play_click(&audio); if (dec == DECISION_KOI_KOI) audio_play_koi(&audio);
                                engine_decide(&g, dec); replay_rec_koi(0, dec == DECISION_KOI_KOI);
                            }
                        }
                    }

                    if (g.phase == PHASE_ROUND_END && input_unlocked && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        if (screens_check_round_result_click(vm)) {
                            audio_play_click(&audio);
                            if (g.current_round >= g.total_rounds) {
                                anim_init(&anim); engine_next_round(&g);
                                storage_record_match(&g, &settings, &stats, &history);
#ifndef __EMSCRIPTEN__
                                ui_replay_commit_auto(&history);
#endif
                                if (g.total_score[0] > g.total_score[1]) {
                                    audio_play_win(&audio);
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
                                } else if (g.total_score[0] < g.total_score[1]) audio_play_lose(&audio);
                                start_transition(SCREEN_MATCH_RESULT, 2, 0);
                            } else {
                                anim_init(&anim); engine_next_round(&g); motion_deal_round(&g, 0.45f);
                                cinematic_start(g.current_round);
                                taunt_trigger(campaign_get_opponent(settings.current_opponent)->taunt_text, 3.5f);
                            }
                        }
                    }
                }
            }
        }
    }

    if (g.phase != PHASE_TURN_HAND || g.current_player != 0) drag.selected = 0;
    int replay_running = replay_pb_active();
    int overlay_active = (screen == SCREEN_GAME) &&
                         (replay_running ? is_paused :
                          (g.phase == PHASE_HAND_CHOICE || g.phase == PHASE_DRAW_CHOICE ||
                          (g.phase == PHASE_DECIDE && g.current_player == 0) || g.phase == PHASE_DEALER_SELECT ||
                          g.phase == PHASE_ROUND_END || is_paused));
    if (overlay_active) {
        if (g.phase != last_overlay_phase || is_paused != last_overlay_paused) {
            overlay_alpha = (g.phase == PHASE_DEALER_SELECT) ? 1.0f : 0.0f;
            last_overlay_phase = g.phase; last_overlay_paused = is_paused;
        }
        overlay_alpha = fminf(1.0f, overlay_alpha + dt / 0.50f);
    } else {
        overlay_alpha = 0.0f; last_overlay_phase = g.phase; last_overlay_paused = is_paused;
    }

    if (screen == SCREEN_GAME && !replay_pb_active() && !is_paused) taunt_update(dt);

    card_tooltip_begin(screen == SCREEN_GAME && !replay_pb_active() && settings.difficulty == DIFFICULTY_EASY &&
                       !is_paused && !drag.on && !cinematic_is_active() &&
                       transition_dir == 0 && transition_alpha <= 0.0f &&
                       !(anim.active && anim.type == ANIM_BANNER) &&
                       IsCursorOnScreen() && IsWindowFocused(), vm);
    BeginTextureMode(canvas);
    if (screen == SCREEN_START) {
        screens_draw_start(&assets, &stats, vm, saved_timer);
        if (campaign_overlay_open) campaign_draw_overlay(&assets, &settings, settings.current_opponent, vm);
    }
    else if (screen == SCREEN_OPTIONS) screens_draw_options(&assets, &settings, vm);
    else if (screen == SCREEN_STATS) screens_draw_stats(&assets, &stats, &settings, &history, vm, confirm_reset, log_timer);
    else if (screen == SCREEN_CREDITS) screens_draw_credits(&assets, vm);
    else if (screen == SCREEN_MATCH_RESULT) screens_draw_match_result(&assets, &g, &settings, vm);
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
        else if (cinematic_is_active()) status = "";
        else if (anim.active && anim.type == ANIM_STOCK_FLY) {
            snprintf(cpu_status, sizeof(cpu_status), "DRAW STOCK: %s", CARDS[anim.card_id].name); status = cpu_status;
        } else if (g.phase == PHASE_TURN_HAND) {
            status = (g.current_player == 0) ? (drag.selected ? "SELECTED - CLICK A MATCH OR EMPTY FIELD / ESC TO CANCEL" : "YOUR TURN - DRAG OR CLICK A CARD TO SELECT") :
                     (anim.active && anim.type == ANIM_CPU_FLY) ? "CPU PLAYS CARD" : "CPU IS THINKING...";
        } else if (g.phase == PHASE_HAND_CHOICE || g.phase == PHASE_DRAW_CHOICE) status = "CHOOSE ONE OF THE MATCHING CARDS";
        else if (g.phase == PHASE_DECIDE) status = (g.current_player == 0) ? "CALL KOI-KOI TO CONTINUE OR SHOBU TO SCORE" : "";

        board_draw_table(&assets, &g, &anim, hovered_idx, drag.on ? drag.idx : -1, drag.pos,
                         replay_running ? (replay_pb_difficulty() == DIFFICULTY_EASY) : (settings.difficulty == DIFFICULTY_EASY),
                         replay_running, status);
        if (!replay_pb_active()) { input_draw_selection(&g, &drag); taunt_draw(); }

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
        if (anim.active && anim.type == ANIM_BANNER) screens_draw_banner(anim.banner_text);
        cinematic_draw();   /* a replay shows the round deal too */
        if (overlay_active && overlay_alpha >= 0.999f) draw_active_game_overlays(&assets, &g, vm, is_paused, confirm_restart, replay_pb_active());
    }
    EndTextureMode();

    if (overlay_active && overlay_alpha > 0.0f && overlay_alpha < 0.999f) {
        BeginTextureMode(overlay_canvas); ClearBackground(BLANK);
        draw_active_game_overlays(&assets, &g, vm, is_paused, confirm_restart, replay_pb_active());
        EndTextureMode();
        BeginTextureMode(canvas);
        DrawTextureRec(overlay_canvas.texture, (Rectangle){ 0, 0, (float)CANVAS_W, -(float)CANVAS_H }, (Vector2){ 0, 0 }, (Color){ 255, 255, 255, (unsigned char)(overlay_alpha * 255.0f) });
        EndTextureMode();
    }
    if (transition_alpha > 0.0f) {
        BeginTextureMode(canvas);
        DrawRectangle(0, 0, CANVAS_W, CANVAS_H, (Color){ 0, 0, 0, (unsigned char)(fminf(1.0f, fmaxf(0.0f, transition_alpha)) * 255.0f) });
        EndTextureMode();
    }
    /* DEUS timeout banner: big red transparent text, drawn above everything so it
       hits the match field and rides the fade back to the start screen */
    if (deus_banner_timer > 0.0f) {
        BeginTextureMode(canvas);
        float t = fminf(1.0f, deus_banner_timer / 0.6f);              /* fade-out tail */
        unsigned char a = (unsigned char)(t * 200.0f);                /* ~78% - transparent */
        const char *msg = "/// TIME EXPIRED ///";
        Vector2 ts = MeasureTextEx(font_title, msg, 56.0f, 1.6f);
        DrawTextEx(font_title, msg, (Vector2){ floorf((CANVAS_W - ts.x) / 2.0f), 330.0f }, 56.0f, 1.6f, (Color){ 0xFF, 0x30, 0x30, a });
        EndTextureMode();
    }

    BeginTextureMode(canvas);
    card_tooltip_draw();
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
    theme_set(settings.terminal_color);
    audio_init(&audio, settings.bgm_volume, settings.sfx_volume);
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

    font_body = LoadFontEx("assets/play.ttf", 36, NULL, 0);
    if (!IsFontReady(font_body)) font_body = LoadFontEx(TextFormat("%s/assets/play.ttf", GetApplicationDirectory() ? GetApplicationDirectory() : ""), 36, NULL, 0);
    if (!IsFontReady(font_body)) font_body = GetFontDefault();
    else { GenTextureMipmaps(&font_body.texture); SetTextureFilter(font_body.texture, TEXTURE_FILTER_BILINEAR); }
    small_font = font_body;

    storage_load_stats(&stats); storage_load_history(&history);
    screen = (auto_start || auto_dealer) ? SCREEN_GAME : auto_options ? SCREEN_OPTIONS : auto_stats ? SCREEN_STATS : SCREEN_START;
    engine_init(&g, settings.ruleset, settings.rounds, seed);
    motion_init(); taunt_init(); ai_set_opponent(settings.current_opponent);
    if (auto_start) {
        engine_start_round(&g); motion_deal_round(&g, 0.45f); cinematic_start(g.current_round);
        audio_start_match_music(&audio, (rand() % 10) + 1);
        taunt_trigger(campaign_get_opponent(settings.current_opponent)->taunt_text, 4.0f);
    }

    anim_init(&anim); memset(&drag, 0, sizeof(drag));
    ai_rng = seed + 999; campaign_overlay_open = auto_campaign;

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(update_draw_frame, 0, 1);
#else
    while (!WindowShouldClose() && !quit_requested) update_draw_frame();
#endif

    audio_close(&audio);
    if (font_title.texture.id != GetFontDefault().texture.id) UnloadFont(font_title);
    if (font_body.texture.id != GetFontDefault().texture.id) UnloadFont(font_body);
    board_unload_assets(&assets);
    if (post_shader.id > 0) UnloadShader(post_shader);
    UnloadRenderTexture(overlay_canvas); UnloadRenderTexture(canvas);
    CloseWindow();
    return 0;
}
