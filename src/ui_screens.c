#include "ui_screens.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

extern Font small_font;

static int draw_btn(Rectangle r, const char *txt, int fsz, Vector2 vm, Color txt_col) {
    int hov = CheckCollisionPointRec(vm, r);
    DrawRectangleRec(r, hov ? COLOR_BTN_HOVER_BG : COLOR_PANEL_BG);
    ui_draw_panel_border(r, hov ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
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
    float step = (count > 1) ? fminf(MINI_CARD_W + 3.0f, (260.0f - MINI_CARD_W) / (count - 1)) : 0.0f;
    for (int c = 0; c < count; c++) board_draw_card(ba, cards[c], (Rectangle){ x + c * step, y, MINI_CARD_W, MINI_CARD_H });
}

void screens_draw_start(const BoardAssets *ba, const GameStats *st, Vector2 vm, float saved_timer) {
    (void)st; /* ticker removed; kept in signature for caller compatibility */
    draw_bg_if_loaded(ba, 140);
    const char *title = "[XENO/KOI]";
    DrawText(title, (1280 - MeasureText(title, 72)) / 2, 80, 72, COLOR_PHOSPHOR);
    const char *sub = "BIOMECHANICAL EDITION";
    float stw = MeasureTextEx(small_font, sub, 20.0f, 2.0f).x;
    DrawTextEx(small_font, sub, (Vector2){ (1280.0f - stw) / 2.0f, 165.0f }, 20.0f, 2.0f, COLOR_PHOSPHOR_DIM);

    const char *lbls[6] = { "START MATCH", "OPTION", "STATS", "SAVE", "CREDITS", "QUIT" };
    for (int i = 0; i < 6; i++) draw_btn((Rectangle){ 490, 235 + i * 50, 300, 40 }, lbls[i], 18, vm, COLOR_PHOSPHOR);
    if (saved_timer > 0.0f) DrawText("ALL FILES SAVED", 570, 555, 20, COLOR_PHOSPHOR);
}

ScreenAction screens_handle_start_click(Vector2 vm) {
    ScreenAction acts[6] = { ACT_START_GAME, ACT_GOTO_OPTIONS, ACT_GOTO_STATS, ACT_SAVE, ACT_GOTO_CREDITS, ACT_QUIT };
    for (int i = 0; i < 6; i++) {
        if (CheckCollisionPointRec(vm, (Rectangle){ 490, 235 + i * 50, 300, 40 })) return acts[i];
    }
    return ACT_NONE;
}

void screens_draw_credits(const BoardAssets *ba, Vector2 vm) {
    draw_bg_if_loaded(ba, 180);
    Rectangle panel = { 320, 80, 640, 540 };
    DrawRectangleRec(panel, COLOR_PANEL_BG);
    ui_draw_panel_border(panel, COLOR_PHOSPHOR_DIM);
    DrawText("[]", (1280 - MeasureText("[]", 24)) / 2, 130, 24, COLOR_PHOSPHOR_DIM);
    DrawText("XENO/KOI", (1280 - MeasureText("XENO/KOI", 46)) / 2, 180, 46, COLOR_PHOSPHOR);
    DrawText("Vibe Coder: [Aquaslopper]", (1280 - MeasureText("Vibe Coder: [Aquaslopper]", 22)) / 2, 275, 22, COLOR_PHOSPHOR);
    DrawText("Music: [Graf Punk]", (1280 - MeasureText("Music: [Graf Punk]", 22)) / 2, 330, 22, COLOR_PHOSPHOR);
    DrawText("Development Platform: Arena.ai - Agent mode.", (1280 - MeasureText("Development Platform: Arena.ai - Agent mode.", 20)) / 2, 385, 20, COLOR_PHOSPHOR);
    DrawText("[]", (1280 - MeasureText("[]", 24)) / 2, 445, 24, COLOR_PHOSPHOR_DIM);
    draw_btn((Rectangle){ 520, 530, 240, 46 }, "BACK", 22, vm, COLOR_PHOSPHOR);
}

ScreenAction screens_handle_credits_click(Vector2 vm) {
    if (CheckCollisionPointRec(vm, (Rectangle){ 520, 530, 240, 46 })) return ACT_GOTO_MENU;
    return ACT_NONE;
}

static void draw_slider(int y, float val, const char *lbl, const char *val_txt, Vector2 vm) {
    DrawText(lbl, 340, y + 4, 18, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 580, (float)y, 36, 26 }, "-", 18, vm, COLOR_PHOSPHOR);
    Rectangle track = { 626, (float)y + 5, 180, 16 };
    DrawRectangleRec(track, COLOR_PANEL_BG);
    DrawRectangleLinesEx(track, 1.0f, COLOR_PHOSPHOR_DIM);
    DrawRectangle(626, y + 5, (int)(180.0f * val), 16, COLOR_PHOSPHOR);
    DrawRectangle((int)(626 + 180.0f * val - 3), y + 1, 6, 24, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 816, (float)y, 36, 26 }, "+", 18, vm, COLOR_PHOSPHOR);
    DrawText(val_txt, 865, y + 4, 17, COLOR_PHOSPHOR);
}

