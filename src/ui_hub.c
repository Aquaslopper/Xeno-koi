#include "ui_hub.h"
#include "ui_theme.h"
#include <math.h>
#include <stdio.h>

extern Font font_title;
extern Font font_body;

/* --- Geometry (1280 x 720 virtual canvas) -------------------------------- */

static const Rectangle k_main_left_panel    = { 120.0f, 148.0f, 500.0f, 390.0f };
static const Rectangle k_main_right_panel   = { 660.0f, 148.0f, 500.0f, 390.0f };
static const Rectangle k_main_campaign_btn  = { 210.0f, 456.0f, 320.0f,  52.0f };
static const Rectangle k_main_atrium_btn    = { 750.0f, 456.0f, 320.0f,  52.0f };
static const Rectangle k_main_back_btn      = { 510.0f, 594.0f, 260.0f,  48.0f };

static const Rectangle k_camp_panel         = { 280.0f,  96.0f, 720.0f, 548.0f };
static const Rectangle k_camp_select_btn    = { 430.0f, 266.0f, 420.0f,  54.0f };
static const Rectangle k_camp_stats_btn     = { 430.0f, 342.0f, 420.0f,  54.0f };
static const Rectangle k_camp_deus_box      = { 430.0f, 418.0f, 420.0f,  64.0f };
static const Rectangle k_camp_back_btn      = { 510.0f, 536.0f, 260.0f,  48.0f };

/* --- Pure readout helpers ------------------------------------------------ */

int ui_hub_unlocked_sectors(const GameSettings *s) {
    if (!s) return 1;
    if (s->unlocked_opponents < 1) return 1;
    if (s->unlocked_opponents > 12) return 12;
    return s->unlocked_opponents;
}

int ui_hub_count_coins(const GameSettings *s) {
    if (!s) return 0;
    int count = 0;
    for (int i = 0; i < 36; i++) {
        if ((s->cleared_mask >> i) & 1ULL) count++;
    }
    return count;
}

static const char *hub_ruleset_label(Ruleset r) {
    switch (r) {
        case RULESET_NINTENDO:   return "NINTENDO";
        case RULESET_FUDAWIKI:   return "FUDAWIKI";
        case RULESET_KK_ADD_10:  return "KK-ADD-10";
        case RULESET_TOURNAMENT: return "TOURNAMENT";
        case RULESET_YAKUZA:     return "YAKUZA";
        default:                 return "NINTENDO";
    }
}

static const char *hub_difficulty_label(Difficulty d) {
    switch (d) {
        case DIFFICULTY_EASY:   return "EASY";
        case DIFFICULTY_HARD:   return "HARD";
        case DIFFICULTY_MASTER: return "MASTER";
        case DIFFICULTY_DEUS:   return "DEUS";
        default:                return "EASY";
    }
}

/* --- Static draw helpers (side-effect free) ------------------------------ */

static void hub_draw_text_snapped(Font font, const char *text, float x, float y, float fsz, float sp, Color col) {
    DrawTextEx(font, text, (Vector2){ floorf(x), floorf(y) }, fsz, sp, col);
}

static void hub_draw_centered(Font font, const char *text, float cx, float y, float fsz, float sp, Color col) {
    Vector2 ts = MeasureTextEx(font, text, fsz, sp);
    Vector2 pos = { floorf(cx - ts.x * 0.5f), floorf(y) };
    DrawTextEx(font, text, pos, fsz, sp, col);
    if (font.texture.id == font_title.texture.id && fsz >= 26.0f)
        ui_theme_draw_text_flares(font, text, pos, fsz, sp, col);
}

