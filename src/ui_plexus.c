#include "ui_plexus.h"
#include "cards.h"
#include "ui_motion.h"
#include "ui_speed.h"
#include "ui_theme.h"
#include <stdio.h>
#include <stdlib.h>
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
    int  read;
    int  viewed_open;
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
static int s_tut_telemetry_saved = 0;
static int s_saved_drops = 0;
static float s_saved_time_sum = 0.0f;
static float s_saved_cover_sum = 0.0f;
static int s_saved_best_streak = 0;
static int s_saved_lifetime_mp = 0;
static int s_saved_audio_triggers = 0;
static float s_saved_audio_peak = 0.0f;
static float s_saved_audio_hold = 0.0f;
static int s_training_active = 0;

/* Solo Speedcoin mode & persistent Top-8 Ranklist (saved in plexsave.txt) */
#define SPEEDCOIN_UNLOCKED_FOR_TESTING 1
#define SPEEDCOIN_RANK_MAX 8

typedef struct {
    int score;
    int waves;
    int coins;
} SpeedcoinRankEntry;

static int s_speedcoin_active = 0;
static int s_speedcoin_result_view = 0;
static SpeedcoinRankEntry s_sc_ranks[SPEEDCOIN_RANK_MAX];
static int s_sc_rank_count = 0;
static int s_sc_last_rank_idx = -1;
static int s_sc_last_score = 0;
static int s_sc_last_waves = 1;
static int s_sc_last_coins = 0;
static int s_sc_last_bonus = 0;
static int s_sc_last_streak = 0;

static void tut_save_telemetry(void) {
    if (s_tut_telemetry_saved) return;
    speed_bar_get_telemetry(&s_saved_drops, &s_saved_time_sum, &s_saved_cover_sum,
                            &s_saved_best_streak, &s_saved_lifetime_mp);
    s_saved_audio_triggers = board_get_audio_action_triggers();
    s_saved_audio_peak     = board_get_audio_peak_synergy();
    s_saved_audio_hold     = board_get_audio_full_hold_seconds();
    s_tut_telemetry_saved  = 1;
}

static void tut_restore_telemetry(int clear_saved) {
    if (!s_tut_telemetry_saved) return;
    speed_bar_set_telemetry(s_saved_drops, s_saved_time_sum, s_saved_cover_sum,
                            s_saved_best_streak, s_saved_lifetime_mp);
    board_set_audio_telemetry(s_saved_audio_triggers, s_saved_audio_peak, s_saved_audio_hold);
    if (clear_saved) s_tut_telemetry_saved = 0;
}

static PlexusLogEntry s_log_entries[PLEXUS_LOG_MAX];
static int s_log_count  = 0;
static int s_log_cursor = 0; /* 0 = newest (last given to player), increases toward older entries */
static int s_log_seeded = 0;
static unsigned int s_last_briefing_hash = 0u;
static int s_briefing_hash_ready = 0;
static float s_briefing_glow = 0.0f;

static void commit_viewed_log_entries(int except_idx) {
    for (int i = 0; i < s_log_count; i++) {
        if (i != except_idx && s_log_entries[i].viewed_open) {
            s_log_entries[i].read = 1;
            s_log_entries[i].viewed_open = 0;
        }
    }
}

static unsigned int hash_briefing_line(unsigned int h, const char *s) {
    if (!s) return h;
    while (*s) {
        h ^= (unsigned int)(unsigned char)(*s++);
        h *= 16777619u;
    }
    h ^= 255u;
    h *= 16777619u;
    return h;
}

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

static void copy_clean(char *dst, size_t cap, const char *src) {
    size_t i = 0;
    if (!cap) return;
    if (src) {
        for (; src[i] != '\0' && i + 1 < cap; i++) {
            char c = src[i];
            dst[i] = (c == '\t' || c == '\r' || c == '\n') ? ' ' : c;
        }
    }
    dst[i] = '\0';
}

static void plexus_log_push(const char *tag, const char *title,
                            const char *l1, const char *l2, const char *l3) {
    commit_viewed_log_entries(-1);
    if (s_log_count < PLEXUS_LOG_MAX) {
        s_log_count++;
    }
    for (int i = s_log_count - 1; i > 0; i--) {
        s_log_entries[i] = s_log_entries[i - 1];
    }
    copy_clean(s_log_entries[0].tag,   sizeof(s_log_entries[0].tag),   tag);
    copy_clean(s_log_entries[0].title, sizeof(s_log_entries[0].title), title);
    copy_clean(s_log_entries[0].line1, sizeof(s_log_entries[0].line1), l1);
    copy_clean(s_log_entries[0].line2, sizeof(s_log_entries[0].line2), l2);
    copy_clean(s_log_entries[0].line3, sizeof(s_log_entries[0].line3), l3);
    s_log_entries[0].read = 0;
    s_log_entries[0].viewed_open = 0;
    s_log_cursor = 0;
}

static void ensure_log_seeded(void) {
    if (s_log_seeded) return;
    s_log_seeded = 1;
    plexus_log_push("STORY // SEQ 00 -- INTRODUCTION",
                    "ATRIUM PLEX // RESEARCH & CONDITIONING THRESHOLD",
                    "Biomechanical neural link established. Deep inside the vessel,",
                    "an anomalous entity [DEUS SUPREMUS] has been detected.",
                    "Conditioning across all 12 Months (48 cards) is required.");
    plexus_log_push("DIRECTIVE // MONTHLY PROGRESSION",
                    "MONTH 01 // JANUARY // PINE -- ACTIVE",
                    "Complete all 4 cards of the current month in any order to",
                    "unlock the next month and continue the non-linear story.",
                    "Finished cards illuminate inside the bottom-right Atlas map.");
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
    return SPEEDCOIN_UNLOCKED_FOR_TESTING || is_month_unlocked(4); /* Unlocks after all 4 cards of Month 1..4 are finished */
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
    if (FileExists(norm_path)) {
        s_plexus_normal = LoadTexture(norm_path);
        if (s_plexus_normal.id > 0) {
            SetTextureFilter(s_plexus_normal, TEXTURE_FILTER_BILINEAR);
            SetTextureWrap(s_plexus_normal, TEXTURE_WRAP_CLAMP);
        }
    }
}

