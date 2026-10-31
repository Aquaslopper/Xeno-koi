#include "ui_capture_info.h"
#include "cards.h"
#include "ui_theme.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

extern Font font_body;

static int s_enabled;
static Vector2 s_mouse;
static int s_card_id = -1;
static int s_player = -1;
static int s_lane_index;
static int s_lane_total;
static Rectangle s_hovered_bounds;

Rectangle capture_info_panel_rect(void) {
    return (Rectangle){
        CAPTURE_INFO_PANEL_X,
        CAPTURE_INFO_PANEL_Y,
        CAPTURE_INFO_PANEL_W,
        CAPTURE_INFO_PANEL_H
    };
}

void capture_info_clear(void) {
    s_card_id = -1;
    s_player = -1;
    s_lane_index = 0;
    s_lane_total = 0;
    s_hovered_bounds = (Rectangle){ 0, 0, 0, 0 };
}

void capture_info_begin(int enabled, Vector2 mouse) {
    s_enabled = enabled;
    s_mouse = mouse;
    capture_info_clear();
}

void capture_info_record(int card_id, int player, int lane_index, int lane_total, Rectangle bounds) {
    if (!s_enabled || bounds.width <= 0.0f || bounds.height <= 0.0f) return;
    if ((unsigned)card_id >= 48u || (player != 0 && player != 1)) return;
    if (CheckCollisionPointRec(s_mouse, bounds)) {
        /* Last drawn mini-card in the stack wins so overlapping captures match visual top order. */
        s_card_id = card_id;
        s_player = player;
        s_lane_index = lane_index;
        s_lane_total = lane_total;
        s_hovered_bounds = bounds;
    }
}

int capture_info_hovered_card(void) {
    return s_enabled ? s_card_id : -1;
}

static void draw_fitted_line(const char *text, float x, float y, float max_w, float base_size, Color col) {
    const float sp = 0.6f;
    float sz = base_size;
    while (sz > 11.0f && MeasureTextEx(font_body, text, sz, sp).x > max_w) {
        sz -= 0.5f;
    }
    char clipped[96];
    snprintf(clipped, sizeof(clipped), "%s", text);
    size_t n = strlen(clipped);
    if (MeasureTextEx(font_body, clipped, sz, sp).x > max_w) {
        if (n > sizeof(clipped) - 4) n = sizeof(clipped) - 4;
        while (n > 0) {
            memcpy(clipped + n, "...", 4);
            if (MeasureTextEx(font_body, clipped, sz, sp).x <= max_w) break;
            n--;
        }
    }
    DrawTextEx(font_body, clipped, (Vector2){ floorf(x), floorf(y) }, sz, sp, col);
}

static const char *card_category_label(const Card *c) {
    if (c->type == CARD_TYPE_BRIGHT) {
        return (c->flags & CARD_FLAG_RAIN) ? "Rain Bright" : "Bright / Hikari";
    }
    if (c->type == CARD_TYPE_ANIMAL) {
        return (c->flags & CARD_FLAG_CUP) ? "Animal (Cup)" : "Animal / Tane";
    }
    if (c->type == CARD_TYPE_RIBBON) {
        if (c->flags & CARD_FLAG_POETRY) return "Poetry Ribbon";
        if (c->flags & CARD_FLAG_BLUE)   return "Blue Ribbon";
        return "Ribbon / Tanzaku";
    }
    return "Plain / Kasu";
}

void capture_info_draw(const BoardAssets *ba) {
    static const char *const month_labels[12] = {
        "January (01)", "February (02)", "March (03)",
        "April (04)",   "May (05)",      "June (06)",
        "July (07)",    "August (08)",   "September (09)",
        "October (10)", "November (11)", "December (12)"
    };
    static const char *const flower_labels[12] = {
        "Pine",   "Plum",    "Cherry",        "Wisteria",
        "Iris",   "Peony",   "Clover",        "Pampas",
        "Chrysanthemum", "Maple", "Willow",   "Paulownia"
    };
    static const char *const lane_codes[4] = { "B", "A", "R", "P" };

    if (!s_enabled || (unsigned)s_card_id >= 48u || !ba) return;

    const Card *card = &CARDS[s_card_id];
    const char *raw_name = strchr(card->name, ' ');
    const char *name = raw_name ? raw_name + 1 : card->name;
    const char *month = month_labels[card->month - 1];
    const char *flower = flower_labels[card->month - 1];
    const char *category = card_category_label(card);
    char slot_line[48];
    snprintf(slot_line, sizeof(slot_line), "%s  [%s %d/%d]",
             (s_player == 0) ? "YOU" : "CPU",
             lane_codes[(unsigned)card->type < 4u ? (unsigned)card->type : 3u],
             s_lane_index, s_lane_total);

    /* Highlight the exact hovered mini-card in its capture lane stack. */
    board_draw_card_glow(s_hovered_bounds, 1.0f);

    /* Fixed left-column panel aligned 1:1 with all capture boxes (x=12, w=255, 4px gap above y=508). */
    Rectangle panel = capture_info_panel_rect();
    Color bg = COLOR_PANEL_BG;
    bg.a = 235;
    DrawRectangleRec(panel, bg);
    ui_draw_panel_border(panel, COLOR_PHOSPHOR);

    /* Crisp full-resolution card preview on the left (106 x 172 matches 176:286 card ratio).
       A presentation draw, not a table card: no table bezel over the panel chrome. */
    Rectangle card_rect = { 22.0f, 318.0f, 106.0f, 172.0f };
    board_set_table_bezel_muted(1);
    board_draw_card(ba, s_card_id, card_rect);
    board_set_table_bezel_muted(0);
    board_draw_card_glow(card_rect, 0.55f);

    /* Five aligned info rows on the right column inside x = 138..259. */
    const float text_x = 138.0f;
    const float max_w  = 121.0f;
    Color divider = COLOR_PHOSPHOR_DIM;
    divider.a = 85;

    draw_fitted_line(name,      text_x, 330.0f, max_w, 16.0f, COLOR_PHOSPHOR);
    DrawLine(138, 353, 255, 353, divider);
    draw_fitted_line(month,     text_x, 362.0f, max_w, 14.0f, COLOR_TEXT_INFO);
    draw_fitted_line(flower,    text_x, 392.0f, max_w, 14.0f, COLOR_TEXT_INFO);
    draw_fitted_line(category,  text_x, 422.0f, max_w, 14.0f, COLOR_TEXT_INFO_DIM);
    DrawLine(138, 446, 255, 446, divider);
    draw_fitted_line(slot_line, text_x, 454.0f, max_w, 13.5f, COLOR_PHOSPHOR_DIM);
}