void screens_draw_options(const BoardAssets *ba, GameSettings *s, Vector2 vm) {
    draw_bg_if_loaded(ba, 180);
    DrawText("GAME OPTIONS", (1280 - MeasureText("GAME OPTIONS", 30)) / 2, 18, 30, COLOR_PHOSPHOR);

    DrawText("Match Length:", 340, 59, 18, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 580, 55, 44, 26 }, "-", 20, vm, COLOR_PHOSPHOR);
    DrawText(TextFormat("%d Rounds", s->rounds), 650, 59, 18, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 770, 55, 44, 26 }, "+", 20, vm, COLOR_PHOSPHOR);

    DrawText("Difficulty:", 340, 101, 18, COLOR_PHOSPHOR);
    const char *d_names[3] = { "EASY", "HARD", "MASTER" };
    draw_btn((Rectangle){ 580, 97, 44, 26 }, "<", 18, vm, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 630, 97, 140, 26 }, d_names[s->difficulty], 16, vm, (s->difficulty == DIFFICULTY_MASTER) ? (Color){ 255, 60, 60, 255 } : COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 776, 97, 44, 26 }, ">", 18, vm, COLOR_PHOSPHOR);

    DrawText("Master Timer:", 340, 143, 18, (s->difficulty == DIFFICULTY_MASTER) ? (Color){ 255, 100, 100, 255 } : COLOR_PHOSPHOR_DIM);
    if (s->difficulty == DIFFICULTY_MASTER) {
        draw_btn((Rectangle){ 580, 139, 105, 26 }, "15 SEC", 14, vm, (s->master_timer == 15) ? (Color){ 255, 80, 80, 255 } : COLOR_PHOSPHOR_DIM);
        draw_btn((Rectangle){ 695, 139, 105, 26 }, "30 SEC", 14, vm, (s->master_timer == 30) ? (Color){ 255, 80, 80, 255 } : COLOR_PHOSPHOR_DIM);
    } else {
        DrawText("[ MASTER ONLY ]", 580, 143, 16, COLOR_PHOSPHOR_DIM);
    }

    DrawText("Ruleset:", 340, 185, 18, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 580, 181, 44, 26 }, "<", 18, vm, COLOR_PHOSPHOR);
    const char *r_names[5] = { "NINTENDO", "FUDAWIKI", "KK-ADD-10", "TOURNAMENT", "YAKUZA" };
    Color dbg = (s->ruleset == RULESET_NINTENDO || s->ruleset == RULESET_KK_ADD_10) ? (Color){ 20, 160, 40, 55 } :
                (s->ruleset == RULESET_FUDAWIKI) ? (Color){ 210, 140, 20, 55 } : (Color){ 210, 30, 30, 55 };
    Color dbdr = (s->ruleset == RULESET_NINTENDO || s->ruleset == RULESET_KK_ADD_10) ? (Color){ 30, 220, 60, 140 } :
                 (s->ruleset == RULESET_FUDAWIKI) ? (Color){ 240, 160, 30, 140 } : (Color){ 240, 40, 40, 140 };
    Rectangle rbox = { 630, 181, 144, 26 };
    DrawRectangleRec(rbox, dbg); DrawRectangleLinesEx(rbox, 1.0f, dbdr);
    DrawText(r_names[s->ruleset], 630 + (144 - MeasureText(r_names[s->ruleset], 15)) / 2, 186, 15, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 780, 181, 44, 26 }, ">", 18, vm, COLOR_PHOSPHOR);

    const char *cnames[5] = { "GREEN", "AMBER", "TEAL", "CRIMSON", "VIOLET" };
    DrawText("TERMINAL COLOR:", 340, 227, 18, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 580, 223, 44, 26 }, "<", 18, vm, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 630, 223, 140, 26 }, cnames[s->terminal_color % 5], 15, vm, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 776, 223, 44, 26 }, ">", 18, vm, COLOR_PHOSPHOR);

    const char *bg_names[5] = { "0: Harvest", "1: Hole", "2: Arctic", "3: Castle", "4: City" };
    DrawText("Background:", 340, 269, 18, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 580, 265, 44, 26 }, "<", 18, vm, COLOR_PHOSPHOR);
    DrawText(bg_names[s->background], 640, 269, 16, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 780, 265, 44, 26 }, ">", 18, vm, COLOR_PHOSPHOR);
    if (s->background >= 0 && s->background < 5 && ba->bgs[s->background].id > 0) {
        Rectangle prev = { 850, 259, 76, 38 };
        DrawTexturePro(ba->bgs[s->background], (Rectangle){0,0,1280,720}, prev, (Vector2){0}, 0, WHITE);
        DrawRectangleLinesEx(prev, 1.0f, COLOR_PHOSPHOR_DIM);
    }

    DrawText("Anim Speed:", 340, 311, 18, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 580, 307, 95, 26 }, "SLOW", 14, vm, (s->anim_speed == 0) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    draw_btn((Rectangle){ 690, 307, 95, 26 }, "NORMAL", 14, vm, (s->anim_speed == 1) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    draw_btn((Rectangle){ 800, 307, 95, 26 }, "FAST", 14, vm, (s->anim_speed == 2) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);

    draw_slider(349, s->bgm_volume, "BGM Volume:", TextFormat("%d%%", (int)(s->bgm_volume * 100.0f + 0.5f)), vm);
    draw_slider(391, s->sfx_volume, "SFX Volume:", TextFormat("%d%%", (int)(s->sfx_volume * 100.0f + 0.5f)), vm);

    DrawText("Fullscreen:", 340, 437, 18, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 580, 433, 44, 26 }, "<", 18, vm, COLOR_PHOSPHOR);
    const char *fs_names[2] = { "WINDOWED", "BORDERLESS" };
    draw_btn((Rectangle){ 630, 433, 140, 26 }, fs_names[s->fullscreen ? 1 : 0], 15, vm, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 776, 433, 44, 26 }, ">", 18, vm, COLOR_PHOSPHOR);

    DrawText("Bloom Effect:", 340, 479, 18, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 580, 475, 44, 26 }, "<", 18, vm, COLOR_PHOSPHOR);
    const char *bl_names[3] = { "OFF", "MID", "FULL" };
    draw_btn((Rectangle){ 630, 475, 140, 26 }, bl_names[s->bloom % 3], 15, vm, (s->bloom > 0) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    draw_btn((Rectangle){ 776, 475, 44, 26 }, ">", 18, vm, COLOR_PHOSPHOR);

    DrawText("CRT Filter:", 340, 521, 18, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 580, 517, 44, 26 }, "<", 18, vm, COLOR_PHOSPHOR);
    const char *crt_names[4] = { "0: OFF", "1: LOW", "2: MID", "3: HIGH" };
    draw_btn((Rectangle){ 630, 517, 140, 26 }, crt_names[s->crt_filter % 4], 15, vm, (s->crt_filter > 0) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    draw_btn((Rectangle){ 776, 517, 44, 26 }, ">", 18, vm, COLOR_PHOSPHOR);

    float g_frac = fmaxf(0.0f, fminf(1.0f, (s->gamma - 0.5f) / 1.5f));
    draw_slider(559, g_frac, "Display Gamma:", TextFormat("%.2f", s->gamma), vm);

    draw_btn((Rectangle){ 510, 622, 260, 44 }, "BACK (SAVE)", 20, vm, COLOR_PHOSPHOR);
}

