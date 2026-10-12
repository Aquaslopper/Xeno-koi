#include "ui_screens.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int draw_btn(Rectangle r, const char *txt, int fsz, Vector2 vm, Color txt_col) {
    int hov = CheckCollisionPointRec(vm, r);
    DrawRectangleRec(r, hov ? COLOR_BTN_HOVER_BG : COLOR_PANEL_BG);
    DrawRectangleLinesEx(r, 1.0f, hov ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    int tw = MeasureText(txt, fsz);
    DrawText(txt, (int)(r.x + (r.width - tw) / 2.0f), (int)(r.y + (r.height - fsz) / 2.0f), fsz, txt_col);
    return hov;
}

static void draw_bg_if_loaded(const BoardAssets *ba, unsigned char alpha) {
    if (ba->current_bg >= 0 && ba->current_bg < 5 && ba->bgs[ba->current_bg].id > 0) {
        DrawTexturePro(ba->bgs[ba->current_bg], (Rectangle){0,0,1280,720}, (Rectangle){0,0,1280,720}, (Vector2){0}, 0, WHITE);
    }
    DrawRectangle(0, 0, 1280, 720, (Color){ 0, 0, 0, alpha });
}

static void draw_scanlines(void) {
    for (int y = 0; y < 720; y += 3) DrawLine(0, y, 1280, y, COLOR_SCANLINE);
}

static void get_yaku_cards(const GameState *g, int winner, const ScoredYaku *y, int *out, int *out_cnt) {
    if (y->card_count > 0) {
        *out_cnt = y->card_count;
        for (int i = 0; i < y->card_count; i++) out[i] = y->card_ids[i];
        return;
    }
    int cnt = 0;
    if (winner >= 0 && winner <= 1) {
        CardType t = (y->id == YAKU_TANE) ? CARD_TYPE_ANIMAL : (y->id == YAKU_TANZAKU) ? CARD_TYPE_RIBBON : CARD_TYPE_PLAIN;
        for (int i = 0; i < g->captured_count[winner] && cnt < 10; i++) {
            int cid = g->captured[winner][i];
            if (CARDS[cid].type == t || (y->id == YAKU_KASU && cid == 32)) out[cnt++] = cid;
        }
    }
    *out_cnt = cnt;
}

static void draw_yaku_cards(const BoardAssets *ba, const int *cards, int count, float x, float y) {
    float max_w = 260.0f;
    float step = (count > 1) ? fminf(MINI_CARD_W + 3.0f, (max_w - MINI_CARD_W) / (count - 1)) : 0.0f;
    for (int c = 0; c < count; c++) {
        Rectangle r = { x + c * step, y, MINI_CARD_W, MINI_CARD_H };
        board_draw_card(ba, cards[c], r);
    }
}

void screens_draw_start(const BoardAssets *ba, const GameStats *st, Vector2 vm, float saved_timer) {
    draw_bg_if_loaded(ba, 140);
    const char *title = "[XENO/KOI]";
    int tw = MeasureText(title, 48);
    DrawText(title, (1280 - tw) / 2, 90, 48, COLOR_PHOSPHOR);
    DrawText("BIOMECHANICAL EDITION", 490, 150, 20, COLOR_PHOSPHOR_DIM);

    Rectangle btns[6] = {
        { 490, 190, 300, 42 }, { 490, 242, 300, 42 },
        { 490, 294, 300, 42 }, { 490, 346, 300, 42 },
        { 490, 398, 300, 42 }, { 490, 450, 300, 42 }
    };
    const char *lbls[6] = { "START GAME", "OPTIONS", "STATS", "CREDITS", "SAVE", "QUIT" };
    for (int i = 0; i < 6; i++) draw_btn(btns[i], lbls[i], 20, vm, COLOR_PHOSPHOR);
    if (saved_timer > 0.0f) DrawText("ALL FILES SAVED", 570, 510, 20, COLOR_PHOSPHOR);

    /* Arcade Ticker (marquee scrolling right to left) */
    static float ticker_x = 0.0f;
    ticker_x -= 110.0f * GetFrameTime();
    char ticker[512];
    snprintf(ticker, sizeof(ticker),
             "/// SYSTEM ONLINE /// XENO-KOI V1.0 /// VIBE CODER: AQUASLOPPER /// MUSIC: GRAF PUNK /// HIGHEST ROUND SCORE: %d PTS /// FAVORITE COMBO: %s /// NO NETWORK CONNECTION DETECTED ///",
             st ? st->best_award : 0, st ? storage_get_favorite_combo(st) : "NONE");
    int mtw = MeasureText(ticker, 16);
    float rep = mtw + 100.0f;
    if (ticker_x <= -rep) ticker_x += rep;
    Rectangle bar = { 0, 686, 1280, 34 };
    DrawRectangleRec(bar, COLOR_PANEL_BG);
    DrawLine(0, 686, 1280, 686, COLOR_PHOSPHOR);
    DrawText(ticker, (int)ticker_x, 695, 16, COLOR_PHOSPHOR);
    if (ticker_x + rep < 1280.0f) DrawText(ticker, (int)(ticker_x + rep), 695, 16, COLOR_PHOSPHOR);

    draw_scanlines();
}

