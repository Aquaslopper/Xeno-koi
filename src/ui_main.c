#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "raylib.h"
#include "engine.h"
#include "ai.h"
#include "storage.h"
#include "ui_theme.h"
#include "ui_board.h"
#include "ui_anim.h"
#include "ui_motion.h"
#include "ui_audio.h"
#include "ui_screens.h"
#include "ui_input.h"

static float transition_alpha = 0.0f;
static int transition_dir = 0;
static ScreenState transition_target_screen = SCREEN_START;
static int transition_action = 0;
static int transition_track = 1;

static void start_transition(ScreenState target, int action, int track) {
    transition_dir = 1;
    transition_target_screen = target;
    transition_action = action;
    transition_track = track;
}

static float get_flight_duration(int s) { return (s == 0) ? 0.65f : (s == 1) ? 0.50f : 0.40f; }
static float get_think_duration(int s)  { return (s == 0) ? 0.40f : (s == 1) ? 0.25f : 0.15f; }

static void record_match_stats(const GameState *g, const GameSettings *s, GameStats *st, MatchHistory *h) {
    st->matches_played++;
    if (s->difficulty == DIFFICULTY_HARD) st->hard_matches++; else st->easy_matches++;
    int you = g->total_score[0], cpu = g->total_score[1];
    st->points_for += you; st->points_against += cpu;
    const char *outcome = (you > cpu) ? (st->matches_won++, st->current_streak++, "WIN") :
                          (you < cpu) ? (st->matches_lost++, st->current_streak = 0, "LOSS") :
                          (st->matches_drawn++, "DRAW");
    if (st->current_streak > st->best_streak) st->best_streak = st->current_streak;
    for (int i = 0; i < g->ledger_count; i++) {
        if (g->ledger[i].winner == 0) st->rounds_won_you++;
        else if (g->ledger[i].winner == 1) st->rounds_won_cpu++;
        if (g->ledger[i].award > st->best_award) st->best_award = g->ledger[i].award;
    }
    for (int y = 0; y < YAKU_COUNT; y++) st->yaku_counts[y] += g->human_yaku_counts[y];
    st->koi_calls_you += g->koi_called[0]; st->koi_calls_cpu += g->koi_called[1];
    time_t now = time(NULL);
    char time_str[32], record_line[160];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", localtime(&now));
    const char *r_str = (s->ruleset == RULESET_FUDAWIKI) ? "fudawiki" :
                        (s->ruleset == RULESET_KK_ADD_10) ? "kk-add-10" :
                        (s->ruleset == RULESET_TOURNAMENT) ? "tournament" :
                        (s->ruleset == RULESET_YAKUZA) ? "yakuza" : "nintendo";
    const char *d_str = (s->difficulty == DIFFICULTY_HARD) ? "hard" : "easy";
    snprintf(record_line, sizeof(record_line), "%s;rounds=%d;%s;%s;you=%d;cpu=%d;%s",
             time_str, s->rounds, r_str, d_str, you, cpu, outcome);
    storage_add_history(h, record_line);
    storage_save_stats(st);
    storage_write_report(st, h);
}

