#include "ui_plexus.h"
#include "cards.h"
#include "ui_speed.h"
#include "ui_theme.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

extern Font font_title;
extern Font font_body;
extern GameSettings settings;

#define PLEXUS_LOG_MAX 32

typedef struct {
    char tag[64];
    char title[80];
    char line1[96];
    char line2[96];
    char line3[96];
} PlexusLogEntry;

static Texture2D s_plexus_bg = { 0 };
static Texture2D s_plexus_normal = { 0 };
static int s_plexus_loaded = 0;
static int s_selected_month = 0; /* 0..11 */
static int s_selected_slot  = 0; /* 0..3 within current month */
static int s_active_tab     = 0; /* 0: node briefing, 1: logfile window, 2..4: training 1..3 */
static unsigned long long s_finished_cards_mask = 0ULL; /* 48-bit mask of completed cards */

static PlexusLogEntry s_log_entries[PLEXUS_LOG_MAX];
static int s_log_count  = 0;
static int s_log_cursor = 0; /* 0 = newest (last given to player), increases toward older entries */
static int s_log_seeded = 0;

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

/* Geometry matching plexus/plexus_mockup.png and plexus/plexus_logfile_mockup.png */
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

/* Logfile middle-screen window geometry (from plexus/plexus_logfile_mockup.png) */
static const Rectangle k_logfile_win_rect     = { 244.0f,  45.0f, 740.0f, 557.0f };
static const Rectangle k_log_top_area_rect    = { 244.0f,  98.0f, 740.0f, 306.0f };
static const Rectangle k_log_scroll_up_rect   = { 590.0f, 101.0f,  48.0f,  26.0f };
static const Rectangle k_log_scroll_down_rect = { 590.0f, 373.0f,  48.0f,  26.0f };
static const Rectangle k_log_stats_rect       = { 265.0f, 431.0f, 423.0f, 150.0f };
static const Rectangle k_log_atlas_rect       = { 703.0f, 431.0f, 255.0f, 151.0f };

static void plexus_log_push(const char *tag, const char *title,
                            const char *l1, const char *l2, const char *l3) {
    if (s_log_count < PLEXUS_LOG_MAX) {
        s_log_count++;
    }
    for (int i = s_log_count - 1; i > 0; i--) {
        s_log_entries[i] = s_log_entries[i - 1];
    }
    snprintf(s_log_entries[0].tag,   sizeof(s_log_entries[0].tag),   "%s", tag   ? tag   : "");
    snprintf(s_log_entries[0].title, sizeof(s_log_entries[0].title), "%s", title ? title : "");
    snprintf(s_log_entries[0].line1, sizeof(s_log_entries[0].line1), "%s", l1    ? l1    : "");
    snprintf(s_log_entries[0].line2, sizeof(s_log_entries[0].line2), "%s", l2    ? l2    : "");
    snprintf(s_log_entries[0].line3, sizeof(s_log_entries[0].line3), "%s", l3    ? l3    : "");
    s_log_cursor = 0;
}

static void ensure_log_seeded(void) {
    if (s_log_seeded) return;
    s_log_seeded = 1;
    plexus_log_push("STORY // SEQ 00 — INTRODUCTION",
                    "ATRIUM PLEX // RESEARCH & CONDITIONING THRESHOLD",
                    "Biomechanical neural link established. Deep inside the vessel,",
                    "an anomalous entity [DEUS SUPREMUS] has been detected.",
                    "Conditioning across all 12 Months (48 cards) is required.");
    plexus_log_push("DIRECTIVE // MONTHLY PROGRESSION",
                    "MONTH 01 // JANUARY // PINE — ACTIVE",
                    "Complete all 4 cards of the current month in any order to",
                    "unlock the next month and continue the non-linear story.",
                    "Finished cards illuminate inside the bottom-right Atlas map.");
    plexus_log_push("LAST EVENT // NODE SELECTED",
                    "CARD 1 OF 4 :  [01-B Crane] — BRIEFING LOADED",
                    "Select any of the 4 month cards on the spine to inspect its task.",
                    "Click START to run conditioning (or toggle card completion in",
                    "placeholder mode) and review updates inside LOGFILE.");
}