ScreenAction screens_handle_start_click(Vector2 vm) {
    Rectangle btns[6] = {
        { 490, 190, 300, 42 }, { 490, 242, 300, 42 },
        { 490, 294, 300, 42 }, { 490, 346, 300, 42 },
        { 490, 398, 300, 42 }, { 490, 450, 300, 42 }
    };
    if (CheckCollisionPointRec(vm, btns[0])) return ACT_START_GAME;
    if (CheckCollisionPointRec(vm, btns[1])) return ACT_GOTO_OPTIONS;
    if (CheckCollisionPointRec(vm, btns[2])) return ACT_GOTO_STATS;
    if (CheckCollisionPointRec(vm, btns[3])) return ACT_GOTO_CREDITS;
    if (CheckCollisionPointRec(vm, btns[4])) return ACT_SAVE;
    if (CheckCollisionPointRec(vm, btns[5])) return ACT_QUIT;
    return ACT_NONE;
}

void screens_draw_credits(const BoardAssets *ba, Vector2 vm) {
    draw_bg_if_loaded(ba, 180);
    Rectangle panel = { 320, 80, 640, 540 };
    DrawRectangleRec(panel, COLOR_PANEL_BG);
    DrawRectangleLinesEx(panel, 1.0f, COLOR_PHOSPHOR_DIM);
    DrawText("[]", (1280 - MeasureText("[]", 24)) / 2, 130, 24, COLOR_PHOSPHOR_DIM);
    DrawText("XENO/KOI", (1280 - MeasureText("XENO/KOI", 46)) / 2, 180, 46, COLOR_PHOSPHOR);
    DrawText("Vibe Coder: [Aquaslopper]", (1280 - MeasureText("Vibe Coder: [Aquaslopper]", 22)) / 2, 275, 22, COLOR_PHOSPHOR);
    DrawText("Music: [Graf Punk]", (1280 - MeasureText("Music: [Graf Punk]", 22)) / 2, 330, 22, COLOR_PHOSPHOR);
    DrawText("Development Platform: Arena.ai - Agent mode.", (1280 - MeasureText("Development Platform: Arena.ai - Agent mode.", 20)) / 2, 385, 20, COLOR_PHOSPHOR);
    DrawText("[]", (1280 - MeasureText("[]", 24)) / 2, 445, 24, COLOR_PHOSPHOR_DIM);
    draw_btn((Rectangle){ 520, 530, 240, 46 }, "BACK", 22, vm, COLOR_PHOSPHOR);
    draw_scanlines();
}

ScreenAction screens_handle_credits_click(Vector2 vm) {
    if (CheckCollisionPointRec(vm, (Rectangle){ 520, 530, 240, 46 })) return ACT_GOTO_MENU;
    return ACT_NONE;
}