ScreenAction screens_handle_options_click(GameSettings *s, BoardAssets *ba, AudioAssets *audio, Vector2 vm) {
    int pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    int down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    if (down) {
        if (CheckCollisionPointRec(vm, (Rectangle){ 626, 349, 180, 26 })) {
            s->bgm_volume = fmaxf(0.0f, fminf(1.0f, (vm.x - 626.0f) / 180.0f));
            if (audio) audio_set_bgm_volume(audio, s->bgm_volume);
        } else if (CheckCollisionPointRec(vm, (Rectangle){ 626, 391, 180, 26 })) {
            s->sfx_volume = fmaxf(0.0f, fminf(1.0f, (vm.x - 626.0f) / 180.0f));
            if (audio) { audio_set_sfx_volume(audio, s->sfx_volume); if (pressed) audio_play_click(audio); }
        } else if (CheckCollisionPointRec(vm, (Rectangle){ 626, 559, 180, 26 })) {
            s->gamma = 0.5f + fmaxf(0.0f, fminf(1.0f, (vm.x - 626.0f) / 180.0f)) * 1.5f;
            if (pressed && audio) audio_play_click(audio);
        }
    }
    if (!pressed) return ACT_NONE;

    if (CheckCollisionPointRec(vm, (Rectangle){ 580, 55, 44, 26 })) { if (s->rounds > 1) s->rounds--; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 770, 55, 44, 26 })) { if (s->rounds < 12) s->rounds++; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 580, 97, 44, 26 })) { s->difficulty = (Difficulty)((s->difficulty + 2) % 3); if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 630, 97, 140, 26 }) || CheckCollisionPointRec(vm, (Rectangle){ 776, 97, 44, 26 })) { s->difficulty = (Difficulty)((s->difficulty + 1) % 3); if (audio) audio_play_click(audio); }
    else if (s->difficulty == DIFFICULTY_MASTER && CheckCollisionPointRec(vm, (Rectangle){ 580, 139, 105, 26 })) { s->master_timer = 15; if (audio) audio_play_click(audio); }
    else if (s->difficulty == DIFFICULTY_MASTER && CheckCollisionPointRec(vm, (Rectangle){ 695, 139, 105, 26 })) { s->master_timer = 30; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 580, 181, 44, 26 })) { s->ruleset = (s->ruleset + 4) % 5; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 630, 181, 144, 26 }) || CheckCollisionPointRec(vm, (Rectangle){ 780, 181, 44, 26 })) { s->ruleset = (s->ruleset + 1) % 5; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 580, 223, 44, 26 })) { s->terminal_color = (s->terminal_color + 4) % 5; theme_set(s->terminal_color); if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 630, 223, 140, 26 }) || CheckCollisionPointRec(vm, (Rectangle){ 776, 223, 44, 26 })) { s->terminal_color = (s->terminal_color + 1) % 5; theme_set(s->terminal_color); if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 580, 265, 44, 26 })) { s->background = (s->background + 4) % 5; ba->current_bg = s->background; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 780, 265, 44, 26 })) { s->background = (s->background + 1) % 5; ba->current_bg = s->background; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 580, 307, 95, 26 })) { s->anim_speed = 0; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 690, 307, 95, 26 })) { s->anim_speed = 1; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 800, 307, 95, 26 })) { s->anim_speed = 2; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 580, 349, 36, 26 })) { s->bgm_volume = fmaxf(0.0f, s->bgm_volume - 0.10f); if (audio) { audio_set_bgm_volume(audio, s->bgm_volume); audio_play_click(audio); } }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 816, 349, 36, 26 })) { s->bgm_volume = fminf(1.0f, s->bgm_volume + 0.10f); if (audio) { audio_set_bgm_volume(audio, s->bgm_volume); audio_play_click(audio); } }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 580, 391, 36, 26 })) { s->sfx_volume = fmaxf(0.0f, s->sfx_volume - 0.10f); if (audio) { audio_set_sfx_volume(audio, s->sfx_volume); audio_play_click(audio); } }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 816, 391, 36, 26 })) { s->sfx_volume = fminf(1.0f, s->sfx_volume + 0.10f); if (audio) { audio_set_sfx_volume(audio, s->sfx_volume); audio_play_click(audio); } }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 580, 433, 44, 26 }) || CheckCollisionPointRec(vm, (Rectangle){ 630, 433, 140, 26 }) || CheckCollisionPointRec(vm, (Rectangle){ 776, 433, 44, 26 })) {
        s->fullscreen = 1 - s->fullscreen; ToggleBorderlessWindowed(); if (audio) audio_play_click(audio);
    }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 580, 475, 44, 26 })) { s->bloom = (s->bloom + 2) % 3; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 630, 475, 140, 26 }) || CheckCollisionPointRec(vm, (Rectangle){ 776, 475, 44, 26 })) { s->bloom = (s->bloom + 1) % 3; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 580, 517, 44, 26 })) { s->crt_filter = (s->crt_filter + 3) % 4; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 630, 517, 140, 26 }) || CheckCollisionPointRec(vm, (Rectangle){ 776, 517, 44, 26 })) { s->crt_filter = (s->crt_filter + 1) % 4; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 580, 559, 36, 26 })) { s->gamma = fmaxf(0.5f, s->gamma - 0.05f); if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 816, 559, 36, 26 })) { s->gamma = fminf(2.0f, s->gamma + 0.05f); if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 510, 622, 260, 44 })) { storage_save_settings(s); if (audio) audio_play_click(audio); return ACT_GOTO_MENU; }
    return ACT_NONE;
}

