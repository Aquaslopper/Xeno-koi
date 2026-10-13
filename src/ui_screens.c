#include "ui_screens.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

extern Font font_title;
extern Font font_body;

static void draw_text_ex_snapped(Font font, const char *text, Vector2 position,
                                 float font_size, float spacing, Color color) {
    position.x = floorf(position.x);
    position.y = floorf(position.y);
    DrawTextEx(font, text, position, font_size, spacing, color);
}

static void draw_centered_text(Font font, const char *text, float center_x, float y,
                               float font_size, float spacing, Color color) {
    Vector2 text_size = MeasureTextEx(font, text, font_size, spacing);
    draw_text_ex_snapped(font, text, (Vector2){ center_x - text_size.x / 2.0f, y },
               font_size, spacing, color);
}

static void draw_text_in_rect(Font font, const char *text, Rectangle bounds,
                              float font_size, float spacing, Color color) {
    Vector2 text_size = MeasureTextEx(font, text, font_size, spacing);
    draw_text_ex_snapped(font, text,
               (Vector2){ bounds.x + (bounds.width - text_size.x) / 2.0f,
                          bounds.y + (bounds.height - font_size) / 2.0f },
               font_size, spacing, color);
}

static int draw_btn(Rectangle r, const char *txt, int fsz, Vector2 vm, Color txt_col) {
    int hov = CheckCollisionPointRec(vm, r);
    DrawRectangleRec(r, hov ? COLOR_BTN_HOVER_BG : COLOR_PANEL_BG);
    ui_draw_panel_border(r, hov ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    const float font_size = (float)fsz;
    const float spacing = 1.2f;
    Vector2 text_size = MeasureTextEx(font_title, txt, font_size, spacing);
    float tx = floorf(r.x + (r.width - text_size.x) * 0.5f);
    float ty = floorf(r.y + (r.height - font_size) * 0.5f);
    draw_text_ex_snapped(font_title, txt, (Vector2){ tx, ty }, font_size, spacing, txt_col);
    return hov;
}

/* Selector contents are data values, so retain the Play body face inside controls. */
static void draw_option_value(Rectangle r, const char *text, int fsz,
                              Vector2 vm, Color text_color) {
    int hov = CheckCollisionPointRec(vm, r);
    DrawRectangleRec(r, hov ? COLOR_BTN_HOVER_BG : COLOR_PANEL_BG);
    ui_draw_panel_border(r, hov ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    draw_text_in_rect(font_body, text, r, (float)fsz, 1.0f, text_color);
}

static void draw_section_header(const char *text, float y) {
    draw_text_ex_snapped(font_title, text, (Vector2){ 340.0f, y }, 17.0f, 1.2f, COLOR_PHOSPHOR);
    DrawLine(340, (int)(y + 21.0f), 900, (int)(y + 21.0f), COLOR_PHOSPHOR_DIM);
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
    draw_centered_text(font_title, "[XENO / KOI]", 640.0f, 78.0f, 52.0f, 2.0f, COLOR_PHOSPHOR);
    draw_centered_text(font_body, "BIOMECHANICAL EDITION", 640.0f, 151.0f, 20.0f, 2.0f, COLOR_PHOSPHOR_DIM);

    const char *lbls[6] = { "START MATCH", "OPTION", "STATS", "SAVE", "CREDITS", "QUIT" };
    for (int i = 0; i < 6; i++) draw_btn((Rectangle){ 490, 235 + i * 50, 300, 40 }, lbls[i], 18, vm, COLOR_PHOSPHOR);
    if (saved_timer > 0.0f) draw_centered_text(font_body, "ALL FILES SAVED", 640.0f, 555.0f, 20.0f, 1.5f, COLOR_PHOSPHOR);
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
    draw_centered_text(font_title, "CREDITS", 640.0f, 100.0f, 32.0f, 1.6f, COLOR_PHOSPHOR);
    draw_centered_text(font_body, "[]", 640.0f, 145.0f, 24.0f, 1.5f, COLOR_PHOSPHOR_DIM);
    draw_centered_text(font_title, "XENO / KOI", 640.0f, 190.0f, 42.0f, 1.8f, COLOR_PHOSPHOR);
    draw_centered_text(font_body, "Vibe Coder: [Aquaslopper]", 640.0f, 275.0f, 22.0f, 1.5f, COLOR_PHOSPHOR);
    draw_centered_text(font_body, "Music: [Graf Punk]", 640.0f, 330.0f, 22.0f, 1.5f, COLOR_PHOSPHOR);
    draw_centered_text(font_body, "Development Platform: Arena.ai - Agent mode.", 640.0f, 385.0f, 20.0f, 1.4f, COLOR_PHOSPHOR);
    draw_centered_text(font_body, "[]", 640.0f, 445.0f, 24.0f, 1.5f, COLOR_PHOSPHOR_DIM);
    draw_btn((Rectangle){ 520, 530, 240, 46 }, "BACK", 22, vm, COLOR_PHOSPHOR);
}

ScreenAction screens_handle_credits_click(Vector2 vm) {
    if (CheckCollisionPointRec(vm, (Rectangle){ 520, 530, 240, 46 })) return ACT_GOTO_MENU;
    return ACT_NONE;
}

static void draw_slider(int y, float val, const char *lbl, const char *val_txt, Vector2 vm) {
    draw_text_ex_snapped(font_body, lbl, (Vector2){ 340.0f, (float)y + 4.0f }, 18.0f, 1.2f, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 580, (float)y, 36, 26 }, "-", 18, vm, COLOR_PHOSPHOR);
    Rectangle track = { 626, (float)y + 5, 180, 16 };
    DrawRectangleRec(track, COLOR_PANEL_BG);
    DrawRectangleLinesEx(track, 1.0f, COLOR_PHOSPHOR_DIM);
    DrawRectangle(626, y + 5, (int)(180.0f * val), 16, COLOR_PHOSPHOR);
    DrawRectangle((int)(626 + 180.0f * val - 3), y + 1, 6, 24, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 816, (float)y, 36, 26 }, "+", 18, vm, COLOR_PHOSPHOR);
    draw_text_ex_snapped(font_body, val_txt, (Vector2){ 865.0f, (float)y + 4.0f }, 17.0f, 1.2f, COLOR_TEXT_INFO);
}

