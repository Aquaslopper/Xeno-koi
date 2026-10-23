#include "ui_plexus.h"
#include "cards.h"
#include "ui_theme.h"
#include <stdio.h>
#include <math.h>

extern Font font_title;
extern Font font_body;

static Texture2D s_plexus_bg = { 0 };
static Texture2D s_plexus_normal = { 0 };
static int s_plexus_loaded = 0;
static int s_selected_month = 0; /* 0..11 */
static int s_selected_slot  = 0; /* 0..3 within current month */
static int s_active_tab     = 0; /* 0: node briefing, 1: logfile placeholder, 2..4: training 1..3 placeholder */

static const char * const k_month_names[12] = {
    "JANUARY // PINE",
    "FEBRUARY // PLUM",
    "MARCH // CHERRY",
    "APRIL // WISTERIA",
    "MAY // IRIS",
    "JUNE // PEONY",
    "JULY // BUSH CLOVER",
    "AUGUST // SUSUKI",
    "SEPTEMBER // CHRYSANTHEMUM",
    "OCTOBER // MAPLE",
    "NOVEMBER // WILLOW",
    "DECEMBER // PAULOWNIA"
};

/* Geometry matching plexus/plexus_mockup.png */
static const Rectangle k_arrow_up_rect   = { 1096.0f,  60.0f,  48.0f,  30.0f };
static const Rectangle k_arrow_down_rect = { 1096.0f, 610.0f,  48.0f,  30.0f };
static const Rectangle k_card_rects[4] = {
    { 1092.0f, 102.0f, 56.0f, 88.0f },
    { 1092.0f, 228.0f, 56.0f, 88.0f },
    { 1092.0f, 354.0f, 56.0f, 88.0f },
    { 1092.0f, 496.0f, 56.0f, 88.0f }
};
static const Rectangle k_briefing_rect = { 528.0f, 109.0f, 528.0f, 210.0f };
static const Rectangle k_start_rect    = { 896.0f, 348.0f, 160.0f,  48.0f };
static const Rectangle k_bottom_rects[5] = {
    {  64.0f, 644.0f, 156.0f, 48.0f }, /* SAVE & BACK */
    { 246.0f, 644.0f, 152.0f, 48.0f }, /* LOGFILE */
    { 428.0f, 644.0f, 170.0f, 48.0f }, /* TRAINING 1 */
    { 624.0f, 644.0f, 170.0f, 48.0f }, /* TRAINING 2 */
    { 820.0f, 644.0f, 170.0f, 48.0f }  /* TRAINING 3 */
};
static const char * const k_bottom_labels[5] = {
    "SAVE & BACK", "LOGFILE", "TRAINING 1", "TRAINING 2", "TRAINING 3"
};

static const char *resolve_asset_path(const char *rel) {
    static char buf[512];
    const char *base = GetApplicationDirectory();
    if (base && base[0] != '\0') {
        snprintf(buf, sizeof(buf), "%s%s", base, rel);
        if (FileExists(buf)) return buf;
    }
    snprintf(buf, sizeof(buf), "./%s", rel);
    return buf;
}