/* Left-column row: label @X=135 (dim), value @X=350; optional faint divider under the row. */
static void draw_stat_row(int row, const char *label, const char *value, int divider) {
    float y = 120.0f + row * 30.0f;
    DrawTextEx(small_font, label, (Vector2){ 135, y }, 18.0f, 2.0f, COLOR_PHOSPHOR_DIM);
    DrawTextEx(small_font, value, (Vector2){ 350, y }, 18.0f, 2.0f, COLOR_TEXT_INFO);
    if (divider) DrawLine(135, (int)(y + 24), 585, (int)(y + 24), (Color){ COLOR_PHOSPHOR_DIM.r, COLOR_PHOSPHOR_DIM.g, COLOR_PHOSPHOR_DIM.b, 60 });
}

void screens_draw_stats(const BoardAssets *ba, const GameStats *st, const MatchHistory *h, Vector2 vm, int confirm_reset, float log_timer) {
    draw_bg_if_loaded(ba, 180);
    DrawText("PLAYER STATISTICS", 500, 40, 32, COLOR_PHOSPHOR);
    Rectangle p = { 100, 90, 1080, 500 };
    DrawRectangleRec(p, COLOR_PANEL_BG);
    ui_draw_panel_border(p, COLOR_PHOSPHOR_DIM);

    float wr = (st->matches_played > 0) ? ((float)st->matches_won / st->matches_played * 100.0f) : 0.0f;
    draw_stat_row(0, "Matches Played", TextFormat("%d", st->matches_played), 0);
    draw_stat_row(1, "Won / Lost / Draw", TextFormat("%d / %d / %d", st->matches_won, st->matches_lost, st->matches_drawn), 0);
    draw_stat_row(2, "Win Rate", TextFormat("%.1f%%", wr), 1);
    draw_stat_row(3, "Rounds Won", TextFormat("YOU %d | CPU %d", st->rounds_won_you, st->rounds_won_cpu), 0);
    draw_stat_row(4, "Total Points", TextFormat("FOR %d | AGAINST %d", st->points_for, st->points_against), 0);
    draw_stat_row(5, "Highest Round Score", TextFormat("%d pts", st->best_award), 1);
    draw_stat_row(6, "Koi Calls", TextFormat("YOU %d | CPU %d", st->koi_calls_you, st->koi_calls_cpu), 0);
    draw_stat_row(7, "Difficulty", TextFormat("Easy %d | Hard %d | Master %d", st->easy_matches, st->hard_matches, st->master_matches), 1);
    draw_stat_row(8, "Current Streak", TextFormat("%d", st->current_streak), 0);
    draw_stat_row(9, "Max Win Streak", TextFormat("%d", st->best_streak), 0);
    draw_stat_row(10, "Favorite Combo", storage_get_favorite_combo(st), 0);

    DrawLine(620, 110, 620, 560, COLOR_PHOSPHOR_DIM);
    DrawText("LAST 10 MATCHES", 650, 120, 20, COLOR_PHOSPHOR);
    int hy = 155;
    for (int i = 0; i < h->count; i++) { DrawTextEx(small_font, h->lines[i], (Vector2){ 650, (float)hy }, 16.0f, 2.0f, COLOR_TEXT_INFO_DIM); hy += 28; }

    draw_btn((Rectangle){ 220, 610, 200, 44 }, "SAVE LOG", 20, vm, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 540, 610, 200, 44 }, "RESET STATS", 20, vm, COLOR_RED);
    draw_btn((Rectangle){ 860, 610, 200, 44 }, "BACK", 20, vm, COLOR_PHOSPHOR);
    if (log_timer > 0.0f) DrawTextEx(small_font, "stats_report.txt written!", (Vector2){ 230, 664 }, 16.0f, 2.0f, COLOR_TEXT_INFO);

    if (confirm_reset) {
        Rectangle modal = { 440, 240, 400, 200 };
        DrawRectangleRec(modal, (Color){ 0, 0, 0, 240 });
        ui_draw_panel_border(modal, COLOR_RED);
        DrawText("RESET ALL STATS & LOGS?", 490, 270, 20, COLOR_RED);
        DrawTextEx(small_font, "This action cannot be undone.", (Vector2){ 510, 305 }, 16.0f, 2.0f, COLOR_TEXT_INFO_DIM);
        draw_btn((Rectangle){ 480, 360, 140, 40 }, "CONFIRM", 18, vm, COLOR_RED);
        draw_btn((Rectangle){ 660, 360, 140, 40 }, "CANCEL", 18, vm, COLOR_PHOSPHOR);
    }
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
    ui_draw_panel_border(p, COLOR_PHOSPHOR);
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
    ui_draw_panel_border(p, COLOR_PHOSPHOR_DIM);

    DrawText(TextFormat("FINAL SCORE:   YOU %d  -  CPU %d", p0, p1), 380, 190, 26, COLOR_PHOSPHOR);
    DrawLine(380, 230, 900, 230, COLOR_PHOSPHOR_DIM);
    DrawText("ROUND LEDGER:", 380, 245, 20, COLOR_PHOSPHOR);
    for (int i = 0, ly = 275; i < g->ledger_count && i < 6; i++, ly += 26) {
        const RoundRecord *r = &g->ledger[i];
        const char *w = (r->winner == 0) ? "YOU" : (r->winner == 1) ? "CPU" : "DRAW";
        DrawTextEx(small_font, TextFormat("Round %d: %s (%d pts)", r->round_number, w, r->award), (Vector2){ 400, (float)ly }, 18.0f, 2.0f, COLOR_TEXT_INFO);
    }
    draw_btn((Rectangle){ 490, 470, 300, 46 }, "PLAY AGAIN", 20, vm, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 490, 526, 300, 46 }, "MAIN MENU", 20, vm, COLOR_PHOSPHOR);
}