static void hub_draw_backdrop(const BoardAssets *ba) {
    if (ba && ba->current_bg >= 0 && ba->current_bg < 8 && ba->bgs[ba->current_bg].id > 0) {
        int bg = ba->current_bg;
        if (ba->has_fog_shader) {
            float cur_time = (float)GetTime();
            float light_on = (ba->bg_normals[bg].id > 0 &&
                              ba->light_normal_loc >= 0 &&
                              ba->light_enabled_loc >= 0) ? 1.0f : 0.0f;
            float flash_zero = 0.0f;
            SetShaderValue(ba->fog_shader, ba->fog_time_loc, &cur_time, SHADER_UNIFORM_FLOAT);
            SetShaderValue(ba->fog_shader, ba->light_enabled_loc, &light_on, SHADER_UNIFORM_FLOAT);
            if (ba->light_flash_loc >= 0)
                SetShaderValue(ba->fog_shader, ba->light_flash_loc, &flash_zero, SHADER_UNIFORM_FLOAT);
            BeginShaderMode(ba->fog_shader);
            if (light_on > 0.5f)
                SetShaderValueTexture(ba->fog_shader, ba->light_normal_loc, ba->bg_normals[bg]);
        }
        DrawTexturePro(ba->bgs[bg],
                       (Rectangle){ 0.0f, 0.0f, (float)CANVAS_W, (float)CANVAS_H },
                       (Rectangle){ 0.0f, 0.0f, (float)CANVAS_W, (float)CANVAS_H },
                       (Vector2){ 0.0f, 0.0f }, 0.0f, WHITE);
        if (ba->has_fog_shader) EndShaderMode();
    } else {
        ClearBackground(BLACK);
    }
    DrawRectangle(0, 0, CANVAS_W, CANVAS_H, (Color){ 0, 0, 0, 168 });
}

