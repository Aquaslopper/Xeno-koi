#include "ui_hub.h"
#include "ui_theme.h"
#include <math.h>
#include <stdio.h>

extern Font font_title;
extern Font font_body;
/* Audiowide rasterised at the title screen's own 176 px draw size (1:1, no bitmap
   stretch). Declared locally like font_title/font_body; only the brand line below
   uses it, and it always travels together with its own MeasureTextEx call. */
extern Font font_display;

/* Deus ON state is derived from settings; only pre-activation values are cached. */
static StorageDeusSessionSnapshot s_deus_snapshot;

void ui_hub_deus_snapshot_capture(const GameSettings *s) {
    if (!s || s->difficulty == DIFFICULTY_DEUS) {
        s_deus_snapshot.valid = 0;
        return;
    }
    s_deus_snapshot.difficulty = s->difficulty;
    s_deus_snapshot.ruleset = s->ruleset;
    s_deus_snapshot.master_timer = s->master_timer;
    s_deus_snapshot.terminal_color = s->terminal_color;
    s_deus_snapshot.background = s->background;
    s_deus_snapshot.current_opponent = s->current_opponent;
    s_deus_snapshot.valid = 1;
}

void ui_hub_deus_snapshot_invalidate(void) {
    s_deus_snapshot.valid = 0;
}

int ui_hub_deus_snapshot_is_valid(void) {
    return s_deus_snapshot.valid;
}

const StorageDeusSessionSnapshot *ui_hub_deus_snapshot(void) {
    return &s_deus_snapshot;
}

/* --- Geometry (1280 x 720 virtual canvas) -------------------------------- */

static const Rectangle k_title_btns[3] = {
    { 490.0f, 374.0f, 300.0f, 48.0f },
    { 490.0f, 440.0f, 300.0f, 48.0f },
    { 490.0f, 506.0f, 300.0f, 48.0f }
};
static const Rectangle k_options_menu_btns[4] = {
    { 430.0f, 316.0f, 420.0f, 46.0f },
    { 430.0f, 374.0f, 420.0f, 46.0f },
    { 430.0f, 432.0f, 420.0f, 46.0f },
    { 430.0f, 490.0f, 420.0f, 46.0f }
};
static const char *const k_options_menu_labels[4] = {
    "SYSTEM OPTIONS", "LOAD / SAVE", "CREDITS", "BACK"
};
static const HubAction k_options_menu_actions[4] = {
    HUB_ACT_OPTIONS_SYSTEM, HUB_ACT_OPTIONS_PROFILES,
    HUB_ACT_OPTIONS_CREDITS, HUB_ACT_OPTIONS_BACK
};
static const Rectangle k_profile_rows[3] = {
    { 265.0f, 220.0f, 750.0f, 66.0f },
    { 265.0f, 306.0f, 750.0f, 66.0f },
    { 265.0f, 392.0f, 750.0f, 66.0f }
};
static const Rectangle k_profile_actions[5] = {
    { 108.0f, 524.0f, 204.0f, 48.0f }, { 328.0f, 524.0f, 204.0f, 48.0f },
    { 548.0f, 524.0f, 204.0f, 48.0f }, { 768.0f, 524.0f, 204.0f, 48.0f },
    { 988.0f, 524.0f, 184.0f, 48.0f }
};
static const char *const k_profile_action_labels[5] = {
    "CREATE", "LOAD", "ERASE", "SAVE ACTIVE", "BACK"
};


/* Mode select: one data strip and one button per card. The card heading is the button
   label, so it is not drawn twice, and the two doors carry different weight: Atrium is
   the wide, double-framed card with the Spine ladder, Campaign the slim one. Both share
   one band (y 256, h 178) and one button row, so the pair reads as a single unit. */
static const Rectangle k_main_left_panel    = { 130.0f, 256.0f, 360.0f, 178.0f };
static const Rectangle k_main_right_panel   = { 530.0f, 256.0f, 620.0f, 178.0f };
static const Rectangle k_main_campaign_box  = { 154.0f, 276.0f, 312.0f,  58.0f };
static const Rectangle k_main_atrium_box    = { 586.0f, 276.0f, 540.0f,  58.0f };
static const Rectangle k_main_campaign_btn  = { 154.0f, 366.0f, 312.0f,  48.0f };
static const Rectangle k_main_atrium_btn    = { 586.0f, 366.0f, 540.0f,  48.0f };
static const Rectangle k_main_back_btn      = { 510.0f, 594.0f, 260.0f,  48.0f };