ScreenAction screens_handle_match_result_click(Vector2 vm) {
    if (CheckCollisionPointRec(vm, (Rectangle){ 490, 470, 300, 46 })) return ACT_START_GAME;
    if (CheckCollisionPointRec(vm, (Rectangle){ 490, 526, 300, 46 })) return ACT_GOTO_MENU;
    return ACT_NONE;
}

void screens_draw_choice_overlay(const BoardAssets *ba, const GameState *g, Vector2 vm, int is_hand) {
    DrawRectangle(0, 0, 1280, 720, COLOR_OVERLAY_BG);
    DrawText("CHOOSE ONE OF THE MATCHING CARDS", (1280 - MeasureText("CHOOSE ONE OF THE MATCHING CARDS", 28)) / 2, 140, 28, COLOR_PHOSPHOR);
    Rectangle r0 = { 400, 180, CHOICE_CARD_W, CHOICE_CARD_H }, r1 = { 704, 180, CHOICE_CARD_W, CHOICE_CARD_H };
    board_draw_card(ba, g->choice_targets[0], r0); board_draw_card(ba, g->choice_targets[1], r1);
    if (CheckCollisionPointRec(vm, r0)) board_draw_card_glow(r0, 1.0f);
    if (CheckCollisionPointRec(vm, r1)) board_draw_card_glow(r1, 1.0f);
    if (is_hand) draw_btn((Rectangle){ 560, 500, 160, 44 }, "CANCEL", 20, vm, COLOR_RED);
}