void screens_draw_options(const BoardAssets *ba, GameSettings *s, Vector2 vm) {
    draw_bg_if_loaded(ba, 180);
    draw_centered_text(font_title, "SYSTEM CONFIGURATION", 640.0f, 8.0f, 32.0f, 1.6f, COLOR_PHOSPHOR);

    /* Group the dense controls into clear, title-font sections. */
    draw_section_header("MATCH RULES", 47.0f);
    draw_text_ex_snapped(font_body, "Match Length:", (Vector2){ 340.0f, 75.0f }, 18.0f, 1.2f, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 580, 70, 44, 26 }, "-", 18, vm, COLOR_PHOSPHOR);
    draw_centered_text(font_body, TextFormat("%d Rounds", s->rounds), 697.0f, 74.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
    draw_btn((Rectangle){ 770, 70, 44, 26 }, "+", 18, vm, COLOR_PHOSPHOR);

    draw_text_ex_snapped(font_body, "Ruleset:", (Vector2){ 340.0f, 110.0f }, 18.0f, 1.2f, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 580, 105, 44, 26 }, "<", 18, vm, COLOR_PHOSPHOR);
    const char *r_names[5] = { "NINTENDO", "FUDAWIKI", "KK-ADD-10", "TOURNAMENT", "YAKUZA" };
    Color dbg = (s->ruleset == RULESET_NINTENDO || s->ruleset == RULESET_KK_ADD_10) ? (Color){ 20, 160, 40, 55 } :
                (s->ruleset == RULESET_FUDAWIKI) ? (Color){ 210, 140, 20, 55 } : (Color){ 210, 30, 30, 55 };
    Color dbdr = (s->ruleset == RULESET_NINTENDO || s->ruleset == RULESET_KK_ADD_10) ? (Color){ 30, 220, 60, 140 } :
                 (s->ruleset == RULESET_FUDAWIKI) ? (Color){ 240, 160, 30, 140 } : (Color){ 240, 40, 40, 140 };
    Rectangle rbox = { 630, 105, 144, 26 };
    DrawRectangleRec(rbox, dbg);
    DrawRectangleLinesEx(rbox, 1.0f, dbdr);
    draw_text_in_rect(font_body, r_names[s->ruleset], rbox, 15.0f, 1.0f, COLOR_TEXT_INFO);
    draw_btn((Rectangle){ 780, 105, 44, 26 }, ">", 18, vm, COLOR_PHOSPHOR);

    draw_section_header("AI DIFFICULTY", 145.0f);
    draw_text_ex_snapped(font_body, "Difficulty:", (Vector2){ 340.0f, 171.0f }, 18.0f, 1.2f, COLOR_PHOSPHOR);
    const char *d_names[3] = { "EASY", "HARD", "MASTER" };
    draw_btn((Rectangle){ 580, 166, 44, 26 }, "<", 18, vm, COLOR_PHOSPHOR);
    draw_option_value((Rectangle){ 630, 166, 140, 26 }, d_names[s->difficulty], 16, vm,
                      (s->difficulty == DIFFICULTY_MASTER) ? (Color){ 255, 60, 60, 255 } : COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 776, 166, 44, 26 }, ">", 18, vm, COLOR_PHOSPHOR);

    draw_text_ex_snapped(font_body, "Master Timer:", (Vector2){ 340.0f, 206.0f }, 18.0f, 1.2f,
               (s->difficulty == DIFFICULTY_MASTER) ? (Color){ 255, 100, 100, 255 } : COLOR_PHOSPHOR_DIM);
    if (s->difficulty == DIFFICULTY_MASTER) {
        draw_option_value((Rectangle){ 580, 201, 105, 26 }, "15 SEC", 15, vm,
                          (s->master_timer == 15) ? (Color){ 255, 80, 80, 255 } : COLOR_PHOSPHOR_DIM);
        draw_option_value((Rectangle){ 695, 201, 105, 26 }, "30 SEC", 15, vm,
                          (s->master_timer == 30) ? (Color){ 255, 80, 80, 255 } : COLOR_PHOSPHOR_DIM);
    } else {
        draw_text_ex_snapped(font_body, "[ MASTER ONLY ]", (Vector2){ 580.0f, 206.0f }, 16.0f, 1.2f, COLOR_PHOSPHOR_DIM);
    }

    draw_section_header("AUDIO & VISUAL", 241.0f);
    const char *cnames[5] = { "GREEN", "AMBER", "TEAL", "CRIMSON", "VIOLET" };
    draw_text_ex_snapped(font_body, "Terminal Color:", (Vector2){ 340.0f, 267.0f }, 18.0f, 1.2f, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 580, 262, 44, 26 }, "<", 18, vm, COLOR_PHOSPHOR);
    draw_option_value((Rectangle){ 630, 262, 140, 26 }, cnames[s->terminal_color % 5], 15, vm, COLOR_TEXT_INFO);
    draw_btn((Rectangle){ 776, 262, 44, 26 }, ">", 18, vm, COLOR_PHOSPHOR);

    const char *bg_names[5] = { "0: Harvest", "1: Hole", "2: Arctic", "3: Castle", "4: City" };
    draw_text_ex_snapped(font_body, "Background:", (Vector2){ 340.0f, 302.0f }, 18.0f, 1.2f, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 580, 297, 44, 26 }, "<", 18, vm, COLOR_PHOSPHOR);
    draw_option_value((Rectangle){ 630, 297, 140, 26 }, bg_names[s->background], 15, vm, COLOR_TEXT_INFO);
    draw_btn((Rectangle){ 780, 297, 44, 26 }, ">", 18, vm, COLOR_PHOSPHOR);
    if (s->background >= 0 && s->background < 5 && ba->bgs[s->background].id > 0) {
        Rectangle prev = { 850, 291, 76, 38 };
        DrawTexturePro(ba->bgs[s->background], (Rectangle){0,0,1280,720}, prev, (Vector2){0}, 0, WHITE);
        DrawRectangleLinesEx(prev, 1.0f, COLOR_PHOSPHOR_DIM);
    }

    draw_text_ex_snapped(font_body, "Anim Speed:", (Vector2){ 340.0f, 337.0f }, 18.0f, 1.2f, COLOR_PHOSPHOR);
    draw_option_value((Rectangle){ 580, 332, 95, 26 }, "SLOW", 15, vm,
                      (s->anim_speed == 0) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    draw_option_value((Rectangle){ 690, 332, 95, 26 }, "NORMAL", 15, vm,
                      (s->anim_speed == 1) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    draw_option_value((Rectangle){ 800, 332, 95, 26 }, "FAST", 15, vm,
                      (s->anim_speed == 2) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);

    draw_slider(368, s->bgm_volume, "BGM Volume:", TextFormat("%d%%", (int)(s->bgm_volume * 100.0f + 0.5f)), vm);
    draw_slider(403, s->sfx_volume, "SFX Volume:", TextFormat("%d%%", (int)(s->sfx_volume * 100.0f + 0.5f)), vm);

    draw_text_ex_snapped(font_body, "Fullscreen:", (Vector2){ 340.0f, 442.0f }, 18.0f, 1.2f, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 580, 438, 44, 26 }, "<", 18, vm, COLOR_PHOSPHOR);
    const char *fs_names[2] = { "WINDOWED", "BORDERLESS" };
    draw_option_value((Rectangle){ 630, 438, 140, 26 }, fs_names[s->fullscreen ? 1 : 0], 15, vm, COLOR_TEXT_INFO);
    draw_btn((Rectangle){ 776, 438, 44, 26 }, ">", 18, vm, COLOR_PHOSPHOR);

    draw_text_ex_snapped(font_body, "Bloom Effect:", (Vector2){ 340.0f, 477.0f }, 18.0f, 1.2f, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 580, 473, 44, 26 }, "<", 18, vm, COLOR_PHOSPHOR);
    const char *bl_names[3] = { "OFF", "MID", "FULL" };
    draw_option_value((Rectangle){ 630, 473, 140, 26 }, bl_names[s->bloom % 3], 15, vm,
                      (s->bloom > 0) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    draw_btn((Rectangle){ 776, 473, 44, 26 }, ">", 18, vm, COLOR_PHOSPHOR);

    draw_text_ex_snapped(font_body, "CRT Filter:", (Vector2){ 340.0f, 512.0f }, 18.0f, 1.2f, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 580, 508, 44, 26 }, "<", 18, vm, COLOR_PHOSPHOR);
    const char *crt_names[4] = { "0: OFF", "1: LOW", "2: MID", "3: HIGH" };
    draw_option_value((Rectangle){ 630, 508, 140, 26 }, crt_names[s->crt_filter % 4], 15, vm,
                      (s->crt_filter > 0) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    draw_btn((Rectangle){ 776, 508, 44, 26 }, ">", 18, vm, COLOR_PHOSPHOR);

    float g_frac = fmaxf(0.0f, fminf(1.0f, (s->gamma - 0.5f) / 1.5f));
    draw_slider(543, g_frac, "Display Gamma:", TextFormat("%.2f", s->gamma), vm);

    draw_btn((Rectangle){ 510, 622, 260, 44 }, "BACK (SAVE)", 20, vm, COLOR_PHOSPHOR);
}