static void ensure_plexus_assets(void) {
    if (s_plexus_loaded) return;
    s_plexus_loaded = 1;
    s_plexus_bg = LoadTexture(resolve_asset_path("assets/plexus.png"));
    if (s_plexus_bg.id > 0) {
        SetTextureFilter(s_plexus_bg, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(s_plexus_bg, TEXTURE_WRAP_CLAMP);
    }
    const char *norm_path = resolve_asset_path("assets/plexus_normal.png");
    if (!FileExists(norm_path)) norm_path = resolve_asset_path("plexus/plexus_normal.png");
    if (FileExists(norm_path)) {
        s_plexus_normal = LoadTexture(norm_path);
        if (s_plexus_normal.id > 0) {
            SetTextureFilter(s_plexus_normal, TEXTURE_FILTER_BILINEAR);
            SetTextureWrap(s_plexus_normal, TEXTURE_WRAP_CLAMP);
        }
    }
}

void plexus_unload_assets(void) {
    if (s_plexus_bg.id > 0) { UnloadTexture(s_plexus_bg); s_plexus_bg.id = 0; }
    if (s_plexus_normal.id > 0) { UnloadTexture(s_plexus_normal); s_plexus_normal.id = 0; }
    s_plexus_loaded = 0;
}

static void draw_plexus_btn(Rectangle r, const char *txt, int fsz, Vector2 vm, int active) {
    int hov = CheckCollisionPointRec(vm, r);
    DrawRectangleRec(r, (hov || active) ? COLOR_BTN_HOVER_BG : COLOR_PANEL_BG);
    ui_draw_panel_border(r, (hov || active) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    Color col = (hov || active) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR;
    Vector2 ts = MeasureTextEx(font_title, txt, (float)fsz, 1.2f);
    Vector2 pos = { floorf(r.x + (r.width - ts.x) * 0.5f), floorf(r.y + (r.height - (float)fsz) * 0.5f) };
    DrawTextEx(font_title, txt, pos, (float)fsz, 1.2f, col);
    if (hov || active) ui_theme_draw_text_flares(font_title, txt, pos, (float)fsz, 1.2f, col);
}

static void draw_month_triangle(Rectangle r, int is_up, Vector2 vm) {
    int hov = CheckCollisionPointRec(vm, r);
    float cx = r.x + r.width * 0.5f;
    float top_y = r.y + 4.0f;
    float bot_y = r.y + r.height - 4.0f;
    float hw = 13.0f;
    Vector2 v1, v2, v3;
    if (is_up) {
        v1 = (Vector2){ cx, top_y };
        v2 = (Vector2){ cx - hw, bot_y };
        v3 = (Vector2){ cx + hw, bot_y };
    } else {
        v1 = (Vector2){ cx, bot_y };
        v2 = (Vector2){ cx + hw, top_y };
        v3 = (Vector2){ cx - hw, top_y };
    }
    Color fill = hov ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM;
    if (hov) {
        Color glow = COLOR_PHOSPHOR;
        glow.a = 70;
        DrawCircleGradient((int)cx, (int)(r.y + r.height * 0.5f), 20.0f, glow, (Color){ 0, 0, 0, 0 });
    }
    DrawTriangle(v1, v2, v3, fill);
    DrawTriangle(v1, v3, v2, fill);
}

void plexus_draw(const BoardAssets *ba, Vector2 vm) {
    ensure_plexus_assets();

    /* 1. Background (assets/plexus.png + plexus_normal.png via fog/light shader) */
    if (s_plexus_bg.id > 0) {
        if (ba && ba->has_fog_shader) {
            float cur_time = (float)GetTime();
            float light_enabled = (s_plexus_normal.id > 0 &&
                                   ba->light_normal_loc >= 0 && ba->light_enabled_loc >= 0) ? 1.0f : 0.0f;
            float flash_zero = 0.0f;
            SetShaderValue(ba->fog_shader, ba->fog_time_loc, &cur_time, SHADER_UNIFORM_FLOAT);
            SetShaderValue(ba->fog_shader, ba->light_enabled_loc, &light_enabled, SHADER_UNIFORM_FLOAT);
            if (ba->light_flash_loc >= 0)
                SetShaderValue(ba->fog_shader, ba->light_flash_loc, &flash_zero, SHADER_UNIFORM_FLOAT);
            BeginShaderMode(ba->fog_shader);
            if (light_enabled > 0.5f)
                SetShaderValueTexture(ba->fog_shader, ba->light_normal_loc, s_plexus_normal);
        }
        DrawTexturePro(s_plexus_bg, (Rectangle){ 0, 0, 1280, 720 }, (Rectangle){ 0, 0, 1280, 720 }, (Vector2){ 0, 0 }, 0.0f, WHITE);
        if (ba && ba->has_fog_shader) EndShaderMode();
    } else {
        ClearBackground(BLACK);
    }
    DrawRectangle(0, 0, 1280, 720, (Color){ 0, 0, 0, 45 });

    /* 2. Right Spine: Month ▲ / ▼ triangles + 4 Month Cards */
    draw_month_triangle(k_arrow_up_rect, 1, vm);
    draw_month_triangle(k_arrow_down_rect, 0, vm);

    float pulse = 0.55f + 0.45f * sinf((float)GetTime() * 3.8f);
    for (int i = 0; i < 4; i++) {
        Rectangle cr = k_card_rects[i];
        int card_id = s_selected_month * 4 + i;
        int hov = CheckCollisionPointRec(vm, cr);
        int sel = (s_active_tab == 0 && i == s_selected_slot);
        board_draw_card_shadow(cr, (sel || hov) ? 0.85f : 0.35f);
        if (sel) board_draw_card_glow(cr, pulse);
        else if (hov) board_draw_card_glow(cr, 0.45f);
        if (ba) board_draw_card(ba, card_id, cr);
        DrawRectangleLinesEx((Rectangle){ cr.x - 1.0f, cr.y - 1.0f, cr.width + 2.0f, cr.height + 2.0f },
                             sel ? 2.0f : 1.0f, (sel || hov) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    }

    /* 3. Task / Training Briefing Panel */
    DrawRectangleRec(k_briefing_rect, COLOR_PANEL_BG);
    ui_draw_panel_border(k_briefing_rect, COLOR_PHOSPHOR_DIM);

    char hdr[96];
    char sub[96];
    const char *line1 = "";
    const char *line2 = "";
    const char *line3 = "";
    if (s_active_tab == 0) {
        int card_id = s_selected_month * 4 + s_selected_slot;
        snprintf(hdr, sizeof(hdr), "MONTH %02d  //  %s", s_selected_month + 1, k_month_names[s_selected_month]);
        snprintf(sub, sizeof(sub), "CARD %d OF 4 :  [%s]", s_selected_slot + 1, CARDS[card_id].name);
        line1 = "TASK / TRAINING BRIEFING  [PLACEHOLDER]";
        line2 = "Select any of the 4 month cards on the spine to inspect its task.";
        line3 = "Complete all 4 cards in any order to unlock the next month.";
    } else if (s_active_tab == 1) {
        snprintf(hdr, sizeof(hdr), "ATRIUM PLEX  //  LOGFILE");
        snprintf(sub, sizeof(sub), "ARCHIVE & STORY LOG  [PLACEHOLDER]");
        line1 = "Recorded transmissions, non-linear story fragments, and";
        line2 = "archived conditioning statistics will be displayed here.";
        line3 = "Click any card on the right spine to return to Month view.";
    } else {
        snprintf(hdr, sizeof(hdr), "ATRIUM PLEX  //  TRAINING %d", s_active_tab - 1);
        snprintf(sub, sizeof(sub), "UNLOCKABLE SPARRING MODULE  [PLACEHOLDER]");
        line1 = "Once unlocked from the 4-card monthly progression, this slot";
        line2 = "launches the training minigame directly for free-play sparring.";
        line3 = "Click START to test button or select a card on the right spine.";
    }

    DrawTextEx(font_title, hdr, (Vector2){ floorf(k_briefing_rect.x + 20.0f), floorf(k_briefing_rect.y + 16.0f) }, 19.0f, 1.4f, COLOR_PHOSPHOR);
    DrawLine((int)k_briefing_rect.x + 20, (int)k_briefing_rect.y + 44, (int)(k_briefing_rect.x + k_briefing_rect.width - 20.0f), (int)k_briefing_rect.y + 44, COLOR_PHOSPHOR_DIM);
    DrawTextEx(font_body, sub, (Vector2){ floorf(k_briefing_rect.x + 20.0f), floorf(k_briefing_rect.y + 56.0f) }, 18.0f, 1.2f, COLOR_PHOSPHOR);
    DrawTextEx(font_body, line1, (Vector2){ floorf(k_briefing_rect.x + 20.0f), floorf(k_briefing_rect.y + 96.0f) }, 17.0f, 1.1f, COLOR_TEXT_INFO);
    DrawTextEx(font_body, line2, (Vector2){ floorf(k_briefing_rect.x + 20.0f), floorf(k_briefing_rect.y + 124.0f) }, 16.0f, 1.1f, COLOR_PHOSPHOR_DIM);
    DrawTextEx(font_body, line3, (Vector2){ floorf(k_briefing_rect.x + 20.0f), floorf(k_briefing_rect.y + 150.0f) }, 16.0f, 1.1f, COLOR_PHOSPHOR_DIM);

    /* 4. START Button underneath right side of Briefing Panel */
    draw_plexus_btn(k_start_rect, "START", 18, vm, 0);

    /* 5. Bottom Bar: SAVE & BACK, LOGFILE, TRAINING 1, TRAINING 2, TRAINING 3 */
    for (int i = 0; i < 5; i++) {
        int active = (i > 0 && s_active_tab == i);
        draw_plexus_btn(k_bottom_rects[i], k_bottom_labels[i], 16, vm, active);
    }
}

ScreenAction plexus_handle_click(Vector2 vm, int *out_clicked) {
    if (out_clicked) *out_clicked = 0;

    if (CheckCollisionPointRec(vm, k_arrow_up_rect)) {
        s_selected_month = (s_selected_month + 11) % 12;
        s_active_tab = 0;
        if (out_clicked) *out_clicked = 1;
        return ACT_NONE;
    }
    if (CheckCollisionPointRec(vm, k_arrow_down_rect)) {
        s_selected_month = (s_selected_month + 1) % 12;
        s_active_tab = 0;
        if (out_clicked) *out_clicked = 1;
        return ACT_NONE;
    }
    for (int i = 0; i < 4; i++) {
        if (CheckCollisionPointRec(vm, k_card_rects[i])) {
            s_selected_slot = i;
            s_active_tab = 0;
            if (out_clicked) *out_clicked = 1;
            return ACT_NONE;
        }
    }
    if (CheckCollisionPointRec(vm, k_start_rect)) {
        if (out_clicked) *out_clicked = 1;
        return ACT_NONE;
    }
    /* Bottom Bar: index 0 is SAVE & BACK */
    if (CheckCollisionPointRec(vm, k_bottom_rects[0])) {
        if (out_clicked) *out_clicked = 1;
        return ACT_GOTO_MENU;
    }
    for (int i = 1; i < 5; i++) {
        if (CheckCollisionPointRec(vm, k_bottom_rects[i])) {
            s_active_tab = (s_active_tab == i) ? 0 : i;
            if (out_clicked) *out_clicked = 1;
            return ACT_NONE;
        }
    }
    return ACT_NONE;
}
