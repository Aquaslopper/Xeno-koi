#include "ui_plexus.h"
#include "cards.h"
#include "rules.h"
#include "ui_audio.h"
#include "ui_card_tooltip.h"
#include "ui_capture_info.h"
#include "ui_input.h"
#include "ui_motion.h"
#include "ui_speed.h"
#include "ui_theme.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

extern Font font_title;
extern Font font_body;
extern GameSettings settings;

#define PLEXUS_LOG_MAX 32
#define PLEXUS_PANEL_BG (Color){ 0, 0, 0, 228 }
#define PLEXUS_SUBBOX_BG (Color){ 4, 8, 10, 236 }

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
static int s_selected_slot  = 3; /* 0..3 within current month (starts at bottom card 4 for Tutorial 1) */
static int s_active_tab     = 0; /* 0: node briefing, 1: logfile window, 2..4: training 1..3 */
static unsigned long long s_finished_cards_mask = 0ULL; /* 48-bit mask of completed cards */

/* Scripted Month 1 Tutorial state machine */
typedef enum {
    TUT1_NONE = 0,
    TUT1_WAIT_DEAL,
    TUT1_PANEL_INTRO,
    TUT1_PLAY_MOON,
    TUT1_PANEL_DECK_DRAW,
    TUT1_WAIT_LEFT_PANEL,
    TUT1_PANEL_LEFT_AREA,
    TUT1_PANEL_TRY_COMBO,
    TUT1_PLAY_SAKE,
    TUT1_PANEL_KOI_DECIDE,
    TUT1_FREE_PLAY
} TutorialStep;

static int s_tut_active  = 0;
static int s_tut_card_id = -1;
static TutorialStep s_tut_step = TUT1_NONE;

/* Speedcoin Minigame state machine */
typedef enum {
    SC_STEP_NONE = 0,
    SC_STEP_INTRO,
    SC_STEP_PLAY,
    SC_STEP_WAVE_END,
    SC_STEP_TIME_UP
} SpeedcoinStep;

static int           s_sc_active               = 0;
static int           s_sc_card_id              = -1;
static SpeedcoinStep s_sc_step                 = SC_STEP_NONE;
static uint32_t      s_sc_rng                  = 0x9E3779B9u;
static float         s_sc_clock                = 180.0f;
static float         s_sc_move_timer           = 0.0f;
static int           s_sc_wave_num             = 1;
static int           s_sc_waves_completed      = 0;
static int           s_sc_hand_num             = 1;
static int           s_sc_hands_synthesized    = 0;
static int           s_sc_wave_coins           = 0;
static int           s_sc_total_coins          = 0;
static int           s_sc_rewarded_tens        = 0;
static int           s_sc_bonus_seconds_total  = 0;
static int           s_sc_damage_seconds_total = 0;
static int           s_sc_banked_combo_pts     = 0;
static int           s_sc_last_wave_pts        = 0;
static int           s_sc_last_wave_yakus      = 0;
static int           s_sc_last_wave_had_h2     = 0;
static int           s_sc_row_cards[2][16];
static int           s_sc_row_count[2]         = { 0, 0 };
static float         s_sc_damage_flash         = 0.0f;
static float         s_sc_bonus_flash          = 0.0f;
static int           s_sc_last_bonus_sec       = 10;
static float         s_sc_synth_banner_timer   = 0.0f;
static float         s_sc_refill_banner_timer  = 0.0f;
static float         s_sc_drop_popup_timer     = 0.0f;
static int           s_sc_drop_popup_mp        = 0;
static Vector2       s_sc_drop_popup_pos       = { 0.0f, 0.0f };
static float         s_sc_reverb_hold          = 0.0f;
static float         s_sc_reverb_energy        = 0.0f;
static float         s_sc_reverb_mid           = 0.0f;
static float         s_sc_reverb_curve         = 0.0f;

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

/* Geometry matching plexus/plexus_mockup.png and expanded vertically for un-squished 18px-22px text */
static const Rectangle k_arrow_up_rect   = { 1096.0f,  60.0f,  48.0f,  30.0f };
static const Rectangle k_arrow_down_rect = { 1096.0f, 610.0f,  48.0f,  30.0f };
static const Rectangle k_card_rects[4] = {
    { 1092.0f, 102.0f, 56.0f, 88.0f },
    { 1092.0f, 228.0f, 56.0f, 88.0f },
    { 1092.0f, 354.0f, 56.0f, 88.0f },
    { 1092.0f, 496.0f, 56.0f, 88.0f }
};
static const Rectangle k_briefing_rect = { 516.0f,  82.0f, 540.0f, 250.0f };
static const Rectangle k_start_rect    = { 896.0f, 348.0f, 160.0f,  48.0f };
static const Rectangle k_bottom_rects[5] = {
    {  64.0f, 644.0f, 156.0f, 48.0f }, /* SAVE & BACK */
    { 246.0f, 644.0f, 152.0f, 48.0f }, /* LOGFILE */
    { 428.0f, 644.0f, 170.0f, 48.0f }, /* SPEEDCOIN */
    { 624.0f, 644.0f, 170.0f, 48.0f }, /* ACTION CARD */
    { 820.0f, 644.0f, 170.0f, 48.0f }  /* TRAINING */
};
static const char * const k_bottom_labels[5] = {
    "SAVE & BACK", "LOGFILE", "SPEEDCOIN", "ACTION CARD", "TRAINING"
};

/* Expanded Logfile middle-screen window geometry (taller top & bottom for full-size readable text) */
static const Rectangle k_logfile_win_rect     = { 236.0f,  22.0f, 756.0f, 602.0f };
static const Rectangle k_log_top_area_rect    = { 236.0f,  74.0f, 756.0f, 312.0f };
static const Rectangle k_log_scroll_up_rect   = { 590.0f,  80.0f,  48.0f,  28.0f };
static const Rectangle k_log_scroll_down_rect = { 590.0f, 352.0f,  48.0f,  28.0f };
static const Rectangle k_log_stats_rect       = { 254.0f, 400.0f, 436.0f, 208.0f };
static const Rectangle k_log_atlas_rect       = { 704.0f, 400.0f, 270.0f, 208.0f };

static void draw_text_snapped(Font font, const char *text, float x, float y, float fsz, float sp, Color col) {
    DrawTextEx(font, text, (Vector2){ floorf(x), floorf(y) }, fsz, sp, col);
}

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

static int is_month_completed(int m) {
    if (m < 0 || m >= 12) return 0;
    return (((s_finished_cards_mask >> (m * 4)) & 0xFULL) == 0xFULL) ? 1 : 0;
}

static int is_month_unlocked(int m) {
    if (m <= 0) return 1;
    if (m >= 12) return 0;
    for (int prev = 0; prev < m; prev++) {
        if (!is_month_completed(prev)) return 0;
    }
    return 1;
}

static int is_card_locked_by_completion(int cid) {
    if (cid < 0 || cid >= 48) return 0;
    /* Exemption: Month 1 Card 4 (cid == 3, Koi-Koi tutorial) can be replayed any time */
    if (cid == 3) return 0;
    return (int)((s_finished_cards_mask >> cid) & 1ULL);
}

static int is_speedcoin_bottom_unlocked(void) {
    return is_month_unlocked(4); /* Unlocks after all 4 cards of Month 1..4 are finished */
}

static int is_action_card_bottom_unlocked(void) {
    return is_month_unlocked(8); /* Unlocks after all 4 cards of Month 1..8 are finished */
}

static int is_bottom_btn_unlocked(int idx) {
    if (idx == 2) return is_speedcoin_bottom_unlocked();
    if (idx == 3) return is_action_card_bottom_unlocked();
    return 1; /* SAVE & BACK (0), LOGFILE (1), and TRAINING (4) are unlocked from the beginning */
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
    DrawRectangleRec(r, (hov || active) ? COLOR_BTN_HOVER_BG : PLEXUS_PANEL_BG);
    ui_draw_panel_border(r, (hov || active) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    Color col = (hov || active) ? COLOR_TEXT_INFO : COLOR_PHOSPHOR;
    Vector2 ts = MeasureTextEx(font_title, txt, (float)fsz, 1.2f);
    Vector2 pos = { floorf(r.x + (r.width - ts.x) * 0.5f), floorf(r.y + (r.height - (float)fsz) * 0.5f) };
    DrawTextEx(font_title, txt, pos, (float)fsz, 1.2f, col);
    if (hov || active) ui_theme_draw_text_flares(font_title, txt, pos, (float)fsz, 1.2f, COLOR_PHOSPHOR);
}

static void draw_triangle_btn(Rectangle r, int is_up, Vector2 vm, int enabled) {
    int hov = enabled && CheckCollisionPointRec(vm, r);
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
    Color fill = !enabled ? (Color){ COLOR_PHOSPHOR_DIM.r, COLOR_PHOSPHOR_DIM.g, COLOR_PHOSPHOR_DIM.b, 85 }
                          : (hov ? COLOR_TEXT_INFO : COLOR_PHOSPHOR);
    if (hov) {
        Color glow = COLOR_PHOSPHOR;
        glow.a = 85;
        DrawCircleGradient((int)cx, (int)(r.y + r.height * 0.5f), 20.0f, glow, (Color){ 0, 0, 0, 0 });
    }
    DrawTriangle(v1, v2, v3, fill);
    DrawTriangle(v1, v3, v2, fill);
}

static void draw_logfile_atlas_map(const BoardAssets *ba, Rectangle box, Vector2 vm) {
    DrawRectangleRec(box, PLEXUS_SUBBOX_BG);
    ui_draw_panel_border(box, COLOR_PHOSPHOR_DIM);

    int done_cnt = count_finished_cards();
    char hdr[48];
    snprintf(hdr, sizeof(hdr), "ATLAS CARDS  [%02d/48]", done_cnt);
    draw_text_snapped(font_title, hdr, box.x + 12.0f, box.y + 10.0f, 16.0f, 1.3f, COLOR_PHOSPHOR);
    DrawLine((int)box.x + 12, (int)box.y + 34, (int)(box.x + box.width - 12.0f), (int)box.y + 34, COLOR_PHOSPHOR_DIM);

    /* 12 columns (Months 01..12) x 4 rows (Cards 0..3) = 48 cards */
    float start_x = box.x + 10.0f;
    float start_y = box.y + 42.0f;
    float cell_w  = 19.0f;
    float cell_h  = 36.0f;
    float step_x  = 21.0f;
    float step_y  = 40.0f;

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
                Color tint = finished ? WHITE : (Color){ 70, 80, 88, 125 };
                DrawTexturePro(tex, src, dst, (Vector2){ 0, 0 }, 0.0f, tint);
                if (!finished) {
                    DrawRectangleRec(dst, (Color){ 0, 0, 0, 145 });
                }
            } else {
                DrawRectangleRec(dst, finished ? COLOR_BTN_HOVER_BG : (Color){ 10, 14, 16, 200 });
            }

            if (finished) {
                DrawRectangleLinesEx(dst, hov ? 2.0f : 1.5f, COLOR_PHOSPHOR);
            } else {
                Color dim_b = COLOR_PHOSPHOR_DIM;
                dim_b.a = hov ? 180 : 75;
                DrawRectangleLinesEx(dst, 1.0f, dim_b);
            }
        }
    }
}

static void draw_logfile_stats_box(const GameStats *st, Rectangle box) {
    DrawRectangleRec(box, PLEXUS_SUBBOX_BG);
    ui_draw_panel_border(box, COLOR_PHOSPHOR_DIM);

    draw_text_snapped(font_title, "STATS // SPEEDBAR, AUDIO & ARCHIVE",
                      box.x + 14.0f, box.y + 10.0f, 16.0f, 1.3f, COLOR_PHOSPHOR);
    DrawLine((int)box.x + 14, (int)box.y + 34, (int)(box.x + box.width - 14.0f), (int)box.y + 34, COLOR_PHOSPHOR_DIM);

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
        snprintf(row1, sizeof(row1), "AVG SPEED   :  %.2fs  [%d DROPS]", avg_sec, drops);
        snprintf(row2, sizeof(row2), "HOW CLOSE   :  %.1f%% MATCH OVERLAP", avg_cov);
    } else {
        snprintf(row1, sizeof(row1), "AVG SPEED   :  --.--s  [0 DROPS]");
        snprintf(row2, sizeof(row2), "HOW CLOSE   :  --.-%% MATCH OVERLAP");
    }
    snprintf(row3, sizeof(row3), "SPEEDCOIN   :  %d MP  [BEST x%d]", life_mp, best_strk);
    snprintf(row4, sizeof(row4), "AUDIO FX    :  %.0f%% PEAK  [%.1fs / %d]", aud_peak, aud_hold, aud_trig);
    if (st) {
        snprintf(row5, sizeof(row5), "[%02d/48 CARDS]  [%dW-%dL]  [KOI-%d]",
                 done_cards, st->matches_won, st->matches_lost, st->koi_calls_you);
    } else {
        snprintf(row5, sizeof(row5), "[%02d/48 CARDS]  [0W-0L]  [KOI-0]", done_cards);
    }

    float lx = box.x + 14.0f;
    draw_text_snapped(font_body, row1, lx, box.y +  44.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body, row2, lx, box.y +  76.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body, row3, lx, box.y + 108.0f, 18.0f, 1.2f, COLOR_PHOSPHOR);
    draw_text_snapped(font_body, row4, lx, box.y + 140.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body, row5, lx, box.y + 172.0f, 18.0f, 1.2f, COLOR_PHOSPHOR);
}