void plexus_load(void) {
    s_finished_cards_mask = 0ULL;
    s_selected_month = 0;
    s_selected_slot = 3;
    s_active_tab = 0;
    s_tut_telemetry_saved = 0;
    s_training_active = 0;
    s_speedcoin_active = 0;
    s_speedcoin_result_view = 0;
    s_sc_rank_count = 0;
    s_sc_last_rank_idx = -1;
    speed_mode_stop();
    s_log_count = 0;
    s_log_cursor = 0;
    s_log_seeded = 0;
    s_briefing_hash_ready = 0;
    s_briefing_glow = 0.0f;

    int drops = 0, best_strk = 0, life_mp = 0, aud_trig = 0;
    unsigned long log_read_mask = 0UL;
    int has_read_mask = 0;
    float t_sum = 0.0f, c_sum = 0.0f, aud_peak = 0.0f, aud_hold = 0.0f;
    speed_bar_set_telemetry(0, 0.0f, 0.0f, 0, 0);
    board_set_audio_telemetry(0, 0.0f, 0.0f);

    FILE *f = fopen(resolve_asset_path("plexsave.txt"), "r");
    if (!f) { ensure_log_seeded(); return; }

    char line[600];
    while (fgets(line, sizeof(line), f)) {
        if (!strchr(line, '\n') && !feof(f)) {
            int ch;
            while ((ch = fgetc(f)) != '\n' && ch != EOF) {}
            continue;
        }
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, "log=", 4) == 0) {
            char *p[5] = { line + 4, "", "", "", "" };
            char *cur = line + 4;
            for (int fi = 1; fi < 5; fi++) {
                char *tab = strchr(cur, '\t');
                if (!tab) break;
                *tab = '\0';
                cur = tab + 1;
                p[fi] = cur;
            }
            plexus_log_push(p[0], p[1], p[2], p[3], p[4]);
            s_log_seeded = 1;
            continue;
        }
        if (strncmp(line, "sc_rank=", 8) == 0 && s_sc_rank_count < SPEEDCOIN_RANK_MAX) {
            int r_sc = 0, r_wv = 0, r_mp = 0;
            if (sscanf(line + 8, "%d,%d,%d", &r_sc, &r_wv, &r_mp) == 3 && r_sc >= 0 && r_wv >= 1 && r_mp >= 0) {
                s_sc_ranks[s_sc_rank_count].score = r_sc;
                s_sc_ranks[s_sc_rank_count].waves = r_wv;
                s_sc_ranks[s_sc_rank_count].coins = r_mp;
                s_sc_rank_count++;
            }
            continue;
        }
        char k[64], v[64];
        if (sscanf(line, "%63[^=]=%63s", k, v) == 2) {
            if (strcmp(k, "finished_cards_mask") == 0) {
                s_finished_cards_mask = strtoull(v, NULL, 10) & ((1ULL << 48) - 1ULL);
            } else if (strcmp(k, "selected_month") == 0) {
                int m = atoi(v);
                if (m >= 0 && m < 12) s_selected_month = m;
            } else if (strcmp(k, "selected_slot") == 0) {
                int sl = atoi(v);
                if (sl >= 0 && sl < 4) s_selected_slot = sl;
            } else if (strcmp(k, "log_read_mask") == 0) {
                log_read_mask = strtoul(v, NULL, 10);
                has_read_mask = 1;
            } else if (strcmp(k, "speed_drops") == 0) {
                int d = atoi(v);
                if (d >= 0) drops = d;
            } else if (strcmp(k, "speed_time_sum") == 0) {
                float x = strtof(v, NULL);
                if (x >= 0.0f && x < 1e30f) t_sum = x;
            } else if (strcmp(k, "speed_cover_sum") == 0) {
                float x = strtof(v, NULL);
                if (x >= 0.0f && x < 1e30f) c_sum = x;
            } else if (strcmp(k, "speed_best_streak") == 0) {
                int b = atoi(v);
                if (b >= 0) best_strk = b;
            } else if (strcmp(k, "speed_lifetime_mp") == 0) {
                int l = atoi(v);
                if (l >= 0) life_mp = l;
            } else if (strcmp(k, "audio_triggers") == 0) {
                int t = atoi(v);
                if (t >= 0) aud_trig = t;
            } else if (strcmp(k, "audio_peak") == 0) {
                float x = strtof(v, NULL);
                if (x >= 0.0f && x <= 1.0f) aud_peak = x;
            } else if (strcmp(k, "audio_hold") == 0) {
                float x = strtof(v, NULL);
                if (x >= 0.0f && x < 1e30f) aud_hold = x;
            }
        }
    }
    fclose(f);
    if (has_read_mask) {
        for (int i = 0; i < s_log_count && i < 32; i++) {
            s_log_entries[i].read = (int)((log_read_mask >> i) & 1UL);
            s_log_entries[i].viewed_open = 0;
        }
    }
    if (drops == 0) { t_sum = 0.0f; c_sum = 0.0f; }
    speed_bar_set_telemetry(drops, t_sum, c_sum, best_strk, life_mp);
    board_set_audio_telemetry(aud_trig, aud_peak, aud_hold);
    ensure_log_seeded();
}