static int count_finished_cards(void) {
    int cnt = 0;
    for (int i = 0; i < 48; i++) {
        if ((s_finished_cards_mask >> i) & 1ULL) cnt++;
    }
    return cnt;
}

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
    Color col = COLOR_PHOSPHOR;
    Vector2 ts = MeasureTextEx(font_title, txt, (float)fsz, 1.2f);
    Vector2 pos = { floorf(r.x + (r.width - ts.x) * 0.5f), floorf(r.y + (r.height - (float)fsz) * 0.5f) };
    DrawTextEx(font_title, txt, pos, (float)fsz, 1.2f, col);
    if (hov || active) ui_theme_draw_text_flares(font_title, txt, pos, (float)fsz, 1.2f, col);
}

static void draw_triangle_btn(Rectangle r, int is_up, Vector2 vm, int enabled) {
    int hov = enabled && CheckCollisionPointRec(vm, r);
    float cx = r.x + r.width * 0.5f;
    float top_y = r.y + 4.0f;
    float bot_y = r.y + r.height - 4.0f;
    float hw = 12.0f;
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
    Color fill = !enabled ? (Color){ COLOR_PHOSPHOR_DIM.r, COLOR_PHOSPHOR_DIM.g, COLOR_PHOSPHOR_DIM.b, 70 }
                          : (hov ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    if (hov) {
        Color glow = COLOR_PHOSPHOR;
        glow.a = 70;
        DrawCircleGradient((int)cx, (int)(r.y + r.height * 0.5f), 18.0f, glow, (Color){ 0, 0, 0, 0 });
    }
    DrawTriangle(v1, v2, v3, fill);
    DrawTriangle(v1, v3, v2, fill);
}

static void draw_logfile_atlas_map(const BoardAssets *ba, Rectangle box, Vector2 vm) {
    DrawRectangleRec(box, (Color){ 0, 0, 0, 195 });
    ui_draw_panel_border(box, COLOR_PHOSPHOR_DIM);

    int done_cnt = count_finished_cards();
    char hdr[48];
    snprintf(hdr, sizeof(hdr), "ATLAS CARDS  [%02d / 48]", done_cnt);
    DrawTextEx(font_title, hdr, (Vector2){ floorf(box.x + 10.0f), floorf(box.y + 6.0f) }, 13.0f, 1.2f, COLOR_PHOSPHOR);

    /* 12 columns (Months 01..12) x 4 rows (Cards 0..3) = 48 cards */
    float start_x = box.x + 9.0f;
    float start_y = box.y + 24.0f;
    float cell_w  = 18.0f;
    float cell_h  = 28.0f;
    float step_x  = 20.0f;
    float step_y  = 30.5f;

    Texture2D tex = (ba && settings.difficulty == DIFFICULTY_DEUS && ba->atlas_deus.id > 0)
                    ? ba->atlas_deus
                    : (ba ? ba->atlas : (Texture2D){ 0 });

    for (int m = 0; m < 12; m++) {
        for (int r = 0; r < 4; r++) {
            int cid = m * 4 + r;
            int finished = (int)((s_finished_cards_mask >> cid) & 1ULL);
            Rectangle dst = { floorf(start_x + (float)m * step_x), floorf(start_y + (float)r * step_y), cell_w, cell_h };
            int hov = CheckCollisionPointRec(vm, dst);

            if (tex.id > 0) {
                Rectangle src = { (float)(m * ATLAS_CELL_W), (float)(r * ATLAS_CELL_H), (float)ATLAS_CELL_W, (float)ATLAS_CELL_H };
                Color tint = finished ? WHITE : (Color){ 65, 75, 82, 120 };
                DrawTexturePro(tex, src, dst, (Vector2){ 0, 0 }, 0.0f, tint);
                if (!finished) {
                    DrawRectangleRec(dst, (Color){ 0, 0, 0, 145 });
                }
            } else {
                DrawRectangleRec(dst, finished ? COLOR_BTN_HOVER_BG : (Color){ 10, 14, 16, 200 });
            }

            if (finished) {
                DrawRectangleLinesEx(dst, hov ? 1.5f : 1.0f, COLOR_PHOSPHOR);
            } else {
                Color dim_b = COLOR_PHOSPHOR_DIM;
                dim_b.a = hov ? 165 : 65;
                DrawRectangleLinesEx(dst, 1.0f, dim_b);
            }
        }
    }
}

static void draw_logfile_stats_box(const GameStats *st, Rectangle box) {
    DrawRectangleRec(box, (Color){ 0, 0, 0, 195 });
    ui_draw_panel_border(box, COLOR_PHOSPHOR_DIM);

    DrawTextEx(font_title, "STATS // SPEEDBAR, AUDIO & ARCHIVE",
               (Vector2){ floorf(box.x + 12.0f), floorf(box.y + 7.0f) }, 13.0f, 1.2f, COLOR_PHOSPHOR);
    DrawLine((int)box.x + 12, (int)box.y + 24, (int)(box.x + box.width - 12.0f), (int)box.y + 24, COLOR_PHOSPHOR_DIM);

    int drops       = speed_bar_graded_drops();
    float avg_sec   = speed_bar_avg_drop_seconds();
    float avg_cov   = speed_bar_avg_cover_pct();
    int best_strk   = speed_bar_best_streak();
    int life_mp     = speed_bar_lifetime_minipoints();
    int aud_trig    = board_get_audio_action_triggers();
    float aud_peak  = board_get_audio_peak_synergy() * 100.0f;
    float aud_hold  = board_get_audio_full_hold_seconds();
    int done_cards  = count_finished_cards();

    char row1[96], row2[96], row3[96], row4[96], row5[96];
    if (drops > 0) {
        snprintf(row1, sizeof(row1), "AVG SPEEDBAR TIME  :  %.2fs  (%d GRADED DROPS)", avg_sec, drops);
        snprintf(row2, sizeof(row2), "HOW CLOSE (COVER)  :  %.1f%% MATCH OVERLAP", avg_cov);
    } else {
        snprintf(row1, sizeof(row1), "AVG SPEEDBAR TIME  :  --.--s  (0 GRADED DROPS)");
        snprintf(row2, sizeof(row2), "HOW CLOSE (COVER)  :  --.-%% MATCH OVERLAP");
    }
    snprintf(row3, sizeof(row3), "SPEEDCOIN / STREAK :  %d MP   //   BEST CHAIN x%d", life_mp, best_strk);
    snprintf(row4, sizeof(row4), "ACTION AUDIO FX    :  PEAK %.0f%%  //  100%% HOLD %.1fs (%d)", aud_peak, aud_hold, aud_trig);
    if (st) {
        snprintf(row5, sizeof(row5), "ARCHIVE & MATCHES  :  %02d/48 CARDS  //  %dW-%dL  //  KOI %d",
                 done_cards, st->matches_won, st->matches_lost, st->koi_calls_you);
    } else {
        snprintf(row5, sizeof(row5), "ARCHIVE & MATCHES  :  %02d/48 CARDS ARCHIVED", done_cards);
    }

    float lx = floorf(box.x + 12.0f);
    DrawTextEx(font_body, row1, (Vector2){ lx, floorf(box.y +  31.0f) }, 15.0f, 1.0f, COLOR_TEXT_INFO);
    DrawTextEx(font_body, row2, (Vector2){ lx, floorf(box.y +  54.0f) }, 15.0f, 1.0f, COLOR_TEXT_INFO);
    DrawTextEx(font_body, row3, (Vector2){ lx, floorf(box.y +  77.0f) }, 15.0f, 1.0f, COLOR_PHOSPHOR);
    DrawTextEx(font_body, row4, (Vector2){ lx, floorf(box.y + 100.0f) }, 15.0f, 1.0f, COLOR_TEXT_INFO);
    DrawTextEx(font_body, row5, (Vector2){ lx, floorf(box.y + 123.0f) }, 15.0f, 1.0f, COLOR_PHOSPHOR_DIM);
}

static void draw_logfile_window(const BoardAssets *ba, const GameStats *st, Vector2 vm) {
    ensure_log_seeded();

    DrawRectangleRec(k_logfile_win_rect, COLOR_PANEL_BG);
    ui_draw_panel_border(k_logfile_win_rect, COLOR_PHOSPHOR);

    /* 1. Title Bar & Top Divider (y = 45..98) */
    DrawTextEx(font_title, "LOGFILE",
               (Vector2){ floorf(k_logfile_win_rect.x + 21.0f), floorf(k_logfile_win_rect.y + 18.0f) },
               22.0f, 1.5f, COLOR_PHOSPHOR);
    char idx_txt[64];
    snprintf(idx_txt, sizeof(idx_txt), "ENTRY %02d / %02d   [USE ARROWS TO SCROLL]",
             s_log_count > 0 ? (s_log_cursor + 1) : 0, s_log_count);
    Vector2 its = MeasureTextEx(font_body, idx_txt, 15.0f, 1.1f);
    DrawTextEx(font_body, idx_txt,
               (Vector2){ floorf(k_logfile_win_rect.x + k_logfile_win_rect.width - its.x - 22.0f),
                          floorf(k_logfile_win_rect.y + 22.0f) },
               15.0f, 1.1f, COLOR_PHOSPHOR_DIM);
    DrawLine((int)k_logfile_win_rect.x + 18, 98,
             (int)(k_logfile_win_rect.x + k_logfile_win_rect.width - 18.0f), 98, COLOR_PHOSPHOR_DIM);

    /* 2. Top Section: Story / Last Event / Information Log + Up/Down Scroll Triangles (y = 98..404) */
    int can_scroll_up   = (s_log_cursor > 0);
    int can_scroll_down = (s_log_cursor + 1 < s_log_count);
    draw_triangle_btn(k_log_scroll_up_rect,   1, vm, can_scroll_up);
    draw_triangle_btn(k_log_scroll_down_rect, 0, vm, can_scroll_down);

    float tx = floorf(k_logfile_win_rect.x + 24.0f);
    if (s_log_count > 0) {
        const PlexusLogEntry *cur = &s_log_entries[s_log_cursor];
        DrawTextEx(font_body, cur->tag,   (Vector2){ tx, 132.0f }, 15.0f, 1.2f, COLOR_PHOSPHOR_DIM);
        DrawTextEx(font_title, cur->title, (Vector2){ tx, 154.0f }, 18.0f, 1.3f, COLOR_PHOSPHOR);
        DrawTextEx(font_body, cur->line1, (Vector2){ tx, 186.0f }, 16.0f, 1.1f, COLOR_TEXT_INFO);
        DrawTextEx(font_body, cur->line2, (Vector2){ tx, 210.0f }, 16.0f, 1.1f, COLOR_TEXT_INFO);
        DrawTextEx(font_body, cur->line3, (Vector2){ tx, 234.0f }, 16.0f, 1.1f, COLOR_TEXT_INFO);

        /* Show older entry preview below divider inside top log box */
        if (s_log_cursor + 1 < s_log_count) {
            const PlexusLogEntry *prev = &s_log_entries[s_log_cursor + 1];
            Color dim_line = COLOR_PHOSPHOR_DIM;
            dim_line.a = 90;
            DrawLine((int)tx, 268, (int)(k_logfile_win_rect.x + k_logfile_win_rect.width - 24.0f), 268, dim_line);
            DrawTextEx(font_body, prev->tag,   (Vector2){ tx, 278.0f }, 14.0f, 1.1f, COLOR_PHOSPHOR_DIM);
            DrawTextEx(font_body, prev->title, (Vector2){ tx, 298.0f }, 16.0f, 1.1f, COLOR_PHOSPHOR_DIM);
            DrawTextEx(font_body, prev->line1, (Vector2){ tx, 322.0f }, 15.0f, 1.1f, COLOR_PHOSPHOR_DIM);
            DrawTextEx(font_body, prev->line2, (Vector2){ tx, 344.0f }, 15.0f, 1.1f, COLOR_PHOSPHOR_DIM);
        }
    }

    DrawLine((int)k_logfile_win_rect.x + 18, 404,
             (int)(k_logfile_win_rect.x + k_logfile_win_rect.width - 18.0f), 404, COLOR_PHOSPHOR_DIM);

    /* 3. Bottom-Left Stats Box & Bottom-Right 48-Card Atlas Map */
    draw_logfile_stats_box(st, k_log_stats_rect);
    draw_logfile_atlas_map(ba, k_log_atlas_rect, vm);
}

void plexus_draw(const BoardAssets *ba, const GameStats *st, Vector2 vm) {
    ensure_plexus_assets();
    ensure_log_seeded();

    /* Mouse wheel scroll inside Logfile top section when Logfile is open */
    if (s_active_tab == 1 && CheckCollisionPointRec(vm, k_log_top_area_rect)) {
        float wheel = GetMouseWheelMove();
        if (wheel > 0.1f && s_log_cursor > 0) s_log_cursor--;
        else if (wheel < -0.1f && s_log_cursor + 1 < s_log_count) s_log_cursor++;
    }

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
    draw_triangle_btn(k_arrow_up_rect,   1, vm, 1);
    draw_triangle_btn(k_arrow_down_rect, 0, vm, 1);

    float pulse = 0.55f + 0.45f * sinf((float)GetTime() * 3.8f);
    for (int i = 0; i < 4; i++) {
        Rectangle cr = k_card_rects[i];
        int card_id = s_selected_month * 4 + i;
        int hov = CheckCollisionPointRec(vm, cr);
        int sel = (i == s_selected_slot);
        int done = (int)((s_finished_cards_mask >> card_id) & 1ULL);
        board_draw_card_shadow(cr, (sel || hov) ? 0.85f : 0.35f);
        if (sel) board_draw_card_glow(cr, pulse);
        else if (hov || done) board_draw_card_glow(cr, 0.45f);
        if (ba) board_draw_card(ba, card_id, cr);
        DrawRectangleLinesEx((Rectangle){ cr.x - 1.0f, cr.y - 1.0f, cr.width + 2.0f, cr.height + 2.0f },
                             sel ? 2.0f : 1.0f, (sel || hov || done) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    }

    /* 3. Middle Screen: either Logfile Window (tab 1) or Task / Training Briefing + START button */
    if (s_active_tab == 1) {
        draw_logfile_window(ba, st, vm);
    } else {
        DrawRectangleRec(k_briefing_rect, COLOR_PANEL_BG);
        ui_draw_panel_border(k_briefing_rect, COLOR_PHOSPHOR_DIM);

        char hdr[96];
        char sub[96];
        const char *line1 = "";
        const char *line2 = "";
        const char *line3 = "";
        if (s_active_tab == 0) {
            int card_id = s_selected_month * 4 + s_selected_slot;
            int done = (int)((s_finished_cards_mask >> card_id) & 1ULL);
            snprintf(hdr, sizeof(hdr), "MONTH %02d  //  %s", s_selected_month + 1, k_month_names[s_selected_month]);
            snprintf(sub, sizeof(sub), "CARD %d OF 4 :  [%s]%s", s_selected_slot + 1, CARDS[card_id].name,
                     done ? "   [FINISHED]" : "");
            line1 = "TASK / TRAINING BRIEFING  [PLACEHOLDER]";
            line2 = "Select any of the 4 month cards on the spine to inspect its task.";
            line3 = "Complete all 4 cards in any order to unlock the next month.";
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

        draw_plexus_btn(k_start_rect, "START", 18, vm, 0);
    }

    /* 4. Bottom Bar: SAVE & BACK, LOGFILE, TRAINING 1, TRAINING 2, TRAINING 3 */
    for (int i = 0; i < 5; i++) {
        int active = (i > 0 && s_active_tab == i);
        draw_plexus_btn(k_bottom_rects[i], k_bottom_labels[i], 16, vm, active);
    }
}

ScreenAction plexus_handle_click(Vector2 vm, int *out_clicked) {
    ensure_log_seeded();
    if (out_clicked) *out_clicked = 0;

    /* Logfile scroll triangles when Logfile window is open */
    if (s_active_tab == 1) {
        if (CheckCollisionPointRec(vm, k_log_scroll_up_rect)) {
            if (s_log_cursor > 0) s_log_cursor--;
            if (out_clicked) *out_clicked = 1;
            return ACT_NONE;
        }
        if (CheckCollisionPointRec(vm, k_log_scroll_down_rect)) {
            if (s_log_cursor + 1 < s_log_count) s_log_cursor++;
            if (out_clicked) *out_clicked = 1;
            return ACT_NONE;
        }
    }

    if (CheckCollisionPointRec(vm, k_arrow_up_rect)) {
        s_selected_month = (s_selected_month + 11) % 12;
        s_active_tab = 0;
        char title[80];
        snprintf(title, sizeof(title), "MONTH %02d  //  %s SELECTED", s_selected_month + 1, k_month_names[s_selected_month]);
        plexus_log_push("LAST EVENT // MONTH SWITCHED", title,
                        "Switched active conditioning spine to previous month.",
                        "Select any of the 4 cards on the right spine to inspect its task.",
                        "Complete all 4 cards of the month to advance the story.");
        if (out_clicked) *out_clicked = 1;
        return ACT_NONE;
    }
    if (CheckCollisionPointRec(vm, k_arrow_down_rect)) {
        s_selected_month = (s_selected_month + 1) % 12;
        s_active_tab = 0;
        char title[80];
        snprintf(title, sizeof(title), "MONTH %02d  //  %s SELECTED", s_selected_month + 1, k_month_names[s_selected_month]);
        plexus_log_push("LAST EVENT // MONTH SWITCHED", title,
                        "Switched active conditioning spine to next month.",
                        "Select any of the 4 cards on the right spine to inspect its task.",
                        "Complete all 4 cards of the month to advance the story.");
        if (out_clicked) *out_clicked = 1;
        return ACT_NONE;
    }
    for (int i = 0; i < 4; i++) {
        if (CheckCollisionPointRec(vm, k_card_rects[i])) {
            s_selected_slot = i;
            s_active_tab = 0;
            int cid = s_selected_month * 4 + i;
            char title[80];
            snprintf(title, sizeof(title), "MONTH %02d — CARD %d OF 4 : [%s]",
                     s_selected_month + 1, i + 1, CARDS[cid].name);
            plexus_log_push("LAST EVENT // CARD BRIEFING", title,
                            "Select any of the 4 month cards on the spine to inspect its task.",
                            "Complete all 4 cards in any order to unlock the next month.",
                            "Task briefing recorded to LOGFILE.");
            if (out_clicked) *out_clicked = 1;
            return ACT_NONE;
        }
    }
    if (s_active_tab != 1 && CheckCollisionPointRec(vm, k_start_rect)) {
        int cid = s_selected_month * 4 + s_selected_slot;
        s_finished_cards_mask ^= (1ULL << cid);
        int done = (int)((s_finished_cards_mask >> cid) & 1ULL);
        char title[80];
        snprintf(title, sizeof(title), "[%s] — %s", CARDS[cid].name,
                 done ? "MARKED FINISHED IN ATLAS" : "RESET TO UNFINISHED");
        plexus_log_push("LAST EVENT // START CONDITIONING [PLACEHOLDER]", title,
                        done ? "Card node marked finished and highlighted in the LOGFILE Atlas map."
                             : "Card node reset to dimmed/unfinished state in the LOGFILE Atlas map.",
                        "Open LOGFILE to inspect the 48-card Atlas highlight and telemetry.",
                        "Full solo sparring & story sequences will hook into START next.");
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