static void draw_logfile_window(const BoardAssets *ba, const GameStats *st, Vector2 vm) {
    ensure_log_seeded();

    DrawRectangleRec(k_logfile_win_rect, PLEXUS_PANEL_BG);
    ui_draw_panel_border(k_logfile_win_rect, COLOR_PHOSPHOR);

    /* 1. Title Bar & Top Divider (y = 22..74) */
    draw_text_snapped(font_title, "LOGFILE",
                      k_logfile_win_rect.x + 22.0f, k_logfile_win_rect.y + 16.0f,
                      24.0f, 1.5f, COLOR_PHOSPHOR);
    char idx_txt[64];
    snprintf(idx_txt, sizeof(idx_txt), "[ENTRY %02d / %02d]   USE ARROWS TO SCROLL",
             s_log_count > 0 ? (s_log_cursor + 1) : 0, s_log_count);
    Vector2 its = MeasureTextEx(font_body, idx_txt, 18.0f, 1.2f);
    draw_text_snapped(font_body, idx_txt,
                      k_logfile_win_rect.x + k_logfile_win_rect.width - its.x - 22.0f,
                      k_logfile_win_rect.y + 20.0f,
                      18.0f, 1.2f, COLOR_TEXT_INFO);
    DrawLine((int)k_logfile_win_rect.x + 18, 74,
             (int)(k_logfile_win_rect.x + k_logfile_win_rect.width - 18.0f), 74, COLOR_PHOSPHOR_DIM);

    /* 2. Top Section: Story / Last Event / Information Log + Up/Down Scroll Triangles (y = 74..386) */
    int can_scroll_up   = (s_log_cursor > 0);
    int can_scroll_down = (s_log_cursor + 1 < s_log_count);
    draw_triangle_btn(k_log_scroll_up_rect,   1, vm, can_scroll_up);
    draw_triangle_btn(k_log_scroll_down_rect, 0, vm, can_scroll_down);

    float tx = k_logfile_win_rect.x + 24.0f;
    if (s_log_count > 0) {
        const PlexusLogEntry *cur = &s_log_entries[s_log_cursor];
        draw_text_snapped(font_body,  cur->tag,   tx, 110.0f, 18.0f, 1.2f, COLOR_PHOSPHOR);
        draw_text_snapped(font_title, cur->title, tx, 136.0f, 20.0f, 1.3f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body,  cur->line1, tx, 170.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body,  cur->line2, tx, 198.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body,  cur->line3, tx, 226.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);

        /* Show older entry preview below divider inside top log box */
        if (s_log_cursor + 1 < s_log_count) {
            const PlexusLogEntry *prev = &s_log_entries[s_log_cursor + 1];
            DrawLine((int)tx, 260, (int)(k_logfile_win_rect.x + k_logfile_win_rect.width - 24.0f), 260, COLOR_PHOSPHOR_DIM);
            draw_text_snapped(font_body,  prev->tag,   tx, 270.0f, 17.0f, 1.2f, COLOR_PHOSPHOR);
            draw_text_snapped(font_title, prev->title, tx, 294.0f, 18.0f, 1.2f, COLOR_TEXT_INFO_DIM);
            draw_text_snapped(font_body,  prev->line1, tx, 322.0f, 18.0f, 1.2f, COLOR_TEXT_INFO_DIM);
        }
    }

    DrawLine((int)k_logfile_win_rect.x + 18, 386,
             (int)(k_logfile_win_rect.x + k_logfile_win_rect.width - 18.0f), 386, COLOR_PHOSPHOR_DIM);

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

    int month_unlocked = is_month_unlocked(s_selected_month);
    float pulse = 0.55f + 0.45f * sinf((float)GetTime() * 3.8f);
    for (int i = 0; i < 4; i++) {
        Rectangle cr = k_card_rects[i];
        int card_id = s_selected_month * 4 + i;
        int hov = CheckCollisionPointRec(vm, cr);
        int sel = (i == s_selected_slot);
        int done = (int)((s_finished_cards_mask >> card_id) & 1ULL);
        board_draw_card_shadow(cr, (month_unlocked && (sel || hov)) ? 0.85f : 0.30f);
        if (month_unlocked && sel) board_draw_card_glow(cr, pulse);
        else if (month_unlocked && (hov || done)) board_draw_card_glow(cr, 0.45f);
        if (ba) {
            if (month_unlocked && done) {
                board_draw_card_back(ba, cr);
            } else {
                board_draw_card(ba, card_id, cr);
                if (!month_unlocked) {
                    DrawRectangleRec(cr, (Color){ 0, 0, 0, 185 });
                }
            }
        }
        Color bcol = !month_unlocked ? (Color){ COLOR_PHOSPHOR_DIM.r, COLOR_PHOSPHOR_DIM.g, COLOR_PHOSPHOR_DIM.b, 95 }
                                     : ((sel || hov || done) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
        DrawRectangleLinesEx((Rectangle){ cr.x - 1.0f, cr.y - 1.0f, cr.width + 2.0f, cr.height + 2.0f },
                             sel ? 2.0f : 1.0f, bcol);
    }

    /* 3. Middle Screen: either Logfile Window (tab 1) or Task / Training Briefing + START button */
    if (s_active_tab == 1) {
        draw_logfile_window(ba, st, vm);
    } else {
        DrawRectangleRec(k_briefing_rect, PLEXUS_PANEL_BG);
        ui_draw_panel_border(k_briefing_rect, COLOR_PHOSPHOR);

        char hdr[96];
        char sub[96];
        const char *line1 = "";
        const char *line2 = "";
        const char *line3 = "";
        int card_id = s_selected_month * 4 + s_selected_slot;
        int card_locked_done = (s_active_tab == 0) && is_card_locked_by_completion(card_id);
        if (s_active_tab == 0) {
            if (!month_unlocked) {
                snprintf(hdr, sizeof(hdr), "MONTH %02d  //  %s   [LOCKED]", s_selected_month + 1, k_month_names[s_selected_month]);
                snprintf(sub, sizeof(sub), "CARD %d OF 4 :  [%s]   [DIMMED]", s_selected_slot + 1, CARDS[card_id].name);
                line1 = "MONTH LOCKED — PREVIOUS MONTH INCOMPLETE";
                line2 = "All 4 cards of the current unlocked month must be finished";
                line3 = "before this month activates on the conditioning spine.";
            } else if (s_selected_month == 0) {
                static const char * const k_tut_titles[4] = {
                    "TUTORIAL 04 // FINAL VERIFICATION MATCH",
                    "SPECIAL GAME // KK-ADD-10 ACTION CARD AUDIO EFFECT",
                    "SPECIAL GAME // KK-ADD-10 SPECIAL SPEEDCOIN",
                    "TUTORIAL 01 // STANDARD RULES OF KOI-KOI"
                };
                static const char * const k_tut_desc1[4] = {
                    "Top card: play a short match to prove everything was understood.",
                    "Third card: special game for the Action Card Audio Effect.",
                    "Second card: special game for the KK-ADD-10 Speedcoin bar.",
                    "Bottom card: learn floral matching, deck draw, yaku & Koi-Koi."
                };
                snprintf(hdr, sizeof(hdr), "MONTH 01  //  %s   [TUTORIAL]", k_month_names[0]);
                snprintf(sub, sizeof(sub), "CARD %d OF 4 :  [%s]", s_selected_slot + 1, CARDS[card_id].name);
                line1 = k_tut_titles[s_selected_slot & 3];
                line2 = k_tut_desc1[s_selected_slot & 3];
                line3 = (s_selected_slot == 3)
                        ? "Koi-Koi tutorial exemption: can be replayed at any time."
                        : "Complete all 4 tutorial cards (bottom to top) to unlock Month 02.";
            } else {
                snprintf(hdr, sizeof(hdr), "MONTH %02d  //  %s", s_selected_month + 1, k_month_names[s_selected_month]);
                snprintf(sub, sizeof(sub), "CARD %d OF 4 :  [%s]", s_selected_slot + 1, CARDS[card_id].name);
                line1 = "TASK / TRAINING BRIEFING  [PLACEHOLDER]";
                line2 = "Complete all 4 cards of this month in any order to turn";
                line3 = "each card face down and unlock the next month.";
            }
        } else if (s_active_tab == 2) {
            snprintf(hdr, sizeof(hdr), "ATRIUM PLEX  //  SPEEDCOIN");
            snprintf(sub, sizeof(sub), "SPECIAL GAME  //  KK-ADD-10 SPEEDCOIN");
            line1 = "Special precision & speed minigame built around the KK-ADD-10";
            line2 = "1.0-second timer bar and card drop overlap accuracy.";
            line3 = "Select a card on the right spine to return to Month view.";
        } else if (s_active_tab == 3) {
            snprintf(hdr, sizeof(hdr), "ATRIUM PLEX  //  ACTION CARD");
            snprintf(sub, sizeof(sub), "SPECIAL GAME  //  KK-ADD-10 ACTION CARD AUDIO");
            line1 = "Special game focused on the KK-ADD-10 Action Card Audio Effect";
            line2 = "stereo reverb synergy and capture streak resonance.";
            line3 = "Select a card on the right spine to return to Month view.";
        } else {
            snprintf(hdr, sizeof(hdr), "ATRIUM PLEX  //  TRAINING");
            snprintf(sub, sizeof(sub), "NORMAL MATCH  //  OPTION MENU SETTINGS");
            line1 = "Launches a standard match using your current Option menu";
            line2 = "settings (Ruleset, Difficulty, and Round count).";
            line3 = "Click START to play a training match, or select a spine card.";
        }

        draw_text_snapped(font_title, hdr,   k_briefing_rect.x + 22.0f, k_briefing_rect.y + 18.0f, 21.0f, 1.4f, COLOR_PHOSPHOR);
        DrawLine((int)k_briefing_rect.x + 22, (int)k_briefing_rect.y + 50,
                 (int)(k_briefing_rect.x + k_briefing_rect.width - 22.0f), (int)k_briefing_rect.y + 50, COLOR_PHOSPHOR_DIM);
        draw_text_snapped(font_body,  sub,   k_briefing_rect.x + 22.0f, k_briefing_rect.y + 64.0f, 19.0f, 1.2f, COLOR_PHOSPHOR);
        draw_text_snapped(font_body,  line1, k_briefing_rect.x + 22.0f, k_briefing_rect.y + 108.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body,  line2, k_briefing_rect.x + 22.0f, k_briefing_rect.y + 142.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body,  line3, k_briefing_rect.x + 22.0f, k_briefing_rect.y + 176.0f, 18.0f, 1.2f, COLOR_TEXT_INFO_DIM);

        int start_locked = (s_active_tab == 0 && (!month_unlocked || card_locked_done)) ||
                           (s_active_tab == 2 && !is_speedcoin_bottom_unlocked()) ||
                           (s_active_tab == 3 && !is_action_card_bottom_unlocked());
        if (start_locked) {
            DrawRectangleRec(k_start_rect, PLEXUS_PANEL_BG);
            ui_draw_panel_border(k_start_rect, COLOR_PHOSPHOR_DIM);
            Vector2 ts = MeasureTextEx(font_title, "LOCKED", 18.0f, 1.2f);
            draw_text_snapped(font_title, "LOCKED",
                              k_start_rect.x + (k_start_rect.width - ts.x) * 0.5f,
                              k_start_rect.y + (k_start_rect.height - 18.0f) * 0.5f,
                              18.0f, 1.2f, COLOR_PHOSPHOR_DIM);
        } else {
            draw_plexus_btn(k_start_rect, "START", 18, vm, 0);
        }
    }

    /* 4. Bottom Bar: SAVE & BACK, LOGFILE, SPEEDCOIN, ACTION CARD, TRAINING */
    for (int i = 0; i < 5; i++) {
        int btn_unlocked = is_bottom_btn_unlocked(i);
        if (!btn_unlocked) {
            Rectangle r = k_bottom_rects[i];
            DrawRectangleRec(r, PLEXUS_PANEL_BG);
            ui_draw_panel_border(r, COLOR_PHOSPHOR_DIM);
            Vector2 ts = MeasureTextEx(font_title, k_bottom_labels[i], 16.0f, 1.2f);
            draw_text_snapped(font_title, k_bottom_labels[i],
                              r.x + (r.width - ts.x) * 0.5f,
                              r.y + (r.height - 16.0f) * 0.5f,
                              16.0f, 1.2f, COLOR_PHOSPHOR_DIM);
        } else {
            int active = (i > 0 && s_active_tab == i);
            draw_plexus_btn(k_bottom_rects[i], k_bottom_labels[i], 16, vm, active);
        }
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
        /* Bottom tab 4 (TRAINING): launch normal match based on Option menu settings */
        if (s_active_tab == 4) {
            plexus_tutorial_abort();
            plexus_speedcoin_abort();
            plexus_log_push("TRAINING // NORMAL MATCH LAUNCHED",
                            "FREE SPARRING — OPTION MENU SETTINGS",
                            "Starting a standard match configured by your current",
                            "Option menu settings (Ruleset, Difficulty, and Rounds).",
                            "Match telemetry recorded to Atrium Plex LOGFILE.");
            if (out_clicked) *out_clicked = 1;
            return ACT_START_GAME;
        }
        if (s_active_tab == 2) {
            if (!is_speedcoin_bottom_unlocked()) return ACT_NONE;
            plexus_tutorial_abort();
            s_sc_active  = 1;
            s_sc_card_id = -1;
            s_sc_step    = SC_STEP_INTRO;
            plexus_log_push("SPEEDCOIN // SYNAPTIC COUNTDOWN INITIATED",
                            "SPECIAL GAME // KK-ADD-10 SPEEDCOIN",
                            "Warning: Synaptic countdown active (03:00).",
                            "1000ms per move to extract Speedcoins; 50 SC synthesizes 2nd hand.",
                            "+10s per 10 Speedcoins; -1s clock drain per 1000ms timeout.");
            if (out_clicked) *out_clicked = 1;
            return ACT_START_GAME;
        }
        if (s_active_tab == 3) {
            if (!is_action_card_bottom_unlocked()) return ACT_NONE;
            if (out_clicked) *out_clicked = 1;
            return ACT_NONE;
        }
        int cid = s_selected_month * 4 + s_selected_slot;
        if (s_active_tab == 0 && (!is_month_unlocked(s_selected_month) || is_card_locked_by_completion(cid))) {
            return ACT_NONE;
        }
        /* Month 1 Card 4 (bottom card, slot == 3, cid == 3): Koi-Koi tutorial (replayable any time) */
        if (s_active_tab == 0 && s_selected_month == 0 && s_selected_slot == 3) {
            plexus_speedcoin_abort();
            s_tut_active  = 1;
            s_tut_card_id = cid;
            s_tut_step    = TUT1_WAIT_DEAL;
            plexus_log_push("TUTORIAL // PROTOCOL 01 INITIATED",
                            "CARD 4 OF 4 : [01-P2 Plain] — STANDARD RULES OF KOI-KOI",
                            "Informational upgrade started for Entity 001.",
                            "Follow the tutorial prompts to capture the Moon and Sake Cup,",
                            "form Moon Viewing (Tsukimi-zake), and complete Card 4.");
            if (out_clicked) *out_clicked = 1;
            return ACT_START_GAME;
        }
        /* Month 1 Card 3 (2nd from bottom, slot == 2, cid == 2): KK-ADD-10 Special Speedcoin */
        if (s_active_tab == 0 && s_selected_month == 0 && s_selected_slot == 2) {
            plexus_tutorial_abort();
            s_sc_active  = 1;
            s_sc_card_id = cid;
            s_sc_step    = SC_STEP_INTRO;
            plexus_log_push("SPEEDCOIN // PROTOCOL 02 INITIATED",
                            "CARD 3 OF 4 : [01-P1 Plain] — KK-ADD-10 SPECIAL SPEEDCOIN",
                            "Warning: Synaptic countdown active (03:00).",
                            "1000ms per move to extract Speedcoins; 50 SC synthesizes 2nd hand.",
                            "Complete the synaptic countdown run to finish Card 3.");
            if (out_clicked) *out_clicked = 1;
            return ACT_START_GAME;
        }
        s_finished_cards_mask |= (1ULL << cid);
        int month_done = is_month_completed(s_selected_month);
        char title[80];
        if (month_done && s_selected_month == 3) {
            snprintf(title, sizeof(title), "MONTH 04 COMPLETE — SPEEDCOIN UNLOCKED!");
            plexus_log_push("STORY // SPEEDCOIN MODULE UNLOCKED", title,
                            "All 4 cards of Month 04 are finished and locked.",
                            "SPEEDCOIN is now unlocked in the bottom bar, and",
                            "Month 05 is now active on the conditioning spine.");
        } else if (month_done && s_selected_month == 7) {
            snprintf(title, sizeof(title), "MONTH 08 COMPLETE — ACTION CARD UNLOCKED!");
            plexus_log_push("STORY // ACTION CARD MODULE UNLOCKED", title,
                            "All 4 cards of Month 08 are finished and locked.",
                            "ACTION CARD is now unlocked in the bottom bar, and",
                            "Month 09 is now active on the conditioning spine.");
        } else if (month_done && s_selected_month + 1 < 12) {
            snprintf(title, sizeof(title), "MONTH %02d COMPLETE — MONTH %02d UNLOCKED!",
                     s_selected_month + 1, s_selected_month + 2);
            plexus_log_push("STORY // MONTHLY PROGRESSION UNLOCKED", title,
                            "All 4 cards of this month are finished and locked.",
                            "The next month on the conditioning spine is now active.",
                            "Use the up/down arrows to advance to the newly unlocked month.");
        } else {
            snprintf(title, sizeof(title), "[%s] — FINISHED & LOCKED", CARDS[cid].name);
            plexus_log_push("LAST EVENT // CARD COMPLETED", title,
                            "Card turned face down on the spine and locked.",
                            "Complete all 4 cards of the current month in any order to",
                            "unlock the next month on the spine.");
        }
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
            if (!is_bottom_btn_unlocked(i)) {
                return ACT_NONE;
            }
            s_active_tab = (s_active_tab == i) ? 0 : i;
            if (out_clicked) *out_clicked = 1;
            return ACT_NONE;
        }
    }
    return ACT_NONE;
}