void screens_draw_options(const BoardAssets *ba, GameSettings *s, Vector2 vm) {
    draw_bg_if_loaded(ba, 180);
    DrawText("GAME OPTIONS", 520, 45, 34, COLOR_PHOSPHOR);

    DrawText("Match Length:", 340, 95, 22, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 580, 87, 44, 36 }, "-", 24, vm, COLOR_PHOSPHOR);
    DrawText(TextFormat("%d Rounds", s->rounds), 650, 95, 22, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 770, 87, 44, 36 }, "+", 24, vm, COLOR_PHOSPHOR);

    DrawText("Difficulty:", 340, 145, 22, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 580, 137, 110, 36 }, "EASY", 18, vm, (s->difficulty == DIFFICULTY_EASY) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    draw_btn((Rectangle){ 710, 137, 110, 36 }, "HARD", 18, vm, (s->difficulty == DIFFICULTY_HARD) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);

    DrawText("Ruleset:", 340, 195, 22, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 580, 187, 44, 36 }, "<", 22, vm, COLOR_PHOSPHOR);
    const char *r_names[5] = { "NINTENDO", "FUDAWIKI", "KK-ADD-10", "TOURNAMENT", "YAKUZA" };
    Color dbg = (s->ruleset == RULESET_NINTENDO || s->ruleset == RULESET_KK_ADD_10) ? (Color){ 20, 160, 40, 55 } :
                (s->ruleset == RULESET_FUDAWIKI) ? (Color){ 210, 140, 20, 55 } : (Color){ 210, 30, 30, 55 };
    Color dbdr = (s->ruleset == RULESET_NINTENDO || s->ruleset == RULESET_KK_ADD_10) ? (Color){ 30, 220, 60, 140 } :
                 (s->ruleset == RULESET_FUDAWIKI) ? (Color){ 240, 160, 30, 140 } : (Color){ 240, 40, 40, 140 };
    Rectangle rbox = { 630, 187, 144, 36 };
    DrawRectangleRec(rbox, dbg);
    DrawRectangleLinesEx(rbox, 1.0f, dbdr);
    DrawText(r_names[s->ruleset], 630 + (144 - MeasureText(r_names[s->ruleset], 18)) / 2, 196, 18, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 780, 187, 44, 36 }, ">", 22, vm, COLOR_PHOSPHOR);

    const char *cnames[3] = { "GREEN", "AMBER", "TEAL" };
    DrawText("TERMINAL COLOR:", 340, 245, 22, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 580, 237, 44, 36 }, "<", 22, vm, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 630, 237, 140, 36 }, cnames[s->terminal_color], 18, vm, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 776, 237, 44, 36 }, ">", 22, vm, COLOR_PHOSPHOR);

    const char *bg_names[5] = { "0: Harvest", "1: Hole", "2: Arctic", "3: Castle", "4: City" };
    DrawText("Background:", 340, 295, 22, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 580, 287, 44, 36 }, "<", 22, vm, COLOR_PHOSPHOR);
    DrawText(bg_names[s->background], 640, 295, 20, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 780, 287, 44, 36 }, ">", 22, vm, COLOR_PHOSPHOR);
    if (s->background >= 0 && s->background < 5 && ba->bgs[s->background].id > 0) {
        Rectangle prev = { 850, 282, 90, 46 };
        DrawTexturePro(ba->bgs[s->background], (Rectangle){0,0,1280,720}, prev, (Vector2){0}, 0, WHITE);
        DrawRectangleLinesEx(prev, 1.0f, COLOR_PHOSPHOR_DIM);
    }

    DrawText("Anim Speed:", 340, 345, 22, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 580, 337, 95, 36 }, "SLOW", 16, vm, (s->anim_speed == 0) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    draw_btn((Rectangle){ 690, 337, 95, 36 }, "NORMAL", 16, vm, (s->anim_speed == 1) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    draw_btn((Rectangle){ 800, 337, 95, 36 }, "FAST", 16, vm, (s->anim_speed == 2) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);

    DrawText("BGM Volume:", 340, 400, 22, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 580, 392, 36, 34 }, "-", 20, vm, COLOR_PHOSPHOR);
    DrawRectangleRec((Rectangle){ 626, 401, 180, 16 }, COLOR_PANEL_BG);
    DrawRectangleLinesEx((Rectangle){ 626, 401, 180, 16 }, 1.0f, COLOR_PHOSPHOR_DIM);
    DrawRectangle(626, 401, (int)(180.0f * s->bgm_volume), 16, COLOR_PHOSPHOR);
    DrawRectangle((int)(626 + 180.0f * s->bgm_volume - 3), 397, 6, 24, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 816, 392, 36, 34 }, "+", 20, vm, COLOR_PHOSPHOR);
    DrawText(TextFormat("%d%%", (int)(s->bgm_volume * 100.0f + 0.5f)), 865, 400, 20, COLOR_PHOSPHOR);

    DrawText("SFX Volume:", 340, 455, 22, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 580, 447, 36, 34 }, "-", 20, vm, COLOR_PHOSPHOR);
    DrawRectangleRec((Rectangle){ 626, 456, 180, 16 }, COLOR_PANEL_BG);
    DrawRectangleLinesEx((Rectangle){ 626, 456, 180, 16 }, 1.0f, COLOR_PHOSPHOR_DIM);
    DrawRectangle(626, 456, (int)(180.0f * s->sfx_volume), 16, COLOR_PHOSPHOR);
    DrawRectangle((int)(626 + 180.0f * s->sfx_volume - 3), 452, 6, 24, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 816, 447, 36, 34 }, "+", 20, vm, COLOR_PHOSPHOR);
    DrawText(TextFormat("%d%%", (int)(s->sfx_volume * 100.0f + 0.5f)), 865, 455, 20, COLOR_PHOSPHOR);

    draw_btn((Rectangle){ 520, 530, 240, 46 }, "BACK (SAVE)", 22, vm, COLOR_PHOSPHOR);
    draw_scanlines();
}