int screens_check_choice_click(const GameState *g, Vector2 vm, int is_hand, int *out_target_id) {
    if (CheckCollisionPointRec(vm, (Rectangle){ 400, 180, CHOICE_CARD_W, CHOICE_CARD_H })) { *out_target_id = g->choice_targets[0]; return 1; }
    if (CheckCollisionPointRec(vm, (Rectangle){ 704, 180, CHOICE_CARD_W, CHOICE_CARD_H })) { *out_target_id = g->choice_targets[1]; return 1; }
    if (is_hand && CheckCollisionPointRec(vm, (Rectangle){ 560, 500, 160, 44 })) { *out_target_id = -1; return 2; }
    return 0;
}

void screens_draw_decide_overlay(const BoardAssets *ba, const GameState *g, Vector2 vm) {
    DrawRectangle(0, 0, 1280, 720, COLOR_OVERLAY_BG);
    Rectangle panel = { 200, 45, 880, 630 };
    DrawRectangleRec(panel, COLOR_PANEL_BG);
    ui_draw_panel_border(panel, COLOR_PHOSPHOR);

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
    DrawTextEx(small_font, "Continue round to aim for higher combinations.", (Vector2){ 280, 590 }, 15.0f, 2.0f, COLOR_TEXT_INFO_DIM);
    DrawTextEx(small_font, "Bank your current score and win this round.", (Vector2){ 680, 590 }, 15.0f, 2.0f, COLOR_TEXT_INFO_DIM);
}