ScreenAction screens_handle_options_click(GameSettings *s, BoardAssets *ba, AudioAssets *audio, Vector2 vm) {
    int pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    int down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    if (down) {
        if (CheckCollisionPointRec(vm, (Rectangle){ 626, 368, 180, 26 })) {
            s->bgm_volume = fmaxf(0.0f, fminf(1.0f, (vm.x - 626.0f) / 180.0f));
            if (audio) audio_set_bgm_volume(audio, s->bgm_volume);
        } else if (CheckCollisionPointRec(vm, (Rectangle){ 626, 403, 180, 26 })) {
            s->sfx_volume = fmaxf(0.0f, fminf(1.0f, (vm.x - 626.0f) / 180.0f));
            if (audio) { audio_set_sfx_volume(audio, s->sfx_volume); if (pressed) audio_play_click(audio); }
        } else if (CheckCollisionPointRec(vm, (Rectangle){ 626, 543, 180, 26 })) {
            s->gamma = 0.5f + fmaxf(0.0f, fminf(1.0f, (vm.x - 626.0f) / 180.0f)) * 1.5f;
            if (pressed && audio) audio_play_click(audio);
        }
    }
    if (!pressed) return ACT_NONE;

    if (CheckCollisionPointRec(vm, (Rectangle){ 580, 70, 44, 26 })) { if (s->rounds > 1) s->rounds--; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 770, 70, 44, 26 })) { if (s->rounds < 12) s->rounds++; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 580, 166, 44, 26 })) { s->difficulty = (Difficulty)((s->difficulty + 2) % 3); if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 630, 166, 140, 26 }) || CheckCollisionPointRec(vm, (Rectangle){ 776, 166, 44, 26 })) { s->difficulty = (Difficulty)((s->difficulty + 1) % 3); if (audio) audio_play_click(audio); }
    else if (s->difficulty == DIFFICULTY_MASTER && CheckCollisionPointRec(vm, (Rectangle){ 580, 201, 105, 26 })) { s->master_timer = 15; if (audio) audio_play_click(audio); }
    else if (s->difficulty == DIFFICULTY_MASTER && CheckCollisionPointRec(vm, (Rectangle){ 695, 201, 105, 26 })) { s->master_timer = 30; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 580, 105, 44, 26 })) { s->ruleset = (s->ruleset + 4) % 5; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 630, 105, 144, 26 }) || CheckCollisionPointRec(vm, (Rectangle){ 780, 105, 44, 26 })) { s->ruleset = (s->ruleset + 1) % 5; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 580, 262, 44, 26 })) { s->terminal_color = (s->terminal_color + 4) % 5; theme_set(s->terminal_color); if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 630, 262, 140, 26 }) || CheckCollisionPointRec(vm, (Rectangle){ 776, 262, 44, 26 })) { s->terminal_color = (s->terminal_color + 1) % 5; theme_set(s->terminal_color); if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 580, 297, 44, 26 })) { s->background = (s->background + 4) % 5; ba->current_bg = s->background; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 630, 297, 140, 26 }) || CheckCollisionPointRec(vm, (Rectangle){ 780, 297, 44, 26 })) { s->background = (s->background + 1) % 5; ba->current_bg = s->background; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 580, 332, 95, 26 })) { s->anim_speed = 0; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 690, 332, 95, 26 })) { s->anim_speed = 1; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 800, 332, 95, 26 })) { s->anim_speed = 2; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 580, 368, 36, 26 })) { s->bgm_volume = fmaxf(0.0f, s->bgm_volume - 0.10f); if (audio) { audio_set_bgm_volume(audio, s->bgm_volume); audio_play_click(audio); } }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 816, 368, 36, 26 })) { s->bgm_volume = fminf(1.0f, s->bgm_volume + 0.10f); if (audio) { audio_set_bgm_volume(audio, s->bgm_volume); audio_play_click(audio); } }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 580, 403, 36, 26 })) { s->sfx_volume = fmaxf(0.0f, s->sfx_volume - 0.10f); if (audio) { audio_set_sfx_volume(audio, s->sfx_volume); audio_play_click(audio); } }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 816, 403, 36, 26 })) { s->sfx_volume = fminf(1.0f, s->sfx_volume + 0.10f); if (audio) { audio_set_sfx_volume(audio, s->sfx_volume); audio_play_click(audio); } }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 580, 438, 44, 26 }) || CheckCollisionPointRec(vm, (Rectangle){ 630, 438, 140, 26 }) || CheckCollisionPointRec(vm, (Rectangle){ 776, 438, 44, 26 })) {
        s->fullscreen = 1 - s->fullscreen; ToggleBorderlessWindowed(); if (audio) audio_play_click(audio);
    }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 580, 473, 44, 26 })) { s->bloom = (s->bloom + 2) % 3; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 630, 473, 140, 26 }) || CheckCollisionPointRec(vm, (Rectangle){ 776, 473, 44, 26 })) { s->bloom = (s->bloom + 1) % 3; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 580, 508, 44, 26 })) { s->crt_filter = (s->crt_filter + 3) % 4; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 630, 508, 140, 26 }) || CheckCollisionPointRec(vm, (Rectangle){ 776, 508, 44, 26 })) { s->crt_filter = (s->crt_filter + 1) % 4; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 580, 543, 36, 26 })) { s->gamma = fmaxf(0.5f, s->gamma - 0.05f); if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 816, 543, 36, 26 })) { s->gamma = fminf(2.0f, s->gamma + 0.05f); if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 510, 622, 260, 44 })) { storage_save_settings(s); if (audio) audio_play_click(audio); return ACT_GOTO_MENU; }
    return ACT_NONE;
}