int main(int argc, char **argv) {
    uint32_t seed = 7;
    int shot_frame = -1, win_w = 1280, win_h = 720, auto_start = 0, auto_options = 0, auto_dealer = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) seed = (uint32_t)atoi(argv[++i]);
        else if (strcmp(argv[i], "--shot-frame") == 0 && i + 1 < argc) shot_frame = atoi(argv[++i]);
        else if (strcmp(argv[i], "--window") == 0 && i + 1 < argc) sscanf(argv[++i], "%dx%d", &win_w, &win_h);
        else if (strcmp(argv[i], "--autostart") == 0) auto_start = 1;
        else if (strcmp(argv[i], "--options") == 0) auto_options = 1;
        else if (strcmp(argv[i], "--dealer") == 0) auto_dealer = 1;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(win_w, win_h, "[XENO/KOI] (Giger Biomechanical Edition)");
    SetWindowMinSize(960, 540);
    SetTargetFPS(60);

    GameSettings settings;
    storage_load_settings(&settings);
    theme_set(settings.terminal_color);
    AudioAssets audio;
    audio_init(&audio, settings.bgm_volume, settings.sfx_volume);
    RenderTexture2D canvas = LoadRenderTexture(CANVAS_W, CANVAS_H);
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_POINT);
    RenderTexture2D overlay_canvas = LoadRenderTexture(CANVAS_W, CANVAS_H);

    BoardAssets assets;
    board_load_assets(&assets);
    assets.current_bg = settings.background;
    GameStats stats;
    storage_load_stats(&stats);
    MatchHistory history;
    storage_load_history(&history);

    ScreenState screen = (auto_start || auto_dealer) ? SCREEN_GAME : auto_options ? SCREEN_OPTIONS : SCREEN_START;
    GameState g;
    engine_init(&g, settings.ruleset, settings.rounds, seed);
    motion_init();
    if (auto_start) {
        engine_start_round(&g);
        motion_deal_round(&g, 0.45f);
        cinematic_start(g.current_round);
        audio_start_match_music(&audio, (rand() % 5) + 1);
    }

    AnimState anim;
    anim_init(&anim);
    DragState drag;
    memset(&drag, 0, sizeof(drag));

    uint32_t ai_rng = seed + 999;
    int is_paused = 0, confirm_restart = 0, confirm_reset = 0;
    float saved_timer = 0.0f, log_timer = 0.0f, overlay_alpha = 0.0f;
    GamePhase last_overlay_phase = PHASE_NONE;
    int last_overlay_paused = 0, hovered_idx = -1, frame_count = 0;

    while (!WindowShouldClose()) {
        frame_count++;
        float dt = GetFrameTime();
        if (dt > 0.05f) dt = 0.05f;

        if (transition_dir > 0) {
            transition_alpha += dt * 2.5f;
            if (transition_alpha >= 1.0f) {
                transition_alpha = 1.0f; transition_dir = -1;
                screen = transition_target_screen;
                if (transition_action == 1) {
                    anim_init(&anim); memset(&drag, 0, sizeof(drag));
                    hovered_idx = -1; is_paused = 0; confirm_restart = 0;
                    engine_init(&g, settings.ruleset, settings.rounds, (uint32_t)time(NULL));
                    audio_start_match_music(&audio, transition_track);
                } else if (transition_action == 2 || transition_action == 3) {
                    audio_play_menu_music(&audio);
                }
            }
        } else if (transition_dir < 0) {
            transition_alpha -= dt * 2.5f;
            if (transition_alpha <= 0.0f) { transition_alpha = 0.0f; transition_dir = 0; }
        }

        audio_set_bgm_volume(&audio, settings.bgm_volume * (1.0f - transition_alpha));
        audio_update(&audio);

        int in_game_audio = (screen == SCREEN_GAME) || (transition_dir != 0 && transition_target_screen == SCREEN_GAME);
        if (in_game_audio) {
            if (audio.current_music != 2) {
                audio_start_match_music(&audio, transition_track > 0 ? transition_track : ((rand() % 5) + 1));
            } else if (audio_is_match_music_finished(&audio)) {
                audio_start_match_music(&audio, ((audio.current_game_track + 1 + (rand() % 4)) % 5) + 1);
            }
        } else {
            audio_play_menu_music(&audio);
        }

        if (saved_timer > 0.0f) saved_timer -= dt;
        if (log_timer > 0.0f) log_timer -= dt;

        int cur_w = GetScreenWidth(), cur_h = GetScreenHeight();
        if (cur_w < 960 || cur_h < 540) {
            BeginDrawing(); ClearBackground(BLACK);
            DrawText("Enlarge the window to play (minimum 960x540)", 20, 20, 20, COLOR_PHOSPHOR);
            EndDrawing(); continue;
        }

        Letterbox l = input_get_letterbox();
        Vector2 vm = input_vmouse(l);

        if (transition_dir == 0) {
            if (screen == SCREEN_START && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                ScreenAction act = screens_handle_start_click(vm);
                if (act != ACT_NONE) audio_play_click(&audio);
                if (act == ACT_START_GAME) {
                    int trk = (rand() % 5) + 1;
                    audio_start_match_music(&audio, trk);
                    start_transition(SCREEN_GAME, 1, trk);
                }
                else if (act == ACT_GOTO_OPTIONS) screen = SCREEN_OPTIONS;
                else if (act == ACT_GOTO_STATS) screen = SCREEN_STATS;
                else if (act == ACT_GOTO_CREDITS) screen = SCREEN_CREDITS;
                else if (act == ACT_SAVE) { storage_save_all(&settings, &stats, &history); saved_timer = 2.0f; }
                else if (act == ACT_QUIT) break;
            } else if (screen == SCREEN_CREDITS && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (screens_handle_credits_click(vm) == ACT_GOTO_MENU) {
                    audio_play_click(&audio);
                    screen = SCREEN_START;
                }
            } else if (screen == SCREEN_OPTIONS && (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_LEFT))) {
                if (screens_handle_options_click(&settings, &assets, &audio, vm) == ACT_GOTO_MENU) screen = SCREEN_START;
            } else if (screen == SCREEN_STATS && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                ScreenAction act = screens_handle_stats_click(vm, &confirm_reset);
                if (act != ACT_NONE) audio_play_click(&audio);
                if (act == ACT_GOTO_MENU) screen = SCREEN_START;
                else if (act == ACT_SAVE_LOG) { storage_write_report(&stats, &history); log_timer = 2.0f; }
                else if (act == ACT_RESET_STATS) storage_reset_stats(&stats);
            } else if (screen == SCREEN_MATCH_RESULT && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                ScreenAction act = screens_handle_match_result_click(vm);
                if (act != ACT_NONE) audio_play_click(&audio);
                if (act == ACT_START_GAME) {
                    int trk = (rand() % 5) + 1;
                    audio_start_match_music(&audio, trk);
                    start_transition(SCREEN_GAME, 1, trk);
                }
                else if (act == ACT_GOTO_MENU) start_transition(SCREEN_START, 3, 0);
            } else if (screen == SCREEN_GAME) {
                Rectangle pb = { 1160, 560, 112, 40 }, rb = { 1160, 610, 112, 40 };
                int pause_toggled = 0;
                if ((IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) && !drag.on) {
                    is_paused = !is_paused; confirm_restart = 0; pause_toggled = 1;
                } else if (IsKeyPressed(KEY_R) && !drag.on) {
                    is_paused = 1; confirm_restart = 1; pause_toggled = 1;
                }
                if (!pause_toggled && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !drag.on) {
                    if (CheckCollisionPointRec(vm, pb)) { audio_play_click(&audio); is_paused = !is_paused; confirm_restart = 0; pause_toggled = 1; }
                    else if (CheckCollisionPointRec(vm, rb)) { audio_play_click(&audio); is_paused = 1; confirm_restart = 1; pause_toggled = 1; }
                }

                if (is_paused && !pause_toggled) {
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        ScreenAction act = screens_handle_pause_click(vm, &confirm_restart);
                        if (act != ACT_NONE) audio_play_click(&audio);
                        if (act == ACT_RESUME) { is_paused = 0; confirm_restart = 0; }
                        else if (act == ACT_RESTART_MATCH) {
                            int trk = (rand() % 5) + 1;
                            audio_start_match_music(&audio, trk);
                            is_paused = 0; confirm_restart = 0;
                            start_transition(SCREEN_GAME, 1, trk);
                        }
                        else if (act == ACT_ABANDON_MATCH) { is_paused = 0; confirm_restart = 0; start_transition(SCREEN_START, 3, 0); }
                        else if (!CheckCollisionPointRec(vm, (Rectangle){ 440, 180, 400, 360 })) { is_paused = 0; confirm_restart = 0; }
                    }
                } else if (!is_paused) {
                    if (g.hitlag_timer > 0.0f) {
                        g.hitlag_timer -= dt;
                        if (g.hitlag_timer < 0.0f) g.hitlag_timer = 0.0f;
                    } else {
                        anim_update(&anim, dt);
                        motion_update(dt);
                        cinematic_update(dt);

                        int input_unlocked = !anim_is_blocking(&anim) && !cinematic_is_active() && !motion_is_reflowing(&g);

                        if (g.phase == PHASE_DEALER_SELECT) {
                        if (g.oya_player_choice < 0) {
                            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                                int pick = board_check_dealer_select_click(&g, vm);
                                if (pick >= 0) {
                                    g.oya_player_choice = pick;
                                    g.oya_timer = 0.0f;
                                    int p_cid = g.oya_cards[pick], c_cid = g.oya_cards[1 - pick];
                                    int mp = CARDS[p_cid].month, mc = CARDS[c_cid].month;
                                    if (mp < mc) g.oya_winner = 0;
                                    else if (mc < mp) g.oya_winner = 1;
                                    else g.oya_winner = (CARDS[p_cid].dealer_rank >= CARDS[c_cid].dealer_rank) ? 0 : 1;
                                    g.dealer = g.oya_winner;
                                    audio_play_flip(&audio);
                                }
                            }
                        } else {
                            float prev_t = g.oya_timer;
                            g.oya_timer += dt;
                            if (prev_t < 0.45f && g.oya_timer >= 0.45f) audio_play_flip(&audio);
                            if (prev_t < 0.90f && g.oya_timer >= 0.90f) audio_play_click(&audio);
                            if (g.oya_timer >= 3.4f) {
                                engine_start_round(&g);
                                motion_deal_round(&g, 0.45f);
                                cinematic_start(g.current_round);
                                if (audio.current_music != 2) audio_start_match_music(&audio, transition_track);
                            }
                        }
                    }

                    if (g.current_player == 1 && g.phase == PHASE_TURN_HAND && !cinematic_is_active() && !motion_is_reflowing(&g)) {
                        if (anim.active && anim.type == ANIM_CPU_FLY && anim.elapsed >= anim.duration) {
                            anim.active = 0; audio_play_drop(&audio);
                            int card = anim.cpu_card, target = anim.cpu_target;
                            engine_play_hand(&g, card, target);
                            if (g.last_captured_count > 0) {
                                audio_play_capture(&audio);
                            }
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
                            anim.active = 0; audio_play_drop(&audio);
                            engine_reveal_stock(&g);
                            if (g.last_captured_count > 0) {
                                audio_play_capture(&audio);
                            }
                        } else if (!anim.active) {
                            int top_stock = (g.stock_count > 0) ? g.stock[g.stock_count - 1] : -1;
                            if (top_stock >= 0) {
                                audio_play_flip(&audio); int match_target = -1;
                                for (int i = 0; i < g.field_count; i++) {
                                    if (CARDS[g.field[i]].month == CARDS[top_stock].month) { match_target = g.field[i]; break; }
                                }
                                Vector2 to_pos = (match_target >= 0) ? board_get_card_field_pos(&g, match_target) :
                                                (Vector2){ board_get_field_card_rect(g.field_count, g.field_count + 1).x, board_get_field_card_rect(g.field_count, g.field_count + 1).y };
                                anim_start_stock_fly(&anim, top_stock, (Vector2){ 330.0f, 262.0f }, to_pos, get_flight_duration(settings.anim_speed));
                            } else { engine_reveal_stock(&g); }
                        }
                    }

                    if (g.current_player == 1 && g.phase == PHASE_DRAW_CHOICE && input_unlocked) {
                        int target = ai_choose_stock_target(&g, settings.difficulty, &ai_rng);
                        engine_pick_draw_choice(&g, target);
                        if (g.last_captured_count > 0) {
                            audio_play_capture(&audio);
                        }
                    }

                    if (g.current_player == 1 && g.phase == PHASE_DECIDE && input_unlocked) {
                        DecisionType dec = ai_choose_decision(&g, settings.difficulty, &ai_rng);
                        if (dec == DECISION_KOI_KOI) { audio_play_koi(&audio); anim_start_banner(&anim, "CPU: KOI-KOI", 1.5f); }
                        engine_decide(&g, dec);
                    }

                    if (g.current_player == 0 && input_unlocked) {
                        if (g.phase == PHASE_TURN_HAND) {
                            int was_dragging = drag.on;
                            input_handle_mouse(&g, &drag, &anim, l, vm, &hovered_idx, 1);
                            if (was_dragging && !drag.on) {
                                if (g.last_captured_count > 0) audio_play_capture(&audio);
                                else audio_play_drop(&audio);
                            }
                        }
                        else if (g.phase == PHASE_HAND_CHOICE && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                            int target_id; int res = screens_check_choice_click(&g, vm, 1, &target_id);
                            if (res == 1) {
                                audio_play_drop(&audio);
                                engine_pick_hand_choice(&g, target_id);
                                if (g.last_captured_count > 0) audio_play_capture(&audio);
                            } else if (res == 2) { audio_play_click(&audio); engine_cancel_hand_choice(&g); }
                        } else if (g.phase == PHASE_DRAW_CHOICE && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                            int target_id; int res = screens_check_choice_click(&g, vm, 0, &target_id);
                            if (res == 1) {
                                audio_play_drop(&audio);
                                engine_pick_draw_choice(&g, target_id);
                                if (g.last_captured_count > 0) audio_play_capture(&audio);
                            }
                        } else if (g.phase == PHASE_DECIDE && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                            DecisionType dec;
                            if (screens_check_decide_click(vm, &dec)) {
                                audio_play_click(&audio);
                                if (dec == DECISION_KOI_KOI) audio_play_koi(&audio);
                                engine_decide(&g, dec);
                            }
                        }
                    }

                    if (g.phase == PHASE_ROUND_END && input_unlocked && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        if (screens_check_round_result_click(vm)) {
                            audio_play_click(&audio);
                            if (g.current_round >= g.total_rounds) {
                                anim_init(&anim); engine_next_round(&g);
                                record_match_stats(&g, &settings, &stats, &history);
                                if (g.total_score[0] > g.total_score[1]) audio_play_win(&audio);
                                else if (g.total_score[0] < g.total_score[1]) audio_play_lose(&audio);
                                start_transition(SCREEN_MATCH_RESULT, 2, 0);
                            } else {
                                anim_init(&anim); engine_next_round(&g);
                                motion_deal_round(&g, 0.45f);
                                cinematic_start(g.current_round);
                            }
                        }
                    }
                    }
                }
            }
        }

        int overlay_active = (screen == SCREEN_GAME) && (g.phase == PHASE_HAND_CHOICE ||
                              g.phase == PHASE_DRAW_CHOICE ||
                              (g.phase == PHASE_DECIDE && g.current_player == 0) ||
                              g.phase == PHASE_DEALER_SELECT ||
                              g.phase == PHASE_ROUND_END || is_paused);
        if (overlay_active) {
            if (g.phase != last_overlay_phase || is_paused != last_overlay_paused) {
                overlay_alpha = (g.phase == PHASE_DEALER_SELECT) ? 1.0f : 0.0f;
                last_overlay_phase = g.phase; last_overlay_paused = is_paused;
            }
            overlay_alpha += dt / 0.50f;
            if (overlay_alpha > 1.0f) overlay_alpha = 1.0f;
        } else {
            overlay_alpha = 0.0f; last_overlay_phase = g.phase; last_overlay_paused = is_paused;
        }

        BeginTextureMode(canvas);
        if (screen == SCREEN_START) screens_draw_start(&assets, &stats, vm, saved_timer);
        else if (screen == SCREEN_OPTIONS) screens_draw_options(&assets, &settings, vm);
        else if (screen == SCREEN_STATS) screens_draw_stats(&assets, &stats, &history, vm, confirm_reset, log_timer);
        else if (screen == SCREEN_CREDITS) screens_draw_credits(&assets, vm);
        else if (screen == SCREEN_MATCH_RESULT) screens_draw_match_result(&assets, &g, vm);
        else if (screen == SCREEN_GAME) {
            const char *status = ""; char cpu_status[64];
            if (cinematic_is_active()) {
                status = "";
            } else if (anim.active && anim.type == ANIM_STOCK_FLY) {
                snprintf(cpu_status, sizeof(cpu_status), "DRAW STOCK: %s", CARDS[anim.card_id].name); status = cpu_status;
            } else if (g.phase == PHASE_TURN_HAND) {
                status = (g.current_player == 0) ? "YOUR TURN - DRAG A CARD ONTO THE TABLE" :
                         (anim.active && anim.type == ANIM_CPU_FLY) ? "CPU PLAYS CARD" : "CPU IS THINKING...";
            } else if (g.phase == PHASE_HAND_CHOICE || g.phase == PHASE_DRAW_CHOICE) status = "CHOOSE ONE OF THE MATCHING CARDS";
            else if (g.phase == PHASE_DECIDE) status = (g.current_player == 0) ? "CALL KOI-KOI TO CONTINUE OR SHOBU TO SCORE" : "";

            board_draw_table(&assets, &g, &anim, hovered_idx, drag.on ? drag.idx : -1, drag.pos, (settings.difficulty == DIFFICULTY_EASY), status);

            if (!is_paused) {
                Rectangle pb = { 1160, 560, 112, 40 }, rb = { 1160, 610, 112, 40 };
                if (CheckCollisionPointRec(vm, pb)) {
                    DrawRectangleRec(pb, COLOR_BTN_HOVER_BG); DrawRectangleLinesEx(pb, 1.0f, COLOR_PHOSPHOR); DrawText("PAUSE", 1192, 572, 16, COLOR_PHOSPHOR);
                }
                if (CheckCollisionPointRec(vm, rb)) {
                    DrawRectangleRec(rb, COLOR_BTN_HOVER_BG); DrawRectangleLinesEx(rb, 1.0f, COLOR_PHOSPHOR); DrawText("RESTART", 1180, 622, 16, COLOR_PHOSPHOR);
                }
            }
            if (anim.active && anim.type == ANIM_BANNER) screens_draw_banner(anim.banner_text);
            cinematic_draw();
        }
        EndTextureMode();

        if (overlay_active && overlay_alpha > 0.0f) {
            BeginTextureMode(overlay_canvas);
            ClearBackground(BLANK);
            if (g.phase == PHASE_DEALER_SELECT) board_draw_dealer_select(&assets, &g, vm);
            else if (g.phase == PHASE_HAND_CHOICE) screens_draw_choice_overlay(&assets, &g, vm, 1);
            else if (g.phase == PHASE_DRAW_CHOICE) screens_draw_choice_overlay(&assets, &g, vm, 0);
            else if (g.phase == PHASE_DECIDE && g.current_player == 0) screens_draw_decide_overlay(&assets, &g, vm);
            else if (g.phase == PHASE_ROUND_END) screens_draw_round_result(&assets, &g, vm);
            if (is_paused) screens_draw_pause(vm, confirm_restart);
            EndTextureMode();

            BeginTextureMode(canvas);
            DrawTextureRec(overlay_canvas.texture, (Rectangle){ 0, 0, (float)CANVAS_W, -(float)CANVAS_H }, (Vector2){ 0, 0 }, (Color){ 255, 255, 255, (unsigned char)(overlay_alpha * 255.0f) });
            EndTextureMode();
        }

        if (transition_alpha > 0.0f) {
            BeginTextureMode(canvas);
            unsigned char a = (unsigned char)(fminf(1.0f, fmaxf(0.0f, transition_alpha)) * 255.0f);
            DrawRectangle(0, 0, CANVAS_W, CANVAS_H, (Color){ 0, 0, 0, a });
            EndTextureMode();
        }

        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(canvas.texture, (Rectangle){ 0, 0, (float)CANVAS_W, -(float)CANVAS_H },
                       (Rectangle){ l.ox, l.oy, CANVAS_W * l.scale, CANVAS_H * l.scale }, (Vector2){ 0, 0 }, 0.0f, WHITE);
        EndDrawing();

        if (shot_frame > 0 && frame_count >= shot_frame) { TakeScreenshot("screenshot.png"); break; }
    }

    audio_close(&audio);
    board_unload_assets(&assets);
    UnloadRenderTexture(overlay_canvas);
    UnloadRenderTexture(canvas);
    CloseWindow();
    return 0;
}