int screens_check_decide_click(Vector2 vm, DecisionType *out_decision) {
    if (CheckCollisionPointRec(vm, (Rectangle){ 280, 520, 320, 60 })) { *out_decision = DECISION_KOI_KOI; return 1; }
    if (CheckCollisionPointRec(vm, (Rectangle){ 680, 520, 320, 60 })) { *out_decision = DECISION_SHOBU; return 1; }
    return 0;
}

void screens_draw_round_result(const BoardAssets *ba, const GameState *g, Vector2 vm) {
    DrawRectangle(0, 0, 1280, 720, COLOR_OVERLAY_BG);
    Rectangle panel = { 200, 45, 880, 630 };
    DrawRectangleRec(panel, COLOR_PANEL_BG);
    ui_draw_panel_border(panel, COLOR_PHOSPHOR);

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
            DrawTextEx(small_font, "LUCKY HAND (Teshi / Kuttsuki): 6 pts", (Vector2){ 230, (float)y + 10 }, 19.0f, 2.0f, COLOR_TEXT_INFO);
            int lcards[8], lcnt = 0;
            for (int i = 0; i < g->captured_count[rec->winner] && lcnt < 8; i++) lcards[lcnt++] = g->captured[rec->winner][i];
            draw_yaku_cards(ba, lcards, lcnt, 680.0f, (float)y);
            y += 48;
        } else if (rec->score_result.yaku_count == 0) {
            DrawTextEx(small_font, "OYA-KEN (Dealer's Privilege): 6 pts", (Vector2){ 230, (float)y + 10 }, 19.0f, 2.0f, COLOR_TEXT_INFO);
            y += 48;
        } else {
            const ScoreResult *sr = &rec->score_result;
            int max_show = (sr->yaku_count > 5) ? 5 : sr->yaku_count;
            for (int i = 0; i < max_show; i++) {
                const ScoredYaku *yaku = &sr->yakus[i];
                DrawTextEx(small_font, TextFormat("- %s", yaku->name), (Vector2){ 230, (float)y + 9 }, 18.0f, 2.0f, COLOR_TEXT_INFO);
                DrawTextEx(small_font, TextFormat("%d pts", yaku->points), (Vector2){ 600, (float)y + 9 }, 18.0f, 2.0f, COLOR_TEXT_INFO);
                int ycards[16], ycount = 0;
                get_yaku_cards(g, rec->winner, yaku, ycards, &ycount);
                draw_yaku_cards(ba, ycards, ycount, 680.0f, (float)y);
                y += 44;
            }
            if (sr->yaku_count > 5) {
                DrawTextEx(small_font, TextFormat("+ %d more combos...", sr->yaku_count - 5), (Vector2){ 230, (float)y + 4 }, 16.0f, 2.0f, COLOR_TEXT_INFO_DIM);
                y += 24;
            }
            y += 6;
            DrawLine(230, y, 1050, y, COLOR_PHOSPHOR_DIM);
            y += 10;
            DrawTextEx(small_font, TextFormat("Base: %d pts", rec->base_points), (Vector2){ 230, (float)y }, 18.0f, 2.0f, COLOR_TEXT_INFO); y += 22;
            if (rec->mult_seven > 1) { DrawTextEx(small_font, "x2 7+ Points Multiplier", (Vector2){ 230, (float)y }, 16.0f, 2.0f, COLOR_TEXT_INFO); y += 20; }
            if (rec->mult_koi > 1) { DrawTextEx(small_font, "x2 Opponent Koi-Koi Multiplier", (Vector2){ 230, (float)y }, 16.0f, 2.0f, COLOR_TEXT_INFO); y += 20; }
        }
        DrawText(TextFormat("ROUND AWARD: %d PTS", rec->award), 230, y + 4, 22, COLOR_PHOSPHOR);
    }

    DrawText(TextFormat("MATCH RUNNING SCORE:  YOU %d  -  CPU %d", g->total_score[0], g->total_score[1]), 230, 565, 20, COLOR_PHOSPHOR_DIM);
    const char *btn_text = (g->current_round >= g->total_rounds) ? "MATCH RESULTS" : "NEXT ROUND";
    draw_btn((Rectangle){ 520, 610, 240, 48 }, btn_text, 20, vm, COLOR_PHOSPHOR);
}

int screens_check_round_result_click(Vector2 vm) { return CheckCollisionPointRec(vm, (Rectangle){ 520, 610, 240, 48 }); }

void screens_draw_banner(const char *text) {
    int tw = MeasureText(text, 28);
    Rectangle br = { (1280 - tw) / 2.0f - 30.0f, 320.0f, tw + 60.0f, 60.0f };
    DrawRectangleRec(br, (Color){ 0, 0, 0, 230 });
    ui_draw_panel_border(br, COLOR_PHOSPHOR);
    DrawText(text, (1280 - tw) / 2, 336, 28, COLOR_PHOSPHOR);
}