/* Left-column statistics table: labels and values use the readable body face. */
static void draw_stat_row(int row, const char *label, const char *value, int divider) {
    float y = 120.0f + row * 30.0f;
    draw_text_ex_snapped(font_body, label, (Vector2){ 135.0f, y }, 18.0f, 1.2f, COLOR_PHOSPHOR_DIM);
    draw_text_ex_snapped(font_body, value, (Vector2){ 350.0f, y }, 18.0f, 1.2f, COLOR_TEXT_INFO);
    if (divider) DrawLine(135, (int)(y + 24), 585, (int)(y + 24), (Color){ COLOR_PHOSPHOR_DIM.r, COLOR_PHOSPHOR_DIM.g, COLOR_PHOSPHOR_DIM.b, 60 });
}

void screens_draw_stats(const BoardAssets *ba, const GameStats *st, const MatchHistory *h, Vector2 vm, int confirm_reset, float log_timer) {
    draw_bg_if_loaded(ba, 180);
    draw_centered_text(font_title, "COMBAT TELEMETRY // STATS", 640.0f, 34.0f, 30.0f, 1.4f, COLOR_PHOSPHOR);
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
    draw_text_ex_snapped(font_title, "LAST 10 MATCHES", (Vector2){ 650.0f, 120.0f }, 18.0f, 1.2f, COLOR_PHOSPHOR);
    int hy = 155;
    for (int i = 0; i < h->count; i++) {
        draw_text_ex_snapped(font_body, h->lines[i], (Vector2){ 650.0f, (float)hy }, 16.0f, 1.2f, COLOR_TEXT_INFO_DIM);
        hy += 28;
    }

    draw_btn((Rectangle){ 220, 610, 200, 44 }, "SAVE LOG", 20, vm, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 540, 610, 200, 44 }, "RESET STATS", 20, vm, COLOR_RED);
    draw_btn((Rectangle){ 860, 610, 200, 44 }, "BACK", 20, vm, COLOR_PHOSPHOR);
    if (log_timer > 0.0f) draw_text_ex_snapped(font_body, "stats_report.txt written!", (Vector2){ 230.0f, 664.0f }, 16.0f, 1.2f, COLOR_TEXT_INFO);

    if (confirm_reset) {
        Rectangle modal = { 440, 240, 400, 200 };
        DrawRectangleRec(modal, (Color){ 0, 0, 0, 240 });
        ui_draw_panel_border(modal, COLOR_RED);
        draw_centered_text(font_title, "RESET ALL STATS & LOGS?", 640.0f, 270.0f, 20.0f, 1.1f, COLOR_RED);
        draw_text_ex_snapped(font_body, "This action cannot be undone.", (Vector2){ 510.0f, 305.0f }, 16.0f, 1.2f, COLOR_TEXT_INFO_DIM);
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
    draw_centered_text(font_title, "GAME PAUSED", 640.0f, 210.0f, 28.0f, 1.4f, COLOR_PHOSPHOR);

    if (confirm_restart) {
        draw_centered_text(font_body, "Restart this match?", 640.0f, 280.0f, 20.0f, 1.2f, COLOR_PHOSPHOR);
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
    draw_centered_text(font_title, "MATCH OVER", 640.0f, 30.0f, 27.0f, 1.4f, COLOR_PHOSPHOR_DIM);
    draw_centered_text(font_title, title, 640.0f, 72.0f, 42.0f, 1.7f, tcol);

    Rectangle p = { 340, 150, 600, 440 };
    DrawRectangleRec(p, COLOR_PANEL_BG);
    ui_draw_panel_border(p, COLOR_PHOSPHOR_DIM);

    draw_text_ex_snapped(font_body, TextFormat("FINAL SCORE:   YOU %d  -  CPU %d", p0, p1), (Vector2){ 380.0f, 190.0f }, 24.0f, 1.3f, COLOR_PHOSPHOR);
    DrawLine(380, 230, 900, 230, COLOR_PHOSPHOR_DIM);
    draw_text_ex_snapped(font_title, "ROUND LEDGER:", (Vector2){ 380.0f, 245.0f }, 18.0f, 1.2f, COLOR_PHOSPHOR);
    for (int i = 0, ly = 275; i < g->ledger_count && i < 6; i++, ly += 26) {
        const RoundRecord *r = &g->ledger[i];
        const char *w = (r->winner == 0) ? "YOU" : (r->winner == 1) ? "CPU" : "DRAW";
        draw_text_ex_snapped(font_body, TextFormat("Round %d: %s (%d pts)", r->round_number, w, r->award),
                   (Vector2){ 400.0f, (float)ly }, 18.0f, 1.2f, COLOR_TEXT_INFO);
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
    draw_centered_text(font_title, "CHOOSE ONE OF THE MATCHING CARDS", 640.0f, 140.0f, 28.0f, 1.2f, COLOR_PHOSPHOR);
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

    draw_centered_text(font_title, "YAKU DETECTED", 640.0f, 56.0f, 25.0f, 1.4f, COLOR_PHOSPHOR);
    draw_centered_text(font_title, "KOI-KOI / SHOBU", 640.0f, 86.0f, 22.0f, 1.2f, COLOR_PHOSPHOR_DIM);

    int y = 122;
    int max_show = (sr.yaku_count > 6) ? 6 : sr.yaku_count;
    for (int i = 0; i < max_show; i++) {
        const ScoredYaku *yaku = &sr.yakus[i];
        draw_text_ex_snapped(font_body, TextFormat("- %s", yaku->name), (Vector2){ 240.0f, (float)y + 9.0f }, 20.0f, 1.3f, COLOR_TEXT_INFO);
        draw_text_ex_snapped(font_body, TextFormat("%d pts", yaku->points), (Vector2){ 600.0f, (float)y + 9.0f }, 20.0f, 1.3f, COLOR_TEXT_INFO);
        int ycards[16], ycount = 0;
        get_yaku_cards(g, p, yaku, ycards, &ycount);
        draw_yaku_cards(ba, ycards, ycount, 690.0f, (float)y);
        y += 46;
    }

    DrawLine(240, 430, 1040, 430, COLOR_PHOSPHOR_DIM);
    draw_text_ex_snapped(font_body, TextFormat("Base: %d pts", sr.base_points), (Vector2){ 240.0f, 442.0f }, 20.0f, 1.3f, COLOR_TEXT_INFO);
    if (sr.base_points >= 7) draw_text_ex_snapped(font_body, "x2  7+ Multiplier", (Vector2){ 420.0f, 442.0f }, 18.0f, 1.3f, COLOR_TEXT_INFO);
    if (g->koi_called[opp] > 0) draw_text_ex_snapped(font_body, "x2  Koi-Koi Multiplier", (Vector2){ 660.0f, 442.0f }, 18.0f, 1.3f, COLOR_TEXT_INFO);
    draw_text_ex_snapped(font_body, TextFormat("IF YOU SHOBU NOW: %d POINTS", award), (Vector2){ 240.0f, 475.0f }, 22.0f, 1.3f, COLOR_PHOSPHOR);

    draw_btn((Rectangle){ 280, 520, 320, 60 }, "KOI-KOI (CONTINUE)", 20, vm, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 680, 520, 320, 60 }, "SHOBU (BANK & END)", 20, vm, COLOR_PHOSPHOR);
    draw_text_ex_snapped(font_body, "Continue round to aim for higher combinations.", (Vector2){ 280.0f, 590.0f }, 15.0f, 1.2f, COLOR_TEXT_INFO_DIM);
    draw_text_ex_snapped(font_body, "Bank your current score and win this round.", (Vector2){ 680.0f, 590.0f }, 15.0f, 1.2f, COLOR_TEXT_INFO_DIM);
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
    draw_centered_text(font_title, title, 640.0f, 65.0f, 28.0f, 1.3f, tcol);

    if (rec->winner < 0) {
        if (rec->draw_reason == 1) draw_text_ex_snapped(font_body, "Reason: Exhaustion (No one scored)", (Vector2){ 460.0f, 150.0f }, 20.0f, 1.2f, COLOR_PHOSPHOR_DIM);
        else if (rec->draw_reason == 2) draw_text_ex_snapped(font_body, "Reason: Both players had Lucky Hands", (Vector2){ 460.0f, 150.0f }, 20.0f, 1.2f, COLOR_PHOSPHOR_DIM);
    } else {
        int y = 110;
        if (rec->lucky_hand) {
            draw_text_ex_snapped(font_body, "LUCKY HAND (Teshi / Kuttsuki): 6 pts", (Vector2){ 230.0f, (float)y + 10.0f }, 19.0f, 1.3f, COLOR_TEXT_INFO);
            int lcards[8], lcnt = 0;
            for (int i = 0; i < g->captured_count[rec->winner] && lcnt < 8; i++) lcards[lcnt++] = g->captured[rec->winner][i];
            draw_yaku_cards(ba, lcards, lcnt, 680.0f, (float)y);
            y += 48;
        } else if (rec->score_result.yaku_count == 0) {
            draw_text_ex_snapped(font_body, "OYA-KEN (Dealer's Privilege): 6 pts", (Vector2){ 230.0f, (float)y + 10.0f }, 19.0f, 1.3f, COLOR_TEXT_INFO);
            y += 48;
        } else {
            const ScoreResult *sr = &rec->score_result;
            int max_show = (sr->yaku_count > 5) ? 5 : sr->yaku_count;
            for (int i = 0; i < max_show; i++) {
                const ScoredYaku *yaku = &sr->yakus[i];
                draw_text_ex_snapped(font_body, TextFormat("- %s", yaku->name), (Vector2){ 230.0f, (float)y + 9.0f }, 18.0f, 1.2f, COLOR_TEXT_INFO);
                draw_text_ex_snapped(font_body, TextFormat("%d pts", yaku->points), (Vector2){ 600.0f, (float)y + 9.0f }, 18.0f, 1.2f, COLOR_TEXT_INFO);
                int ycards[16], ycount = 0;
                get_yaku_cards(g, rec->winner, yaku, ycards, &ycount);
                draw_yaku_cards(ba, ycards, ycount, 680.0f, (float)y);
                y += 44;
            }
            if (sr->yaku_count > 5) {
                draw_text_ex_snapped(font_body, TextFormat("+ %d more combos...", sr->yaku_count - 5), (Vector2){ 230.0f, (float)y + 4.0f }, 16.0f, 1.2f, COLOR_TEXT_INFO_DIM);
                y += 24;
            }
            y += 6;
            DrawLine(230, y, 1050, y, COLOR_PHOSPHOR_DIM);
            y += 10;
            draw_text_ex_snapped(font_body, TextFormat("Base: %d pts", rec->base_points), (Vector2){ 230.0f, (float)y }, 18.0f, 1.2f, COLOR_TEXT_INFO); y += 22;
            if (rec->mult_seven > 1) { draw_text_ex_snapped(font_body, "x2 7+ Points Multiplier", (Vector2){ 230.0f, (float)y }, 16.0f, 1.2f, COLOR_TEXT_INFO); y += 20; }
            if (rec->mult_koi > 1) { draw_text_ex_snapped(font_body, "x2 Opponent Koi-Koi Multiplier", (Vector2){ 230.0f, (float)y }, 16.0f, 1.2f, COLOR_TEXT_INFO); y += 20; }
        }
        draw_text_ex_snapped(font_body, TextFormat("ROUND AWARD: %d PTS", rec->award), (Vector2){ 230.0f, (float)y + 4.0f }, 22.0f, 1.2f, COLOR_PHOSPHOR);
    }

    draw_text_ex_snapped(font_body, TextFormat("MATCH RUNNING SCORE:  YOU %d  -  CPU %d", g->total_score[0], g->total_score[1]),
               (Vector2){ 230.0f, 565.0f }, 20.0f, 1.2f, COLOR_PHOSPHOR_DIM);
    const char *btn_text = (g->current_round >= g->total_rounds) ? "MATCH RESULTS" : "NEXT ROUND";
    draw_btn((Rectangle){ 520, 610, 240, 48 }, btn_text, 20, vm, COLOR_PHOSPHOR);
}

int screens_check_round_result_click(Vector2 vm) { return CheckCollisionPointRec(vm, (Rectangle){ 520, 610, 240, 48 }); }

void screens_draw_banner(const char *text) {
    const float font_size = 28.0f;
    const float spacing = 1.4f;
    Vector2 text_size = MeasureTextEx(font_title, text, font_size, spacing);
    Rectangle br = { (1280.0f - text_size.x) / 2.0f - 30.0f, 320.0f, text_size.x + 60.0f, 60.0f };
    DrawRectangleRec(br, (Color){ 0, 0, 0, 230 });
    ui_draw_panel_border(br, COLOR_PHOSPHOR);
    draw_text_ex_snapped(font_title, text, (Vector2){ (1280.0f - text_size.x) / 2.0f, 336.0f }, font_size, spacing, COLOR_PHOSPHOR);
}
