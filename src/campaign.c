#include "campaign.h"
#include "cards.h"
#include "storage.h"
#include "ui_theme.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* Owner decision: open to everyone while the mode is being tested. The long-term intent
   is that Deus Supremus unlocks only after the story is completed; when that gate lands,
   this flag stays 1 for testing builds and the real condition goes beside it
   (see AGENTS.md, owner decisions). */
#define DEUS_UNLOCKED_FOR_TESTING 1

extern Font font_title;
extern Font font_body;

/* Pancratium stage names in TABLE_DESIGN_* order. Index 0 is the default CRT table;
   each other index previews its own assets/maps/<name>/ overlay and bezel. Keep this
   list the same length and order as TABLE_DESIGN_COUNT in storage.h. */
static const char *const k_table_names[TABLE_DESIGN_COUNT] = {
    "CRT", "CITY", "CASTLE", "HOLE", "ARCTIC", "HARVEST", "FACE", "REALM"
};

static int draw_btn_custom(Rectangle r, const char *txt, int fsz, Vector2 vm, Color col);

typedef enum {
    CAMPAIGN_PAGE_OPPONENT = 0,
    CAMPAIGN_PAGE_DIFFICULTY,
    CAMPAIGN_PAGE_STAGE
} CampaignSetupPage;

static CampaignSetupPage s_campaign_page = CAMPAIGN_PAGE_OPPONENT;

/* Map order follows TABLE_DESIGN_*; background indices follow ui_board.c's bg_names[].
   CRT has no authored map preset, so selecting it leaves the player's current choices. */
static const int k_map_background[TABLE_DESIGN_COUNT] = { -1, 4, 3, 1, 2, 0, 6, 7 };
static const int k_map_color[TABLE_DESIGN_COUNT] = {
    -1, TERMINAL_THEME_RUST, TERMINAL_THEME_GOLD, TERMINAL_THEME_SAND,
    TERMINAL_THEME_TEAL, TERMINAL_THEME_BLOOD, TERMINAL_THEME_CRIMSON, TERMINAL_THEME_VIOLET
};
static const int k_campaign_color_choices[] = {
    TERMINAL_THEME_GREEN, TERMINAL_THEME_AMBER, TERMINAL_THEME_TEAL,
    TERMINAL_THEME_CRIMSON, TERMINAL_THEME_VIOLET, TERMINAL_THEME_GOLD,
    TERMINAL_THEME_RUST, TERMINAL_THEME_BLOOD, TERMINAL_THEME_SAND
};
static const int k_campaign_background_choices[] = { 0, 1, 2, 3, 4, 6, 7 };
static const char *const k_campaign_background_names[8] = {
    "HARVEST", "HOLE", "ARCTIC", "CASTLE", "CITY", "DEUS", "FACE", "REALM"
};

static Rectangle campaign_page_button(int page) {
    return (Rectangle){ 365.0f, 245.0f + page * 66.0f, 175.0f, 52.0f };
}

static Rectangle campaign_map_card(int map) {
    return (Rectangle){ 579.0f + (map % 4) * 136.0f, 278.0f + (map / 4) * 88.0f, 126.0f, 84.0f };
}

static void campaign_apply_map_defaults(GameSettings *settings, int map) {
    if (!settings || map < 0 || map >= TABLE_DESIGN_COUNT) return;
    settings->table_design = map;
    if (map > TABLE_DESIGN_CRT) {
        settings->background = k_map_background[map];
        settings->terminal_color = k_map_color[map];
    }
}

static int campaign_cycle_background(int current, int direction) {
    int count = (int)(sizeof(k_campaign_background_choices) / sizeof(k_campaign_background_choices[0]));
    int index = 0;
    for (int i = 0; i < count; i++) {
        if (k_campaign_background_choices[i] == current) { index = i; break; }
    }
    return k_campaign_background_choices[(index + count + direction) % count];
}

static int campaign_cycle_color(int current, int direction) {
    int count = (int)(sizeof(k_campaign_color_choices) / sizeof(k_campaign_color_choices[0]));
    int index = 0;
    for (int i = 0; i < count; i++) {
        if (k_campaign_color_choices[i] == current) { index = i; break; }
    }
    return k_campaign_color_choices[(index + count + direction) % count];
}

