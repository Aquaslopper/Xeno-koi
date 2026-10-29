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
        if (speed_mode_is_active() && p == 1) continue;
        if (CheckCollisionPointRec(mouse, combos_panel_rect(p))) return p;
    }
    return -1;
}

void combos_draw_sidebar(const GameState *g) {
    int sc_mode = speed_mode_is_active();
    const char *mode_name = sc_mode ? "SPEEDCOIN" : ruleset_names[g->ruleset];
    Color rule = COLOR_PHOSPHOR_DIM;
    rule.a = 90;
    label(sc_mode ? TextFormat("WAVE %d", speed_mode_wave())
                  : TextFormat("ROUND %d / %d", g->current_round, g->total_rounds),
          1168, 15, 96, 14, COLOR_PHOSPHOR);
    float size = 14;
    while (size > 11 && MeasureTextEx(font_body, mode_name, size, 0.5f).x > 104) size--;
    label(mode_name, 1168, 39, 96, size, COLOR_TEXT_INFO);
    DrawLine(1168, 72, 1264, 72, rule);
    for (int p = 1; p >= 0; p--) {
        if (sc_mode && p == 1) continue;
        ScoreResult s = score_captures(g->ruleset, g->captured[p], g->captured_count[p]);
        Rectangle r = combos_panel_rect(p);
        label(p ? "OPPONENT" : "YOUR SETS", r.x + 8, r.y + 4, 96, 11, COLOR_PHOSPHOR);
        label(TextFormat("%d", s.base_points), r.x + 8, r.y + 20, 54, 23, COLOR_TEXT_INFO);
        label(sc_mode ? TextFormat("BANK %d", calculate_award(s.base_points, 0, 0))
                      : TextFormat("KOI x%d", koi_caller_multiplier(g->koi_called[p])),
              r.x + 48, r.y + 28, 56, 11, COLOR_TEXT_INFO_DIM);
        DrawLine((int)r.x + 4, (int)r.y + 53, (int)r.x + (int)r.width - 4, (int)r.y + 53, rule);
        int count = s.yaku_count < 4 ? s.yaku_count : 4;
        for (int i = 0; i < count; i++) {
            float y = r.y + 63 + i * 31;
            label(s.yakus[i].name, r.x + 8, y, 72, 12, COLOR_TEXT_INFO);
            label(TextFormat("%d", s.yakus[i].points), r.x + 84, y, 20, 12, COLOR_PHOSPHOR);
            if (i + 1 < count) DrawLine((int)r.x + 4, (int)y + 23, (int)r.x + (int)r.width - 4, (int)y + 23, rule);
        }
        if (!count) label("No sets yet", r.x + 8, r.y + 72, 96, 12, COLOR_TEXT_INFO_DIM);
        DrawLine((int)r.x + 4, (int)r.y + 189, (int)r.x + (int)r.width - 4, (int)r.y + 189, rule);
        label(s.yaku_count > 4 ? TextFormat("+%d  OPEN >", s.yaku_count - 4) : "OPEN DETAILS >",
              r.x + 8, r.y + 198, 96, 10.5f, COLOR_PHOSPHOR);
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
    if (speed_mode_is_active()) {
        label(TextFormat("SPEEDCOIN   /   Wave %d   /   Base: %d", speed_mode_wave(), s.base_points),
              180, 115, 900, 20, COLOR_TEXT_INFO);
        label(TextFormat("Wave bank award: %d   /   7+ bonus: x%d",
              calculate_award(s.base_points, 0, 0), s.base_points >= 7 ? 2 : 1),
              180, 143, 900, 18, COLOR_TEXT_INFO_DIM);
    } else {
        label(TextFormat("%s   /   Base: %d   /   Koi calls: %d (x%d)", ruleset_names[g->ruleset],
              s.base_points, g->koi_called[player], koi_caller_multiplier(g->koi_called[player])),
              180, 115, 900, 20, COLOR_TEXT_INFO);
        label(TextFormat("Award if banked now: %d   /   7+ bonus: x%d   /   Opponent Koi: x%d",
              calculate_award(s.base_points, g->koi_called[player], g->koi_called[1 - player]),
              s.base_points >= 7 ? 2 : 1, g->koi_called[1 - player] ? 2 : 1),
              180, 143, 900, 18, COLOR_TEXT_INFO_DIM);
    }
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