/* =========================================================================
 * Scripted Month 1 Tutorial Match — Card 4 (Bottom): Standard Rules of Koi-Koi
 * ========================================================================= */

int plexus_tutorial_is_active(void) {
    return s_tut_active;
}

void plexus_tutorial_abort(void) {
    s_tut_active  = 0;
    s_tut_card_id = -1;
    s_tut_step    = TUT1_NONE;
}

void plexus_tutorial_setup_round(GameState *g) {
    if (!g || !s_tut_active) return;

    g->ruleset        = RULESET_FUDAWIKI;
    g->total_rounds   = 1;
    g->current_round  = 1;
    g->dealer         = 0;
    g->current_player = 0;
    g->phase          = PHASE_TURN_HAND;
    g->hand_count[0]  = 8;
    g->hand_count[1]  = 8;
    g->field_count    = 8;
    g->stock_count    = 24;
    g->captured_count[0] = 0;
    g->captured_count[1] = 0;
    g->has_pending    = 0;
    g->pending_card   = -1;
    g->last_base[0]   = 0;
    g->last_base[1]   = 0;
    memset(g->last_yaku_points, 0, sizeof(g->last_yaku_points));
    g->koi_called[0]  = 0;
    g->koi_called[1]  = 0;

    /* Table (8 cards): includes 28 (08-B Moon) and 32 (09-A Sake Cup) + 6 distinct plain months */
    static const int k_field[8] = { 28, 32, 2, 10, 14, 18, 22, 26 };
    /* Player Hand (8 cards): 29 (08-A Geese -> Moon) and 34 (09-P1 Plain -> Sake Cup) first */
    static const int k_hand0[8] = { 29, 34, 0, 4, 8, 20, 24, 36 };
    /* CPU Hand (8 cards): harmless plain matches so CPU Turn 1 captures a single plain */
    static const int k_hand1[8] = { 11, 15, 19, 23, 27, 3, 7, 39 };

    int used[48];
    memset(used, 0, sizeof(used));
    for (int i = 0; i < 8; i++) {
        g->field[i]   = k_field[i]; used[k_field[i]] = 1;
        g->hand[0][i] = k_hand0[i]; used[k_hand0[i]] = 1;
        g->hand[1][i] = k_hand1[i]; used[k_hand1[i]] = 1;
    }

    /* Reserve top 3 stock draws (stock[23], stock[22], stock[21]):
       23 -> 38 (10-P1 Plain, drawn after player captures Moon)
       22 -> 42 (11-R Ribbon, drawn on CPU Turn 1)
       21 -> 46 (12-P2 Plain, drawn after player captures Sake Cup) */
    used[38] = 1;
    used[42] = 1;
    used[46] = 1;

    int s_idx = 0;
    for (int cid = 0; cid < 48; cid++) {
        if (!used[cid]) {
            g->stock[s_idx++] = cid;
        }
    }
    g->stock[21] = 46;
    g->stock[22] = 42;
    g->stock[23] = 38;
    s_tut_step   = TUT1_WAIT_DEAL;
}

static int tut_step_is_frozen(TutorialStep st) {
    return (st == TUT1_PANEL_INTRO ||
            st == TUT1_PANEL_DECK_DRAW ||
            st == TUT1_PANEL_LEFT_AREA ||
            st == TUT1_PANEL_TRY_COMBO ||
            st == TUT1_PANEL_KOI_DECIDE) ? 1 : 0;
}

static Rectangle tut_panel_rect_for_step(TutorialStep st) {
    if (st == TUT1_PANEL_KOI_DECIDE) {
        return (Rectangle){ 260.0f, 26.0f, 760.0f, 178.0f };
    }
    return (Rectangle){ 280.0f, 150.0f, 720.0f, 380.0f };
}

static Rectangle tut_continue_rect_for_panel(Rectangle pr) {
    return (Rectangle){ pr.x + pr.width - 186.0f, pr.y + pr.height - 56.0f, 164.0f, 40.0f };
}

int plexus_tutorial_allowed_hand_card(const GameState *g) {
    if (!s_tut_active || !g) return -1;
    if (s_tut_step == TUT1_PLAY_MOON) return 29; /* 08-A Geese -> captures 08-B Moon */
    if (s_tut_step == TUT1_PLAY_SAKE) return 34; /* 09-P1 Plain -> captures 09-A Sake Cup */
    return -1;
}

int plexus_tutorial_update_and_click(const GameState *g, const AnimState *anim,
                                     int board_settled, float overlay_alpha,
                                     Vector2 vm, int mouse_pressed, int *out_clicked) {
    if (out_clicked) *out_clicked = 0;
    if (!s_tut_active || !g) return 0;

    /* Advance non-frozen trigger states */
    if (s_tut_step == TUT1_WAIT_DEAL) {
        if (board_settled && g->phase == PHASE_TURN_HAND && g->current_player == 0) {
            s_tut_step = TUT1_PANEL_INTRO;
        }
    } else if (s_tut_step == TUT1_PLAY_MOON) {
        /* Freeze as soon as the deck card flies from the stock pile onto the table */
        if (g->phase == PHASE_TURN_DRAW && anim && anim->active &&
            anim->type == ANIM_STOCK_FLY && anim->elapsed >= anim->duration * 0.72f) {
            s_tut_step = TUT1_PANEL_DECK_DRAW;
        }
    } else if (s_tut_step == TUT1_WAIT_LEFT_PANEL) {
        /* Once the capture animation finishes and cards sit in the left panel, open left-panel explanation */
        if (board_settled && g->captured_count[0] >= 2) {
            s_tut_step = TUT1_PANEL_LEFT_AREA;
        }
    } else if (s_tut_step == TUT1_PLAY_SAKE) {
        if (g->phase == PHASE_DECIDE && g->current_player == 0 && overlay_alpha >= 0.75f) {
            s_tut_step = TUT1_PANEL_KOI_DECIDE;
        }
    }

    if (!tut_step_is_frozen(s_tut_step)) return 0;

    if (mouse_pressed) {
        Rectangle pr = tut_panel_rect_for_step(s_tut_step);
        Rectangle cr = tut_continue_rect_for_panel(pr);
        /* On Koi-Koi decision screen, if the player clicks directly on CALL KOI-KOI or SHOBU,
           unfreeze and let that click pass straight through to the Koi-Koi buttons! */
        if (s_tut_step == TUT1_PANEL_KOI_DECIDE &&
            (CheckCollisionPointRec(vm, (Rectangle){ 410.0f, 518.0f, 210.0f, 48.0f }) ||
             CheckCollisionPointRec(vm, (Rectangle){ 660.0f, 518.0f, 210.0f, 48.0f }))) {
            s_tut_step = TUT1_FREE_PLAY;
            return 0;
        }
        if (CheckCollisionPointRec(vm, pr) || CheckCollisionPointRec(vm, cr)) {
            if (out_clicked) *out_clicked = 1;
            if (s_tut_step == TUT1_PANEL_INTRO)           s_tut_step = TUT1_PLAY_MOON;
            else if (s_tut_step == TUT1_PANEL_DECK_DRAW)  s_tut_step = TUT1_WAIT_LEFT_PANEL;
            else if (s_tut_step == TUT1_PANEL_LEFT_AREA)  s_tut_step = TUT1_PANEL_TRY_COMBO;
            else if (s_tut_step == TUT1_PANEL_TRY_COMBO)  s_tut_step = TUT1_PLAY_SAKE;
            else if (s_tut_step == TUT1_PANEL_KOI_DECIDE) s_tut_step = TUT1_FREE_PLAY;
        }
    }
    return 1;
}

void plexus_tutorial_draw_board_guides(const BoardAssets *ba, const GameState *g) {
    (void)ba;
    if (!s_tut_active || !g) return;

    float pulse = 0.55f + 0.45f * sinf((float)GetTime() * 5.0f);
    int allowed = plexus_tutorial_allowed_hand_card(g);

    /* Dim locked hand cards and highlight the single allowed hand card + target table card */
    if (allowed >= 0 && g->current_player == 0 && g->phase == PHASE_TURN_HAND) {
        int target_field_cid = (allowed == 29) ? 28 : 32;
        for (int i = 0; i < g->hand_count[0]; i++) {
            int cid = g->hand[0][i];
            Rectangle hr = motion_get_rect(cid);
            if (hr.width <= 0.0f) hr = board_get_hand_card_rect(i, g->hand_count[0], 0);
            if (cid != allowed) {
                DrawRectangleRec(hr, (Color){ 0, 0, 0, 175 });
            } else {
                board_draw_card_glow(hr, pulse);
                DrawRectangleLinesEx((Rectangle){ hr.x - 3.0f, hr.y - 3.0f, hr.width + 6.0f, hr.height + 6.0f },
                                     2.5f, COLOR_PHOSPHOR);
            }
        }
        Rectangle fr = motion_get_rect(target_field_cid);
        if (fr.width <= 0.0f) fr = board_get_field_card_rect_of(g, target_field_cid);
        if (fr.width > 0.0f) {
            board_draw_card_glow(fr, pulse);
            DrawRectangleLinesEx((Rectangle){ fr.x - 3.0f, fr.y - 3.0f, fr.width + 6.0f, fr.height + 6.0f },
                                 2.5f, COLOR_PHOSPHOR);
        }
    }

    /* Highlight left-side scoring area when TUT1_PANEL_LEFT_AREA is active */
    if (s_tut_step == TUT1_PANEL_LEFT_AREA) {
        Rectangle left_box = { 6.0f, 405.0f, 232.0f, 305.0f };
        DrawRectangleLinesEx(left_box, 3.0f, COLOR_PHOSPHOR);
        Color glow = COLOR_PHOSPHOR;
        glow.a = (unsigned char)(65.0f * pulse);
        DrawRectangleRec(left_box, glow);
    }
}