static void campaign_draw_map_card(const BoardAssets *ba, const GameSettings *settings,
                                   int map, Vector2 vm) {
    Rectangle card = campaign_map_card(map);
    int hover = CheckCollisionPointRec(vm, card);
    int preview_bg = (map == settings->table_design) ? settings->background :
                     (map == TABLE_DESIGN_CRT ? settings->background : k_map_background[map]);
    float art_height = 62.0f;
    float art_width = art_height * (16.0f / 9.0f);
    Rectangle art = { card.x + (card.width - art_width) * 0.5f, card.y + 2.0f, art_width, art_height };
    DrawRectangleRec(art, (Color){ 5, 5, 5, 255 });
    if (preview_bg >= 0 && preview_bg < 8 && ba->bgs[preview_bg].id > 0) {
        DrawTexturePro(ba->bgs[preview_bg], (Rectangle){ 0, 0, 1280, 720 }, art,
                       (Vector2){ 0, 0 }, 0.0f, WHITE);
    }
    if (map > TABLE_DESIGN_CRT && ba->table_skins[map].ready) {
        const Texture2D overlay = ba->table_skins[map].overlay;
        DrawTexturePro(overlay, (Rectangle){ 0, 0, (float)overlay.width, (float)overlay.height }, art,
                       (Vector2){ 0, 0 }, 0.0f, WHITE);
    }
    DrawRectangleLinesEx(card, (map == settings->table_design || hover) ? 2.0f : 1.0f,
                         (map == settings->table_design) ? COLOR_PHOSPHOR :
                         (hover ? COLOR_PHOSPHOR_DIM : (Color){ 100, 100, 100, 150 }));
    Vector2 label_size = MeasureTextEx(font_body, k_table_names[map], 13.0f, 1.0f);
    DrawTextEx(font_body, k_table_names[map],
               (Vector2){ card.x + (card.width - label_size.x) * 0.5f, card.y + 66.0f },
               13.0f, 1.0f, (map == settings->table_design) ? COLOR_PHOSPHOR : COLOR_TEXT_INFO_DIM);
}

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
    ui_theme_draw_panel_flares(r);
    const float font_size = (float)fsz;
    const float spacing = 1.2f;
    Vector2 text_size = MeasureTextEx(font_title, txt, font_size, spacing);
    DrawTextEx(font_title, txt,
               (Vector2){ floorf(r.x + (r.width - text_size.x) / 2.0f),
                          floorf(r.y + (r.height - font_size) / 2.0f) },
               font_size, spacing, col);
    return hov;
}

static void campaign_draw_opponent_page(const BoardAssets *ba, const GameSettings *settings,
                                        const Opponent *opp, int selected_opponent) {
    DrawTextEx(font_title, opp->name, (Vector2){ 580.0f, 258.0f }, 23.0f, 1.2f, COLOR_PHOSPHOR);
    DrawTextEx(font_body, opp->title, (Vector2){ 580.0f, 288.0f }, 15.0f, 1.6f, COLOR_TEXT_INFO_DIM);
    DrawLine(580, 310, 1105, 310, COLOR_PHOSPHOR_DIM);
    DrawTextEx(font_body, TextFormat("PRIORITY YAKU: %s", opp->favorite_combo),
               (Vector2){ 580.0f, 322.0f }, 16.0f, 1.1f, COLOR_TEXT_INFO);
    DrawTextEx(font_body, TextFormat("SIGNATURE: %s", CARDS[opp->favorite_card_id].name),
               (Vector2){ 580.0f, 345.0f }, 15.0f, 1.0f, COLOR_TEXT_INFO);
    DrawTextEx(font_body, "CLEARANCE // MASTER 3 RDS   HARD 6 RDS   EASY 9 RDS",
               (Vector2){ 580.0f, 371.0f }, 13.0f, 0.8f, COLOR_PHOSPHOR_DIM);
    const Texture2D *coins[3] = { &ba->coin_gold, &ba->coin_silver, &ba->coin_bronze };
    const char *names[3] = { "MASTER", "HARD", "EASY" };
    const int medal_offset[3] = { 2, 1, 0 };
    for (int i = 0; i < 3; i++) {
        float x = 580.0f + i * 174.0f;
        DrawTextEx(font_body, names[i], (Vector2){ x, 392.0f }, 12.0f, 0.8f, COLOR_TEXT_INFO_DIM);
        if ((settings->cleared_mask & (1ULL << (selected_opponent * 3 + medal_offset[i]))) && coins[i]->id > 0) {
            Rectangle src = { 0, 0, (float)coins[i]->width, (float)coins[i]->height };
            DrawTexturePro(*coins[i], src, (Rectangle){ x + 62.0f, 389.0f, 18.0f, 18.0f },
                           (Vector2){ 0, 0 }, 0.0f, WHITE);
        }
    }
    DrawLine(580, 418, 1105, 418, COLOR_PHOSPHOR_DIM);
    draw_text_wrapped(font_body, opp->lore_text, 580.0f, 430.0f, 525.0f, 15.0f, 1.2f, COLOR_TEXT_INFO);
}