ScreenAction screens_handle_options_click(GameSettings *s, BoardAssets *ba, AudioAssets *audio, Vector2 vm) {
    int pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    int down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    if (down) {
        if (CheckCollisionPointRec(vm, (Rectangle){ 626, 392, 180, 34 })) {
            s->bgm_volume = fmaxf(0.0f, fminf(1.0f, (vm.x - 626.0f) / 180.0f));
            if (audio) audio_set_bgm_volume(audio, s->bgm_volume);
        } else if (CheckCollisionPointRec(vm, (Rectangle){ 626, 447, 180, 34 })) {
            s->sfx_volume = fmaxf(0.0f, fminf(1.0f, (vm.x - 626.0f) / 180.0f));
            if (audio) { audio_set_sfx_volume(audio, s->sfx_volume); if (pressed) audio_play_click(audio); }
        }
    }
    if (!pressed) return ACT_NONE;

    if (CheckCollisionPointRec(vm, (Rectangle){ 580, 87, 44, 36 })) { if (s->rounds > 1) s->rounds--; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 770, 87, 44, 36 })) { if (s->rounds < 12) s->rounds++; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 580, 137, 110, 36 })) { s->difficulty = DIFFICULTY_EASY; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 710, 137, 110, 36 })) { s->difficulty = DIFFICULTY_HARD; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 580, 187, 44, 36 })) { s->ruleset = (s->ruleset + 4) % 5; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 630, 187, 144, 36 }) || CheckCollisionPointRec(vm, (Rectangle){ 780, 187, 44, 36 })) {
        s->ruleset = (s->ruleset + 1) % 5; if (audio) audio_play_click(audio);
    }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 580, 237, 44, 36 })) { s->terminal_color = (s->terminal_color + 2) % 3; theme_set(s->terminal_color); if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 630, 237, 140, 36 }) || CheckCollisionPointRec(vm, (Rectangle){ 776, 237, 44, 36 })) { s->terminal_color = (s->terminal_color + 1) % 3; theme_set(s->terminal_color); if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 580, 287, 44, 36 })) { s->background = (s->background + 4) % 5; ba->current_bg = s->background; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 780, 287, 44, 36 })) { s->background = (s->background + 1) % 5; ba->current_bg = s->background; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 580, 337, 95, 36 })) { s->anim_speed = 0; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 690, 337, 95, 36 })) { s->anim_speed = 1; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 800, 337, 95, 36 })) { s->anim_speed = 2; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 580, 392, 36, 34 })) { s->bgm_volume = fmaxf(0.0f, s->bgm_volume - 0.10f); if (audio) { audio_set_bgm_volume(audio, s->bgm_volume); audio_play_click(audio); } }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 816, 392, 36, 34 })) { s->bgm_volume = fminf(1.0f, s->bgm_volume + 0.10f); if (audio) { audio_set_bgm_volume(audio, s->bgm_volume); audio_play_click(audio); } }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 580, 447, 36, 34 })) { s->sfx_volume = fmaxf(0.0f, s->sfx_volume - 0.10f); if (audio) { audio_set_sfx_volume(audio, s->sfx_volume); audio_play_click(audio); } }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 816, 447, 36, 34 })) { s->sfx_volume = fminf(1.0f, s->sfx_volume + 0.10f); if (audio) { audio_set_sfx_volume(audio, s->sfx_volume); audio_play_click(audio); } }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 520, 530, 240, 46 })) { storage_save_settings(s); if (audio) audio_play_click(audio); return ACT_GOTO_MENU; }
    return ACT_NONE;
}

