#include "ui_combos.h"
#include "ui_speed.h"
#include <stdio.h>
#include <string.h>

extern Font font_body;

#define COMBOS_PER_PAGE 3
static const Rectangle close_button = { 1000, 70, 100, 36 };
static const Rectangle previous_button = { 180, 610, 120, 40 };
static const Rectangle next_button = { 980, 610, 120, 40 };
static const char *const ruleset_names[] = { "NINTENDO", "FUDAWIKI", "KK-ADD-10", "TOURNAMENT", "YAKUZA" };

/* Ellipsize narrow previews, never shrink their names into unreadable text. */
static void label(const char *text, float x, float y, float width, float size, Color color) {
    char clipped[160];
    snprintf(clipped, sizeof(clipped), "%s", text);
    size_t n = strlen(clipped);
    if (MeasureTextEx(font_body, clipped, size, 0.5f).x > width) {
        if (n > sizeof(clipped) - 4) n = sizeof(clipped) - 4;
        do {
            memcpy(clipped + n, "...", 4);
            if (MeasureTextEx(font_body, clipped, size, 0.5f).x <= width) break;
        } while (n-- > 0);
    }
    DrawTextEx(font_body, clipped, (Vector2){ x, y }, size, 0.5f, color);
}

Rectangle combos_panel_rect(int player) {
    return (Rectangle){ 1160, player == 1 ? 88 : 316, 112, 218 };
}

int combos_panel_hit(Vector2 mouse) {
    for (int p = 0; p < 2; p++) {
        if (p == 1 && speed_solo_active()) continue;
        if (CheckCollisionPointRec(mouse, combos_panel_rect(p))) return p;
    }
    return -1;
}

void combos_draw_sidebar(const GameState *g) {
    Rectangle mode = { 1160, 12, 112, 66 };
    DrawRectangleRec(mode, COLOR_PANEL_BG);
    ui_draw_panel_border(mode, COLOR_PHOSPHOR_DIM);
    label(TextFormat("RND %d/%d", g->current_round, g->total_rounds), 1168, 20, 96, 16, COLOR_PHOSPHOR);
    float size = 16;
    while (size > 12 && MeasureTextEx(font_body, ruleset_names[g->ruleset], size, 0.5f).x > 96) size--;
    label(ruleset_names[g->ruleset], 1168, 47, 96, size, COLOR_TEXT_INFO);
    for (int p = 1; p >= 0; p--) {
        if (p == 1 && speed_solo_active()) continue;
        ScoreResult s = score_captures(g->ruleset, g->captured[p], g->captured_count[p]);
        Rectangle r = combos_panel_rect(p);
        DrawRectangleRec(r, COLOR_PANEL_BG);
        ui_draw_panel_border(r, COLOR_PHOSPHOR);
        label(p ? "CPU COMBOS" : "YOUR COMBOS", r.x + 8, r.y + 8, 96, 13, COLOR_PHOSPHOR);
        label(TextFormat("Base: %d", s.base_points), r.x + 8, r.y + 29, 96, 16, COLOR_TEXT_INFO);
        label(TextFormat("Koi: x%d", koi_caller_multiplier(g->koi_called[p])), r.x + 8, r.y + 49, 96, 14, COLOR_TEXT_INFO_DIM);
        int count = s.yaku_count < 4 ? s.yaku_count : 4;
        for (int i = 0; i < count; i++) {
            float y = r.y + 73 + i * 29;
            label(s.yakus[i].name, r.x + 8, y, 96, 13, COLOR_TEXT_INFO);
            label(TextFormat("%d pts", s.yakus[i].points), r.x + 8, y + 14, 96, 12, COLOR_TEXT_INFO_DIM);
        }
        if (!count) label("No combos yet", r.x + 8, r.y + 78, 96, 13, COLOR_TEXT_INFO_DIM);
        label(s.yaku_count > 4 ? TextFormat("+%d / VIEW ALL", s.yaku_count - 4) : "CLICK TO VIEW",
              r.x + 8, r.y + 197, 96, 12, COLOR_PHOSPHOR);
    }
}

static void button(Rectangle r, const char *text) {
    DrawRectangleRec(r, COLOR_PANEL_BG);
    ui_draw_panel_border(r, COLOR_PHOSPHOR);
    label(text, r.x + 12, r.y + 10, r.width - 24, 16, COLOR_PHOSPHOR);
}