static void campaign_draw_difficulty_page(Vector2 vm, const GameSettings *settings) {
    static const char *const labels[3] = { "EASY // 9 ROUNDS", "HARD // 6 ROUNDS", "MASTER // 3 ROUNDS" };
    static const char *const accuracy[3] = {
        "AI ACCURACY: 20-60%", "AI ACCURACY: 50-80%", "AI ACCURACY: 78-100%"
    };
    static const char *const run_length[3] = {
        "CLEARANCE: 9 ROUNDS", "CLEARANCE: 6 ROUNDS", "CLEARANCE: 3 ROUNDS"
    };
    static const Difficulty modes[3] = { DIFFICULTY_EASY, DIFFICULTY_HARD, DIFFICULTY_MASTER };
    DrawTextEx(font_title, "SELECT DIFFICULTY", (Vector2){ 580.0f, 260.0f }, 22.0f, 1.2f, COLOR_PHOSPHOR);
    DrawTextEx(font_body, "Opponent behavior becomes more accurate as the roster advances.",
               (Vector2){ 580.0f, 290.0f }, 14.0f, 1.0f, COLOR_TEXT_INFO_DIM);
    for (int i = 0; i < 3; i++) {
        Rectangle r = { 580.0f + i * 176.0f, 327.0f, 166.0f, 42.0f };
        int active = settings->difficulty == modes[i];
        draw_btn_custom(r, labels[i], 13, vm, active ? COLOR_PHOSPHOR : COLOR_TEXT_INFO_DIM);
        if (active) DrawRectangleLinesEx(r, 2.0f, COLOR_PHOSPHOR);
        DrawTextEx(font_body, accuracy[i], (Vector2){ r.x, 379.0f }, 12.0f, 0.6f, COLOR_TEXT_INFO);
        DrawTextEx(font_body, run_length[i], (Vector2){ r.x, 398.0f }, 12.0f, 0.6f, COLOR_TEXT_INFO_DIM);
        if (i == 2) DrawTextEx(font_body, TextFormat("TURN LIMIT: %d SEC", settings->master_timer),
                                (Vector2){ r.x, 417.0f }, 11.0f, 0.5f, COLOR_TEXT_INFO_DIM);
    }
    DrawLine(580, 428, 1105, 428, COLOR_PHOSPHOR_DIM);
    DrawTextEx(font_body, "Clearance medals require the round counts shown above.",
               (Vector2){ 580.0f, 444.0f }, 14.0f, 1.0f, COLOR_TEXT_INFO_DIM);
    DrawTextEx(font_body, TextFormat("MASTER TURN LIMIT: %d SECONDS", settings->master_timer),
               (Vector2){ 580.0f, 470.0f }, 14.0f, 1.0f, COLOR_TEXT_INFO);
    DrawTextEx(font_body, "Choose a tier to set its matching clearance length.",
               (Vector2){ 580.0f, 503.0f }, 14.0f, 1.0f, COLOR_TEXT_INFO_DIM);
}