void plexus_save(void) {
    ensure_log_seeded();
    commit_viewed_log_entries(-1);
    unsigned long log_read_mask = 0UL;
    for (int i = 0; i < s_log_count && i < 32; i++) {
        if (s_log_entries[i].read) log_read_mask |= (1UL << i);
    }
    char target[512], tmp[520];
    snprintf(target, sizeof(target), "%s", resolve_asset_path("plexsave.txt"));
    snprintf(tmp, sizeof(tmp), "%s.tmp", target);
    FILE *f = fopen(tmp, "w");
    if (!f) return;

    int drops = 0, best_strk = 0, life_mp = 0;
    float t_sum = 0.0f, c_sum = 0.0f;
    int aud_trig = board_get_audio_action_triggers();
    float aud_peak = board_get_audio_peak_synergy();
    float aud_hold = board_get_audio_full_hold_seconds();
    if (s_tut_telemetry_saved) {
        drops = s_saved_drops;
        t_sum = s_saved_time_sum;
        c_sum = s_saved_cover_sum;
        best_strk = s_saved_best_streak;
        life_mp = s_saved_lifetime_mp;
        aud_trig = s_saved_audio_triggers;
        aud_peak = s_saved_audio_peak;
        aud_hold = s_saved_audio_hold;
    } else {
        speed_bar_get_telemetry(&drops, &t_sum, &c_sum, &best_strk, &life_mp);
    }
    fprintf(f,
            "finished_cards_mask=%llu\n"
            "selected_month=%d\n"
            "selected_slot=%d\n"
            "log_read_mask=%lu\n"
            "speed_drops=%d\n"
            "speed_time_sum=%.4f\n"
            "speed_cover_sum=%.4f\n"
            "speed_best_streak=%d\n"
            "speed_lifetime_mp=%d\n"
            "audio_triggers=%d\n"
            "audio_peak=%.4f\n"
            "audio_hold=%.4f\n",
            s_finished_cards_mask & ((1ULL << 48) - 1ULL),
            s_selected_month, s_selected_slot,
            log_read_mask,
            drops, t_sum, c_sum, best_strk, life_mp,
            aud_trig, aud_peak, aud_hold);
    for (int i = 0; i < s_sc_rank_count && i < SPEEDCOIN_RANK_MAX; i++) {
        fprintf(f, "sc_rank=%d,%d,%d\n",
                s_sc_ranks[i].score, s_sc_ranks[i].waves, s_sc_ranks[i].coins);
    }
    for (int i = s_log_count - 1; i >= 0; i--) {
        fprintf(f, "log=%s\t%s\t%s\t%s\t%s\n",
                s_log_entries[i].tag, s_log_entries[i].title,
                s_log_entries[i].line1, s_log_entries[i].line2, s_log_entries[i].line3);
    }
    if (fclose(f) != 0) { remove(tmp); return; }
    remove(target);
    rename(tmp, target);
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

static void draw_plexus_locked_btn(Rectangle r, const char *txt, int fsz) {
    int rx = (int)r.x, ry = (int)r.y, rw = (int)r.width, rh = (int)r.height, hw = rw >> 1;
    DrawRectangleRec(r, (Color){ 26, 8, 10, 228 });
    if (hw > 0) {
        Color c_mid = { 175, 62, 66, 22 }, c_edge = { 175, 62, 66, 0 };
        DrawRectangleGradientH(rx, ry, hw, 1, c_edge, c_mid);
        DrawRectangleGradientH(rx + hw, ry, rw - hw, 1, c_mid, c_edge);
        DrawRectangleGradientH(rx, ry + rh - 1, hw, 1, c_edge, c_mid);
        DrawRectangleGradientH(rx + hw, ry + rh - 1, rw - hw, 1, c_mid, c_edge);
    }
    DrawRectangleLinesEx(r, 1.0f, (Color){ 148, 56, 60, 200 });
    Vector2 ts = MeasureTextEx(font_title, txt, (float)fsz, 1.2f);
    draw_text_snapped(font_title, txt,
                      r.x + (r.width - ts.x) * 0.5f,
                      r.y + (r.height - (float)fsz) * 0.5f,
                      (float)fsz, 1.2f, (Color){ 182, 82, 86, 225 });
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
    commit_viewed_log_entries(s_log_cursor);

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
    float idx_x = k_logfile_win_rect.x + k_logfile_win_rect.width - its.x - 22.0f;
    draw_text_snapped(font_body, idx_txt,
                      idx_x, k_logfile_win_rect.y + 20.0f,
                      18.0f, 1.2f, COLOR_TEXT_INFO);
    if (s_log_count > 0 && s_log_cursor >= 0 && s_log_cursor < s_log_count &&
        !s_log_entries[s_log_cursor].read) {
        s_log_entries[s_log_cursor].viewed_open = 1;
        Vector2 nts = MeasureTextEx(font_body, "[new]", 15.0f, 1.1f);
        draw_text_snapped(font_body, "[new]",
                          idx_x - nts.x - 10.0f,
                          k_logfile_win_rect.y + 22.0f,
                          15.0f, 1.1f, COLOR_PHOSPHOR);
    }
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
    if (!is_bottom_btn_unlocked(s_active_tab)) s_active_tab = 0;

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
            if (month_unlocked && done && card_id != 3) {
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
        Rectangle border_r = { cr.x - 1.0f, cr.y - 1.0f, cr.width + 2.0f, cr.height + 2.0f };
        DrawRectangleLinesEx(border_r, sel ? 2.0f : 1.0f, bcol);
        if (sel) {
            ui_theme_set_panel_flare_glow(border_r, 0.78f + 0.22f * pulse);
        }
        ui_theme_draw_panel_flares(border_r);
    }

    /* 3. Middle Screen: either Logfile Window (tab 1) or Task / Training Briefing + START button */
    if (s_active_tab == 1) {
        draw_logfile_window(ba, st, vm);
    } else {
        commit_viewed_log_entries(-1);

        char hdr[96];
        char sub[96];
        const char *line1 = "";
        const char *line2 = "";
        const char *line3 = "";
        int card_id = s_selected_month * 4 + s_selected_slot;
        int card_locked_done = (s_active_tab == 0) && is_card_locked_by_completion(card_id);
        char sc_best_line[96];
        if (s_active_tab == 0) {
            if (!month_unlocked) {
                snprintf(hdr, sizeof(hdr), "MONTH %02d  //  %s   [LOCKED]", s_selected_month + 1, k_month_names[s_selected_month]);
                snprintf(sub, sizeof(sub), "CARD %d OF 4 :  [%s]   [DIMMED]", s_selected_slot + 1, CARDS[card_id].name);
                line1 = "MONTH LOCKED -- PREVIOUS MONTH INCOMPLETE";
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
            snprintf(sub, sizeof(sub), "SOLO HIGHSCORE  //  180s CLOCK");
            line1 = "14 field (2x7) + 8 hand cards. No CPU / Koi-Koi.";
            line2 = "-10s drain/miss; +10s per 10 MP; 50 MP lights deck.";
            if (s_sc_rank_count > 0) {
                snprintf(sc_best_line, sizeof(sc_best_line),
                         "Best #01: %d PTS [W-%02d // %d MP] -- START.",
                         s_sc_ranks[0].score, s_sc_ranks[0].waves, s_sc_ranks[0].coins);
                line3 = sc_best_line;
            } else {
                line3 = "Press START to launch a 180s highscore run.";
            }
        } else if (s_active_tab == 3) {
            snprintf(hdr, sizeof(hdr), "ATRIUM PLEX  //  ACTION CARD");
            snprintf(sub, sizeof(sub), "SPECIAL GAME  //  KK-ADD-10 ACTION CARD AUDIO");
            line1 = "Special game focused on the KK-ADD-10 Action Card Audio Effect";
            line2 = "stereo reverb synergy and capture streak resonance.";
            line3 = "Select a card on the right spine to return to Month view.";
        } else {
            snprintf(hdr, sizeof(hdr), "ATRIUM PLEX  //  TRAINING");
            snprintf(sub, sizeof(sub), "TRAINING BRIEFING   [PLACEHOLDER]");
            line1 = "[Placeholder info -- Training text to be added.]";
            line2 = "Starts a normal match from Option menu settings.";
            line3 = "Not a campaign match -- no awards are earned.";
        }

        unsigned int bh = 2166136261u;
        bh = hash_briefing_line(bh, hdr);
        bh = hash_briefing_line(bh, sub);
        bh = hash_briefing_line(bh, line1);
        bh = hash_briefing_line(bh, line2);
        bh = hash_briefing_line(bh, line3);
        float dt = GetFrameTime();
        if (dt < 0.0f) dt = 0.0f;
        if (dt > 0.05f) dt = 0.05f;
        if (!s_briefing_hash_ready) {
            s_last_briefing_hash = bh;
            s_briefing_hash_ready = 1;
        } else if (bh != s_last_briefing_hash) {
            s_last_briefing_hash = bh;
            s_briefing_glow = 1.0f;
        } else if (s_briefing_glow > 0.0f) {
            s_briefing_glow -= dt * 1.15f;
            if (s_briefing_glow < 0.0f) s_briefing_glow = 0.0f;
        }
        float bglow = s_briefing_glow * s_briefing_glow * (3.0f - 2.0f * s_briefing_glow);

        DrawRectangleRec(k_briefing_rect, PLEXUS_PANEL_BG);
        if (bglow > 0.001f) {
            Color fill_glow = COLOR_PHOSPHOR;
            fill_glow.a = (unsigned char)(56.0f * bglow);
            DrawRectangleRec(k_briefing_rect, fill_glow);
            ui_theme_set_panel_flare_glow(k_briefing_rect, bglow);
        }
        ui_draw_panel_border(k_briefing_rect, COLOR_PHOSPHOR);
        if (bglow > 0.001f) {
            Color ring = COLOR_PHOSPHOR;
            ring.a = (unsigned char)(255.0f * bglow);
            DrawRectangleLinesEx(k_briefing_rect, 2.0f, ring);
            Color outer = COLOR_PHOSPHOR;
            outer.a = (unsigned char)(145.0f * bglow);
            DrawRectangleLinesEx((Rectangle){ k_briefing_rect.x - 2.0f, k_briefing_rect.y - 2.0f,
                                              k_briefing_rect.width + 4.0f, k_briefing_rect.height + 4.0f },
                                 1.5f, outer);
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
        if (!is_bottom_btn_unlocked(i)) {
            draw_plexus_locked_btn(k_bottom_rects[i], k_bottom_labels[i], 16);
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
        s_selected_month = (s_selected_month + 1) % 12;
        s_active_tab = 0;
        if (out_clicked) *out_clicked = 1;
        return ACT_NONE;
    }
    if (CheckCollisionPointRec(vm, k_arrow_down_rect)) {
        s_selected_month = (s_selected_month + 11) % 12;
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
    if (s_active_tab != 1 && CheckCollisionPointRec(vm, k_start_rect)) {
        /* Bottom tab 4 (TRAINING): launch normal match based on Option menu settings */
        if (s_active_tab == 4) {
            plexus_tutorial_abort();
            plexus_speedcoin_abort();
            s_training_active = 1;
            plexus_log_push("TRAINING // NORMAL MATCH",
                            "TRAINING MODE -- NO CAMPAIGN AWARDS",
                            "Starting a normal match configured by your current",
                            "Option menu settings (Ruleset, Difficulty, and Rounds).",
                            "Non-campaign game: no clearance coins or unlocks earned.");
            if (out_clicked) *out_clicked = 1;
            return ACT_START_GAME;
        }
        if (s_active_tab == 2) {
            if (!is_speedcoin_bottom_unlocked()) return ACT_NONE;
            plexus_tutorial_abort();
            s_training_active = 0;
            s_speedcoin_result_view = 0;
            s_speedcoin_active = 1;
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
            s_training_active = 0;
            plexus_speedcoin_abort();
            tut_save_telemetry();
            s_tut_active  = 1;
            s_tut_card_id = cid;
            s_tut_step    = TUT1_WAIT_DEAL;
            plexus_log_push("TUTORIAL // PROTOCOL 01 INITIATED",
                            "CARD 4 OF 4 : [01-P2 Plain] -- STANDARD RULES OF KOI-KOI",
                            "Informational upgrade started for Entity 001.",
                            "Follow the tutorial prompts to capture the Moon and Sake Cup,",
                            "form Moon Viewing (Tsukimi-zake), and complete Card 4.");
            if (out_clicked) *out_clicked = 1;
            return ACT_START_GAME;
        }
        s_finished_cards_mask |= (1ULL << cid);
        int month_done = is_month_completed(s_selected_month);
        char title[80];
        if (month_done && s_selected_month == 3) {
            snprintf(title, sizeof(title), "MONTH 04 COMPLETE -- SPEEDCOIN UNLOCKED!");
            plexus_log_push("STORY // SPEEDCOIN MODULE UNLOCKED", title,
                            "All 4 cards of Month 04 are finished and locked.",
                            "SPEEDCOIN is now unlocked in the bottom bar, and",
                            "Month 05 is now active on the conditioning spine.");
        } else if (month_done && s_selected_month == 7) {
            snprintf(title, sizeof(title), "MONTH 08 COMPLETE -- ACTION CARD UNLOCKED!");
            plexus_log_push("STORY // ACTION CARD MODULE UNLOCKED", title,
                            "All 4 cards of Month 08 are finished and locked.",
                            "ACTION CARD is now unlocked in the bottom bar, and",
                            "Month 09 is now active on the conditioning spine.");
        } else if (month_done && s_selected_month + 1 < 12) {
            snprintf(title, sizeof(title), "MONTH %02d COMPLETE -- MONTH %02d UNLOCKED!",
                     s_selected_month + 1, s_selected_month + 2);
            plexus_log_push("STORY // MONTHLY PROGRESSION UNLOCKED", title,
                            "All 4 cards of this month are finished and locked.",
                            "The next month on the conditioning spine is now active.",
                            "Use the up/down arrows to advance to the newly unlocked month.");
        } else {
            snprintf(title, sizeof(title), "[%s] -- FINISHED & LOCKED", CARDS[cid].name);
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
    tut_restore_telemetry(1);
    s_tut_active  = 0;
    s_tut_card_id = -1;
    s_tut_step    = TUT1_NONE;
}

int plexus_training_is_active(void) {
    return s_training_active;
}

void plexus_training_stop(void) {
    s_training_active = 0;
}

void plexus_tutorial_setup_round(GameState *g) {
    if (!g || !s_tut_active) return;

    tut_save_telemetry();
    tut_restore_telemetry(0);

    g->ruleset        = RULESET_KK_ADD_10;
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
            (CheckCollisionPointRec(vm, (Rectangle){ 280.0f, 520.0f, 320.0f, 60.0f }) ||
             CheckCollisionPointRec(vm, (Rectangle){ 680.0f, 520.0f, 320.0f, 60.0f }))) {
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
        Rectangle left_box = { 9.0f, 505.0f, 261.0f, 206.0f };
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
        draw_text_snapped(font_title, "ENTITY 001 -- INFORMATIONAL UPGRADE INITIATED",
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
                          "Each of the 12 months has 4 cards sharing the same plant.",
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
                          "Every hand play is followed by an automatic deck draw.",
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
                          "Captured cards move into your scoring area to build",
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
                          "declare \"Koi-Koi\" or \"Shobu\" to end the round.",
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
        draw_text_snapped(font_title, "TSUKIMI-ZAKE (MOON VIEWING): 5 PTS",
                          x + 216.0f, y + 216.0f, 18.0f, 1.2f, COLOR_PHOSPHOR);
    } else if (s_tut_step == TUT1_PANEL_KOI_DECIDE) {
        draw_text_snapped(font_title, "You have completed a combo!",
                          x, y, 21.0f, 1.3f, COLOR_PHOSPHOR);
        DrawLine((int)x, (int)(y + 30.0f), (int)(pr.x + pr.width - 26.0f), (int)(y + 30.0f), COLOR_PHOSPHOR_DIM);
        draw_text_snapped(font_body,
                          "Select \"KOI-KOI (CONTINUE)\" to keep playing for more points, or",
                          x, y + 46.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body,
                          "select \"SHOBU (BANK & END)\" to bank your points and win the round.",
                          x, y + 74.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
    }

    draw_plexus_btn(cr, "[CONTINUE]", 16, vm, CheckCollisionPointRec(vm, pr));
}

void plexus_tutorial_finish_match(void) {
    if (!s_tut_active) return;
    tut_restore_telemetry(1);
    int cid = (s_tut_card_id >= 0 && s_tut_card_id < 48) ? s_tut_card_id : 3;
    s_finished_cards_mask |= (1ULL << cid);
    s_tut_active  = 0;
    s_tut_card_id = -1;
    s_tut_step    = TUT1_NONE;

    char title[80];
    snprintf(title, sizeof(title), "[%s] -- TUTORIAL 01 COMPLETED", CARDS[cid].name);
    plexus_log_push("STORY // TUTORIAL CARD 1 COMPLETED", title,
                    "Entity 001 completed Standard Rules of Koi-Koi conditioning.",
                    "Card 4 of Month 01 is finished (and remains replayable).",
                    "Select the next card above it on the spine to continue.");
    plexus_save();
}

/* =========================================================================
 * Atrium Plex Speedcoin Solo Highscore Mode & Result Screen
 * ========================================================================= */

static const Rectangle k_sc_play_again_rect = { 360.0f, 628.0f, 260.0f, 50.0f };
static const Rectangle k_sc_back_plex_rect  = { 660.0f, 628.0f, 260.0f, 50.0f };

static int has_four_of_month_in_field(const int *field, int count) {
    int mcnt[13] = { 0 };
    for (int i = 0; i < count; i++) {
        int m = CARDS[field[i]].month;
        if (m >= 1 && m <= 12 && ++mcnt[m] >= 4) return 1;
    }
    return 0;
}

int plexus_speedcoin_is_active(void) {
    return s_speedcoin_active;
}

void plexus_speedcoin_abort(void) {
    s_speedcoin_active = 0;
    s_speedcoin_result_view = 0;
    speed_mode_stop();
}

int plexus_speedcoin_result_active(void) {
    return s_speedcoin_result_view;
}

void plexus_speedcoin_close_result(void) {
    s_speedcoin_result_view = 0;
}

void plexus_speedcoin_setup_wave(GameState *g, int wave_num) {
    if (!g) return;
    int w = (wave_num > 0) ? wave_num : 1;
    int kept_score = (w > 1) ? g->total_score[0] : 0;
    uint32_t rng = g->rng_state ? g->rng_state : 1u;

    memset(g, 0, sizeof(*g));
    g->rng_state = rng;
    g->ruleset = RULESET_KK_ADD_10;
    g->current_round = w;
    g->total_rounds = w + 1;
    g->dealer = 0;
    g->current_player = 0;
    g->total_score[0] = kept_score;
    g->pending_card = -1;
    g->played_hand_card = -1;
    g->oya_player_choice = -1;
    g->oya_winner = -1;
    g->current_round_record.round_number = w;
    g->current_round_record.winner = -1;

    int deck[48];
    while (1) {
        for (int i = 0; i < 48; i++) deck[i] = i;
        for (int i = 47; i > 0; i--) {
            int j = (int)(engine_rand(&g->rng_state) % (uint32_t)(i + 1));
            int tmp = deck[i];
            deck[i] = deck[j];
            deck[j] = tmp;
        }
        if (!has_four_of_month_in_field(deck, 14)) break;
    }

    g->field_count = 14;
    memcpy(g->field, &deck[0], 14 * sizeof(int));
    g->hand_count[0] = 8;
    memcpy(g->hand[0], &deck[14], 8 * sizeof(int));
    g->hand_count[1] = 0;
    g->stock_count = 26;
    memcpy(g->stock, &deck[22], 26 * sizeof(int));
    g->phase = PHASE_TURN_HAND;

    if (w <= 1) speed_mode_begin();
    else speed_mode_begin_wave(w);
}

int plexus_speedcoin_deal_hand2(GameState *g) {
    if (!g || !s_speedcoin_active || !speed_mode_can_deal_hand2() || g->stock_count < 8) return 0;
    if (g->current_round_record.winner == 0 && g->current_round_record.award > 0) {
        g->total_score[0] -= g->current_round_record.award;
    }
    memset(&g->current_round_record, 0, sizeof(g->current_round_record));
    g->current_round_record.round_number = g->current_round;
    g->current_round_record.winner = -1;
    yaku_snapshot(g->ruleset, g->captured[0], g->captured_count[0], g->last_yaku_points[0]);
    g->last_base[0] = score_captures_base(g->ruleset, g->captured[0], g->captured_count[0]);

    for (int i = 0; i < 8; i++) {
        int cid = g->stock[--g->stock_count];
        g->hand[0][i] = cid;
        Rectangle hr = board_get_hand_card_rect(i, 8, 0);
        motion_set_instant(cid, (Vector2){ 330.0f, 262.0f }, (Vector2){ FIELD_CARD_W, FIELD_CARD_H });
        motion_set_target(cid, (Vector2){ hr.x, hr.y }, (Vector2){ hr.width, hr.height }, 0.42f);
    }
    g->hand_count[0] = 8;
    g->current_player = 0;
    g->phase = PHASE_TURN_HAND;
    speed_mode_mark_hand2_dealt();
    return 1;
}

int plexus_speedcoin_bank_wave(GameState *g) {
    if (!g) return 0;
    if (g->current_round_record.winner == 0) {
        return g->current_round_record.award;
    }
    ScoreResult sr = score_captures(g->ruleset, g->captured[0], g->captured_count[0]);
    int award = calculate_award(sr.base_points, 0, 0);
    if (sr.yaku_count > 0 && award > 0) {
        g->total_score[0] += award;
        g->current_round_record.winner = 0;
        g->current_round_record.draw_reason = 0;
        g->current_round_record.lucky_hand = 0;
        g->current_round_record.base_points = sr.base_points;
        g->current_round_record.mult_seven = (sr.base_points >= 7) ? 2 : 1;
        g->current_round_record.mult_opp_koi = 1;
        g->current_round_record.mult_self_koi = 1;
        g->current_round_record.award = award;
        g->current_round_record.score_result = sr;
    }
    return award;
}

void plexus_speedcoin_finish_run(int final_score, int waves, int run_coins, int run_bonus_pts, int run_best_streak) {
    ensure_log_seeded();
    s_sc_last_score = (final_score > 0) ? final_score : 0;
    s_sc_last_waves = (waves > 0) ? waves : 1;
    s_sc_last_coins = (run_coins > 0) ? run_coins : 0;
    s_sc_last_bonus = (run_bonus_pts > 0) ? run_bonus_pts : 0;
    s_sc_last_streak = (run_best_streak > 0) ? run_best_streak : 0;
    s_sc_last_rank_idx = -1;

    int ins = s_sc_rank_count;
    for (int i = 0; i < s_sc_rank_count; i++) {
        int better = (s_sc_last_score > s_sc_ranks[i].score) ||
                     (s_sc_last_score == s_sc_ranks[i].score && s_sc_last_coins > s_sc_ranks[i].coins) ||
                     (s_sc_last_score == s_sc_ranks[i].score && s_sc_last_coins == s_sc_ranks[i].coins &&
                      s_sc_last_waves > s_sc_ranks[i].waves);
        if (better) {
            ins = i;
            break;
        }
    }
    if (ins < SPEEDCOIN_RANK_MAX) {
        if (s_sc_rank_count < SPEEDCOIN_RANK_MAX) s_sc_rank_count++;
        for (int j = s_sc_rank_count - 1; j > ins; j--) {
            s_sc_ranks[j] = s_sc_ranks[j - 1];
        }
        s_sc_ranks[ins].score = s_sc_last_score;
        s_sc_ranks[ins].waves = s_sc_last_waves;
        s_sc_ranks[ins].coins = s_sc_last_coins;
        s_sc_last_rank_idx = ins;
    }

    char title[80], l1[96], l2[96];
    snprintf(title, sizeof(title), "SPEEDCOIN RUN // %d PTS  [WAVE %d]", s_sc_last_score, s_sc_last_waves);
    snprintf(l1, sizeof(l1), "Final Score: %d PTS  (Speedcoin Bonus: +%d PTS from %d MP).",
             s_sc_last_score, s_sc_last_bonus, s_sc_last_coins);
    if (s_sc_last_rank_idx >= 0) {
        snprintf(l2, sizeof(l2), "Ranked #%02d on the Atrium Speedcoin Ranklist  [Best Serie x%d].",
                 s_sc_last_rank_idx + 1, s_sc_last_streak);
    } else {
        snprintf(l2, sizeof(l2), "Waves Reached: %d  //  Best Perfect Serie: x%d.",
                 s_sc_last_waves, s_sc_last_streak);
    }
    plexus_log_push("SPEEDCOIN // RUN SUMMARY", title, l1, l2,
                    "Review Speedbar telemetry below or launch another 180s run.");

    s_speedcoin_active = 0;
    s_speedcoin_result_view = 1;
    speed_mode_stop();
    plexus_save();
}

void plexus_draw_speedcoin_result(const BoardAssets *ba, const GameStats *st, Vector2 vm) {
    ensure_plexus_assets();
    if (s_plexus_bg.id > 0) {
        DrawTexturePro(s_plexus_bg, (Rectangle){ 0, 0, 1280, 720 },
                       (Rectangle){ 0, 0, 1280, 720 }, (Vector2){ 0, 0 }, 0.0f, WHITE);
    } else {
        ClearBackground(BLACK);
    }
    DrawRectangle(0, 0, 1280, 720, (Color){ 0, 0, 0, 200 });

    const char *hdr = "SPEEDCOIN  //  TIME EXPIRED";
    Vector2 hts = MeasureTextEx(font_title, hdr, 28.0f, 1.4f);
    draw_text_snapped(font_title, hdr, (1280.0f - hts.x) * 0.5f, 24.0f, 28.0f, 1.4f, COLOR_PHOSPHOR);

    Rectangle banner = { 160.0f, 70.0f, 960.0f, 68.0f };
    DrawRectangleRec(banner, PLEXUS_PANEL_BG);
    ui_draw_panel_border(banner, COLOR_PHOSPHOR);
    char sum_txt[128];
    snprintf(sum_txt, sizeof(sum_txt),
             "FINAL SCORE: %d PTS     //     WAVE %d     //     %d MP (+%d PTS)",
             s_sc_last_score, s_sc_last_waves, s_sc_last_coins, s_sc_last_bonus);
    Vector2 sts = MeasureTextEx(font_title, sum_txt, 20.0f, 1.2f);
    draw_text_snapped(font_title, sum_txt, banner.x + (banner.width - sts.x) * 0.5f,
                      banner.y + 23.0f, 20.0f, 1.2f, COLOR_TEXT_INFO);

    /* Left Panel: Persistent Top-8 Speedcoin Ranklist */
    Rectangle rank_box = { 160.0f, 154.0f, 464.0f, 448.0f };
    DrawRectangleRec(rank_box, PLEXUS_PANEL_BG);
    ui_draw_panel_border(rank_box, COLOR_PHOSPHOR);
    draw_text_snapped(font_title, "SPEEDCOIN RANKLIST  //  TOP 08",
                      rank_box.x + 18.0f, rank_box.y + 14.0f, 17.0f, 1.2f, COLOR_PHOSPHOR);
    DrawLine((int)rank_box.x + 16, (int)rank_box.y + 42,
             (int)(rank_box.x + rank_box.width - 16.0f), (int)rank_box.y + 42, COLOR_PHOSPHOR_DIM);
    draw_text_snapped(font_body, "RANK    SCORE          WAVE     COINS",
                      rank_box.x + 20.0f, rank_box.y + 52.0f, 16.0f, 1.1f, COLOR_TEXT_INFO_DIM);

    for (int i = 0; i < SPEEDCOIN_RANK_MAX; i++) {
        float ry = rank_box.y + 84.0f + (float)i * 43.0f;
        Rectangle row_r = { rank_box.x + 14.0f, ry - 4.0f, rank_box.width - 28.0f, 34.0f };
        int is_cur = (i == s_sc_last_rank_idx);
        if (is_cur) {
            DrawRectangleRec(row_r, COLOR_BTN_HOVER_BG);
            ui_draw_panel_border(row_r, COLOR_PHOSPHOR);
        }
        if (i < s_sc_rank_count) {
            Color c = is_cur ? COLOR_PHOSPHOR : COLOR_TEXT_INFO;
            draw_text_snapped(font_title, TextFormat("#%02d", i + 1),
                              rank_box.x + 20.0f, ry + 3.0f, 17.0f, 1.1f, c);
            draw_text_snapped(font_body, TextFormat("%d PTS", s_sc_ranks[i].score),
                              rank_box.x + 96.0f, ry + 3.0f, 18.0f, 1.1f, c);
            draw_text_snapped(font_body, TextFormat("W-%02d", s_sc_ranks[i].waves),
                              rank_box.x + 224.0f, ry + 3.0f, 18.0f, 1.1f, c);
            draw_text_snapped(font_body, TextFormat("%d MP%s", s_sc_ranks[i].coins, is_cur ? " [NEW]" : ""),
                              rank_box.x + 306.0f, ry + 3.0f, 17.0f, 1.0f, c);
        } else {
            draw_text_snapped(font_body, TextFormat("#%02d     ---            --       ---", i + 1),
                              rank_box.x + 20.0f, ry + 3.0f, 17.0f, 1.1f, COLOR_PHOSPHOR_DIM);
        }
    }

    /* Right Top Sub-Box: Current Run Breakdown */
    Rectangle run_box = { 656.0f, 154.0f, 464.0f, 224.0f };
    DrawRectangleRec(run_box, PLEXUS_PANEL_BG);
    ui_draw_panel_border(run_box, COLOR_PHOSPHOR);
    draw_text_snapped(font_title, "THIS RUN  //  SCORE BREAKDOWN",
                      run_box.x + 16.0f, run_box.y + 14.0f, 17.0f, 1.2f, COLOR_PHOSPHOR);
    DrawLine((int)run_box.x + 16, (int)run_box.y + 42,
             (int)(run_box.x + run_box.width - 16.0f), (int)run_box.y + 42, COLOR_PHOSPHOR_DIM);
    int combo_pts = s_sc_last_score - s_sc_last_bonus;
    if (combo_pts < 0) combo_pts = 0;
    draw_text_snapped(font_body, TextFormat("COMBO POINTS   :  %d PTS  (KK-ADD-10)", combo_pts),
                      run_box.x + 18.0f, run_box.y + 56.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body, TextFormat("SPEEDCOINS     :  %d MP  (+%d BONUS PTS)", s_sc_last_coins, s_sc_last_bonus),
                      run_box.x + 18.0f, run_box.y + 92.0f, 18.0f, 1.2f, COLOR_PHOSPHOR);
    draw_text_snapped(font_body, TextFormat("WAVES REACHED  :  WAVE %d", s_sc_last_waves),
                      run_box.x + 18.0f, run_box.y + 128.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body, TextFormat("RUN BEST SERIE :  x%d PERFECT DROPS", s_sc_last_streak),
                      run_box.x + 18.0f, run_box.y + 164.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);

    /* Right Bottom Sub-Box: Exact Logfile Stats Box */
    Rectangle log_stats_r = { 656.0f, 394.0f, 464.0f, 208.0f };
    draw_logfile_stats_box(st, log_stats_r);

    draw_plexus_btn(k_sc_play_again_rect, "PLAY AGAIN", 18, vm, 0);
    draw_plexus_btn(k_sc_back_plex_rect,  "ATRIUM PLEX", 18, vm, 0);
    (void)ba;
}

ScreenAction plexus_handle_speedcoin_result_click(Vector2 vm, int *out_clicked) {
    if (out_clicked) *out_clicked = 0;
    if (!s_speedcoin_result_view) return ACT_NONE;
    if (CheckCollisionPointRec(vm, k_sc_play_again_rect)) {
        s_speedcoin_result_view = 0;
        s_speedcoin_active = 1;
        if (out_clicked) *out_clicked = 1;
        return ACT_START_GAME;
    }
    if (CheckCollisionPointRec(vm, k_sc_back_plex_rect)) {
        s_speedcoin_result_view = 0;
        s_speedcoin_active = 0;
        if (out_clicked) *out_clicked = 1;
        return ACT_GOTO_ATRIUM;
    }
    return ACT_NONE;
}