void screens_draw_stats(const BoardAssets *ba, const GameStats *st, const MatchHistory *h, Vector2 vm, int confirm_reset, float log_timer) {
    draw_bg_if_loaded(ba, 180);
    DrawText("PLAYER STATISTICS", 500, 40, 32, COLOR_PHOSPHOR);
    Rectangle p = { 100, 90, 1080, 500 };
    DrawRectangleRec(p, COLOR_PANEL_BG);
    DrawRectangleLinesEx(p, 1.0f, COLOR_PHOSPHOR_DIM);

    float wr = (st->matches_played > 0) ? ((float)st->matches_won / st->matches_played * 100.0f) : 0.0f;
    DrawText(TextFormat("Matches Played:      %d", st->matches_played), 140, 120, 20, COLOR_PHOSPHOR);
    DrawText(TextFormat("Won / Lost:          %d / %d (Draw: %d)", st->matches_won, st->matches_lost, st->matches_drawn), 140, 150, 20, COLOR_PHOSPHOR);
    DrawText(TextFormat("Win Rate:            %.1f%%", wr), 140, 180, 20, COLOR_PHOSPHOR);
    DrawText(TextFormat("Rounds Won:          YOU %d | CPU %d", st->rounds_won_you, st->rounds_won_cpu), 140, 220, 20, COLOR_PHOSPHOR);
    DrawText(TextFormat("Total Points:        FOR %d | AGAINST %d", st->points_for, st->points_against), 140, 250, 20, COLOR_PHOSPHOR);
    DrawText(TextFormat("Highest Round Score: %d pts", st->best_award), 140, 280, 20, COLOR_PHOSPHOR);
    DrawText(TextFormat("Koi Calls:           YOU %d | CPU %d", st->koi_calls_you, st->koi_calls_cpu), 140, 310, 20, COLOR_PHOSPHOR_DIM);
    DrawText(TextFormat("Difficulty:          Easy %d | Hard %d", st->easy_matches, st->hard_matches), 140, 350, 20, COLOR_PHOSPHOR);
    DrawText(TextFormat("Current Streak:      %d", st->current_streak), 140, 380, 20, COLOR_PHOSPHOR);
    DrawText(TextFormat("Max Win Streak:      %d", st->best_streak), 140, 410, 20, COLOR_PHOSPHOR);
    DrawText(TextFormat("Favorite Combo:      %s", storage_get_favorite_combo(st)), 140, 440, 20, COLOR_PHOSPHOR);

    DrawLine(620, 110, 620, 560, COLOR_PHOSPHOR_DIM);
    DrawText("LAST 10 MATCHES", 650, 120, 20, COLOR_PHOSPHOR);
    int hy = 155;
    for (int i = 0; i < h->count; i++) { DrawText(h->lines[i], 650, hy, 14, COLOR_PHOSPHOR_DIM); hy += 28; }

    draw_btn((Rectangle){ 220, 610, 200, 44 }, "SAVE LOG", 20, vm, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 540, 610, 200, 44 }, "RESET STATS", 20, vm, COLOR_RED);
    draw_btn((Rectangle){ 860, 610, 200, 44 }, "BACK", 20, vm, COLOR_PHOSPHOR);
    if (log_timer > 0.0f) DrawText("stats_report.txt written!", 230, 664, 16, COLOR_PHOSPHOR);

    if (confirm_reset) {
        Rectangle modal = { 440, 240, 400, 200 };
        DrawRectangleRec(modal, (Color){ 0, 0, 0, 240 });
        DrawRectangleLinesEx(modal, 2.0f, COLOR_RED);
        DrawText("RESET ALL STATS & LOGS?", 490, 270, 20, COLOR_RED);
        DrawText("This action cannot be undone.", 510, 305, 16, COLOR_PHOSPHOR_DIM);
        draw_btn((Rectangle){ 480, 360, 140, 40 }, "CONFIRM", 18, vm, COLOR_RED);
        draw_btn((Rectangle){ 660, 360, 140, 40 }, "CANCEL", 18, vm, COLOR_PHOSPHOR);
    }
    draw_scanlines();
}

ScreenAction screens_handle_stats_click(Vector2 vm, int *confirm_reset) {
    if (*confirm_reset) {
        if (CheckCollisionPointRec(vm, (Rectangle){ 480, 360, 140, 40 })) { *confirm_reset = 0; return ACT_RESET_STATS; }
        if (CheckCollisionPointRec(vm, (Rectangle){ 660, 360, 140, 40 })) { *confirm_reset = 0; return ACT_NONE; }
        return ACT_NONE;
    }
    if (CheckCollisionPointRec(vm, (Rectangle){ 220, 610, 200, 44 })) return ACT_SAVE_LOG;
    if (CheckCollisionPointRec(vm, (Rectangle){ 540, 610, 200, 44 })) { *confirm_reset = 1; return ACT_NONE; }
    if (CheckCollisionPointRec(vm, (Rectangle){ 860, 610, 200, 44 })) return ACT_GOTO_MENU;
    return ACT_NONE;
}

