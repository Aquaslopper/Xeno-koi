#include "campaign.h"
#include "cards.h"
#include "ui_theme.h"
#include <stdio.h>
#include <string.h>

extern Font font_title;
extern Font font_body;

static void draw_text_wrapped(Font font, const char *text, float x, float y, float max_width, float font_size, float spacing, Color color) {
    if (!text || text[0] == '\0') return;
    char buffer[512];
    strncpy(buffer, text, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char *word = strtok(buffer, " ");
    char line[512] = "";
    float cur_y = y;
    float line_height = font_size + 4.0f;

    while (word != NULL) {
        char test_line[512];
        if (line[0] == '\0') snprintf(test_line, sizeof(test_line), "%s", word);
        else snprintf(test_line, sizeof(test_line), "%s %s", line, word);

        Vector2 sz = MeasureTextEx(font, test_line, font_size, spacing);
        if (sz.x > max_width && line[0] != '\0') {
            DrawTextEx(font, line, (Vector2){ x, cur_y }, font_size, spacing, color);
            cur_y += line_height;
            snprintf(line, sizeof(line), "%s", word);
        } else {
            strncpy(line, test_line, sizeof(line));
        }
        word = strtok(NULL, " ");
    }
    if (line[0] != '\0') {
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
               (Vector2){ r.x + (r.width - text_size.x) / 2.0f,
                          r.y + (r.height - font_size) / 2.0f },
               font_size, spacing, col);
    return hov;
}

void campaign_draw_overlay(const BoardAssets *ba, int unlocked_opponents, int selected_opponent, Vector2 vm) {
    /* Dim background */
    DrawRectangle(0, 0, 1280, 720, (Color){ 0, 0, 0, 225 });

    Rectangle p = { 90, 35, 1100, 650 };
    DrawRectangleRec(p, COLOR_PANEL_BG);
    ui_draw_panel_border(p, COLOR_PHOSPHOR);

    DrawTextEx(font_title, "CAMPAIGN: SELECT BIO-OPPONENT", (Vector2){ 390.0f, 52.0f }, 24.0f, 1.2f, COLOR_PHOSPHOR);
    draw_btn_custom((Rectangle){ 1145, 48, 32, 28 }, "X", 18, vm, COLOR_PHOSPHOR);

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
        DrawTextEx(font_body, TextFormat("SIGNATURE TARGET: %s (#%02d)", CARDS[opp->favorite_card_id].name, opp->favorite_card_id + 1), (Vector2){ 385.0f, 348.0f }, 17.0f, 2.0f, COLOR_TEXT_INFO);
        DrawTextEx(font_body, TextFormat("CPU TAUNT:        \"%s\"", opp->taunt_text), (Vector2){ 385.0f, 372.0f }, 15.0f, 2.0f, COLOR_RED);
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

int campaign_handle_click(int unlocked_opponents, int *out_selected, Vector2 vm) {
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return 0;

    /* Close / Back button */
    if (CheckCollisionPointRec(vm, (Rectangle){ 1145, 48, 32, 28 }) ||
        CheckCollisionPointRec(vm, (Rectangle){ 720.0f, 570.0f, 180.0f, 48.0f })) {
        return -1;
    }

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

    /* Start Match */
    if (CheckCollisionPointRec(vm, (Rectangle){ 460.0f, 570.0f, 240.0f, 48.0f })) {
        if (*out_selected < unlocked_opponents) return 1;
    }

    return 0;
}
