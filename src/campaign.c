#include "campaign.h"
#include "cards.h"
#include "storage.h"
#include "ui_theme.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define DEUS_UNLOCKED_FOR_TESTING 1

extern Font font_title;
extern Font font_body;

static void draw_text_wrapped(Font font, const char *text, float x, float y, float max_width, float font_size, float spacing, Color color) {
    if (!text || text[0] == '\0') return;
    char line[512];
    size_t line_len = 0;
    line[0] = '\0';
    float cur_y = y;
    float line_height = font_size + 4.0f;

    const char *p = text;
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        const char *w_start = p;
        while (*p && *p != ' ') p++;
        size_t w_len = (size_t)(p - w_start);
        if (w_len >= sizeof(line) - 2) w_len = sizeof(line) - 2;

        if (line_len == 0) {
            memcpy(line, w_start, w_len);
            line[w_len] = '\0';
            line_len = w_len;
        } else if (line_len + 1 + w_len < sizeof(line)) {
            line[line_len] = ' ';
            memcpy(line + line_len + 1, w_start, w_len);
            line[line_len + 1 + w_len] = '\0';
            if (MeasureTextEx(font, line, font_size, spacing).x > max_width) {
                line[line_len] = '\0';
                DrawTextEx(font, line, (Vector2){ x, cur_y }, font_size, spacing, color);
                cur_y += line_height;
                memcpy(line, w_start, w_len);
                line[w_len] = '\0';
                line_len = w_len;
            } else {
                line_len += 1 + w_len;
            }
        }
    }
    if (line_len > 0) {
        DrawTextEx(font, line, (Vector2){ x, cur_y }, font_size, spacing, color);
    }
}