void screens_draw_pause(Vector2 vm, int confirm_restart) {
    DrawRectangle(0, 0, 1280, 720, COLOR_OVERLAY_BG);
    Rectangle p = { 440, 180, 400, 360 };
    DrawRectangleRec(p, COLOR_PANEL_BG);
    DrawRectangleLinesEx(p, 1.0f, COLOR_PHOSPHOR);
    DrawText("GAME PAUSED", 540, 210, 28, COLOR_PHOSPHOR);

    if (confirm_restart) {
        DrawText("Restart this match?", 530, 280, 20, COLOR_PHOSPHOR);
        draw_btn((Rectangle){ 480, 350, 140, 40 }, "YES", 18, vm, COLOR_RED);
        draw_btn((Rectangle){ 660, 350, 140, 40 }, "NO", 18, vm, COLOR_PHOSPHOR);
    } else {
        draw_btn((Rectangle){ 490, 280, 300, 50 }, "RESUME", 20, vm, COLOR_PHOSPHOR);
        draw_btn((Rectangle){ 490, 350, 300, 50 }, "RESTART MATCH", 20, vm, COLOR_PHOSPHOR);
        draw_btn((Rectangle){ 490, 420, 300, 50 }, "ABANDON MATCH", 20, vm, COLOR_RED);
    }
    draw_scanlines();
}

ScreenAction screens_handle_pause_click(Vector2 vm, int *confirm_restart) {
    if (*confirm_restart) {
        if (CheckCollisionPointRec(vm, (Rectangle){ 480, 350, 140, 40 })) { *confirm_restart = 0; return ACT_RESTART_MATCH; }
        if (CheckCollisionPointRec(vm, (Rectangle){ 660, 350, 140, 40 })) { *confirm_restart = 0; return ACT_RESUME; }
        return ACT_NONE;
    }
    if (CheckCollisionPointRec(vm, (Rectangle){ 490, 280, 300, 50 })) return ACT_RESUME;
    if (CheckCollisionPointRec(vm, (Rectangle){ 490, 350, 300, 50 })) { *confirm_restart = 1; return ACT_NONE; }
    if (CheckCollisionPointRec(vm, (Rectangle){ 490, 420, 300, 50 })) return ACT_ABANDON_MATCH;
    return ACT_NONE;
}

void screens_draw_match_result(const BoardAssets *ba, const GameState *g, Vector2 vm) {
    draw_bg_if_loaded(ba, 170);
    int p0 = g->total_score[0], p1 = g->total_score[1];
    const char *title = (p0 > p1) ? "VICTORY" : (p0 < p1) ? "DEFEAT" : "DRAW";
    Color tcol = (p0 > p1) ? COLOR_PHOSPHOR : (p0 < p1) ? COLOR_RED : COLOR_PHOSPHOR_DIM;
    DrawText(title, (1280 - MeasureText(title, 48)) / 2, 80, 48, tcol);

    Rectangle p = { 340, 150, 600, 440 };
    DrawRectangleRec(p, COLOR_PANEL_BG);
    DrawRectangleLinesEx(p, 1.0f, COLOR_PHOSPHOR_DIM);

    DrawText(TextFormat("FINAL SCORE:   YOU %d  -  CPU %d", p0, p1), 380, 190, 26, COLOR_PHOSPHOR);
    DrawLine(380, 230, 900, 230, COLOR_PHOSPHOR_DIM);
    DrawText("ROUND LEDGER:", 380, 245, 20, COLOR_PHOSPHOR);
    int ly = 275;
    for (int i = 0; i < g->ledger_count && i < 6; i++) {
        const RoundRecord *r = &g->ledger[i];
        const char *w = (r->winner == 0) ? "YOU" : (r->winner == 1) ? "CPU" : "DRAW";
        DrawText(TextFormat("Round %d: %s (%d pts)", r->round_number, w, r->award), 400, ly, 18, COLOR_PHOSPHOR_DIM);
        ly += 26;
    }
    draw_btn((Rectangle){ 490, 470, 300, 46 }, "PLAY AGAIN", 20, vm, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 490, 526, 300, 46 }, "MAIN MENU", 20, vm, COLOR_PHOSPHOR);
    draw_scanlines();
}

ScreenAction screens_handle_match_result_click(Vector2 vm) {
    if (CheckCollisionPointRec(vm, (Rectangle){ 490, 470, 300, 46 })) return ACT_START_GAME;
    if (CheckCollisionPointRec(vm, (Rectangle){ 490, 526, 300, 46 })) return ACT_GOTO_MENU;
    return ACT_NONE;
}

void screens_draw_choice_overlay(const BoardAssets *ba, const GameState *g, Vector2 vm, int is_hand) {
    DrawRectangle(0, 0, 1280, 720, COLOR_OVERLAY_BG);
    int tw = MeasureText("CHOOSE ONE OF THE MATCHING CARDS", 28);
    DrawText("CHOOSE ONE OF THE MATCHING CARDS", (1280 - tw) / 2, 140, 28, COLOR_PHOSPHOR);
    Rectangle r0 = { 400, 180, CHOICE_CARD_W, CHOICE_CARD_H }, r1 = { 704, 180, CHOICE_CARD_W, CHOICE_CARD_H };
    board_draw_card(ba, g->choice_targets[0], r0);
    board_draw_card(ba, g->choice_targets[1], r1);
    if (CheckCollisionPointRec(vm, r0)) board_draw_card_glow(r0, 1.0f);
    if (CheckCollisionPointRec(vm, r1)) board_draw_card_glow(r1, 1.0f);
    if (is_hand) draw_btn((Rectangle){ 560, 500, 160, 44 }, "CANCEL", 20, vm, COLOR_RED);
}