void combos_draw_details(const BoardAssets *assets, const GameState *g, int player, int page) {
    ScoreResult s = score_captures(g->ruleset, g->captured[player], g->captured_count[player]);
    int pages = (s.yaku_count + COMBOS_PER_PAGE - 1) / COMBOS_PER_PAGE;
    if (pages < 1) pages = 1;
    DrawRectangle(0, 0, 1280, 720, COLOR_OVERLAY_BG);
    Rectangle panel = { 150, 50, 980, 620 };
    DrawRectangleRec(panel, (Color){ 5, 12, 14, 255 });
    ui_draw_panel_border(panel, COLOR_PHOSPHOR);
    label(player ? "CPU COMBOS - PAUSED" : "YOUR COMBOS - PAUSED", 180, 75, 800, 26, COLOR_PHOSPHOR);
    label(TextFormat("%s   /   Base: %d   /   Koi calls: %d (x%d)", ruleset_names[g->ruleset],
          s.base_points, g->koi_called[player], koi_caller_multiplier(g->koi_called[player])),
          180, 115, 900, 20, COLOR_TEXT_INFO);
    label(TextFormat("Award if banked now: %d   /   7+ bonus: x%d   /   Opponent Koi: x%d",
          calculate_award(s.base_points, g->koi_called[player], g->koi_called[1 - player]),
          s.base_points >= 7 ? 2 : 1, g->koi_called[1 - player] ? 2 : 1),
          180, 143, 900, 18, COLOR_TEXT_INFO_DIM);
    if (!s.yaku_count) label("No completed combos yet. Capture matching months to build a set.", 180, 230, 900, 22, COLOR_TEXT_INFO);
    for (int row = 0; row < COMBOS_PER_PAGE; row++) {
        int i = page * COMBOS_PER_PAGE + row;
        if (i >= s.yaku_count) break;
        const ScoredYaku *yaku = &s.yakus[i];
        float y = 190 + row * 132;
        label(yaku->name, 180, y, 760, 22, COLOR_TEXT_INFO);
        label(TextFormat("%d pts", yaku->points), 1000, y, 100, 22, COLOR_PHOSPHOR);
        /* Fixed sets use the engine's card list. Counted sets show the captured
           category (including the house-rule Cup), without recalculating points. */
        int ids[48], n = 0;
        if (yaku->card_count) {
            for (int k = 0; k < yaku->card_count; k++) ids[n++] = yaku->card_ids[k];
        } else {
            for (int k = 0; k < g->captured_count[player]; k++) {
                int id = g->captured[player][k], type = CARDS[id].type;
                if ((yaku->id == YAKU_TANE && type == CARD_TYPE_ANIMAL) ||
                    (yaku->id == YAKU_TANZAKU && type == CARD_TYPE_RIBBON) ||
                    (yaku->id == YAKU_KASU && (type == CARD_TYPE_PLAIN ||
                     (id == 32 && g->ruleset == RULESET_KK_ADD_10)))) ids[n++] = id;
            }
        }
        label(yaku->card_count ? "Core cards" : "Counted cards", 180, y + 44, 140, 16, COLOR_TEXT_INFO_DIM);
        float step = n > 1 && n * 54 > 760 ? (760.0f - 48) / (n - 1) : 54;
        for (int k = 0; k < n; k++)
            board_draw_card(assets, ids[k], (Rectangle){ 340 + k * step, y + 30, 48, 78 });
    }
    button(close_button, "CLOSE");
    if (page > 0) button(previous_button, "PREVIOUS");
    if (page + 1 < pages) button(next_button, "NEXT");
    label(TextFormat("%d / %d    |    ESC / P to resume", page + 1, pages), 410, 620, 560, 18, COLOR_TEXT_INFO_DIM);
}

int combos_handle_details(const GameState *g, int player, int *page, Vector2 mouse) {
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) return 1;
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return 0;
    if (CheckCollisionPointRec(mouse, close_button)) return 1;
    ScoreResult s = score_captures(g->ruleset, g->captured[player], g->captured_count[player]);
    if (*page > 0 && CheckCollisionPointRec(mouse, previous_button)) (*page)--;
    else if ((*page + 1) * COMBOS_PER_PAGE < s.yaku_count && CheckCollisionPointRec(mouse, next_button)) (*page)++;
    return 0;
}