static void hub_draw_button(Rectangle r, const char *label, int fsz, Vector2 vm) {
    int hov = CheckCollisionPointRec(vm, r);
    DrawRectangleRec(r, hov ? COLOR_BTN_HOVER_BG : COLOR_PANEL_BG);
    ui_draw_panel_border(r, hov ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    Vector2 ts = MeasureTextEx(font_title, label, (float)fsz, 1.2f);
    Vector2 pos = {
        floorf(r.x + (r.width - ts.x) * 0.5f),
        floorf(r.y + (r.height - (float)fsz) * 0.5f)
    };
    DrawTextEx(font_title, label, pos, (float)fsz, 1.2f, COLOR_PHOSPHOR);
    if (hov) ui_theme_draw_text_flares(font_title, label, pos, (float)fsz, 1.2f, COLOR_PHOSPHOR);
}

static void hub_record_hit(HubButtonHit *out_hit, Rectangle r, const char *lbl, int fsz) {
    if (!out_hit) return;
    out_hit->rect      = r;
    out_hit->label     = lbl;
    out_hit->font_size = fsz;
}

/* --- Main Game Hub (SCREEN_HUB_MAIN) ------------------------------------- */

void ui_hub_draw_main(const BoardAssets *ba, const GameSettings *s, int spine_count, Vector2 vm) {
    int sectors = ui_hub_unlocked_sectors(s);
    int coins   = ui_hub_count_coins(s);
    int rounds  = (s && s->rounds >= 1 && s->rounds <= 12) ? s->rounds : 2;
    int is_deus = (s && s->difficulty == DIFFICULTY_DEUS);
    char line1[96], line2[96];

    hub_draw_backdrop(ba);

    hub_draw_centered(font_title, "[XENO / KOI]", 640.0f, 36.0f, 44.0f, 2.2f, COLOR_PHOSPHOR);
    hub_draw_centered(font_body, "MAIN TERMINAL  //  MODE SELECT", 640.0f, 92.0f, 20.0f, 2.0f, COLOR_PHOSPHOR_DIM);

    /* Left Card: CAMPAIGN */
    DrawRectangleRec(k_main_left_panel, COLOR_PANEL_BG);
    ui_draw_panel_border(k_main_left_panel, COLOR_PHOSPHOR_DIM);
    hub_draw_centered(font_title, "SECTOR GAUNTLET", 370.0f, 170.0f, 22.0f, 1.4f, COLOR_PHOSPHOR);
    DrawLine(148, 204, 592, 204, COLOR_PHOSPHOR_DIM);

    Rectangle camp_stat_box = { 148.0f, 220.0f, 444.0f, 60.0f };
    DrawRectangleRec(camp_stat_box, (Color){ 4, 8, 10, 210 });
    DrawRectangleLinesEx(camp_stat_box, 1.0f, COLOR_PHOSPHOR_DIM);
    snprintf(line1, sizeof(line1), "UNLOCKED SECTORS %02d/12  |  COINS %02d/36", sectors, coins);
    hub_draw_centered(font_body, line1, 370.0f, 228.0f, 18.0f, 1.1f, COLOR_PHOSPHOR);
    if (is_deus) {
        snprintf(line2, sizeof(line2), "DEUS SUPREMUS ACTIVE  //  YAKUZA 15S");
        hub_draw_centered(font_body, line2, 370.0f, 254.0f, 15.0f, 1.0f, COLOR_RED);
    } else {
        snprintf(line2, sizeof(line2), "RULESET: %s  //  %d ROUNDS",
                 s ? hub_ruleset_label(s->ruleset) : "KK-ADD-10", rounds);
        hub_draw_centered(font_body, line2, 370.0f, 254.0f, 15.0f, 1.0f, COLOR_TEXT_INFO_DIM);
    }

    hub_draw_text_snapped(font_body, "Challenge 12 biomechanical sector entities.", 152.0f, 304.0f, 17.0f, 1.1f, COLOR_TEXT_INFO);
    hub_draw_text_snapped(font_body, "Earn Bronze, Silver, and Gold clearance coins", 152.0f, 332.0f, 17.0f, 1.1f, COLOR_TEXT_INFO_DIM);
    hub_draw_text_snapped(font_body, "to unlock deeper sectors and review replays.", 152.0f, 358.0f, 17.0f, 1.1f, COLOR_TEXT_INFO_DIM);
    hub_draw_button(k_main_campaign_btn, "CAMPAIGN", 20, vm);

    /* Right Card: ATRIUM PLEX */
    DrawRectangleRec(k_main_right_panel, COLOR_PANEL_BG);
    ui_draw_panel_border(k_main_right_panel, COLOR_PHOSPHOR_DIM);
    hub_draw_centered(font_title, "ATRIUM PLEX", 910.0f, 170.0f, 22.0f, 1.4f, COLOR_PHOSPHOR);
    DrawLine(688, 204, 1132, 204, COLOR_PHOSPHOR_DIM);

    Rectangle plex_stat_box = { 688.0f, 220.0f, 444.0f, 60.0f };
    DrawRectangleRec(plex_stat_box, (Color){ 4, 8, 10, 210 });
    DrawRectangleLinesEx(plex_stat_box, 1.0f, COLOR_PHOSPHOR_DIM);
    if (spine_count >= 0) {
        int clamped_spine = (spine_count > 48) ? 48 : spine_count;
        snprintf(line1, sizeof(line1), "SPINE %02d/48", clamped_spine);
        hub_draw_centered(font_body, line1, 910.0f, 228.0f, 18.0f, 1.2f, COLOR_PHOSPHOR);
        if (clamped_spine == 0) {
            hub_draw_centered(font_body, "[ NEW PLAYERS  //  TUTORIALS START HERE ]",
                              910.0f, 254.0f, 15.0f, 1.0f, COLOR_AMBER);
        } else {
            hub_draw_centered(font_body, "TUTORIALS  |  SPEEDCOIN  |  ACTION CARD  |  STORY",
                              910.0f, 254.0f, 15.0f, 1.0f, COLOR_TEXT_INFO_DIM);
        }
    } else {
        hub_draw_centered(font_body, "NEURAL CORTEX ARCHIVE", 910.0f, 228.0f, 18.0f, 1.2f, COLOR_PHOSPHOR);
        hub_draw_centered(font_body, "TUTORIALS  |  SPEEDCOIN  |  ACTION CARD  |  STORY",
                          910.0f, 254.0f, 15.0f, 1.0f, COLOR_TEXT_INFO_DIM);
    }

    hub_draw_text_snapped(font_body, "48-card Spine progression and Story archive.", 692.0f, 304.0f, 17.0f, 1.1f, COLOR_TEXT_INFO);
    hub_draw_text_snapped(font_body, "Includes Koi-Koi Tutorials, Speedcoin,", 692.0f, 332.0f, 17.0f, 1.1f, COLOR_TEXT_INFO_DIM);
    hub_draw_text_snapped(font_body, "Action Card Audio FX, and Sparring Training.", 692.0f, 358.0f, 17.0f, 1.1f, COLOR_TEXT_INFO_DIM);
    hub_draw_button(k_main_atrium_btn, "ATRIUM PLEX", 20, vm);

    /* Footer Back Button */
    hub_draw_button(k_main_back_btn, "BACK", 20, vm);
}

HubAction ui_hub_handle_main_click(Vector2 vm, HubButtonHit *out_hit) {
    if (CheckCollisionPointRec(vm, k_main_campaign_btn)) {
        hub_record_hit(out_hit, k_main_campaign_btn, "CAMPAIGN", 20);
        return HUB_ACT_GOTO_CAMPAIGN_HUB;
    }
    if (CheckCollisionPointRec(vm, k_main_atrium_btn)) {
        hub_record_hit(out_hit, k_main_atrium_btn, "ATRIUM PLEX", 20);
        return HUB_ACT_GOTO_ATRIUM;
    }
    if (CheckCollisionPointRec(vm, k_main_back_btn)) {
        hub_record_hit(out_hit, k_main_back_btn, "BACK", 20);
        return HUB_ACT_BACK_TO_TITLE;
    }
    return HUB_ACT_NONE;
}

/* --- Campaign Hub (SCREEN_HUB_CAMPAIGN) ---------------------------------- */

void ui_hub_draw_campaign(const BoardAssets *ba, const GameSettings *s, Vector2 vm) {
    int sectors = ui_hub_unlocked_sectors(s);
    int coins   = ui_hub_count_coins(s);
    int rounds  = (s && s->rounds >= 1 && s->rounds <= 12) ? s->rounds : 2;
    int is_deus = (s && s->difficulty == DIFFICULTY_DEUS);
    char line1[96], line2[96];

    hub_draw_backdrop(ba);

    DrawRectangleRec(k_camp_panel, COLOR_PANEL_BG);
    ui_draw_panel_border(k_camp_panel, COLOR_PHOSPHOR_DIM);

    hub_draw_centered(font_title, "CAMPAIGN  //  SECTOR COMMAND", 640.0f, 118.0f, 28.0f, 1.5f, COLOR_PHOSPHOR);
    DrawLine(320, 160, 960, 160, COLOR_PHOSPHOR_DIM);

    Rectangle info_box = { 320.0f, 176.0f, 640.0f, 62.0f };
    DrawRectangleRec(info_box, (Color){ 4, 8, 10, 210 });
    DrawRectangleLinesEx(info_box, 1.0f, COLOR_PHOSPHOR_DIM);
    snprintf(line1, sizeof(line1), "UNLOCKED SECTORS %02d/12   |   COINS %02d/36", sectors, coins);
    hub_draw_centered(font_body, line1, 640.0f, 184.0f, 18.0f, 1.2f, COLOR_PHOSPHOR);
    snprintf(line2, sizeof(line2), "DIFFICULTY: %s   |   RULESET: %s   |   %d ROUNDS",
             s ? hub_difficulty_label(s->difficulty) : "EASY",
             s ? hub_ruleset_label(s->ruleset) : "KK-ADD-10",
             rounds);
    hub_draw_centered(font_body, line2, 640.0f, 212.0f, 15.0f, 1.0f,
                      is_deus ? COLOR_RED : COLOR_TEXT_INFO_DIM);

    hub_draw_button(k_camp_select_btn, "CAMPAIGN", 20, vm);
    hub_draw_button(k_camp_stats_btn, "RECORDS & STATS", 20, vm);

    /* Display-only Deus indicator (derived from settings.difficulty; inert until Stage 4) */
    DrawRectangleRec(k_camp_deus_box, (Color){ 4, 8, 10, 210 });
    ui_draw_panel_border(k_camp_deus_box, is_deus ? COLOR_RED : COLOR_PHOSPHOR_DIM);
    hub_draw_centered(font_title, is_deus ? "DEUS MODE: ON" : "DEUS MODE: OFF",
                      640.0f, 427.0f, 17.0f, 1.3f, is_deus ? COLOR_RED : COLOR_PHOSPHOR_DIM);
    hub_draw_centered(font_body, "CONFIGURE IN OPTION -> AI DIFFICULTY",
                      640.0f, 454.0f, 14.0f, 1.0f, COLOR_TEXT_INFO_DIM);

    hub_draw_button(k_camp_back_btn, "BACK", 20, vm);
}

HubAction ui_hub_handle_campaign_click(Vector2 vm, HubButtonHit *out_hit) {
    if (CheckCollisionPointRec(vm, k_camp_select_btn)) {
        hub_record_hit(out_hit, k_camp_select_btn, "CAMPAIGN", 20);
        return HUB_ACT_OPEN_CAMPAIGN_OVERLAY;
    }
    if (CheckCollisionPointRec(vm, k_camp_stats_btn)) {
        hub_record_hit(out_hit, k_camp_stats_btn, "RECORDS & STATS", 20);
        return HUB_ACT_GOTO_STATS;
    }
    if (CheckCollisionPointRec(vm, k_camp_back_btn)) {
        hub_record_hit(out_hit, k_camp_back_btn, "BACK", 20);
        return HUB_ACT_BACK_TO_MAIN_HUB;
    }
    return HUB_ACT_NONE;
}