int screens_check_choice_click(const GameState *g, Vector2 vm, int is_hand, int *out_target_id) {
    Rectangle r0 = { 400, 180, CHOICE_CARD_W, CHOICE_CARD_H }, r1 = { 704, 180, CHOICE_CARD_W, CHOICE_CARD_H };
    if (CheckCollisionPointRec(vm, r0)) { *out_target_id = g->choice_targets[0]; return 1; }
    if (CheckCollisionPointRec(vm, r1)) { *out_target_id = g->choice_targets[1]; return 1; }
    if (is_hand && CheckCollisionPointRec(vm, (Rectangle){ 560, 500, 160, 44 })) { *out_target_id = -1; return 2; }
    return 0;
}

void screens_draw_decide_overlay(const BoardAssets *ba, const GameState *g, Vector2 vm) {
    DrawRectangle(0, 0, 1280, 720, COLOR_OVERLAY_BG);
    Rectangle panel = { 200, 45, 880, 630 };
    DrawRectangleRec(panel, COLOR_PANEL_BG);
    DrawRectangleLinesEx(panel, 2.0f, COLOR_PHOSPHOR);

    int p = g->current_player;
    ScoreResult sr = score_captures(g->ruleset, g->captured[p], g->captured_count[p]);
    int opp = 1 - p;
    int award = calculate_award(sr.base_points, g->koi_called[opp] > 0);

    const char *title = "YAKU FORMED! CALL KOI-KOI OR SHOBU?";
    DrawText(title, (1280 - MeasureText(title, 26)) / 2, 70, 26, COLOR_PHOSPHOR);

    int y = 120;
    int max_show = (sr.yaku_count > 6) ? 6 : sr.yaku_count;
    for (int i = 0; i < max_show; i++) {
        const ScoredYaku *yaku = &sr.yakus[i];
        DrawText(TextFormat("- %s", yaku->name), 240, y + 9, 20, COLOR_PHOSPHOR);
        DrawText(TextFormat("%d pts", yaku->points), 600, y + 9, 20, COLOR_PHOSPHOR);
        int ycards[16], ycount = 0;
        get_yaku_cards(g, p, yaku, ycards, &ycount);
        draw_yaku_cards(ba, ycards, ycount, 690.0f, (float)y);
        y += 46;
    }

    DrawLine(240, 430, 1040, 430, COLOR_PHOSPHOR_DIM);
    DrawText(TextFormat("Base: %d pts", sr.base_points), 240, 442, 20, COLOR_PHOSPHOR);
    if (sr.base_points >= 7) DrawText("x2  7+ Multiplier", 420, 442, 18, COLOR_PHOSPHOR);
    if (g->koi_called[opp] > 0) DrawText("x2  Koi-Koi Multiplier", 660, 442, 18, COLOR_PHOSPHOR);
    DrawText(TextFormat("IF YOU SHOBU NOW: %d POINTS", award), 240, 475, 22, COLOR_PHOSPHOR);

    draw_btn((Rectangle){ 280, 520, 320, 60 }, "KOI-KOI (CONTINUE)", 20, vm, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 680, 520, 320, 60 }, "SHOBU (BANK & END)", 20, vm, COLOR_PHOSPHOR);
    DrawText("Continue round to aim for higher combinations.", 280, 590, 14, COLOR_PHOSPHOR_DIM);
    DrawText("Bank your current score and win this round.", 680, 590, 14, COLOR_PHOSPHOR_DIM);
}

int screens_check_decide_click(Vector2 vm, DecisionType *out_decision) {
    if (CheckCollisionPointRec(vm, (Rectangle){ 280, 520, 320, 60 })) {
        *out_decision = DECISION_KOI_KOI;
        return 1;
    }
    if (CheckCollisionPointRec(vm, (Rectangle){ 680, 520, 320, 60 })) {
        *out_decision = DECISION_SHOBU;
        return 1;
    }
    return 0;
}