static int draw_btn_custom(Rectangle r, const char *txt, int fsz, Vector2 vm, Color col) {
    int hov = CheckCollisionPointRec(vm, r);
    DrawRectangleRec(r, hov ? COLOR_BTN_HOVER_BG : COLOR_PANEL_BG);
    DrawRectangleLinesEx(r, 1.0f, hov ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    const float font_size = (float)fsz;
    const float spacing = 1.2f;
    Vector2 text_size = MeasureTextEx(font_title, txt, font_size, spacing);
    DrawTextEx(font_title, txt,
               (Vector2){ floorf(r.x + (r.width - text_size.x) / 2.0f),
                          floorf(r.y + (r.height - font_size) / 2.0f) },
               font_size, spacing, col);
    return hov;
}

void campaign_draw_overlay(const BoardAssets *ba, const GameSettings *settings, int selected_opponent, Vector2 vm) {
    int unlocked_opponents = settings->unlocked_opponents;
    if (selected_opponent < 0 || selected_opponent >= CAMPAIGN_OPPONENT_COUNT) selected_opponent = 0;

    /* Dim background */
    DrawRectangle(0, 0, 1280, 720, (Color){ 0, 0, 0, 225 });

    Rectangle p = { 90, 35, 1100, 650 };
    DrawRectangleRec(p, COLOR_PANEL_BG);
    ui_draw_panel_border(p, COLOR_PHOSPHOR);
    draw_btn_custom((Rectangle){ 1145, 48, 32, 28 }, "X", 18, vm, COLOR_PHOSPHOR);

    if (settings->difficulty == DIFFICULTY_DEUS) {
        DrawTextEx(font_title, "CAMPAIGN: DEUS SUPREMUS // PRIME INTELLECT", (Vector2){ 295.0f, 52.0f }, 24.0f, 1.2f, COLOR_PHOSPHOR);

        Rectangle big_box = { 155.0f, 140.0f, 200.0f, 320.0f };
        DrawRectangleRec((Rectangle){ big_box.x - 4, big_box.y - 4, big_box.width + 8, big_box.height + 8 }, COLOR_PANEL_BG);
        ui_draw_panel_border((Rectangle){ big_box.x - 2, big_box.y - 2, big_box.width + 4, big_box.height + 4 }, COLOR_PHOSPHOR);
        Rectangle big_src = { (float)(11 * ATLAS_CELL_W), 0.0f, (float)ATLAS_CELL_W, (float)ATLAS_CELL_H };
        Texture2D tex = (ba->atlas_deus.id > 0) ? ba->atlas_deus : ba->atlas;
        DrawTexturePro(tex, big_src, big_box, (Vector2){0,0}, 0.0f, WHITE);

        Rectangle txt_box = { 380.0f, 140.0f, 745.0f, 320.0f };
        DrawRectangleRec(txt_box, COLOR_PANEL_BG);
        ui_draw_panel_border(txt_box, COLOR_PHOSPHOR_DIM);

        DrawTextEx(font_title, "ENTITY 13 // DEUS SUPREMUS", (Vector2){ 400.0f, 158.0f }, 24.0f, 1.2f, COLOR_PHOSPHOR);
        DrawTextEx(font_body, "Omnipresent Machine God // Sector Null", (Vector2){ 400.0f, 188.0f }, 16.0f, 2.0f, COLOR_TEXT_INFO_DIM);
        DrawLine(400, 212, 1105, 212, COLOR_PHOSPHOR_DIM);

        DrawTextEx(font_body, "RULESET: YAKUZA RULES (LOCKED)", (Vector2){ 400.0f, 224.0f }, 17.0f, 2.0f, COLOR_TEXT_INFO);
        DrawTextEx(font_body, "AI INTELLECT: 100.0% OPTIMAL (ZERO BLUNDERS)", (Vector2){ 400.0f, 248.0f }, 17.0f, 2.0f, COLOR_TEXT_INFO);
        DrawTextEx(font_body, "TURN LIMIT: 15.0 SECONDS", (Vector2){ 400.0f, 272.0f }, 17.0f, 2.0f, COLOR_TEXT_INFO);
        DrawTextEx(font_body, "THREAT LEVEL: TERMINAL", (Vector2){ 400.0f, 296.0f }, 17.0f, 2.0f, COLOR_RED);
        DrawLine(400, 324, 1105, 324, COLOR_PHOSPHOR_DIM);

        const char *lore = "The apex consciousness. Born from the convergence of all twelve sectors, Deus Supremus operates with pure mathematical perfection. It does not hesitate, does not blunder, and will exploit every vulnerability under ruthless Yakuza rules.";
        draw_text_wrapped(font_body, lore, 400.0f, 338.0f, 700.0f, 16.0f, 2.0f, COLOR_TEXT_INFO);

        int deus_ok = (DEUS_UNLOCKED_FOR_TESTING || unlocked_opponents >= 12);
        draw_btn_custom((Rectangle){ 460.0f, 570.0f, 240.0f, 48.0f }, "START MATCH", 20, vm, deus_ok ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
        draw_btn_custom((Rectangle){ 720.0f, 570.0f, 180.0f, 48.0f }, "BACK", 20, vm, COLOR_PHOSPHOR);
        return;
    }

    DrawTextEx(font_title, "CAMPAIGN: SELECT BIO-OPPONENT", (Vector2){ 390.0f, 52.0f }, 24.0f, 1.2f, COLOR_PHOSPHOR);

    /* Top Row: 12 mini portraits */
    for (int i = 0; i < CAMPAIGN_OPPONENT_COUNT; i++) {
        Rectangle slot = { 155.0f + i * 82.0f, 96.0f, 68.0f, 110.0f };
        Rectangle src = { (float)(i * ATLAS_CELL_W), 0.0f, (float)ATLAS_CELL_W, (float)ATLAS_CELL_H };
        int unlocked = (i < unlocked_opponents);
        int hov = CheckCollisionPointRec(vm, slot);

        if (unlocked) {
            DrawTexturePro(ba->atlas, src, slot, (Vector2){0,0}, 0.0f, WHITE);
            if (i == selected_opponent) {
                DrawRectangleLinesEx(slot, 2.5f, COLOR_PHOSPHOR);
                DrawRectangleLinesEx((Rectangle){ slot.x - 2, slot.y - 2, slot.width + 4, slot.height + 4 }, 1.0f, COLOR_PHOSPHOR_DIM);
            } else if (hov) {
                DrawRectangleLinesEx(slot, 2.0f, YELLOW);
            } else {
                DrawRectangleLinesEx(slot, 1.0f, COLOR_PHOSPHOR_DIM);
            }
        } else {
            DrawTexturePro(ba->atlas, src, slot, (Vector2){0,0}, 0.0f, (Color){ 15, 15, 15, 255 });
            DrawRectangleRec(slot, (Color){ 0, 0, 0, 210 });
            DrawRectangleLinesEx(slot, 1.0f, (Color){ 40, 40, 40, 255 });
            DrawTextEx(font_body, "LOCKED", (Vector2){ slot.x + 8.0f, slot.y + 45.0f }, 13.0f, 2.0f, (Color){ 160, 160, 160, 255 });
        }
        DrawTextEx(font_body, TextFormat("M-%02d", i + 1), (Vector2){ slot.x + 16.0f, slot.y + 114.0f }, 14.0f, 2.0f, (i == selected_opponent) ? COLOR_TEXT_INFO : COLOR_TEXT_INFO_DIM);
    }

    const Opponent *opp = campaign_get_opponent(selected_opponent);

    /* Bottom Left: Large card portrait */
    Rectangle big_box = { 155.0f, 245.0f, 185.0f, 300.0f };
    DrawRectangleRec((Rectangle){ big_box.x - 4, big_box.y - 4, big_box.width + 8, big_box.height + 8 }, COLOR_PANEL_BG);
    ui_draw_panel_border((Rectangle){ big_box.x - 2, big_box.y - 2, big_box.width + 4, big_box.height + 4 }, COLOR_PHOSPHOR);
    Rectangle big_src = { (float)(selected_opponent * ATLAS_CELL_W), 0.0f, (float)ATLAS_CELL_W, (float)ATLAS_CELL_H };
    DrawTexturePro(ba->atlas, big_src, big_box, (Vector2){0,0}, 0.0f, (selected_opponent < unlocked_opponents) ? WHITE : (Color){ 30, 30, 30, 255 });

    /* Bottom Right: Info Box */
    Rectangle txt_box = { 365.0f, 245.0f, 760.0f, 300.0f };
    DrawRectangleRec(txt_box, COLOR_PANEL_BG);
    ui_draw_panel_border(txt_box, COLOR_PHOSPHOR_DIM);

    if (selected_opponent < unlocked_opponents) {
        DrawTextEx(font_title, opp->name, (Vector2){ 385.0f, 260.0f }, 24.0f, 1.2f, COLOR_PHOSPHOR);
        DrawTextEx(font_body, opp->title, (Vector2){ 385.0f, 290.0f }, 16.0f, 2.0f, COLOR_TEXT_INFO_DIM);
        DrawLine(385, 312, 1105, 312, COLOR_PHOSPHOR_DIM);
        DrawTextEx(font_body, TextFormat("PRIORITY YAKU:   %s", opp->favorite_combo), (Vector2){ 385.0f, 324.0f }, 17.0f, 2.0f, COLOR_TEXT_INFO);
        DrawTextEx(font_body, TextFormat("SIGNATURE TARGET: %s (#%02d)", CARDS[opp->favorite_card_id].name, opp->favorite_card_id + 1), (Vector2){ 385.0f, 344.0f }, 17.0f, 2.0f, COLOR_TEXT_INFO);
        DrawTextEx(font_body, "CLEARANCE PROTOCOL:", (Vector2){ 385.0f, 368.0f }, 15.0f, 1.2f, COLOR_PHOSPHOR_DIM);

        bool is_master = (settings->difficulty == DIFFICULTY_MASTER && settings->rounds >= 3);
        bool is_hard   = (settings->difficulty == DIFFICULTY_HARD   && settings->rounds >= 6);
        bool is_easy   = (settings->difficulty == DIFFICULTY_EASY   && settings->rounds >= 9);

        Rectangle btn_req_m = { 560.0f, 364.0f, 160.0f, 26.0f };
        Rectangle btn_req_h = { 730.0f, 364.0f, 150.0f, 26.0f };
        Rectangle btn_req_e = { 890.0f, 364.0f, 150.0f, 26.0f };

        draw_btn_custom(btn_req_m, "MASTER (3 RDS)", 13, vm, is_master ? COLOR_PHOSPHOR : COLOR_TEXT_INFO_DIM);
        if (is_master) DrawRectangleLinesEx(btn_req_m, 1.5f, COLOR_PHOSPHOR);

        draw_btn_custom(btn_req_h, "HARD (6 RDS)", 13, vm, is_hard ? COLOR_PHOSPHOR : COLOR_TEXT_INFO_DIM);
        if (is_hard) DrawRectangleLinesEx(btn_req_h, 1.5f, COLOR_PHOSPHOR);

        draw_btn_custom(btn_req_e, "EASY (9 RDS)", 13, vm, is_easy ? COLOR_PHOSPHOR : COLOR_TEXT_INFO_DIM);
        if (is_easy) DrawRectangleLinesEx(btn_req_e, 1.5f, COLOR_PHOSPHOR);

        DrawLine(385, 396, 1105, 396, COLOR_PHOSPHOR_DIM);
        draw_text_wrapped(font_body, opp->lore_text, 385.0f, 408.0f, 720.0f, 16.0f, 2.0f, COLOR_TEXT_INFO);
    } else {
        DrawTextEx(font_title, "[ LOCKED SECTOR ]", (Vector2){ 385.0f, 260.0f }, 24.0f, 1.2f, COLOR_RED);
        DrawTextEx(font_body, "DEFEAT PRECEDING TARGETS TO UNLOCK TRANSMISSION", (Vector2){ 385.0f, 295.0f }, 16.0f, 2.0f, COLOR_TEXT_INFO_DIM);
        DrawLine(385, 325, 1105, 325, COLOR_PHOSPHOR_DIM);
        draw_text_wrapped(font_body, opp->lore_text, 385.0f, 350.0f, 720.0f, 17.0f, 2.0f, COLOR_TEXT_INFO_DIM);
    }

    /* Bottom Buttons */
    Rectangle btn_start = { 460.0f, 570.0f, 240.0f, 48.0f };
    Rectangle btn_back = { 720.0f, 570.0f, 180.0f, 48.0f };
    draw_btn_custom(btn_start, "START MATCH", 20, vm, (selected_opponent < unlocked_opponents) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    draw_btn_custom(btn_back, "BACK", 20, vm, COLOR_PHOSPHOR);
}

int campaign_handle_click(GameSettings *settings, int *out_selected, Vector2 vm) {
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return 0;
    int unlocked_opponents = settings->unlocked_opponents;

    /* Close / Back button */
    if (CheckCollisionPointRec(vm, (Rectangle){ 1145, 48, 32, 28 }) ||
        CheckCollisionPointRec(vm, (Rectangle){ 720.0f, 570.0f, 180.0f, 48.0f })) {
        return -1;
    }

    if (settings->difficulty == DIFFICULTY_DEUS) {
        if (CheckCollisionPointRec(vm, (Rectangle){ 460.0f, 570.0f, 240.0f, 48.0f })) {
            if (DEUS_UNLOCKED_FOR_TESTING || unlocked_opponents >= 12) {
                settings->current_opponent = 12;
                if (out_selected) *out_selected = 12;
                return 1;
            }
        }
        return 0;
    }

    if (*out_selected < 0 || *out_selected >= CAMPAIGN_OPPONENT_COUNT) *out_selected = 0;

    /* Mini portraits */
    for (int i = 0; i < CAMPAIGN_OPPONENT_COUNT; i++) {
        Rectangle slot = { 155.0f + i * 82.0f, 96.0f, 68.0f, 110.0f };
        if (CheckCollisionPointRec(vm, slot)) {
            if (i < unlocked_opponents) {
                *out_selected = i;
                return 2;
            }
        }
    }

    /* Clearance Protocol quick-select preset buttons */
    if (*out_selected < unlocked_opponents) {
        if (CheckCollisionPointRec(vm, (Rectangle){ 560.0f, 364.0f, 160.0f, 26.0f })) {
            settings->difficulty = DIFFICULTY_MASTER;
            settings->rounds = 3;
            storage_save_settings(settings);
            return 3;
        }
        if (CheckCollisionPointRec(vm, (Rectangle){ 730.0f, 364.0f, 150.0f, 26.0f })) {
            settings->difficulty = DIFFICULTY_HARD;
            settings->rounds = 6;
            storage_save_settings(settings);
            return 3;
        }
        if (CheckCollisionPointRec(vm, (Rectangle){ 890.0f, 364.0f, 150.0f, 26.0f })) {
            settings->difficulty = DIFFICULTY_EASY;
            settings->rounds = 9;
            storage_save_settings(settings);
            return 3;
        }
    }

    /* Start Match */
    if (CheckCollisionPointRec(vm, (Rectangle){ 460.0f, 570.0f, 240.0f, 48.0f })) {
        if (*out_selected < unlocked_opponents) return 1;
    }

    return 0;
}