static const Rectangle k_camp_panel         = { 280.0f,  96.0f, 720.0f, 548.0f };
static const Rectangle k_camp_select_btn    = { 430.0f, 254.0f, 420.0f,  50.0f };
static const Rectangle k_camp_stats_btn     = { 430.0f, 314.0f, 420.0f,  50.0f };
static const Rectangle k_camp_options_btn   = { 430.0f, 374.0f, 420.0f,  50.0f };
static const Rectangle k_camp_deus_box      = { 430.0f, 434.0f, 420.0f,  56.0f };
static const Rectangle k_camp_back_btn      = { 510.0f, 536.0f, 260.0f,  46.0f };

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

static void hub_draw_backdrop_alpha(const BoardAssets *ba, unsigned char alpha) {
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
    DrawRectangle(0, 0, CANVAS_W, CANVAS_H, (Color){ 0, 0, 0, alpha });
}

static void hub_draw_backdrop(const BoardAssets *ba) {
    hub_draw_backdrop_alpha(ba, 168);
}

/* The Atrium card carries a compressed Spine ladder down its left edge: twelve ticks,
   the cleared months drawn bright and doubled in weight. It is the one thing on this
   screen that hints at how much sits behind the other door, and it costs twelve lines. */
static void hub_draw_spine_ladder(Rectangle panel, int cleared_months) {
    float x0 = panel.x + 16.0f, x1 = panel.x + 30.0f;
    float y0 = panel.y + 24.0f, span = panel.height - 48.0f;
    for (int m = 0; m < 12; m++) {
        float y = floorf(y0 + span * (float)m / 11.0f);
        int lit = (m < cleared_months);
        DrawLineEx((Vector2){ x0, y }, (Vector2){ x1, y }, lit ? 2.0f : 1.0f,
                   lit ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    }
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

/* --- Title Screen (SCREEN_START) ----------------------------------------- */

void ui_hub_draw_title(const BoardAssets *ba, Vector2 vm, float saved_timer, int has_active_profile) {
    const char *labels[3] = { has_active_profile ? "CONTINUE" : "START GAME", "OPTION", "QUIT" };
    hub_draw_backdrop_alpha(ba, 95);
    Color wine_red = { 224, 136, 146, 255 };
    hub_draw_centered((font_display.texture.id > 0) ? font_display : font_title,
                      "[XENO / KOI]", 640.0f, 60.0f, 176.0f, 3.0f, COLOR_PHOSPHOR);
    hub_draw_centered(font_body, "HANAFUDA: ACOUSTICA APEXUS", 640.0f, 248.0f, 34.0f, 4.0f, wine_red);
    for (int i = 0; i < 3; i++) hub_draw_button(k_title_btns[i], labels[i], 18, vm);
    if (saved_timer > 0.0f)
        hub_draw_centered(font_body, "ALL FILES SAVED", 640.0f, 675.0f, 20.0f, 1.5f, COLOR_PHOSPHOR);
}

HubAction ui_hub_handle_title_click(Vector2 vm, HubButtonHit *out_hit, int has_active_profile) {
    static const HubAction actions[3] = { HUB_ACT_TITLE_START_GAME, HUB_ACT_TITLE_OPTIONS, HUB_ACT_TITLE_QUIT };
    const char *labels[3] = { has_active_profile ? "CONTINUE" : "START GAME", "OPTION", "QUIT" };
    for (int i = 0; i < 3; i++) {
        if (CheckCollisionPointRec(vm, k_title_btns[i])) {
            hub_record_hit(out_hit, k_title_btns[i], labels[i], 18);
            return actions[i];
        }
    }
    return HUB_ACT_NONE;
}

static void hub_draw_menu_panel(const BoardAssets *ba, const char *title, const char *subtitle) {
    hub_draw_backdrop(ba);
    hub_draw_centered(font_title, title, 640.0f, 118.0f, 30.0f, 1.4f, COLOR_PHOSPHOR);
    hub_draw_centered(font_body, subtitle, 640.0f, 164.0f, 17.0f, 1.0f, COLOR_PHOSPHOR_DIM);
}

void ui_hub_draw_options_menu(const BoardAssets *ba, Vector2 vm, float saved_timer) {
    hub_draw_menu_panel(ba, "OPTIONS", "SYSTEM SETTINGS  //  PROFILE DATA  //  CREDITS");
    for (int i = 0; i < 4; i++) hub_draw_button(k_options_menu_btns[i], k_options_menu_labels[i], 18, vm);
    if (saved_timer > 0.0f)
        hub_draw_centered(font_body, "PROFILE SAVED", 640.0f, 590.0f, 17.0f, 1.0f, COLOR_PHOSPHOR);
}

HubAction ui_hub_handle_options_menu_click(Vector2 vm, HubButtonHit *out_hit) {
    for (int i = 0; i < 4; i++) {
        if (CheckCollisionPointRec(vm, k_options_menu_btns[i])) {
            hub_record_hit(out_hit, k_options_menu_btns[i], k_options_menu_labels[i], 18);
            return k_options_menu_actions[i];
        }
    }
    return HUB_ACT_NONE;
}

static int s_profile_manager_selected_slot = 1;
void ui_hub_profile_manager_reset(int selected_slot) {
    s_profile_manager_selected_slot = (selected_slot >= 1 && selected_slot <= 3) ? selected_slot : 1;
}

int ui_hub_profile_manager_selected_slot(void) { return s_profile_manager_selected_slot; }

void ui_hub_draw_profile_manager(const BoardAssets *ba, const int slots[3], int active_slot,
                                 int selected_slot, Vector2 vm, float saved_timer, int error) {
    hub_draw_menu_panel(ba, "PROFILE LOAD / SAVE", "SELECT A SLOT  //  PROFILES ARE STORED SEPARATELY");
    for (int i = 0; i < 3; i++) {
        Rectangle r = k_profile_rows[i];
        int slot = i + 1;
        int selected = slot == selected_slot;
        DrawRectangleRec(r, selected ? (Color){ 8, 28, 31, 242 } : (Color){ 0, 0, 0, 224 });
        DrawRectangleLinesEx(r, selected ? 2.0f : 1.0f, selected ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
        char label[96];
        snprintf(label, sizeof(label), "SLOT %02d  //  %s%s", slot,
                 slots[i] ? "PROFILE" : "EMPTY",
                 (slot == active_slot) ? "  //  ACTIVE" : "");
        hub_draw_text_snapped(font_body, label, r.x + 24.0f, r.y + 21.0f, 19.0f, 1.0f,
                              selected ? COLOR_PHOSPHOR : COLOR_TEXT_INFO);
        if (CheckCollisionPointRec(vm, r)) {
            Color glow = COLOR_PHOSPHOR; glow.a = 45;
            DrawRectangleRec(r, glow);
        }
    }
    for (int i = 0; i < 5; i++) {
        int enabled = 1;
        if (i == 0) enabled = !slots[selected_slot - 1];
        else if (i == 1) enabled = slots[selected_slot - 1];
        else if (i == 2) enabled = slots[selected_slot - 1] && selected_slot != active_slot;
        else if (i == 3) enabled = active_slot > 0;
        if (enabled) hub_draw_button(k_profile_actions[i], k_profile_action_labels[i], 15, vm);
        else {
            DrawRectangleRec(k_profile_actions[i], (Color){ 0, 0, 0, 175 });
            DrawRectangleLinesEx(k_profile_actions[i], 1.0f, COLOR_PHOSPHOR_DIM);
            hub_draw_centered(font_body, k_profile_action_labels[i],
                              k_profile_actions[i].x + k_profile_actions[i].width * 0.5f,
                              k_profile_actions[i].y + 14.0f, 15.0f, 1.0f, COLOR_PHOSPHOR_DIM);
        }
    }
    if (error) hub_draw_centered(font_body, "PROFILE OPERATION FAILED  //  ACTIVE DATA PRESERVED",
                                 640.0f, 610.0f, 16.0f, 1.0f, COLOR_RED);
    else if (saved_timer > 0.0f)
        hub_draw_centered(font_body, "PROFILE SAVED", 640.0f, 610.0f, 16.0f, 1.0f, COLOR_PHOSPHOR);
}

HubAction ui_hub_handle_profile_manager_click(Vector2 vm, HubButtonHit *out_hit, int *out_slot,
                                                const int slots[3], int active_slot) {
    for (int i = 0; i < 3; i++) {
        if (CheckCollisionPointRec(vm, k_profile_rows[i])) {
            s_profile_manager_selected_slot = i + 1;
            if (out_slot) *out_slot = i + 1;
            hub_record_hit(out_hit, k_profile_rows[i], "SLOT", 18);
            return HUB_ACT_PROFILE_SLOT_SELECTED;
        }
    }
    int selected = s_profile_manager_selected_slot - 1;
    for (int i = 0; i < 5; i++) {
        if (!CheckCollisionPointRec(vm, k_profile_actions[i])) continue;
        if ((i == 0 && slots[selected]) ||
            (i == 1 && !slots[selected]) ||
            (i == 2 && (!slots[selected] || s_profile_manager_selected_slot == active_slot)) ||
            (i == 3 && active_slot == 0)) return HUB_ACT_NONE;
        if (out_slot) *out_slot = s_profile_manager_selected_slot;
        hub_record_hit(out_hit, k_profile_actions[i], k_profile_action_labels[i], 15);
        return (HubAction)(HUB_ACT_PROFILE_CREATE + i);
    }
    return HUB_ACT_NONE;
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

    /* Left Card: CAMPAIGN -- slim, one frame. */
    DrawRectangleRec(k_main_left_panel, COLOR_PANEL_BG);
    ui_draw_panel_border(k_main_left_panel, COLOR_PHOSPHOR_DIM);
    DrawRectangleRec(k_main_campaign_box, (Color){ 4, 8, 10, 210 });
    DrawRectangleLinesEx(k_main_campaign_box, 1.0f, COLOR_PHOSPHOR_DIM);
    snprintf(line1, sizeof(line1), "SECTORS %02d/12   //   COINS %02d/36", sectors, coins);
    hub_draw_centered(font_body, line1, 310.0f, 283.0f, 18.0f, 1.1f, COLOR_PHOSPHOR);
    if (is_deus) {
        snprintf(line2, sizeof(line2), "DEUS SUPREMUS   //   YAKUZA 15S");
        hub_draw_centered(font_body, line2, 310.0f, 308.0f, 14.0f, 1.0f, COLOR_RED);
    } else {
        snprintf(line2, sizeof(line2), "%s   //   %d ROUNDS",
                 s ? hub_ruleset_label(s->ruleset) : "KK-ADD-10", rounds);
        hub_draw_centered(font_body, line2, 310.0f, 308.0f, 14.0f, 1.0f, COLOR_TEXT_INFO_DIM);
    }

    hub_draw_button(k_main_campaign_btn, "PANCRATIUM CAMPAIGN", 18, vm);

    /* Right Card: ATRIUM PLEX -- the heavier door: wide, a bright 2 px frame and an inner
       hairline, so the depth behind it is legible before the loading card says so too. */
    DrawRectangleRec(k_main_right_panel, COLOR_PANEL_BG);
    DrawRectangleLinesEx(k_main_right_panel, 2.0f, COLOR_PHOSPHOR);
    DrawRectangleLinesEx((Rectangle){ k_main_right_panel.x + 7.0f, k_main_right_panel.y + 7.0f,
                                      k_main_right_panel.width - 14.0f, k_main_right_panel.height - 14.0f },
                         1.0f, COLOR_PHOSPHOR_DIM);
    hub_draw_spine_ladder(k_main_right_panel, (spine_count > 0) ? spine_count / 4 : 0);

    DrawRectangleRec(k_main_atrium_box, (Color){ 4, 8, 10, 210 });
    DrawRectangleLinesEx(k_main_atrium_box, 1.0f, COLOR_PHOSPHOR_DIM);
    if (spine_count >= 0) {
        int clamped_spine = (spine_count > 48) ? 48 : spine_count;
        snprintf(line1, sizeof(line1), "SPINE %02d/48", clamped_spine);
        hub_draw_centered(font_body, line1, 856.0f, 283.0f, 18.0f, 1.1f, COLOR_PHOSPHOR);
        if (clamped_spine == 0) {
            hub_draw_centered(font_body, "NEW PLAYERS  //  TUTORIALS START HERE",
                              856.0f, 308.0f, 15.0f, 1.0f, COLOR_AMBER);
        } else {
            hub_draw_centered(font_body, "TUTORIALS  |  SPEEDCOIN  |  ACTION CARD  |  STORY",
                              856.0f, 308.0f, 15.0f, 1.0f, COLOR_TEXT_INFO_DIM);
        }
    } else {
        hub_draw_centered(font_body, "NEURAL CORTEX ARCHIVE", 856.0f, 283.0f, 18.0f, 1.1f, COLOR_PHOSPHOR);
        hub_draw_centered(font_body, "TUTORIALS  |  SPEEDCOIN  |  ACTION CARD  |  STORY",
                          856.0f, 308.0f, 15.0f, 1.0f, COLOR_TEXT_INFO_DIM);
    }

    hub_draw_button(k_main_atrium_btn, "ATRIUM PLEX", 22, vm);

    /* Footer Back Button */
    hub_draw_button(k_main_back_btn, "BACK", 20, vm);
}

HubAction ui_hub_handle_main_click(Vector2 vm, HubButtonHit *out_hit) {
    if (CheckCollisionPointRec(vm, k_main_campaign_btn)) {
        hub_record_hit(out_hit, k_main_campaign_btn, "PANCRATIUM CAMPAIGN", 18);
        return HUB_ACT_GOTO_CAMPAIGN_HUB;
    }
    if (CheckCollisionPointRec(vm, k_main_atrium_btn)) {
        hub_record_hit(out_hit, k_main_atrium_btn, "ATRIUM PLEX", 22);
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
    /* Double frame marks the campaign as the finite, tournament side of the game; the
       Atrium card answers with its lit Spine ladder. Same vocabulary, opposite signals. */
    DrawRectangleLinesEx((Rectangle){ k_camp_panel.x + 7.0f, k_camp_panel.y + 7.0f,
                                      k_camp_panel.width - 14.0f, k_camp_panel.height - 14.0f },
                         1.0f, COLOR_PHOSPHOR);

    hub_draw_centered(font_title, "PANCRATIUM CAMPAIGN", 640.0f, 118.0f, 28.0f, 1.5f, COLOR_PHOSPHOR);
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
    hub_draw_button(k_camp_options_btn, "SYSTEM OPTIONS", 20, vm);

    DrawRectangleRec(k_camp_deus_box, (Color){ 4, 8, 10, 210 });
    ui_draw_panel_border(k_camp_deus_box, is_deus ? COLOR_RED : COLOR_PHOSPHOR_DIM);
    hub_draw_centered(font_title, is_deus ? "DEUS MODE: ON" : "DEUS MODE: OFF",
                      640.0f, 443.0f, 17.0f, 1.3f, is_deus ? COLOR_RED : COLOR_PHOSPHOR_DIM);
    const char *deus_hint = !is_deus ? "CLICK TO ACTIVATE // SESSION ONLY" :
                            ui_hub_deus_snapshot_is_valid() ? "CLICK TO DEACTIVATE" : "OFF DISABLED // USE OPTIONS";
    hub_draw_centered(font_body, deus_hint, 640.0f, 468.0f, 14.0f, 1.0f, COLOR_TEXT_INFO_DIM);

    hub_draw_button(k_camp_back_btn, "BACK", 20, vm);
}

HubAction ui_hub_handle_campaign_click(const GameSettings *s, Vector2 vm, HubButtonHit *out_hit) {
    if (CheckCollisionPointRec(vm, k_camp_deus_box)) {
        if (s && s->difficulty == DIFFICULTY_DEUS) {
            if (!ui_hub_deus_snapshot_is_valid()) return HUB_ACT_NONE;
            hub_record_hit(out_hit, k_camp_deus_box, "DEUS MODE", 17);
            return HUB_ACT_DEUS_DEACTIVATE;
        }
        hub_record_hit(out_hit, k_camp_deus_box, "DEUS MODE", 17);
        return HUB_ACT_DEUS_ACTIVATE;
    }
    if (CheckCollisionPointRec(vm, k_camp_select_btn)) {
        hub_record_hit(out_hit, k_camp_select_btn, "CAMPAIGN", 20);
        return HUB_ACT_OPEN_CAMPAIGN_OVERLAY;
    }
    if (CheckCollisionPointRec(vm, k_camp_stats_btn)) {
        hub_record_hit(out_hit, k_camp_stats_btn, "RECORDS & STATS", 20);
        return HUB_ACT_GOTO_STATS;
    }
    if (CheckCollisionPointRec(vm, k_camp_options_btn)) {
        hub_record_hit(out_hit, k_camp_options_btn, "SYSTEM OPTIONS", 20);
        return HUB_ACT_CAMPAIGN_OPTIONS;
    }
    if (CheckCollisionPointRec(vm, k_camp_back_btn)) {
        hub_record_hit(out_hit, k_camp_back_btn, "BACK", 20);
        return HUB_ACT_BACK_TO_MAIN_HUB;
    }
    return HUB_ACT_NONE;
}