void screens_draw_round_result(const BoardAssets *ba, const GameState *g, Vector2 vm) {
    DrawRectangle(0, 0, 1280, 720, COLOR_OVERLAY_BG);
    Rectangle panel = { 200, 45, 880, 630 };
    DrawRectangleRec(panel, COLOR_PANEL_BG);
    DrawRectangleLinesEx(panel, 2.0f, COLOR_PHOSPHOR);

    const RoundRecord *rec = &g->current_round_record;
    const char *title = (rec->winner == 0) ? "YOU WIN THE ROUND!" :
                        (rec->winner == 1) ? "CPU WINS THE ROUND!" : "ROUND DRAW!";
    Color tcol = (rec->winner == 0) ? COLOR_PHOSPHOR :
                 (rec->winner == 1) ? COLOR_RED : COLOR_PHOSPHOR_DIM;
    DrawText(title, (1280 - MeasureText(title, 28)) / 2, 65, 28, tcol);

    if (rec->winner < 0) {
        if (rec->draw_reason == 1) DrawText("Reason: Exhaustion (No one scored)", 460, 150, 20, COLOR_PHOSPHOR_DIM);
        else if (rec->draw_reason == 2) DrawText("Reason: Both players had Lucky Hands", 460, 150, 20, COLOR_PHOSPHOR_DIM);
    } else {
        int y = 110;
        if (rec->lucky_hand) {
            DrawText("LUCKY HAND (Teshi / Kuttsuki): 6 pts", 230, y + 10, 20, COLOR_PHOSPHOR);
            int lcards[8], lcnt = 0;
            for (int i = 0; i < g->captured_count[rec->winner] && lcnt < 8; i++) lcards[lcnt++] = g->captured[rec->winner][i];
            draw_yaku_cards(ba, lcards, lcnt, 680.0f, (float)y);
            y += 48;
        } else if (rec->score_result.yaku_count == 0) {
            DrawText("OYA-KEN (Dealer's Privilege): 6 pts", 230, y + 10, 20, COLOR_PHOSPHOR);
            y += 48;
        } else {
            const ScoreResult *sr = &rec->score_result;
            int max_show = (sr->yaku_count > 5) ? 5 : sr->yaku_count;
            for (int i = 0; i < max_show; i++) {
                const ScoredYaku *yaku = &sr->yakus[i];
                DrawText(TextFormat("- %s", yaku->name), 230, y + 9, 18, COLOR_PHOSPHOR);
                DrawText(TextFormat("%d pts", yaku->points), 600, y + 9, 18, COLOR_PHOSPHOR);
                int ycards[16], ycount = 0;
                get_yaku_cards(g, rec->winner, yaku, ycards, &ycount);
                draw_yaku_cards(ba, ycards, ycount, 680.0f, (float)y);
                y += 44;
            }
            if (sr->yaku_count > 5) {
                DrawText(TextFormat("+ %d more combos...", sr->yaku_count - 5), 230, y + 4, 16, COLOR_PHOSPHOR_DIM);
                y += 24;
            }
            y += 6;
            DrawLine(230, y, 1050, y, COLOR_PHOSPHOR_DIM);
            y += 10;
            DrawText(TextFormat("Base: %d pts", rec->base_points), 230, y, 19, COLOR_PHOSPHOR);
            y += 22;
            if (rec->mult_seven > 1) { DrawText("x2 7+ Points Multiplier", 230, y, 16, COLOR_PHOSPHOR); y += 20; }
            if (rec->mult_koi > 1) { DrawText("x2 Opponent Koi-Koi Multiplier", 230, y, 16, COLOR_PHOSPHOR); y += 20; }
        }
        DrawText(TextFormat("ROUND AWARD: %d PTS", rec->award), 230, y + 4, 22, COLOR_PHOSPHOR);
    }

    DrawText(TextFormat("MATCH RUNNING SCORE:  YOU %d  -  CPU %d", g->total_score[0], g->total_score[1]),
             230, 565, 20, COLOR_PHOSPHOR_DIM);

    const char *btn_text = (g->current_round >= g->total_rounds) ? "MATCH RESULTS" : "NEXT ROUND";
    draw_btn((Rectangle){ 520, 610, 240, 48 }, btn_text, 20, vm, COLOR_PHOSPHOR);
}

int screens_check_round_result_click(Vector2 vm) {
    return CheckCollisionPointRec(vm, (Rectangle){ 520, 610, 240, 48 });
}

void screens_draw_banner(const char *text) {
    int tw = MeasureText(text, 28);
    Rectangle br = { (1280 - tw) / 2.0f - 30.0f, 320.0f, tw + 60.0f, 60.0f };
    DrawRectangleRec(br, (Color){ 0, 0, 0, 230 });
    DrawRectangleLinesEx(br, 2.0f, COLOR_PHOSPHOR);
    DrawText(text, (1280 - tw) / 2, 336, 28, COLOR_PHOSPHOR);
}