void plexus_tutorial_draw_panel(const BoardAssets *ba, Vector2 vm) {
    if (!s_tut_active || !tut_step_is_frozen(s_tut_step)) return;

    if (s_tut_step != TUT1_PANEL_KOI_DECIDE) {
        DrawRectangle(0, 0, 1280, 720, (Color){ 0, 0, 0, 145 });
    }

    Rectangle pr = tut_panel_rect_for_step(s_tut_step);
    Rectangle cr = tut_continue_rect_for_panel(pr);
    DrawRectangleRec(pr, PLEXUS_PANEL_BG);
    ui_draw_panel_border(pr, COLOR_PHOSPHOR);

    float x = pr.x + 26.0f;
    float y = pr.y + 18.0f;

    if (s_tut_step == TUT1_PANEL_INTRO) {
        draw_text_snapped(font_title, "WELCOME, ENTITY 001 — INFORMATIONAL UPGRADE INITIATED.",
                          x, y, 18.0f, 1.3f, COLOR_PHOSPHOR);
        DrawLine((int)x, (int)(y + 30.0f), (int)(pr.x + pr.width - 26.0f), (int)(y + 30.0f), COLOR_PHOSPHOR_DIM);

        draw_text_snapped(font_title,
                          "Match cards from your hand or the deck with the table's",
                          x, y + 44.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
        draw_text_snapped(font_title,
                          "matching monthly flora to capture them.",
                          x, y + 68.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body,
                          "Once you build a scoring combination, you can cash out your points",
                          x, y + 98.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body,
                          "immediately or call \"Koi-Koi\" to risk them for a higher score.",
                          x, y + 122.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
        draw_text_snapped(font_title,
                          "Each of the 12 months features 4 identical floral cards.",
                          x, y + 154.0f, 18.0f, 1.2f, COLOR_PHOSPHOR);

        /* Visual card preview of the move to play: 08-A Geese -> 08-B Moon */
        Rectangle c_hand  = { x + 18.0f,  y + 190.0f, 64.0f, 100.0f };
        Rectangle c_table = { x + 150.0f, y + 190.0f, 64.0f, 100.0f };
        if (ba) {
            board_draw_card(ba, 29, c_hand);
            board_draw_card(ba, 28, c_table);
        }
        DrawRectangleLinesEx(c_hand,  2.0f, COLOR_PHOSPHOR);
        DrawRectangleLinesEx(c_table, 2.0f, COLOR_PHOSPHOR);
        draw_text_snapped(font_title, "->", x + 100.0f, y + 228.0f, 24.0f, 1.2f, COLOR_PHOSPHOR);
        draw_text_snapped(font_title, "YOUR MOVE: CAPTURE THE MOON",
                          x + 236.0f, y + 206.0f, 18.0f, 1.2f, COLOR_PHOSPHOR);
        draw_text_snapped(font_body, "Play [08-A Geese] from your hand onto",
                          x + 236.0f, y + 236.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body, "[08-B Moon] on the table (Month 8 Susuki).",
                          x + 236.0f, y + 260.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
    } else if (s_tut_step == TUT1_PANEL_DECK_DRAW) {
        draw_text_snapped(font_title, "DECK DRAW // AUTOMATIC SECOND STEP",
                          x, y, 20.0f, 1.3f, COLOR_PHOSPHOR);
        DrawLine((int)x, (int)(y + 32.0f), (int)(pr.x + pr.width - 26.0f), (int)(y + 32.0f), COLOR_PHOSPHOR_DIM);

        draw_text_snapped(font_title,
                          "Between each move, a card from the deck is drawn automatically.",
                          x, y + 52.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body,
                          "Right after you play a card from your hand, the top card of the",
                          x, y + 90.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body,
                          "deck flips and flies onto the table.",
                          x, y + 116.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body,
                          "If the drawn deck card matches a month on the table, it captures",
                          x, y + 150.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body,
                          "that card too; otherwise, it stays on the table.",
                          x, y + 176.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);

        Rectangle c_back = { x + 18.0f,  y + 210.0f, 58.0f, 92.0f };
        Rectangle c_draw = { x + 136.0f, y + 210.0f, 58.0f, 92.0f };
        if (ba) {
            board_draw_card_back(ba, c_back);
            board_draw_card(ba, 38, c_draw);
        }
        DrawRectangleLinesEx(c_back, 1.5f, COLOR_PHOSPHOR_DIM);
        DrawRectangleLinesEx(c_draw, 2.0f, COLOR_PHOSPHOR);
        draw_text_snapped(font_title, "->", x + 90.0f, y + 244.0f, 22.0f, 1.2f, COLOR_PHOSPHOR);
        draw_text_snapped(font_body, "Deck card drawn onto the table.",
                          x + 214.0f, y + 246.0f, 18.0f, 1.2f, COLOR_PHOSPHOR);
    } else if (s_tut_step == TUT1_PANEL_LEFT_AREA) {
        draw_text_snapped(font_title, "SCORING AREA // LEFT SIDE PANEL",
                          x, y, 20.0f, 1.3f, COLOR_PHOSPHOR);
        DrawLine((int)x, (int)(y + 32.0f), (int)(pr.x + pr.width - 26.0f), (int)(y + 32.0f), COLOR_PHOSPHOR_DIM);

        draw_text_snapped(font_title,
                          "Captured cards are organized into your scoring area to build",
                          x, y + 54.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
        draw_text_snapped(font_title,
                          "specific point-generating combinations, or yaku.",
                          x, y + 80.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body,
                          "Collecting enough cards of the same type like brights, ribbons,",
                          x, y + 124.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body,
                          "or animals instantly completes a combo and allows you to",
                          x, y + 150.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body,
                          "declare \"Koi-Koi\" or end the round.",
                          x, y + 176.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body,
                          "<-- Your captured [08-B Moon] and [08-A Geese] are now stored",
                          x, y + 224.0f, 18.0f, 1.2f, COLOR_PHOSPHOR);
        draw_text_snapped(font_body,
                          "    in your left-side scoring area.",
                          x, y + 250.0f, 18.0f, 1.2f, COLOR_PHOSPHOR);
    } else if (s_tut_step == TUT1_PANEL_TRY_COMBO) {
        draw_text_snapped(font_title, "NEXT OBJECTIVE // FORM A COMBO",
                          x, y, 20.0f, 1.3f, COLOR_PHOSPHOR);
        DrawLine((int)x, (int)(y + 32.0f), (int)(pr.x + pr.width - 26.0f), (int)(y + 32.0f), COLOR_PHOSPHOR_DIM);

        draw_text_snapped(font_title,
                          "Try forming a combo now to view the Koi-Koi screen.",
                          x, y + 60.0f, 20.0f, 1.2f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body,
                          "On your next turn, play your Month 09 card [09-P1 Plain] to",
                          x, y + 106.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body,
                          "capture [09-A Sake Cup] and complete Moon + Sake (5 PTS)!",
                          x, y + 132.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);

        Rectangle c_moon = { x + 18.0f,  y + 178.0f, 64.0f, 100.0f };
        Rectangle c_sake = { x + 136.0f, y + 178.0f, 64.0f, 100.0f };
        if (ba) {
            board_draw_card(ba, 28, c_moon);
            board_draw_card(ba, 32, c_sake);
        }
        DrawRectangleLinesEx(c_moon, 2.0f, COLOR_PHOSPHOR);
        DrawRectangleLinesEx(c_sake, 2.0f, COLOR_PHOSPHOR);
        draw_text_snapped(font_title, "+", x + 98.0f, y + 216.0f, 24.0f, 1.2f, COLOR_PHOSPHOR);
        draw_text_snapped(font_title, "TSUKIMI-ZAKE (MOON VIEWING) = 5 PTS",
                          x + 222.0f, y + 216.0f, 18.0f, 1.2f, COLOR_PHOSPHOR);
    } else if (s_tut_step == TUT1_PANEL_KOI_DECIDE) {
        draw_text_snapped(font_title, "You have completed a combo!",
                          x, y, 21.0f, 1.3f, COLOR_PHOSPHOR);
        DrawLine((int)x, (int)(y + 30.0f), (int)(pr.x + pr.width - 26.0f), (int)(y + 30.0f), COLOR_PHOSPHOR_DIM);
        draw_text_snapped(font_body,
                          "Tap \"Koi-Koi\" to keep playing for more points, or tap \"Stop\"",
                          x, y + 46.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body,
                          "(Shobu) to end the round and claim your victory.",
                          x, y + 74.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
    }

    draw_plexus_btn(cr, "[CONTINUE]", 16, vm, CheckCollisionPointRec(vm, pr));
}

void plexus_tutorial_finish_match(void) {
    if (!s_tut_active) return;
    int cid = (s_tut_card_id >= 0 && s_tut_card_id < 48) ? s_tut_card_id : 3;
    s_finished_cards_mask |= (1ULL << cid);
    s_tut_active  = 0;
    s_tut_card_id = -1;
    s_tut_step    = TUT1_NONE;

    char title[80];
    snprintf(title, sizeof(title), "[%s] — TUTORIAL 01 COMPLETED", CARDS[cid].name);
    plexus_log_push("STORY // TUTORIAL CARD 1 COMPLETED", title,
                    "Entity 001 completed Standard Rules of Koi-Koi conditioning.",
                    "Card 4 of Month 01 is finished (and remains replayable).",
                    "Select the next card above it on the spine to continue.");
}

/* =========================================================================
 * Speedcoin Minigame — KK-ADD-10 Solo Reflex & Accuracy Conditioning
 * ========================================================================= */

#define SC_FIELD_DROP_RECT  (Rectangle){ 276.0f,  60.0f, 864.0f, 428.0f }
#define SC_STOCK_RECT       (Rectangle){ 288.0f, 196.0f,  76.0f, 124.0f }
#define SC_COMBOS_HIT_RECT  (Rectangle){ 1148.0f, 232.0f, 120.0f, 316.0f }

static uint32_t sc_next_u32(void) {
    uint32_t x = s_sc_rng ? s_sc_rng : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_sc_rng = x;
    return x;
}

static void sc_sort_cards_by_month(int *arr, int n) {
    for (int i = 1; i < n; i++) {
        int key = arr[i];
        int km  = CARDS[key].month;
        int j   = i - 1;
        while (j >= 0 && (CARDS[arr[j]].month > km ||
                         (CARDS[arr[j]].month == km && arr[j] > key))) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

static Rectangle sc_get_row_card_rect(int row, int col, int row_count) {
    int cnt = (row_count > 0) ? row_count : 1;
    float cx = 758.0f;
    float avail_w = 720.0f;
    float step = 94.0f;
    float total_w = (cnt > 1) ? ((float)(cnt - 1) * step + (float)FIELD_CARD_W) : (float)FIELD_CARD_W;
    if (total_w > avail_w && cnt > 1) {
        step = (avail_w - (float)FIELD_CARD_W) / (float)(cnt - 1);
        total_w = (float)(cnt - 1) * step + (float)FIELD_CARD_W;
    }
    float x = cx - total_w * 0.5f + (float)col * step;
    float y = (row == 0) ? 104.0f : 264.0f;
    return (Rectangle){ x, y, (float)FIELD_CARD_W, (float)FIELD_CARD_H };
}

static Rectangle sc_find_table_card_rect(int card_id) {
    for (int r = 0; r < 2; r++) {
        for (int c = 0; c < s_sc_row_count[r]; c++) {
            if (s_sc_row_cards[r][c] == card_id) {
                return sc_get_row_card_rect(r, c, s_sc_row_count[r]);
            }
        }
    }
    return (Rectangle){ 0, 0, 0, 0 };
}

static void sc_sync_game_field(GameState *g) {
    if (!g) return;
    int idx = 0;
    for (int r = 0; r < 2; r++) {
        sc_sort_cards_by_month(s_sc_row_cards[r], s_sc_row_count[r]);
        for (int c = 0; c < s_sc_row_count[r]; c++) {
            if (idx < 48) {
                g->field[idx++] = s_sc_row_cards[r][c];
            }
        }
    }
    g->field_count = idx;
}

static void sc_start_wave(GameState *g, int wave_idx) {
    if (!g) return;
    s_sc_wave_num             = wave_idx;
    s_sc_hand_num             = 1;
    s_sc_wave_coins           = 0;
    s_sc_move_timer           = 0.0f;
    s_sc_synth_banner_timer   = 0.0f;
    s_sc_refill_banner_timer  = 0.0f;
    s_sc_drop_popup_timer     = 0.0f;

    g->ruleset                = RULESET_KK_ADD_10;
    g->current_round          = wave_idx;
    g->total_rounds           = 99;
    g->dealer                 = 0;
    g->current_player         = 0;
    g->phase                  = PHASE_TURN_HAND;
    g->captured_count[0]      = 0;
    g->captured_count[1]      = 0;
    g->hand_count[1]          = 0;
    g->koi_called[0]          = 0;
    g->koi_called[1]          = 0;
    g->has_pending            = 0;
    g->pending_card           = -1;
    g->last_captured_count    = 0;

    int deck[48];
    for (int i = 0; i < 48; i++) deck[i] = i;
    for (int i = 47; i > 0; i--) {
        int j = (int)(sc_next_u32() % (uint32_t)(i + 1));
        int tmp = deck[i];
        deck[i] = deck[j];
        deck[j] = tmp;
    }

    int d_idx = 0;
    s_sc_row_count[0] = 7;
    for (int i = 0; i < 7; i++) s_sc_row_cards[0][i] = deck[d_idx++];
    s_sc_row_count[1] = 7;
    for (int i = 0; i < 7; i++) s_sc_row_cards[1][i] = deck[d_idx++];

    g->hand_count[0] = 8;
    for (int i = 0; i < 8; i++) g->hand[0][i] = deck[d_idx++];
    sc_sort_cards_by_month(g->hand[0], g->hand_count[0]);

    g->stock_count = 0;
    while (d_idx < 48) {
        g->stock[g->stock_count++] = deck[d_idx++];
    }

    sc_sync_game_field(g);

    /* Position cards at the stock pile and fan them out to the 2x7 table and 8-card hand */
    Vector2 stock_pos = { SC_STOCK_RECT.x, SC_STOCK_RECT.y };
    for (int cid = 0; cid < 48; cid++) {
        motion_set_instant(cid, stock_pos, (Vector2){ FIELD_CARD_W, FIELD_CARD_H });
    }
    for (int r = 0; r < 2; r++) {
        for (int c = 0; c < s_sc_row_count[r]; c++) {
            int cid = s_sc_row_cards[r][c];
            Rectangle tr = sc_get_row_card_rect(r, c, s_sc_row_count[r]);
            motion_set_target(cid, (Vector2){ tr.x, tr.y }, (Vector2){ tr.width, tr.height }, 0.36f);
        }
    }
    for (int i = 0; i < g->hand_count[0]; i++) {
        int cid = g->hand[0][i];
        Rectangle hr = board_get_hand_card_rect(i, g->hand_count[0], 0);
        motion_set_target(cid, (Vector2){ hr.x, hr.y }, (Vector2){ hr.width, hr.height }, 0.38f);
    }
}

int plexus_speedcoin_is_active(void) {
    return s_sc_active;
}

void plexus_speedcoin_abort(void) {
    s_sc_active  = 0;
    s_sc_card_id = -1;
    s_sc_step    = SC_STEP_NONE;
}

int plexus_speedcoin_combos_hit(Vector2 vm) {
    if (!s_sc_active || s_sc_step != SC_STEP_PLAY) return -1;
    if (CheckCollisionPointRec(vm, SC_COMBOS_HIT_RECT)) return 0;
    return -1;
}

void plexus_speedcoin_setup_match(GameState *g, uint32_t seed) {
    if (!g || !s_sc_active) return;
    s_sc_rng                  = seed ^ 0xA5A55A5Au;
    s_sc_clock                = 180.0f;
    s_sc_move_timer           = 0.0f;
    s_sc_wave_num             = 1;
    s_sc_waves_completed      = 0;
    s_sc_hand_num             = 1;
    s_sc_hands_synthesized    = 0;
    s_sc_wave_coins           = 0;
    s_sc_total_coins          = 0;
    s_sc_rewarded_tens        = 0;
    s_sc_bonus_seconds_total  = 0;
    s_sc_damage_seconds_total = 0;
    s_sc_banked_combo_pts     = 0;
    s_sc_last_wave_pts        = 0;
    s_sc_last_wave_yakus      = 0;
    s_sc_last_wave_had_h2     = 0;
    s_sc_damage_flash         = 0.0f;
    s_sc_bonus_flash          = 0.0f;
    s_sc_reverb_hold          = 0.0f;
    s_sc_reverb_energy        = 0.0f;
    s_sc_reverb_mid           = 0.0f;
    s_sc_reverb_curve         = 0.0f;
    s_sc_step                 = SC_STEP_INTRO;

    g->total_score[0] = 0;
    g->total_score[1] = 0;
    speed_bar_reset();
    sc_start_wave(g, 1);
}

static float sc_drop_cover(Vector2 c, Rectangle tr) {
    if (tr.width <= 0.0f || tr.height <= 0.0f) return 0.0f;
    Rectangle dr = { c.x - HAND_CARD_W * 0.5f, c.y - HAND_CARD_H * 0.5f,
                     (float)HAND_CARD_W, (float)HAND_CARD_H };
    float x0 = fmaxf(dr.x, tr.x), y0 = fmaxf(dr.y, tr.y);
    float x1 = fminf(dr.x + dr.width, tr.x + tr.width);
    float y1 = fminf(dr.y + dr.height, tr.y + tr.height);
    float inter = (x1 > x0 && y1 > y0) ? (x1 - x0) * (y1 - y0) : 0.0f;
    float min_area = fminf((float)(HAND_CARD_W * HAND_CARD_H), tr.width * tr.height);
    return (min_area > 0.0f) ? (inter / min_area) : 0.0f;
}

static void sc_remove_row_card(int r, int idx) {
    if (r < 0 || r >= 2 || idx < 0 || idx >= s_sc_row_count[r]) return;
    for (int i = idx; i + 1 < s_sc_row_count[r]; i++) {
        s_sc_row_cards[r][i] = s_sc_row_cards[r][i + 1];
    }
    s_sc_row_count[r]--;
}

static void sc_remove_hand_card(GameState *g, int idx) {
    if (!g || idx < 0 || idx >= g->hand_count[0]) return;
    for (int i = idx; i + 1 < g->hand_count[0]; i++) {
        g->hand[0][i] = g->hand[0][i + 1];
    }
    g->hand_count[0]--;
}

static void sc_finish_current_wave(GameState *g) {
    ScoreResult sr = score_captures(RULESET_KK_ADD_10, g->captured[0], g->captured_count[0]);
    int wave_pts = (sr.base_points > 0) ? calculate_award(sr.base_points, 0, 0) : 0;
    s_sc_last_wave_pts    = wave_pts;
    s_sc_last_wave_yakus  = sr.yaku_count;
    s_sc_last_wave_had_h2 = (s_sc_hand_num >= 2) ? 1 : 0;
    s_sc_banked_combo_pts += wave_pts;
    s_sc_waves_completed++;
    g->total_score[0]     = s_sc_banked_combo_pts;
    s_sc_step             = SC_STEP_WAVE_END;
    s_sc_move_timer       = 0.0f;
    speed_bar_consume();
}

static void sc_refill_row_from_stock(GameState *g, int r, int *out_evts) {
    if (!g || r < 0 || r >= 2 || g->stock_count <= 0) return;
    int take = (g->stock_count >= 7) ? 7 : g->stock_count;
    Vector2 stock_pos = { SC_STOCK_RECT.x, SC_STOCK_RECT.y };
    s_sc_row_count[r] = 0;
    for (int i = 0; i < take; i++) {
        int cid = g->stock[--g->stock_count];
        s_sc_row_cards[r][s_sc_row_count[r]++] = cid;
        motion_set_instant(cid, stock_pos, (Vector2){ FIELD_CARD_W, FIELD_CARD_H });
    }
    sc_sort_cards_by_month(s_sc_row_cards[r], s_sc_row_count[r]);
    for (int c = 0; c < s_sc_row_count[r]; c++) {
        int cid = s_sc_row_cards[r][c];
        Rectangle tr = sc_get_row_card_rect(r, c, s_sc_row_count[r]);
        motion_set_target(cid, (Vector2){ tr.x, tr.y }, (Vector2){ tr.width, tr.height }, 0.34f);
    }
    s_sc_refill_banner_timer = 1.35f;
    if (out_evts) *out_evts |= PLEXUS_SC_EVT_FLIP;
}

static void sc_post_move_check(GameState *g, int *out_evts) {
    if (!g) return;

    /* 1. Check if the player's hand just ran out */
    if (g->hand_count[0] <= 0) {
        if (s_sc_hand_num == 1 && s_sc_wave_coins >= 50 && g->stock_count > 0) {
            /* Synthesize 2nd Hand (capped at 2 hands per wave) */
            int take = (g->stock_count >= 8) ? 8 : g->stock_count;
            Vector2 stock_pos = { SC_STOCK_RECT.x, SC_STOCK_RECT.y };
            g->hand_count[0] = 0;
            for (int i = 0; i < take; i++) {
                int cid = g->stock[--g->stock_count];
                g->hand[0][g->hand_count[0]++] = cid;
                motion_set_instant(cid, stock_pos, (Vector2){ FIELD_CARD_W, FIELD_CARD_H });
            }
            sc_sort_cards_by_month(g->hand[0], g->hand_count[0]);
            for (int i = 0; i < g->hand_count[0]; i++) {
                int cid = g->hand[0][i];
                Rectangle hr = board_get_hand_card_rect(i, g->hand_count[0], 0);
                motion_set_target(cid, (Vector2){ hr.x, hr.y }, (Vector2){ hr.width, hr.height }, 0.36f);
            }
            s_sc_hand_num = 2;
            s_sc_hands_synthesized++;
            s_sc_synth_banner_timer = 1.85f;
            if (out_evts) *out_evts |= (PLEXUS_SC_EVT_SYNTH | PLEXUS_SC_EVT_FLIP);
        } else {
            /* Hand ran dry -> Wave Over (timer freezes, combos counted) */
            sc_sync_game_field(g);
            sc_finish_current_wave(g);
            return;
        }
    }

    /* 2. Auto-refill any emptied 7-card row on the table if stock has cards */
    for (int r = 0; r < 2; r++) {
        if (s_sc_row_count[r] == 0 && g->stock_count > 0) {
            sc_refill_row_from_stock(g, r, out_evts);
        }
    }

    sc_sync_game_field(g);

    /* 3. If both table rows and stock are completely empty, finish the wave */
    if (s_sc_row_count[0] == 0 && s_sc_row_count[1] == 0 && g->stock_count == 0) {
        sc_finish_current_wave(g);
    }
}

static void sc_resolve_drop(GameState *g, DragState *drag, AnimState *anim, Vector2 c, int *out_evts) {
    int hand_idx = drag->idx;
    if (hand_idx < 0 || hand_idx >= g->hand_count[0]) return;
    int card_id = g->hand[0][hand_idx];

    /* Outside field drop zone: glide card back to hand */
    if (!CheckCollisionPointRec(c, SC_FIELD_DROP_RECT)) {
        Rectangle slot = board_get_hand_card_rect(hand_idx, g->hand_count[0], 0);
        anim_start_glide(anim, card_id, drag->pos, (Vector2){ slot.x, slot.y }, 0.36f);
        return;
    }

    /* Find all matching month cards across both table rows */
    int match_r[4], match_c[4], match_cid[4];
    Rectangle match_rect[4];
    int mc = 0;
    int m = CARDS[card_id].month;
    for (int r = 0; r < 2; r++) {
        for (int col = 0; col < s_sc_row_count[r]; col++) {
            int fcid = s_sc_row_cards[r][col];
            if (CARDS[fcid].month == m && mc < 4) {
                match_r[mc]    = r;
                match_c[mc]    = col;
                match_cid[mc]  = fcid;
                match_rect[mc] = sc_get_row_card_rect(r, col, s_sc_row_count[r]);
                mc++;
            }
        }
    }

    sc_remove_hand_card(g, hand_idx);
    Vector2 cap_dest = { 96.0f, 380.0f };

    if (mc > 0) {
        Vector2 cap_starts[4];
        cap_starts[0] = drag->pos;
        g->last_captured[0] = card_id;
        g->last_captured_count = 1;
        if (g->captured_count[0] < 48) g->captured[0][g->captured_count[0]++] = card_id;

        float best_cover = 0.0f;
        if (mc >= 3) {
            /* Capture all 3 matching cards on the table */
            for (int k = 0; k < mc; k++) {
                float cov = sc_drop_cover(c, match_rect[k]);
                if (cov > best_cover) best_cover = cov;
                cap_starts[g->last_captured_count] = (Vector2){ match_rect[k].x, match_rect[k].y };
                g->last_captured[g->last_captured_count++] = match_cid[k];
                if (g->captured_count[0] < 48) g->captured[0][g->captured_count[0]++] = match_cid[k];
            }
            /* Remove in reverse index order so indices stay valid */
            for (int r = 1; r >= 0; r--) {
                for (int col = s_sc_row_count[r] - 1; col >= 0; col--) {
                    if (CARDS[s_sc_row_cards[r][col]].month == m) {
                        sc_remove_row_card(r, col);
                    }
                }
            }
        } else {
            /* Pick the matching card with highest overlap (or closest center to drop point) */
            int best_k = 0;
            float best_dist2 = 1e18f;
            for (int k = 0; k < mc; k++) {
                float cov = sc_drop_cover(c, match_rect[k]);
                float tx = match_rect[k].x + match_rect[k].width * 0.5f;
                float ty = match_rect[k].y + match_rect[k].height * 0.5f;
                float d2 = (c.x - tx) * (c.x - tx) + (c.y - ty) * (c.y - ty);
                if (cov > best_cover + 0.001f || (fabsf(cov - best_cover) <= 0.001f && d2 < best_dist2)) {
                    best_cover = cov;
                    best_dist2 = d2;
                    best_k     = k;
                }
            }
            int t_cid = match_cid[best_k];
            cap_starts[1] = (Vector2){ match_rect[best_k].x, match_rect[best_k].y };
            g->last_captured[1] = t_cid;
            g->last_captured_count = 2;
            if (g->captured_count[0] < 48) g->captured[0][g->captured_count[0]++] = t_cid;
            sc_remove_row_card(match_r[best_k], match_c[best_k]);
        }

        anim_start_capture(anim, 0, g->last_captured, g->last_captured_count, cap_starts, cap_dest, 0.36f);
        if (out_evts) *out_evts |= PLEXUS_SC_EVT_CAPTURE;

        /* Drive reverb synergy on capture */
        s_sc_reverb_energy += (float)g->last_captured_count * 0.22f;
        if (s_sc_reverb_energy > 1.0f) s_sc_reverb_energy = 1.0f;
        s_sc_reverb_hold = 0.60f;

        /* Grade Speedbar (Speed x Overlap Accuracy) */
        int before_mp = speed_bar_total();
        speed_bar_grade(best_cover);
        int gained_mp = speed_bar_total() - before_mp;
        if (gained_mp < 0) gained_mp = 0;
        s_sc_wave_coins += gained_mp;
        s_sc_total_coins = speed_bar_total();
        s_sc_move_timer  = 0.0f;

        s_sc_drop_popup_timer = 0.90f;
        s_sc_drop_popup_mp    = gained_mp;
        s_sc_drop_popup_pos   = c;

        /* Every 10 Speedcoins adds +10s to the 3-minute timer */
        int cur_tens = s_sc_total_coins / 10;
        if (cur_tens > s_sc_rewarded_tens) {
            int add_sec = (cur_tens - s_sc_rewarded_tens) * 10;
            s_sc_rewarded_tens        = cur_tens;
            s_sc_clock               += (float)add_sec;
            s_sc_bonus_seconds_total += add_sec;
            s_sc_last_bonus_sec       = add_sec;
            s_sc_bonus_flash          = 1.65f;
        }
    } else {
        /* No matching month on the table: place card onto the chosen row */
        g->last_captured_count = 0;
        int target_r = (c.y < 236.0f) ? 0 : 1;
        if (s_sc_row_count[target_r] >= 12) target_r = 1 - target_r;
        if (s_sc_row_count[target_r] < 16) {
            s_sc_row_cards[target_r][s_sc_row_count[target_r]++] = card_id;
            sc_sort_cards_by_month(s_sc_row_cards[target_r], s_sc_row_count[target_r]);
        }
        motion_set_instant(card_id, drag->pos, (Vector2){ FIELD_CARD_W, FIELD_CARD_H });
        speed_bar_consume();
        s_sc_move_timer = 0.0f;
        if (out_evts) *out_evts |= PLEXUS_SC_EVT_DROP;
    }

    sc_post_move_check(g, out_evts);
}

int plexus_speedcoin_update(GameState *g, void *drag_ptr, AnimState *anim,
                            Vector2 vm, int *hovered_idx, float dt) {
    if (!s_sc_active || !g || !anim) return PLEXUS_SC_EVT_NONE;
    DragState *drag = (DragState *)drag_ptr;
    int evts = PLEXUS_SC_EVT_NONE;

    if (s_sc_damage_flash > 0.0f) { s_sc_damage_flash -= dt; if (s_sc_damage_flash < 0.0f) s_sc_damage_flash = 0.0f; }
    if (s_sc_bonus_flash > 0.0f)  { s_sc_bonus_flash  -= dt; if (s_sc_bonus_flash  < 0.0f) s_sc_bonus_flash  = 0.0f; }
    if (s_sc_synth_banner_timer > 0.0f)  { s_sc_synth_banner_timer  -= dt; if (s_sc_synth_banner_timer  < 0.0f) s_sc_synth_banner_timer  = 0.0f; }
    if (s_sc_refill_banner_timer > 0.0f) { s_sc_refill_banner_timer -= dt; if (s_sc_refill_banner_timer < 0.0f) s_sc_refill_banner_timer = 0.0f; }
    if (s_sc_drop_popup_timer > 0.0f)    { s_sc_drop_popup_timer    -= dt; if (s_sc_drop_popup_timer    < 0.0f) s_sc_drop_popup_timer    = 0.0f; }

    anim_update(anim, dt);
    int dragged_cid = (drag && drag->on && drag->idx >= 0 && drag->idx < g->hand_count[0]) ? g->hand[0][drag->idx] : -1;
    motion_update(dt, dragged_cid);

    if (s_sc_step == SC_STEP_INTRO) {
        if (hovered_idx) *hovered_idx = -1;
        Rectangle btn = { 824.0f, 488.0f, 196.0f, 44.0f };
        if ((IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(vm, btn)) ||
            IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
            s_sc_step       = SC_STEP_PLAY;
            s_sc_move_timer = 0.0f;
            speed_bar_consume();
            evts |= PLEXUS_SC_EVT_CLICK;
        }
        return evts;
    }

    if (s_sc_step == SC_STEP_WAVE_END) {
        if (hovered_idx) *hovered_idx = -1;
        if (drag) { drag->on = 0; drag->selected = 0; }
        Rectangle btn = { 804.0f, 498.0f, 196.0f, 44.0f };
        if ((IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(vm, btn)) ||
            IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
            sc_start_wave(g, s_sc_wave_num + 1);
            s_sc_step = SC_STEP_PLAY;
            evts |= (PLEXUS_SC_EVT_CLICK | PLEXUS_SC_EVT_FLIP);
        }
        return evts;
    }

    if (s_sc_step == SC_STEP_TIME_UP) {
        if (hovered_idx) *hovered_idx = -1;
        if (drag) { drag->on = 0; drag->selected = 0; }
        Rectangle btn = { 792.0f, 518.0f, 224.0f, 44.0f };
        if ((IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(vm, btn)) ||
            IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
            if (s_sc_card_id >= 0 && s_sc_card_id < 48) {
                int cid = s_sc_card_id;
                s_finished_cards_mask |= (1ULL << cid);
                char title[80], l1[96], l2[96];
                snprintf(title, sizeof(title), "[%s] — SPEEDCOIN CONDITIONING COMPLETE", CARDS[cid].name);
                snprintf(l1, sizeof(l1), "Cleared %d wave(s) | Extracted %d Speedcoins | %d 2nd Hand(s).",
                         s_sc_waves_completed, s_sc_total_coins, s_sc_hands_synthesized);
                snprintf(l2, sizeof(l2), "Final Score: %d PTS (+%ds bonus / -%ds drain).",
                         g->total_score[0], s_sc_bonus_seconds_total, s_sc_damage_seconds_total);
                plexus_log_push("STORY // SPEEDCOIN CARD COMPLETED", title, l1, l2,
                                "Card turned face down and locked on the Month 01 spine.");
            } else {
                char l1[96], l2[96];
                snprintf(l1, sizeof(l1), "Cleared %d wave(s) | Extracted %d Speedcoins | %d 2nd Hand(s).",
                         s_sc_waves_completed, s_sc_total_coins, s_sc_hands_synthesized);
                snprintf(l2, sizeof(l2), "Final Score: %d PTS (+%ds bonus / -%ds drain).",
                         g->total_score[0], s_sc_bonus_seconds_total, s_sc_damage_seconds_total);
                plexus_log_push("SPEEDCOIN // RUN SUMMARY RECORDED",
                                "KK-ADD-10 SPEEDCOIN — 03:00 RUN COMPLETE",
                                l1, l2, "Telemetry updated inside Atrium Plex LOGFILE.");
            }
            s_sc_active  = 0;
            s_sc_card_id = -1;
            s_sc_step    = SC_STEP_NONE;
            evts |= (PLEXUS_SC_EVT_CLICK | PLEXUS_SC_EVT_EXIT_ATRIUM);
        }
        return evts;
    }

    /* SC_STEP_PLAY: 3-minute countdown is active */
    s_sc_clock -= dt;

    int input_unlocked = !anim_is_blocking(anim);
    speed_bar_update(g, dt, input_unlocked);

    if (input_unlocked) {
        s_sc_move_timer += dt;
        while (s_sc_move_timer >= 1.0f) {
            /* 1000ms bar reached its end without taking a card -> -1s damage to 3min clock */
            s_sc_move_timer -= 1.0f;
            s_sc_clock      -= 1.0f;
            s_sc_damage_seconds_total++;
            s_sc_damage_flash = 0.85f;
            /* Re-arm the bottom-right 1000ms bar for the next 1s drain cycle */
            speed_bar_consume();
            speed_bar_update(g, 0.0f, 1);
        }
    }

    if (s_sc_clock <= 0.0f) {
        s_sc_clock = 0.0f;
        if (drag) { drag->on = 0; drag->selected = 0; }
        /* Bank any completed combos from the active wave + Speedcoin match bonus (+total/10) */
        ScoreResult sr = score_captures(RULESET_KK_ADD_10, g->captured[0], g->captured_count[0]);
        int partial_pts = (sr.base_points > 0) ? calculate_award(sr.base_points, 0, 0) : 0;
        s_sc_banked_combo_pts += partial_pts;
        g->total_score[0] = s_sc_banked_combo_pts + (s_sc_total_coins / 10);
        s_sc_step = SC_STEP_TIME_UP;
        speed_bar_consume();
        return evts;
    }

    /* Handle mouse drag-and-drop and click-to-play on the 2x7 table */
    if (hovered_idx) *hovered_idx = -1;
    if (!drag || !input_unlocked) return evts;

    if (IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        drag->selected = 0;
        if (drag->on) {
            Rectangle slot = board_get_hand_card_rect(drag->idx, g->hand_count[0], 0);
            anim_start_glide(anim, g->hand[0][drag->idx], drag->pos, (Vector2){ slot.x, slot.y }, 0.36f);
            drag->on = 0;
        }
        return evts;
    }

    /* Click-to-play when a hand card was clicked & selected */
    if (!drag->on && drag->selected > 0 && drag->selected <= g->hand_count[0] &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        int cid = g->hand[0][drag->selected - 1];
        int matches = 0, hit_cid = -1;
        Rectangle hit_rect = { 0, 0, 0, 0 };
        for (int r = 0; r < 2; r++) {
            for (int col = 0; col < s_sc_row_count[r]; col++) {
                int fcid = s_sc_row_cards[r][col];
                Rectangle fr = sc_get_row_card_rect(r, col, s_sc_row_count[r]);
                if (CARDS[fcid].month == CARDS[cid].month) matches++;
                if (CheckCollisionPointRec(vm, fr)) { hit_cid = fcid; hit_rect = fr; }
            }
        }
        if ((hit_cid >= 0 && CARDS[hit_cid].month == CARDS[cid].month) ||
            (matches == 0 && hit_cid < 0 && CheckCollisionPointRec(vm, SC_FIELD_DROP_RECT))) {
            drag->idx = drag->selected - 1;
            Rectangle slot = board_get_hand_card_rect(drag->idx, g->hand_count[0], 0);
            drag->pos = (Vector2){ slot.x, slot.y };
            Vector2 target = (hit_cid >= 0)
                             ? (Vector2){ hit_rect.x + hit_rect.width * 0.5f, hit_rect.y + hit_rect.height * 0.5f }
                             : vm;
            drag->selected = 0;
            sc_resolve_drop(g, drag, anim, target, &evts);
            return evts;
        }
    }

    /* Hover hand card */
    if (!drag->on && hovered_idx) {
        for (int i = g->hand_count[0] - 1; i >= 0; i--) {
            Rectangle r = board_get_hand_card_rect(i, g->hand_count[0], 0);
            if (CheckCollisionPointRec(vm, r)) {
                *hovered_idx = i;
                break;
            }
        }
    }

    /* Start drag on hand card press */
    if (!drag->on && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        for (int i = g->hand_count[0] - 1; i >= 0; i--) {
            Rectangle r = board_get_hand_card_rect(i, g->hand_count[0], 0);
            if (CheckCollisionPointRec(vm, r)) {
                drag->on    = 1;
                drag->idx   = i;
                drag->press = vm;
                drag->moved = 0;
                drag->grab  = (Vector2){ vm.x - r.x, vm.y - r.y };
                drag->pos   = (Vector2){ r.x, r.y };
                break;
            }
        }
    }

    if (drag->on) {
        float dx = vm.x - drag->press.x, dy = vm.y - drag->press.y;
        if (dx * dx + dy * dy > 144.0f) { drag->moved = 1; drag->selected = 0; }
        if (drag->moved) drag->pos = (Vector2){ vm.x - drag->grab.x, vm.y - drag->grab.y };
    }

    if (drag->on && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        drag->on = 0;
        if (!drag->moved) {
            drag->selected = (drag->selected == drag->idx + 1) ? 0 : (drag->idx + 1);
            return evts;
        }
        Vector2 c = { drag->pos.x + HAND_CARD_W * 0.5f, drag->pos.y + HAND_CARD_H * 0.5f };
        sc_resolve_drop(g, drag, anim, c, &evts);
    }

    return evts;
}

static void sc_draw_player_capture_panel(const BoardAssets *ba, const GameState *g) {
    Rectangle outer = { 12.0f, 134.0f, 255.0f, 562.0f };
    DrawRectangleRec(outer, COLOR_PANEL_BG);
    ui_draw_panel_border(outer, COLOR_PHOSPHOR_DIM);

    draw_text_snapped(font_title, "PLAYER CAPTURES", 24.0f, 144.0f, 16.0f, 1.2f, COLOR_PHOSPHOR);
    char sub[64];
    snprintf(sub, sizeof(sub), "WAVE %02d  //  %d CARDS", s_sc_wave_num, g->captured_count[0]);
    draw_text_snapped(font_body, sub, 24.0f, 166.0f, 15.0f, 1.0f, COLOR_TEXT_INFO);
    DrawLine(22, 188, 255, 188, COLOR_PHOSPHOR_DIM);

    static const char * const k_lane_titles[4] = {
        "BRIGHT (B)", "ANIMAL (A)", "RIBBON (R)", "PLAIN (P)"
    };
    int lanes[4][48], counts[4] = { 0, 0, 0, 0 }, cup = 0;
    for (int i = 0; i < g->captured_count[0]; i++) {
        int cid = g->captured[0][i], t = (int)CARDS[cid].type;
        if (cid == 32) cup = 1;
        if ((unsigned)t < 4u) lanes[t][counts[t]++] = cid;
    }

    float card_w = 34.0f, card_h = 54.0f;
    for (int t = 0; t < 4; t++) {
        Rectangle lr = { 20.0f, 198.0f + (float)t * 122.0f, 239.0f, 112.0f };
        DrawRectangleRec(lr, PLEXUS_SUBBOX_BG);
        DrawRectangleLinesEx(lr, 1.0f, COLOR_PHOSPHOR_DIM);

        char hdr[48];
        snprintf(hdr, sizeof(hdr), (t == 1 && cup) ? "%s : %d*" : "%s : %d", k_lane_titles[t], counts[t]);
        draw_text_snapped(font_body, hdr, lr.x + 10.0f, lr.y + 8.0f, 15.0f, 1.0f, COLOR_PHOSPHOR);

        int count = counts[t];
        float max_w = lr.width - 20.0f;
        float step = (count > 1) ? fminf(28.0f, (max_w - card_w) / (float)(count - 1)) : 0.0f;
        float card_y = lr.y + 38.0f;
        for (int c = 0; c < count; c++) {
            int cid = lanes[t][c];
            motion_set_target(cid, (Vector2){ lr.x + 10.0f + (float)c * step, card_y },
                              (Vector2){ card_w, card_h }, 0.32f);
            Rectangle dr = motion_get_rect(cid);
            if (motion_is_card_moving(cid)) {
                board_draw_card_shadow(dr, motion_get_card_lift(cid));
                board_draw_card(ba, cid, dr);
                board_draw_card_glow(dr, 0.8f);
            } else {
                board_draw_card(ba, cid, dr);
                capture_info_record(cid, 0, c + 1, count, dr);
            }
        }
    }
    card_tooltip_clear();
}

static void sc_draw_right_player_panel(const GameState *g, Vector2 vm) {
    Rectangle outer = { 1148.0f, 12.0f, 120.0f, 536.0f };
    DrawRectangleRec(outer, COLOR_PANEL_BG);
    ui_draw_panel_border(outer, COLOR_PHOSPHOR);

    /* 1. Wave & Hand Status */
    draw_text_snapped(font_title, TextFormat("WAVE %02d", s_sc_wave_num), 1158.0f, 20.0f, 17.0f, 1.0f, COLOR_PHOSPHOR);
    Color hcol = (s_sc_hand_num >= 2) ? COLOR_AMBER : COLOR_TEXT_INFO;
    draw_text_snapped(font_body, TextFormat("HAND %d / 2", s_sc_hand_num), 1158.0f, 44.0f, 16.0f, 1.0f, hcol);
    draw_text_snapped(font_body, TextFormat("SCORE: %d", s_sc_banked_combo_pts), 1158.0f, 66.0f, 16.0f, 1.0f, COLOR_PHOSPHOR);
    draw_text_snapped(font_body, TextFormat("TOTAL SC: %d", s_sc_total_coins), 1158.0f, 88.0f, 15.0f, 1.0f, COLOR_AMBER);
    DrawLine(1154, 112, 1262, 112, COLOR_PHOSPHOR_DIM);

    /* 2. 2nd Hand Synthesis Meter (50 SC target) & +10s Bonus Counter */
    draw_text_snapped(font_title, "2ND HAND", 1158.0f, 120.0f, 14.0f, 1.0f, COLOR_PHOSPHOR);
    int w_sc = s_sc_wave_coins;
    float fill_frac = fminf(1.0f, (float)w_sc / 50.0f);
    if (s_sc_hand_num >= 2) {
        draw_text_snapped(font_body, "SYNTHESIZED", 1158.0f, 140.0f, 14.0f, 0.8f, COLOR_AMBER);
    } else if (w_sc >= 50) {
        draw_text_snapped(font_body, "READY! (50+)", 1158.0f, 140.0f, 14.0f, 0.8f, COLOR_AMBER);
    } else {
        draw_text_snapped(font_body, TextFormat("%d / 50 SC", w_sc), 1158.0f, 140.0f, 15.0f, 1.0f, COLOR_TEXT_INFO);
    }
    Rectangle bar_bg = { 1158.0f, 162.0f, 100.0f, 14.0f };
    DrawRectangleRec(bar_bg, (Color){ 10, 16, 18, 230 });
    DrawRectangle((int)bar_bg.x, (int)bar_bg.y, (int)(bar_bg.width * fill_frac), (int)bar_bg.height,
                  (w_sc >= 50 || s_sc_hand_num >= 2) ? COLOR_AMBER : COLOR_PHOSPHOR);
    DrawRectangleLinesEx(bar_bg, 1.0f, COLOR_PHOSPHOR_DIM);

    int to_next_ten = 10 - (s_sc_total_coins % 10);
    draw_text_snapped(font_body, TextFormat("+10s IN: %d SC", to_next_ten), 1158.0f, 184.0f, 14.0f, 0.8f, COLOR_AMBER);
    draw_text_snapped(font_body, TextFormat("BONUS: +%ds", s_sc_bonus_seconds_total), 1158.0f, 204.0f, 14.0f, 0.8f, COLOR_TEXT_INFO_DIM);
    DrawLine(1154, 228, 1262, 228, COLOR_PHOSPHOR_DIM);

    /* 3. Single Player Combos Summary (KK-ADD-10) */
    ScoreResult sr = score_captures(RULESET_KK_ADD_10, g->captured[0], g->captured_count[0]);
    int wave_award = (sr.base_points > 0) ? calculate_award(sr.base_points, 0, 0) : 0;
    if (sr.yaku_count > 0) {
        ui_theme_set_panel_flare_glow(SC_COMBOS_HIT_RECT, fminf(1.0f, 0.28f * (float)sr.yaku_count));
    }
    if (CheckCollisionPointRec(vm, SC_COMBOS_HIT_RECT)) {
        DrawRectangleRec(SC_COMBOS_HIT_RECT, (Color){ 255, 255, 255, 14 });
    }
    draw_text_snapped(font_title, "YOUR COMBOS", 1156.0f, 236.0f, 13.0f, 0.8f, COLOR_PHOSPHOR);
    draw_text_snapped(font_body, TextFormat("Wave: +%d", wave_award), 1156.0f, 256.0f, 16.0f, 1.0f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body, TextFormat("Base: %d pts", sr.base_points), 1156.0f, 276.0f, 14.0f, 0.8f, COLOR_TEXT_INFO_DIM);

    int show_n = (sr.yaku_count < 6) ? sr.yaku_count : 6;
    for (int i = 0; i < show_n; i++) {
        float y = 304.0f + (float)i * 34.0f;
        char clipped[20];
        snprintf(clipped, sizeof(clipped), "%.14s", sr.yakus[i].name);
        draw_text_snapped(font_body, clipped, 1156.0f, y, 13.0f, 0.5f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body, TextFormat("%d pts", sr.yakus[i].points), 1156.0f, y + 14.0f, 12.0f, 0.5f, COLOR_PHOSPHOR);
    }
    if (show_n == 0) {
        draw_text_snapped(font_body, "No combos yet", 1156.0f, 314.0f, 13.0f, 0.5f, COLOR_TEXT_INFO_DIM);
    }
    draw_text_snapped(font_body, (sr.yaku_count > 6) ? TextFormat("+%d / VIEW ALL", sr.yaku_count - 6) : "CLICK TO VIEW",
                      1156.0f, 524.0f, 12.0f, 0.5f, COLOR_PHOSPHOR);
}

static void sc_draw_top_left_timer(void) {
    Rectangle tr = { 12.0f, 12.0f, 255.0f, 112.0f };
    DrawRectangleRec(tr, COLOR_PANEL_BG);

    Color border_col = COLOR_RED;
    if (s_sc_bonus_flash > 0.0f) {
        border_col = COLOR_AMBER;
        Color glow = COLOR_AMBER;
        glow.a = (unsigned char)(65.0f * fminf(1.0f, s_sc_bonus_flash));
        DrawRectangleRec(tr, glow);
    } else if (s_sc_damage_flash > 0.0f) {
        Color glow = COLOR_RED;
        glow.a = (unsigned char)(85.0f * fminf(1.0f, s_sc_damage_flash));
        DrawRectangleRec(tr, glow);
    }
    ui_draw_panel_border(tr, border_col);

    const char *hdr = (s_sc_step == SC_STEP_WAVE_END) ? "TIMER FROZEN // WAVE END" : "SYNAPTIC COUNTDOWN";
    draw_text_snapped(font_title, hdr, 24.0f, 20.0f, 14.0f, 1.1f,
                      (s_sc_step == SC_STEP_WAVE_END) ? COLOR_AMBER : COLOR_RED);

    float t = fmaxf(0.0f, s_sc_clock);
    int mins = (int)(t / 60.0f);
    float secs = t - (float)(mins * 60);
    if (secs >= 59.95f) { secs = 59.9f; }
    char clock_buf[32];
    snprintf(clock_buf, sizeof(clock_buf), "%02d:%04.1f", mins, secs);

    Color clock_col = COLOR_RED;
    if (s_sc_bonus_flash > 0.0f && (((int)(s_sc_bonus_flash * 14.0f)) & 1)) {
        clock_col = COLOR_AMBER;
    } else if (s_sc_damage_flash > 0.0f && (((int)(s_sc_damage_flash * 18.0f)) & 1)) {
        clock_col = (Color){ 255, 225, 225, 255 };
    }
    draw_text_snapped(font_title, clock_buf, 24.0f, 40.0f, 44.0f, 1.5f, clock_col);

    if (s_sc_bonus_flash > 0.0f) {
        draw_text_snapped(font_title, TextFormat("+%ds SPEEDCOIN BONUS!", s_sc_last_bonus_sec),
                          24.0f, 94.0f, 15.0f, 1.1f, COLOR_AMBER);
    } else if (s_sc_damage_flash > 0.0f) {
        draw_text_snapped(font_title, "-1s SYNAPTIC DRAIN! (1000ms)",
                          24.0f, 94.0f, 14.0f, 1.0f, COLOR_RED);
    } else {
        draw_text_snapped(font_body, "-1s / 1000ms IDLE  |  +10s / 10 SC",
                          24.0f, 95.0f, 14.0f, 0.8f, COLOR_TEXT_INFO_DIM);
    }
}

static void sc_draw_overlay_panels(const GameState *g, Vector2 vm) {
    if (s_sc_step == SC_STEP_INTRO) {
        DrawRectangle(0, 0, CANVAS_W, CANVAS_H, (Color){ 0, 0, 0, 185 });
        Rectangle pr = { 230.0f, 140.0f, 820.0f, 412.0f };
        DrawRectangleRec(pr, PLEXUS_PANEL_BG);
        ui_draw_panel_border(pr, COLOR_PHOSPHOR);
        ui_theme_draw_panel_flares(pr);

        float x = pr.x + 28.0f, y = pr.y + 22.0f;
        draw_text_snapped(font_title, "ATRIUM PLEX // KK-ADD-10 SPEEDCOIN", x, y, 22.0f, 1.4f, COLOR_PHOSPHOR);
        DrawLine((int)x, (int)(y + 34.0f), (int)(pr.x + pr.width - 28.0f), (int)(y + 34.0f), COLOR_PHOSPHOR_DIM);

        draw_text_snapped(font_title,
                          "Warning: Synaptic countdown active. You have 1000ms per move",
                          x, y + 54.0f, 18.0f, 1.2f, COLOR_RED);
        draw_text_snapped(font_title,
                          "to extract Speedcoins through rapid, precise sequencing.",
                          x, y + 80.0f, 18.0f, 1.2f, COLOR_RED);
        draw_text_snapped(font_body,
                          "Slow execution degrades the yield; collect 50 units to synthesize 2nd hand.",
                          x, y + 118.0f, 19.0f, 1.2f, COLOR_TEXT_INFO);
        draw_text_snapped(font_title,
                          "Complete as many waves as possible",
                          x, y + 158.0f, 20.0f, 1.3f, COLOR_AMBER);

        DrawLine((int)x, (int)(y + 196.0f), (int)(pr.x + pr.width - 28.0f), (int)(y + 196.0f), COLOR_PHOSPHOR_DIM);
        draw_text_snapped(font_body,
                          "- Table deals 2 rows of 7 cards + 8 in hand. Empty rows auto-refill from deck.",
                          x, y + 212.0f, 17.0f, 1.1f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body,
                          "- Every 10 Speedcoins adds +10s to the 03:00 clock.",
                          x, y + 240.0f, 17.0f, 1.1f, COLOR_AMBER);
        draw_text_snapped(font_body,
                          "- Every 1000ms without taking a card drains -1s from the 03:00 clock.",
                          x, y + 268.0f, 17.0f, 1.1f, COLOR_RED);
        draw_text_snapped(font_body,
                          "- When your hand runs dry, the clock freezes, KK-ADD-10 combos are counted,",
                          x, y + 296.0f, 17.0f, 1.1f, COLOR_TEXT_INFO_DIM);
        draw_text_snapped(font_body,
                          "  and all 48 cards reshuffle for the next wave.",
                          x, y + 320.0f, 17.0f, 1.1f, COLOR_TEXT_INFO_DIM);

        Rectangle btn = { 824.0f, 488.0f, 196.0f, 44.0f };
        draw_plexus_btn(btn, "[BEGIN COUNTDOWN]", 15, vm, CheckCollisionPointRec(vm, pr));
    } else if (s_sc_step == SC_STEP_WAVE_END) {
        DrawRectangle(0, 0, CANVAS_W, CANVAS_H, (Color){ 0, 0, 0, 185 });
        Rectangle pr = { 250.0f, 130.0f, 780.0f, 432.0f };
        DrawRectangleRec(pr, PLEXUS_PANEL_BG);
        ui_draw_panel_border(pr, COLOR_PHOSPHOR);
        ui_theme_draw_panel_flares(pr);

        float x = pr.x + 28.0f, y = pr.y + 22.0f;
        draw_text_snapped(font_title,
                          TextFormat("WAVE %02d COMPLETE // TIMER FROZEN", s_sc_wave_num),
                          x, y, 22.0f, 1.4f, COLOR_PHOSPHOR);
        DrawLine((int)x, (int)(y + 34.0f), (int)(pr.x + pr.width - 28.0f), (int)(y + 34.0f), COLOR_PHOSPHOR_DIM);

        draw_text_snapped(font_title,
                          TextFormat("WAVE COMBO POINTS BANKED:  +%d PTS  (%d COMBOS)",
                                     s_sc_last_wave_pts, s_sc_last_wave_yakus),
                          x, y + 54.0f, 20.0f, 1.2f, COLOR_AMBER);
        draw_text_snapped(font_body,
                          TextFormat("Wave Speedcoins: %d SC   |   2nd Hand Synthesized: %s",
                                     s_sc_wave_coins, s_sc_last_wave_had_h2 ? "YES (2/2 HANDS)" : "NO (1/2 HANDS)"),
                          x, y + 90.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body,
                          TextFormat("Cards Captured in Wave: %d / 48   |   Unreached Deck Cards: %d",
                                     g->captured_count[0], g->stock_count),
                          x, y + 120.0f, 18.0f, 1.2f, COLOR_TEXT_INFO_DIM);

        ScoreResult sr = score_captures(RULESET_KK_ADD_10, g->captured[0], g->captured_count[0]);
        DrawLine((int)x, (int)(y + 154.0f), (int)(pr.x + pr.width - 28.0f), (int)(y + 154.0f), COLOR_PHOSPHOR_DIM);
        int max_y = (sr.yaku_count < 5) ? sr.yaku_count : 5;
        if (max_y == 0) {
            draw_text_snapped(font_body, "No KK-ADD-10 combos completed in this wave.",
                              x, y + 172.0f, 18.0f, 1.2f, COLOR_TEXT_INFO_DIM);
        } else {
            for (int i = 0; i < max_y; i++) {
                draw_text_snapped(font_body,
                                  TextFormat("- %s  (+%d pts)", sr.yakus[i].name, sr.yakus[i].points),
                                  x, y + 168.0f + (float)i * 28.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
            }
        }

        draw_text_snapped(font_title,
                          TextFormat("RUNNING TOTAL SCORE:  %d PTS", s_sc_banked_combo_pts),
                          x, y + 332.0f, 20.0f, 1.3f, COLOR_PHOSPHOR);
        draw_text_snapped(font_body,
                          "All 48 cards reshuffling for the next wave...",
                          x, y + 364.0f, 17.0f, 1.1f, COLOR_TEXT_INFO_DIM);

        Rectangle btn = { 804.0f, 498.0f, 196.0f, 44.0f };
        draw_plexus_btn(btn, "[NEXT WAVE]", 16, vm, CheckCollisionPointRec(vm, pr));
    } else if (s_sc_step == SC_STEP_TIME_UP) {
        DrawRectangle(0, 0, CANVAS_W, CANVAS_H, (Color){ 0, 0, 0, 205 });
        Rectangle pr = { 236.0f, 118.0f, 808.0f, 464.0f };
        DrawRectangleRec(pr, PLEXUS_PANEL_BG);
        ui_draw_panel_border(pr, COLOR_PHOSPHOR);
        ui_theme_draw_panel_flares(pr);

        float x = pr.x + 28.0f, y = pr.y + 22.0f;
        draw_text_snapped(font_title, "/// SYNAPTIC COUNTDOWN COMPLETE ///", x, y, 24.0f, 1.4f, COLOR_RED);
        DrawLine((int)x, (int)(y + 36.0f), (int)(pr.x + pr.width - 28.0f), (int)(y + 36.0f), COLOR_PHOSPHOR_DIM);

        int sc_bonus_pts = s_sc_total_coins / 10;
        draw_text_snapped(font_title,
                          TextFormat("FINAL SCORE:  %d PTS", g->total_score[0]),
                          x, y + 58.0f, 28.0f, 1.4f, COLOR_PHOSPHOR);
        draw_text_snapped(font_body,
                          TextFormat("KK-ADD-10 Wave Combos: %d PTS   +   Speedcoin Bonus (%d SC / 10): +%d PTS",
                                     s_sc_banked_combo_pts, s_sc_total_coins, sc_bonus_pts),
                          x, y + 102.0f, 18.0f, 1.2f, COLOR_AMBER);

        DrawLine((int)x, (int)(y + 140.0f), (int)(pr.x + pr.width - 28.0f), (int)(y + 140.0f), COLOR_PHOSPHOR_DIM);
        draw_text_snapped(font_body,
                          TextFormat("Full Waves Completed:          %d", s_sc_waves_completed),
                          x, y + 160.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body,
                          TextFormat("2nd Hands Synthesized (50 SC): %d", s_sc_hands_synthesized),
                          x, y + 194.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body,
                          TextFormat("Total Speedcoins Extracted:    %d SC", s_sc_total_coins),
                          x, y + 228.0f, 18.0f, 1.2f, COLOR_AMBER);
        draw_text_snapped(font_body,
                          TextFormat("Clock Time Added (+10s / 10):  +%d seconds", s_sc_bonus_seconds_total),
                          x, y + 262.0f, 18.0f, 1.2f, COLOR_PHOSPHOR);
        draw_text_snapped(font_body,
                          TextFormat("Synaptic Drain (-1s / 1000ms): -%d seconds", s_sc_damage_seconds_total),
                          x, y + 296.0f, 18.0f, 1.2f, COLOR_RED);
        draw_text_snapped(font_body,
                          "All speed & overlap accuracy metrics have been logged to ATRIUM PLEX.",
                          x, y + 344.0f, 17.0f, 1.1f, COLOR_TEXT_INFO_DIM);

        Rectangle btn = { 792.0f, 518.0f, 224.0f, 44.0f };
        draw_plexus_btn(btn, "[RETURN TO ATRIUM]", 15, vm, CheckCollisionPointRec(vm, pr));
    }
}

void plexus_speedcoin_draw(const BoardAssets *ba, const GameState *g, const AnimState *anim,
                           const void *drag_ptr, int hovered_hand_idx, Vector2 vm) {
    if (!ba || !g) return;
    const DragState *drag = (const DragState *)drag_ptr;
    int dragging_idx = (drag && drag->on) ? drag->idx : -1;
    float cur_time = (float)GetTime();
    float frame_dt = fminf(0.05f, GetFrameTime());

    /* Handcrafted reverb_synergy envelope (identical curve to board_draw_table) */
    if (s_sc_reverb_hold > 0.0f) {
        s_sc_reverb_hold -= frame_dt;
        if (s_sc_reverb_hold < 0.0f) s_sc_reverb_hold = 0.0f;
    } else {
        s_sc_reverb_energy -= 0.15f * frame_dt;
        if (s_sc_reverb_energy < 0.0f) s_sc_reverb_energy = 0.0f;
    }
    s_sc_reverb_mid   += (s_sc_reverb_energy - s_sc_reverb_mid) * (1.0f - expf(-frame_dt / 0.10f));
    s_sc_reverb_curve += (s_sc_reverb_mid - s_sc_reverb_curve) * (1.0f - expf(-frame_dt / 0.18f));
    if (s_sc_reverb_curve > 1.0f) s_sc_reverb_curve = 1.0f;
    float syn_x = s_sc_reverb_curve / 0.68f;
    if (syn_x < 0.0f) syn_x = 0.0f;
    if (syn_x > 1.0f) syn_x = 1.0f;
    float reverb_synergy = 0.30f * syn_x + 0.70f * (syn_x * syn_x * (3.0f - 2.0f * syn_x));
    audio_set_reverb(reverb_synergy);

    /* 1. Table Background with depth fog and normal-map moving light */
    if (ba->has_fog_shader) {
        int bg = ba->current_bg;
        float light_enabled = (bg >= 0 && bg < 8 && ba->bg_normals[bg].id > 0 &&
                               ba->light_normal_loc >= 0 && ba->light_enabled_loc >= 0) ? 1.0f : 0.0f;
        float flash_zero = 0.0f;
        SetShaderValue(ba->fog_shader, ba->fog_time_loc, &cur_time, SHADER_UNIFORM_FLOAT);
        SetShaderValue(ba->fog_shader, ba->light_enabled_loc, &light_enabled, SHADER_UNIFORM_FLOAT);
        if (ba->light_flash_loc >= 0)
            SetShaderValue(ba->fog_shader, ba->light_flash_loc, &flash_zero, SHADER_UNIFORM_FLOAT);
        BeginShaderMode(ba->fog_shader);
        if (light_enabled > 0.5f)
            SetShaderValueTexture(ba->fog_shader, ba->light_normal_loc, ba->bg_normals[bg]);
    }
    if (ba->current_bg >= 0 && ba->current_bg < 8 && ba->bgs[ba->current_bg].id > 0) {
        DrawTexturePro(ba->bgs[ba->current_bg], (Rectangle){ 0, 0, 1280, 720 },
                       (Rectangle){ 0, 0, 1280, 720 }, (Vector2){ 0, 0 }, 0.0f, WHITE);
    } else {
        ClearBackground(BLACK);
    }
    if (ba->has_fog_shader) EndShaderMode();
    DrawRectangle(0, 0, 1280, 720, COLOR_TABLE_DIM);
    DrawRectangleGradientV(0, 380, 1280, 340, (Color){ 5, 5, 5, 0 }, (Color){ 5, 5, 5, 65 });

    /* 2. Top-Left Big Red 3-Minute Timer + Single Left Capture Panel */
    sc_draw_top_left_timer();
    sc_draw_player_capture_panel(ba, g);

    /* 3. Stock Pile (Left of 2x7 field) */
    int st_off = g->stock_count >> 2;
    for (int i = 0; i <= st_off; i++) {
        board_draw_card_back(ba, (Rectangle){ SC_STOCK_RECT.x - (float)i, SC_STOCK_RECT.y - (float)i,
                                              SC_STOCK_RECT.width, SC_STOCK_RECT.height });
    }
    draw_text_snapped(font_title, TextFormat("%d", g->stock_count),
                      SC_STOCK_RECT.x + 24.0f, SC_STOCK_RECT.y + 132.0f, 18.0f, 1.2f, COLOR_PHOSPHOR);
    draw_text_snapped(font_body, "DECK",
                      SC_STOCK_RECT.x + 18.0f, SC_STOCK_RECT.y + 152.0f, 14.0f, 1.0f, COLOR_TEXT_INFO_DIM);

    /* 4. Center Table: 2 Rows x 7 Cards (no upper CPU row!) */
    for (int r = 0; r < 2; r++) {
        /* Faint 7-slot guide frames so row structure & auto-refill threshold are visible */
        for (int s = 0; s < 7; s++) {
            Rectangle sr = sc_get_row_card_rect(r, s, 7);
            DrawRectangleLinesEx(sr, 1.0f, (Color){ COLOR_PHOSPHOR_DIM.r, COLOR_PHOSPHOR_DIM.g, COLOR_PHOSPHOR_DIM.b, 42 });
        }
        for (int c = 0; c < s_sc_row_count[r]; c++) {
            int cid = s_sc_row_cards[r][c];
            Rectangle tr = sc_get_row_card_rect(r, c, s_sc_row_count[r]);
            motion_set_target(cid, (Vector2){ tr.x, tr.y }, (Vector2){ tr.width, tr.height }, 0.26f);
            Rectangle dr = motion_get_rect(cid);
            if (motion_is_card_moving(cid)) {
                board_draw_card_shadow(dr, motion_get_card_lift(cid));
                board_draw_card(ba, cid, dr);
            } else {
                board_draw_card(ba, cid, dr);
            }
        }
    }

    /* Highlight matching cards when click-selected */
    if (drag && !drag->on && drag->selected > 0 && drag->selected <= g->hand_count[0]) {
        int sel_cid = g->hand[0][drag->selected - 1];
        Rectangle sel_r = motion_get_rect(sel_cid);
        board_draw_card_glow(sel_r, 1.0f);
        ui_theme_draw_panel_flares_keyed(sel_r, sel_cid + 1);
        for (int r = 0; r < 2; r++) {
            for (int c = 0; c < s_sc_row_count[r]; c++) {
                int fcid = s_sc_row_cards[r][c];
                if (CARDS[fcid].month == CARDS[sel_cid].month) {
                    Rectangle mr = motion_get_rect(fcid);
                    board_draw_card_glow(mr, 1.0f);
                    ui_theme_draw_panel_flares_keyed(mr, fcid + 1);
                }
            }
        }
    }

    /* 5. High-Visibility Peripheral 1000ms Synaptic Bar (above player hand) */
    Rectangle syn_bar = { 434.0f, 494.0f, 648.0f, 22.0f };
    float rem_frac = (s_sc_step == SC_STEP_PLAY) ? fmaxf(0.0f, 1.0f - s_sc_move_timer) : 1.0f;
    DrawRectangleRec(syn_bar, COLOR_PANEL_BG);
    Color syn_fill = (rem_frac > 0.5f) ? COLOR_PHOSPHOR : (rem_frac > 0.2f) ? COLOR_AMBER : COLOR_RED;
    if (s_sc_damage_flash > 0.45f) syn_fill = COLOR_RED;
    DrawRectangle((int)syn_bar.x, (int)syn_bar.y, (int)(syn_bar.width * rem_frac), (int)syn_bar.height,
                  (Color){ syn_fill.r, syn_fill.g, syn_fill.b, 175 });
    ui_draw_panel_border(syn_bar, (s_sc_damage_flash > 0.0f) ? COLOR_RED : COLOR_PHOSPHOR_DIM);
    int rem_ms = (int)roundf(rem_frac * 1000.0f);
    draw_text_snapped(font_title,
                      TextFormat("SYNAPTIC WINDOW: %04d ms   //   WAVE SC: %d / 50", rem_ms, s_sc_wave_coins),
                      syn_bar.x + 14.0f, syn_bar.y + 3.0f, 15.0f, 1.1f,
                      (s_sc_damage_flash > 0.0f) ? COLOR_RED : COLOR_TEXT_INFO);

    /* Mid-table banners for 2nd Hand Synthesis, Row Auto-Refill, and Drop Popup */
    if (s_sc_synth_banner_timer > 0.0f) {
        Rectangle br = { 464.0f, 434.0f, 588.0f, 44.0f };
        DrawRectangleRec(br, PLEXUS_PANEL_BG);
        ui_draw_panel_border(br, COLOR_AMBER);
        draw_text_snapped(font_title, "2ND HAND SYNTHESIZED! // +8 CARDS DEALT",
                          br.x + 28.0f, br.y + 12.0f, 20.0f, 1.3f, COLOR_AMBER);
    } else if (s_sc_refill_banner_timer > 0.0f) {
        Rectangle br = { 510.0f, 434.0f, 496.0f, 40.0f };
        DrawRectangleRec(br, PLEXUS_PANEL_BG);
        ui_draw_panel_border(br, COLOR_PHOSPHOR);
        draw_text_snapped(font_title, "ROW CLEARED // +7 CARDS AUTO-FILLED",
                          br.x + 26.0f, br.y + 11.0f, 18.0f, 1.2f, COLOR_PHOSPHOR);
    }
    if (s_sc_drop_popup_timer > 0.0f) {
        float u = 1.0f - (s_sc_drop_popup_timer / 0.90f);
        Vector2 p = { s_sc_drop_popup_pos.x - 36.0f, s_sc_drop_popup_pos.y - 24.0f - u * 28.0f };
        Color pc = (s_sc_drop_popup_mp >= 8) ? COLOR_AMBER : (s_sc_drop_popup_mp > 0) ? COLOR_PHOSPHOR : COLOR_RED;
        draw_text_snapped(font_title, TextFormat("+%d SC", s_sc_drop_popup_mp), p.x, p.y, 22.0f, 1.2f, pc);
    }

    /* 6. Player Hand (8 cards at bottom) */
    for (int i = 0; i < g->hand_count[0]; i++) {
        if (i == dragging_idx) continue;
        int cid = g->hand[0][i];
        if (anim && anim->active && anim->type == ANIM_GLIDE_RETURN && anim->card_id == cid) continue;
        Rectangle r = board_get_hand_card_rect(i, g->hand_count[0], i == hovered_hand_idx);
        motion_set_target(cid, (Vector2){ r.x, r.y }, (Vector2){ r.width, r.height },
                          (i == hovered_hand_idx) ? 0.15f : 0.28f);
        Rectangle dr = motion_get_rect(cid);
        if (motion_is_card_moving(cid)) {
            board_draw_card_shadow(dr, motion_get_card_lift(cid));
            board_draw_card(ba, cid, dr);
        } else {
            board_draw_card(ba, cid, dr);
        }
    }

    /* 7. Dragged Card & Return Glide */
    if (dragging_idx >= 0 && dragging_idx < g->hand_count[0] && drag) {
        int cid = g->hand[0][dragging_idx];
        motion_set_drag_velocity(cid, GetMouseDelta());
        motion_set_instant(cid, drag->pos, (Vector2){ HAND_CARD_W, HAND_CARD_H });
        Rectangle dr = { drag->pos.x, drag->pos.y, HAND_CARD_W, HAND_CARD_H };
        board_draw_card_shadow(dr, 1.0f);
        board_draw_card(ba, cid, dr);
        board_draw_card_shimmer(dr, motion_get_card_shimmer(cid));
        board_draw_card_glow(dr, 1.0f);
        ui_theme_draw_panel_flares_keyed(dr, cid + 1);
    }
    if (anim && anim->active && anim->type == ANIM_GLIDE_RETURN) {
        Rectangle ar = { anim->current_pos.x, anim->current_pos.y, HAND_CARD_W, HAND_CARD_H };
        float lift = sinf((anim->elapsed / anim->duration) * 3.14159265f);
        board_draw_card_shadow(ar, lift);
        board_draw_card(ba, anim->card_id, ar);
    }

    /* 8. Single Right Player Panel + PAUSE / RESTART + 1000ms Speedbar */
    sc_draw_right_player_panel(g, vm);

    Rectangle pause_btn = { 1160.0f, 560.0f, 112.0f, 40.0f };
    Rectangle restart_btn = { 1160.0f, 610.0f, 112.0f, 40.0f };
    DrawRectangleRec(pause_btn, COLOR_PANEL_BG);
    ui_draw_panel_border(pause_btn, COLOR_PHOSPHOR);
    Vector2 pts = MeasureTextEx(font_title, "PAUSE", 16.0f, 1.0f);
    draw_text_snapped(font_title, "PAUSE",
                      pause_btn.x + (pause_btn.width - pts.x) * 0.5f,
                      pause_btn.y + (pause_btn.height - 16.0f) * 0.5f, 16.0f, 1.0f, COLOR_PHOSPHOR);

    DrawRectangleRec(restart_btn, COLOR_PANEL_BG);
    ui_draw_panel_border(restart_btn, COLOR_PHOSPHOR);
    Vector2 rts = MeasureTextEx(font_title, "RESTART", 16.0f, 1.0f);
    draw_text_snapped(font_title, "RESTART",
                      restart_btn.x + (restart_btn.width - rts.x) * 0.5f,
                      restart_btn.y + (restart_btn.height - 16.0f) * 0.5f, 16.0f, 1.0f, COLOR_PHOSPHOR);

    speed_bar_draw();

    /* 9. Foreground Intro / Wave Complete / Time-Up Overlays */
    sc_draw_overlay_panels(g, vm);
    (void)sc_find_table_card_rect;
}