static void campaign_draw_stage_page(const BoardAssets *ba, const GameSettings *settings,
                                     Vector2 vm, int enabled) {
    DrawTextEx(font_title, "STAGE // MAP SELECT", (Vector2){ 580.0f, 258.0f }, 20.0f, 1.0f, COLOR_PHOSPHOR);
    for (int i = 0; i < TABLE_DESIGN_COUNT; i++) campaign_draw_map_card(ba, settings, i, vm);

    Color active = enabled ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM;
    DrawTextEx(font_body, "BACKGROUND", (Vector2){ 580.0f, 465.0f }, 12.0f, 0.8f, active);
    draw_btn_custom((Rectangle){ 676.0f, 461.0f, 30.0f, 25.0f }, "<", 14, vm, active);
    Rectangle bg_value = { 710.0f, 461.0f, 180.0f, 25.0f };
    DrawRectangleRec(bg_value, COLOR_PANEL_BG);
    ui_draw_panel_border(bg_value, active);
    const char *bg_name = (settings->background >= 0 && settings->background < 8) ?
                          k_campaign_background_names[settings->background] : "UNKNOWN";
    Vector2 bg_size = MeasureTextEx(font_body, bg_name, 13.0f, 0.8f);
    DrawTextEx(font_body, bg_name,
               (Vector2){ bg_value.x + (bg_value.width - bg_size.x) * 0.5f,
                          bg_value.y + 5.0f }, 13.0f, 0.8f, COLOR_TEXT_INFO);
    draw_btn_custom((Rectangle){ 894.0f, 461.0f, 30.0f, 25.0f }, ">", 14, vm, active);
    if (settings->background >= 0 && settings->background < 8 && ba->bgs[settings->background].id > 0) {
        DrawTextEx(font_body, "PREVIEW", (Vector2){ 936.0f, 468.0f }, 11.0f, 0.5f, COLOR_TEXT_INFO_DIM);
        Rectangle preview = { 1032.0f, 455.0f, 72.0f, 40.0f };
        DrawTexturePro(ba->bgs[settings->background], (Rectangle){ 0, 0, 1280, 720 }, preview,
                       (Vector2){ 0, 0 }, 0.0f, WHITE);
        DrawRectangleLinesEx(preview, 1.0f, active);
    }

    DrawTextEx(font_body, "TERMINAL COLOR", (Vector2){ 580.0f, 505.0f }, 12.0f, 0.8f, active);
    draw_btn_custom((Rectangle){ 676.0f, 501.0f, 30.0f, 25.0f }, "<", 14, vm, active);
    Rectangle color_value = { 710.0f, 501.0f, 180.0f, 25.0f };
    DrawRectangleRec(color_value, COLOR_PANEL_BG);
    ui_draw_panel_border(color_value, active);
    const char *color_name = theme_get(settings->terminal_color).name;
    Vector2 color_size = MeasureTextEx(font_body, color_name, 13.0f, 0.8f);
    DrawTextEx(font_body, color_name,
               (Vector2){ color_value.x + (color_value.width - color_size.x) * 0.5f,
                          color_value.y + 5.0f }, 13.0f, 0.8f, COLOR_TEXT_INFO);
    draw_btn_custom((Rectangle){ 894.0f, 501.0f, 30.0f, 25.0f }, ">", 14, vm, active);
    DrawTextEx(font_body, "GLOBAL", (Vector2){ 976.0f, 507.0f }, 11.0f, 0.7f, COLOR_TEXT_INFO_DIM);
    Rectangle color_swatch = { 1032.0f, 501.0f, 72.0f, 25.0f };
    DrawRectangleRec(color_swatch, theme_get(settings->terminal_color).base);
    DrawRectangleLinesEx(color_swatch, 1.0f, active);
}

void campaign_draw_overlay(const BoardAssets *ba, const GameSettings *settings, int selected_opponent, Vector2 vm) {
    int unlocked_opponents = settings->unlocked_opponents;
    if (selected_opponent < 0 || selected_opponent >= CAMPAIGN_OPPONENT_COUNT) selected_opponent = 0;

    DrawRectangle(0, 0, 1280, 720, (Color){ 0, 0, 0, 225 });
    Rectangle panel = { 90, 35, 1100, 650 };
    DrawRectangleRec(panel, COLOR_PANEL_BG);
    ui_draw_panel_border(panel, COLOR_PHOSPHOR);
    draw_btn_custom((Rectangle){ 1145, 48, 32, 28 }, "X", 18, vm, COLOR_PHOSPHOR);

    if (settings->difficulty == DIFFICULTY_DEUS) {
        DrawTextEx(font_title, "CAMPAIGN: DEUS SUPREMUS // PRIME INTELLECT", (Vector2){ 295.0f, 52.0f }, 24.0f, 1.2f, COLOR_PHOSPHOR);
        Rectangle big_box = { 155.0f, 140.0f, 200.0f, 320.0f };
        DrawRectangleRec((Rectangle){ big_box.x - 4, big_box.y - 4, big_box.width + 8, big_box.height + 8 }, COLOR_PANEL_BG);
        ui_draw_panel_border((Rectangle){ big_box.x - 2, big_box.y - 2, big_box.width + 4, big_box.height + 4 }, COLOR_PHOSPHOR);
        Rectangle big_src = { (float)(11 * ATLAS_CELL_W), 0.0f, (float)ATLAS_CELL_W, (float)ATLAS_CELL_H };
        Texture2D tex = (ba->atlas_deus.id > 0) ? ba->atlas_deus : ba->atlas;
        DrawTexturePro(tex, big_src, big_box, (Vector2){ 0, 0 }, 0.0f, WHITE);

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

    const char *picker_title = " APEX  PANCRATIUM  CIRCUIT ";
    Vector2 title_size = MeasureTextEx(font_title, picker_title, 24.0f, 1.2f);
    DrawTextEx(font_title, picker_title,
               (Vector2){ floorf(panel.x + (panel.width - title_size.x) * 0.5f), 52.0f },
               24.0f, 1.2f, COLOR_PHOSPHOR);

    for (int i = 0; i < CAMPAIGN_OPPONENT_COUNT; i++) {
        Rectangle slot = { 155.0f + i * 82.0f, 96.0f, 68.0f, 110.0f };
        Rectangle src = { (float)(i * ATLAS_CELL_W), 0.0f, (float)ATLAS_CELL_W, (float)ATLAS_CELL_H };
        int unlocked = (i < unlocked_opponents);
        int hover = CheckCollisionPointRec(vm, slot);
        if (unlocked) {
            DrawTexturePro(ba->atlas, src, slot, (Vector2){ 0, 0 }, 0.0f, WHITE);
            DrawRectangleLinesEx(slot, (i == selected_opponent || hover) ? 2.0f : 1.0f,
                                 (i == selected_opponent) ? COLOR_PHOSPHOR : (hover ? YELLOW : COLOR_PHOSPHOR_DIM));
        } else {
            DrawTexturePro(ba->atlas, src, slot, (Vector2){ 0, 0 }, 0.0f, (Color){ 15, 15, 15, 255 });
            DrawRectangleRec(slot, (Color){ 0, 0, 0, 210 });
            DrawRectangleLinesEx(slot, 1.0f, (Color){ 40, 40, 40, 255 });
            DrawTextEx(font_body, "LOCKED", (Vector2){ slot.x + 8.0f, slot.y + 45.0f }, 13.0f, 2.0f, (Color){ 160, 160, 160, 255 });
        }
        DrawTextEx(font_body, TextFormat("M-%02d", i + 1), (Vector2){ slot.x + 16.0f, slot.y + 114.0f },
                   14.0f, 2.0f, (i == selected_opponent) ? COLOR_TEXT_INFO : COLOR_TEXT_INFO_DIM);
    }

    Rectangle portrait = { 155.0f, 245.0f, 185.0f, 300.0f };
    Rectangle portrait_src = { (float)(selected_opponent * ATLAS_CELL_W), 0.0f, (float)ATLAS_CELL_W, (float)ATLAS_CELL_H };
    DrawRectangleRec((Rectangle){ portrait.x - 4, portrait.y - 4, portrait.width + 8, portrait.height + 8 }, COLOR_PANEL_BG);
    ui_draw_panel_border((Rectangle){ portrait.x - 2, portrait.y - 2, portrait.width + 4, portrait.height + 4 }, COLOR_PHOSPHOR);
    DrawTexturePro(ba->atlas, portrait_src, portrait, (Vector2){ 0, 0 }, 0.0f,
                   (selected_opponent < unlocked_opponents) ? WHITE : (Color){ 30, 30, 30, 255 });

    for (int page = 0; page < 3; page++) {
        Rectangle r = campaign_page_button(page);
        int active = s_campaign_page == (CampaignSetupPage)page;
        draw_btn_custom(r, page == CAMPAIGN_PAGE_OPPONENT ? "OPPONENT" :
                           page == CAMPAIGN_PAGE_DIFFICULTY ? "DIFFICULTY" : "STAGE",
                        17, vm, active ? COLOR_PHOSPHOR : COLOR_TEXT_INFO_DIM);
        if (active) DrawRectangleLinesEx(r, 2.0f, COLOR_PHOSPHOR);
    }

    Rectangle info = { 560.0f, 245.0f, 565.0f, 300.0f };
    DrawRectangleRec(info, COLOR_PANEL_BG);
    ui_draw_panel_border(info, COLOR_PHOSPHOR_DIM);
    int unlocked = selected_opponent < unlocked_opponents;
    if (s_campaign_page == CAMPAIGN_PAGE_OPPONENT) {
        if (unlocked) campaign_draw_opponent_page(ba, settings, campaign_get_opponent(selected_opponent), selected_opponent);
        else {
            static const int req_b[12] = { 0,1,2,3,4,5,6,7,8,9,10,11 };
            static const int req_s[12] = { 0,0,0,0,1,2,3,6,7,8,9,10 };
            static const int req_g[12] = { 0,0,0,0,0,0,0,1,2,4,6,9 };
            DrawTextEx(font_title, "[ LOCKED SECTOR ]", (Vector2){ 580.0f, 260.0f }, 23.0f, 1.2f, COLOR_RED);
            DrawTextEx(font_body, TextFormat("REQUIRES: %d BRONZE / %d SILVER / %d GOLD",
                       req_b[selected_opponent], req_s[selected_opponent], req_g[selected_opponent]),
                       (Vector2){ 580.0f, 304.0f }, 16.0f, 1.0f, COLOR_TEXT_INFO_DIM);
        }
    } else if (s_campaign_page == CAMPAIGN_PAGE_DIFFICULTY) {
        campaign_draw_difficulty_page(vm, settings);
    } else {
        campaign_draw_stage_page(ba, settings, vm, unlocked);
    }

    Rectangle start = { 365.0f, 570.0f, 175.0f, 48.0f };
    Rectangle back = { 720.0f, 570.0f, 180.0f, 48.0f };
    draw_btn_custom(start, "START", 20, vm, (unlocked && settings->difficulty != DIFFICULTY_DEUS) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    draw_btn_custom(back, "BACK", 20, vm, COLOR_PHOSPHOR);
}

int campaign_handle_click(GameSettings *settings, int *out_selected, Vector2 vm) {
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return 0;
    int unlocked_opponents = settings->unlocked_opponents;

    /* Close / Back button */
    if (CheckCollisionPointRec(vm, (Rectangle){ 1145, 48, 32, 28 }) ||
        CheckCollisionPointRec(vm, (Rectangle){ 720.0f, 570.0f, 180.0f, 48.0f })) {
        s_campaign_page = CAMPAIGN_PAGE_OPPONENT;
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

    /* Mini portraits: allow inspecting any opponent, including locked ones */
    for (int i = 0; i < CAMPAIGN_OPPONENT_COUNT; i++) {
        Rectangle slot = { 155.0f + i * 82.0f, 96.0f, 68.0f, 110.0f };
        if (CheckCollisionPointRec(vm, slot)) {
            *out_selected = i;
            return 2;
        }
    }

    /* The three setup selectors reuse the Atrium's one-panel-at-a-time pattern. */
    for (int page = 0; page < 3; page++) {
        if (CheckCollisionPointRec(vm, campaign_page_button(page))) {
            s_campaign_page = (CampaignSetupPage)page;
            return 2;
        }
    }

    if (*out_selected < unlocked_opponents && s_campaign_page == CAMPAIGN_PAGE_DIFFICULTY) {
        static const Difficulty modes[3] = { DIFFICULTY_EASY, DIFFICULTY_HARD, DIFFICULTY_MASTER };
        static const int rounds[3] = { 9, 6, 3 };
        for (int i = 0; i < 3; i++) {
            Rectangle r = { 580.0f + i * 176.0f, 327.0f, 166.0f, 42.0f };
            if (CheckCollisionPointRec(vm, r)) {
                settings->difficulty = modes[i];
                settings->rounds = rounds[i];
                storage_save_settings(settings);
                return 3;
            }
        }
    }

    if (*out_selected < unlocked_opponents && s_campaign_page == CAMPAIGN_PAGE_STAGE) {
        for (int map = 0; map < TABLE_DESIGN_COUNT; map++) {
            if (CheckCollisionPointRec(vm, campaign_map_card(map))) {
                campaign_apply_map_defaults(settings, map);
                storage_save_settings(settings);
                return 5;
            }
        }
        int bg_dir = 0;
        if (CheckCollisionPointRec(vm, (Rectangle){ 676.0f, 461.0f, 30.0f, 25.0f })) bg_dir = -1;
        else if (CheckCollisionPointRec(vm, (Rectangle){ 710.0f, 461.0f, 214.0f, 25.0f })) bg_dir = 1;
        if (bg_dir) {
            settings->background = campaign_cycle_background(settings->background, bg_dir);
            storage_save_settings(settings);
            return 5;
        }
        int color_dir = 0;
        if (CheckCollisionPointRec(vm, (Rectangle){ 676.0f, 501.0f, 30.0f, 25.0f })) color_dir = -1;
        else if (CheckCollisionPointRec(vm, (Rectangle){ 710.0f, 501.0f, 214.0f, 25.0f })) color_dir = 1;
        if (color_dir) {
            settings->terminal_color = campaign_cycle_color(settings->terminal_color, color_dir);
            storage_save_settings(settings);
            return 5;
        }
    }

    /* Start Match */
    if (CheckCollisionPointRec(vm, (Rectangle){ 365.0f, 570.0f, 175.0f, 48.0f })) {
        if (*out_selected < unlocked_opponents) {
            s_campaign_page = CAMPAIGN_PAGE_OPPONENT;
            return 1;
        }
    }

    return 0;
}

static const int UNLOCK_REQ_B[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
static const int UNLOCK_REQ_S[12] = { 0, 0, 0, 0, 1, 2, 3, 6, 7, 8, 9, 10 };
static const int UNLOCK_REQ_G[12] = { 0, 0, 0, 0, 0, 0, 0, 1, 2, 4, 6, 9 };

static void count_coins(unsigned long long mask, int *b, int *s, int *g) {
    *b = 0; *s = 0; *g = 0;
    for (int i = 0; i < 12; i++) {
        if (mask & (1ULL << (i * 3 + 0))) (*b)++;
        if (mask & (1ULL << (i * 3 + 1))) (*s)++;
        if (mask & (1ULL << (i * 3 + 2))) (*g)++;
    }
}

int campaign_update_unlocks(GameSettings *settings) {
    if (!settings) return 0;
    int b, s, g;
    count_coins(settings->cleared_mask, &b, &s, &g);

    /* Unlocks are derived from earned coins, not trusted from an older save. */
    int unlocked = 1;
    while (unlocked < CAMPAIGN_OPPONENT_COUNT) {
        int idx = unlocked; /* 0-based opponent index to unlock */
        if (b < UNLOCK_REQ_B[idx] || s < UNLOCK_REQ_S[idx] || g < UNLOCK_REQ_G[idx]) break;
        unlocked++;
    }
    if (settings->unlocked_opponents == unlocked) return 0;
    settings->unlocked_opponents = unlocked;
    return 1;
}
