#include "ui_plexus.h"
#include "campaign.h"
#include "cards.h"
#include "ui_motion.h"
#include "ui_speed.h"
#include "ui_action.h"
#include "ui_theme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

extern Font font_title;
extern Font font_body;
extern GameSettings settings;

#define PLEXUS_MATCH_LOG_MAX 48
#define DEUS_SPINE_UNLOCKED_FOR_TESTING 0
#define PLEXUS_PANEL_BG (Color){ 0, 0, 0, 228 }
#define PLEXUS_SUBBOX_BG (Color){ 4, 8, 10, 236 }

typedef enum {
    LOG_TAB_STORY    = 0,
    LOG_TAB_TRAINING = 1,
    LOG_TAB_MATCHES  = 2,
    LOG_TAB_ATLAS    = 3
} PlexusLogTab;

typedef enum {
    MLOG_SPINE       = 1,
    MLOG_SPEEDCOIN   = 2,
    MLOG_ACTION_CARD = 3,
    MLOG_TRAINING    = 4
} PlexusMatchMode;

typedef struct {
    int  mode;
    char badge[32];
    char title[88];
    char detail[112];
} PlexusMatchEntry;

static Texture2D s_plexus_bg = { 0 };
static Texture2D s_plexus_normal = { 0 };
static Font      s_story_font = { 0 };
static int       s_story_font_loaded = 0;
static int s_plexus_loaded = 0;
static int   s_intro_seen = 0;
static float s_intro_timer = 0.0f;
static float s_intro_reveal_t = 0.0f;
static Texture2D s_intro_ui_tex = { 0 };
static int s_selected_month = 0; /* 0..11 */
static int s_selected_slot  = 3; /* 0..3 within current month (starts at bottom card 4 for Tutorial 1) */
static int s_active_tab     = 0; /* 0: node briefing, 1: logfile window, 2..4: modes */
static unsigned long long s_finished_cards_mask      = 0ULL; /* 48-bit Standard Atlas mask */
static unsigned long long s_deus_finished_cards_mask = 0ULL; /* 48-bit Hardcore Deus Atlas mask */
static int s_deus_spine_mode  = 0; /* 0: Standard Spine pass, 1: Hardcore Deus Spine pass */
static int s_std_saved_month  = 0, s_std_saved_slot  = 3;
static int s_deus_saved_month = 0, s_deus_saved_slot = 3;

/* 4-Tab Logfile Window state: [ STORY ] [ TRAINING ] [ MATCHES ] [ ATLAS ] */
static int s_log_subtab       = LOG_TAB_STORY;
static int s_story_sel_month  = -1;  /* -1: Prologue, 0..11: selected monthly log */
static unsigned int s_story_read_mask = 0u; /* 12-bit mask of opened monthly logs */
static unsigned int s_story_open_mask = 0u; /* transient open-for-first-time marker */
static int s_tr_runs          = 0;
static int s_tr_wins          = 0;
static int s_tr_losses        = 0;
static int s_tr_best_score    = 0;
static int s_tr_rounds_total  = 0;
static PlexusMatchEntry s_match_logs[PLEXUS_MATCH_LOG_MAX];
static int s_match_log_count  = 0;
static int s_match_filter     = 0; /* 0: ALL, 1..4: MLOG_* */
static int s_match_scroll     = 0;
static int s_atlas_view_deus  = 0; /* 0: Standard Atlas, 1: Deus Atlas */
static int s_atlas_pinned_cid = 3; /* 0..47 inspected card in Atlas viewer */

/* Spine unlock turn-around flip & irreversible [NEXT] month swing-in animation state */
#define SPINE_FLIP_DUR  0.46f
#define SPINE_SWING_DUR 0.54f
static int   s_flip_active = 0, s_flip_month = -1, s_flip_slot = -1;
static float s_flip_delay  = 0.0f, s_flip_elapsed = 0.0f;
static int   s_swing_active = 0, s_swing_prev_month = 0;
static float s_swing_elapsed = 0.0f;

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
    TUT1_FREE_PLAY,
    TUT2_WAIT_START,
    TUT2_PANEL_CORE,
    TUT2_WAIT_MOVE4,
    TUT2_PANEL_SYNC,
    TUT2_PANEL_PARASITE,
    TUT2_PLAY_TO_WAVE_END,
    TUT2_PANEL_HAND2,
    TUT2_WAIT_SCORE_SCREEN,
    TUT2_PANEL_NO_HESITATION,
    TUT2_FREE_PLAY,
    TUT3_WAIT_START,
    TUT3_PANEL_METERS,
    TUT3_PANEL_HORRORS,
    TUT3_FREE_PLAY,
    /* Generic story protocol on any spine card above Month 01 (card_id >= 4):
       one ordinary dealt round, one frozen keeper transmission, then play.
       These ride the tutorial machine on purpose -- see the note above
       story_beat_has_protocol(). Do not add a fourth state, a second freeze
       flag or a second match launcher for story content. */
    PROTO_WAIT_DEAL,
    PROTO_PANEL_STORY,
    PROTO_PLAY
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
static int s_saved_ac_symbols = 0;
static int s_saved_ac_time = 0;
static int s_training_active = 0;
static int s_training_result_view = 0;
static int s_action_card_active = 0;
static int s_action_card_result_view = 0;
static int s_ac_runs = 0;
static int s_ac_wins = 0;
static int s_ac_losses = 0;
static int s_ac_best_score = 0;
static int s_ac_last_mp = 0;
static int s_ac_last_yt = 0;
static int s_ac_last_yp = 0;
static int s_ac_last_ct = 0;
static int s_ac_last_cp = 0;
static int s_ac_last_mt = 0;

/* Per-mode Atrium Plex match settings (persisted in plexsave.txt; isolated from Campaign save) */
static int        s_mode_settings_open = 0;
static int        s_mode_settings_page = 0;
static int        s_plex_cfg_stage     = -1;
static int        s_ac_cfg_rounds      = 6;
static Difficulty s_ac_cfg_diff        = DIFFICULTY_EASY;
static int        s_ac_cfg_timer       = 15;
static Ruleset    s_tr_cfg_ruleset     = RULESET_KK_ADD_10;
static int        s_tr_cfg_rounds      = 6;
static Difficulty s_tr_cfg_diff        = DIFFICULTY_EASY;
static int        s_tr_cfg_timer       = 15;
static int        s_tr_cfg_opponent    = 0;
static int        s_tr_cfg_condition   = 0;

static int        s_cfg_override_active = 0;
static int        s_saved_camp_rounds   = 6;
static Difficulty s_saved_camp_diff     = DIFFICULTY_EASY;
static int        s_saved_camp_timer    = 15;
static Ruleset    s_saved_camp_ruleset  = RULESET_NINTENDO;
static int        s_saved_camp_opponent = 0;
static int        s_saved_camp_bg       = 0;
static int        s_saved_camp_tcol     = 0;
static int        s_saved_camp_table    = 0;

/* Solo Speedcoin & Action Card modes (unlocked for testing; saved in plexsave.txt) */
#define SPEEDCOIN_UNLOCKED_FOR_TESTING   1
#define ACTION_CARD_UNLOCKED_FOR_TESTING 1
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
static int s_sc_runs = 0;
static int s_sc_best_wave = 0;
static int s_sc_last_rank_idx = -1;
static int s_sc_last_score = 0;
static int s_sc_last_waves = 1;
static int s_sc_last_coins = 0;
static int s_sc_last_bonus = 0;
static int s_sc_last_streak = 0;
static int s_sc_last_drops = 0;
static float s_sc_last_avg_sec = 0.0f;
static float s_sc_last_avg_cov = 0.0f;

static void tut_save_telemetry(void) {
    if (s_tut_telemetry_saved) return;
    speed_bar_get_telemetry(&s_saved_drops, &s_saved_time_sum, &s_saved_cover_sum,
                            &s_saved_best_streak, &s_saved_lifetime_mp);
    s_saved_audio_triggers = board_get_audio_action_triggers();
    s_saved_audio_peak     = board_get_audio_peak_synergy();
    s_saved_audio_hold     = board_get_audio_full_hold_seconds();
    s_saved_ac_symbols     = action_card_get_symbols_fired();
    s_saved_ac_time        = action_card_get_lifetime_time();
    s_tut_telemetry_saved  = 1;
}

static void tut_restore_telemetry(int clear_saved) {
    if (!s_tut_telemetry_saved) return;
    speed_bar_set_telemetry(s_saved_drops, s_saved_time_sum, s_saved_cover_sum,
                            s_saved_best_streak, s_saved_lifetime_mp);
    board_set_audio_telemetry(s_saved_audio_triggers, s_saved_audio_peak, s_saved_audio_hold);
    action_card_set_telemetry(s_saved_ac_symbols, s_saved_ac_time);
    if (clear_saved) s_tut_telemetry_saved = 0;
}

static unsigned int s_last_briefing_hash = 0u;
static int s_briefing_hash_ready = 0;
static float s_briefing_glow = 0.0f;
static float s_briefing_hold = 0.0f;

/* HISCORE board overlay (SPEEDCOIN + ACTION CARD tabs). Purely a read-out of
   state that already exists - the speedcoin top-8 ranklist and the ACTION CARD
   rows of the match log - so it adds no save fields and no new persistence. */
static int s_hiscore_open = 0;

static void commit_viewed_story_months(int keep_month) {
    for (int m = 0; m < 12; m++) {
        unsigned int bit = (1u << m);
        if (m != keep_month && (s_story_open_mask & bit)) {
            s_story_read_mask |= bit;
            s_story_open_mask &= ~bit;
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

static const char * const k_month_short_labels[12] = {
    "01 // JANUARY",   "02 // FEBRUARY",  "03 // MARCH",     "04 // APRIL",
    "05 // MAY",       "06 // JUNE",      "07 // JULY",      "08 // AUGUST",
    "09 // SEPTEMBER", "10 // OCTOBER",   "11 // NOVEMBER",  "12 // DECEMBER"
};

/* The briefing card lights up when its copy changes. It used to decay at 1.15/s
   (0.87 s), which was gone before the eye reached the card; it now holds full
   brightness for 0.9 s and then fades over 2.6 s (owner, 29 Oct 2026). */
#define PLEXUS_BRIEFING_HOLD 0.90f
#define PLEXUS_BRIEFING_FADE 0.38f

/* Geometry matching plexus/plexus_mockup.png and expanded vertically for un-squished 18px-22px text */
static const Rectangle k_next_month_rect = { 1072.0f,  54.0f,  96.0f,  34.0f };
static const Rectangle k_card_rects[4] = {
    { 1092.0f, 102.0f, 56.0f, 88.0f },
    { 1092.0f, 228.0f, 56.0f, 88.0f },
    { 1092.0f, 354.0f, 56.0f, 88.0f },
    { 1092.0f, 496.0f, 56.0f, 88.0f }
};
/* The briefing card is 196 tall, not 250: its copy ends at +172 and the extra
   56 px was dead space inside the frame (owner, 29 Oct 2026). The width is at
   its limit and cannot shrink - "MONTH 01 // JANUARY [TUTORIAL]" already draws
   502 px across a 496 px inner width, and the longest speedcoin line 521 px. */
static const Rectangle k_briefing_rect = { 516.0f,  82.0f, 540.0f, 196.0f };
/* One action row under the card: three 160 px slots on a 30 px gutter, right edge
   flush with the briefing card. START is always the LAST slot, SETTINGS the middle
   one; the FIRST slot is HISCORE on the ACTION CARD tab and the middle slot is
   HISCORE on SPEEDCOIN (it has no settings), so every tab fills from the right and
   no tab leaves a hole in the row. */
static const Rectangle k_hiscore_rect  = { 516.0f, 302.0f, 160.0f,  48.0f };
static const Rectangle k_settings_rect = { 706.0f, 302.0f, 160.0f,  48.0f };
static const Rectangle k_start_rect    = { 896.0f, 302.0f, 160.0f,  48.0f };
static const Rectangle k_mode_cfg_rect = { 516.0f, 362.0f, 540.0f, 222.0f };
/* Six identical buttons on a 26 px gutter, centred as one block: the old row ran
   156/152/170/170/170/196 wide, so OPTION was 44 px fatter than LOGFILE for no
   reason but label length, and the row read as six different controls. 158 px is
   the widest label ("ACTION CARD", 137.6 px at 16 pt) plus 10 px of air, and 44 px
   of height keeps them lighter than the 48 px action row above. */
#define PLEXUS_BOTTOM_W 158.0f
#define PLEXUS_BOTTOM_H  44.0f
#define PLEXUS_BOTTOM_GAP 26.0f
static const Rectangle k_bottom_rects[6] = {
    { 101.0f, 648.0f, PLEXUS_BOTTOM_W, PLEXUS_BOTTOM_H }, /* SAVE & BACK */
    { 285.0f, 648.0f, PLEXUS_BOTTOM_W, PLEXUS_BOTTOM_H }, /* LOGFILE */
    { 469.0f, 648.0f, PLEXUS_BOTTOM_W, PLEXUS_BOTTOM_H }, /* SPEEDCOIN */
    { 653.0f, 648.0f, PLEXUS_BOTTOM_W, PLEXUS_BOTTOM_H }, /* ACTION CARD */
    { 837.0f, 648.0f, PLEXUS_BOTTOM_W, PLEXUS_BOTTOM_H }, /* TRAINING */
    {1021.0f, 648.0f, PLEXUS_BOTTOM_W, PLEXUS_BOTTOM_H }  /* OPTION */
};
static const char * const k_bottom_labels[6] = {
    "SAVE & BACK", "LOGFILE", "SPEEDCOIN", "ACTION CARD", "TRAINING", "OPTION"
};

/* Full-Panel 4-Tab Logfile Window geometry ({ 236, 22, 756, 602 }) */
static const Rectangle k_logfile_win_rect = { 236.0f, 22.0f, 756.0f, 602.0f };
static const Rectangle k_log_tab_rects[4] = {
    { 412.0f, 30.0f, 134.0f, 34.0f }, /* STORY */
    { 554.0f, 30.0f, 142.0f, 34.0f }, /* TRAINING */
    { 704.0f, 30.0f, 136.0f, 34.0f }, /* MATCHES */
    { 848.0f, 30.0f, 126.0f, 34.0f }  /* ATLAS */
};
static const char * const k_log_tab_labels[4] = {
    "STORY", "TRAINING", "MATCHES", "ATLAS"
};
static const Rectangle k_log_directive_rect    = { 254.0f,  72.0f, 720.0f,  34.0f };
static const Rectangle k_log_content_rect      = { 254.0f, 122.0f, 720.0f, 484.0f };
static const Rectangle k_match_filter_rects[5] = {
    { 254.0f, 122.0f, 112.0f, 32.0f }, /* ALL */
    { 374.0f, 122.0f, 132.0f, 32.0f }, /* SPINE */
    { 514.0f, 122.0f, 148.0f, 32.0f }, /* SPEEDCOIN */
    { 670.0f, 122.0f, 156.0f, 32.0f }, /* ACTION CARD */
    { 834.0f, 122.0f, 140.0f, 32.0f }  /* TRAINING */
};
static const char * const k_match_filter_labels[5] = {
    "ALL", "STORY", "SPEEDCOIN", "ACTION CARD", "TRAINING"
};
static const Rectangle k_match_scroll_up_rect   = { 934.0f, 204.0f, 40.0f, 32.0f };
static const Rectangle k_match_scroll_down_rect = { 934.0f, 566.0f, 40.0f, 32.0f };
static const Rectangle k_atlas_std_btn_rect     = { 254.0f, 122.0f, 232.0f, 32.0f };
static const Rectangle k_atlas_deus_btn_rect    = { 494.0f, 122.0f, 236.0f, 32.0f };
static const Rectangle k_atlas_spine_btn_rect   = { 738.0f, 122.0f, 236.0f, 32.0f };
static const Rectangle k_atlas_grid_box_rect    = { 254.0f, 162.0f, 498.0f, 286.0f };
static const Rectangle k_atlas_insp_box_rect    = { 760.0f, 162.0f, 214.0f, 286.0f };
static const Rectangle k_atlas_ach_box_rect     = { 254.0f, 456.0f, 720.0f, 150.0f };

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

static void plexus_match_log_push(int mode, const char *badge, const char *title, const char *detail) {
    if (mode < MLOG_SPINE || mode > MLOG_TRAINING) mode = MLOG_SPINE;
    if (s_match_log_count < PLEXUS_MATCH_LOG_MAX) {
        s_match_log_count++;
    }
    for (int i = s_match_log_count - 1; i > 0; i--) {
        s_match_logs[i] = s_match_logs[i - 1];
    }
    s_match_logs[0].mode = mode;
    copy_clean(s_match_logs[0].badge,  sizeof(s_match_logs[0].badge),  badge);
    copy_clean(s_match_logs[0].title,  sizeof(s_match_logs[0].title),  title);
    copy_clean(s_match_logs[0].detail, sizeof(s_match_logs[0].detail), detail);
    s_match_scroll = 0;
}

static int count_mask_cards(unsigned long long mask) {
    int cnt = 0;
    for (int i = 0; i < 48; i++) {
        if ((mask >> i) & 1ULL) cnt++;
    }
    return cnt;
}

static int count_finished_cards(void) {
    return count_mask_cards(s_finished_cards_mask);
}

static int is_deus_spine_unlocked(void) {
    return DEUS_SPINE_UNLOCKED_FOR_TESTING || count_mask_cards(s_finished_cards_mask) >= 48;
}

static int is_deus_spine_active(void) {
    return s_deus_spine_mode && is_deus_spine_unlocked();
}

static unsigned long long active_spine_mask(void) {
    return is_deus_spine_active() ? s_deus_finished_cards_mask : s_finished_cards_mask;
}

static int mark_active_spine_card_done(int cid) {
    if (cid < 0 || cid >= 48) return 1;
    unsigned long long bit = (1ULL << cid);
    if (is_deus_spine_active()) {
        int was = (int)((s_deus_finished_cards_mask & bit) != 0ULL);
        s_deus_finished_cards_mask |= bit;
        return was;
    } else {
        int was = (int)((s_finished_cards_mask & bit) != 0ULL);
        s_finished_cards_mask |= bit;
        return was;
    }
}

static int is_month_completed_mask(unsigned long long mask, int m) {
    if (m < 0 || m >= 12) return 0;
    return (((mask >> (m * 4)) & 0xFULL) == 0xFULL) ? 1 : 0;
}

static int is_month_unlocked_mask(unsigned long long mask, int m) {
    if (m <= 0) return 1;
    if (m >= 12) return 0;
    for (int prev = 0; prev < m; prev++) {
        if (!is_month_completed_mask(mask, prev)) return 0;
    }
    return 1;
}

static int is_story_month_unlocked(int m) {
    return is_month_unlocked_mask(s_finished_cards_mask, m);
}

static int has_unread_story_months(void) {
    for (int m = 0; m < 12; m++) {
        if (is_story_month_unlocked(m) && !((s_story_read_mask >> m) & 1u)) {
            return 1;
        }
    }
    return 0;
}

static int is_month_completed(int m) {
    return is_month_completed_mask(active_spine_mask(), m);
}

static int is_month_unlocked(int m) {
    return is_month_unlocked_mask(active_spine_mask(), m);
}

static int is_slot_unlocked_in_month(int m, int slot) {
    unsigned long long mask = active_spine_mask();
    if (m < 0 || m >= 12 || slot < 0 || slot >= 4) return 0;
    for (int prev = 3; prev > slot; prev--) {
        if (!((mask >> (m * 4 + prev)) & 1ULL)) return 0;
    }
    return 1;
}

static int highest_unlocked_slot(int m) {
    for (int sl = 0; sl < 3; sl++) {
        if (is_slot_unlocked_in_month(m, sl)) return sl;
    }
    return 3;
}

static void clamp_active_spine_cursor(void) {
    while (s_selected_month > 0 && !is_month_unlocked(s_selected_month)) s_selected_month--;
    if (!is_slot_unlocked_in_month(s_selected_month, s_selected_slot))
        s_selected_slot = highest_unlocked_slot(s_selected_month);
}

static void set_deus_spine_mode(int enable_deus) {
    int want = (enable_deus && is_deus_spine_unlocked()) ? 1 : 0;
    int cur  = is_deus_spine_active() ? 1 : 0;
    if (want == cur) return;
    if (cur == 0) {
        s_std_saved_month = s_selected_month;
        s_std_saved_slot  = s_selected_slot;
        s_deus_spine_mode = 1;
        s_selected_month  = s_deus_saved_month;
        s_selected_slot   = s_deus_saved_slot;
    } else {
        s_deus_saved_month = s_selected_month;
        s_deus_saved_slot  = s_selected_slot;
        s_deus_spine_mode  = 0;
        s_selected_month   = s_std_saved_month;
        s_selected_slot    = s_std_saved_slot;
    }
    s_flip_active  = 0;
    s_swing_active = 0;
    clamp_active_spine_cursor();
}

static void stage_spine_flip(int m, int slot, float delay) {
    s_flip_active  = 1;
    s_flip_month   = m;
    s_flip_slot    = slot;
    s_flip_delay   = delay;
    s_flip_elapsed = 0.0f;
}

static int is_speedcoin_bottom_unlocked(void) {
    return SPEEDCOIN_UNLOCKED_FOR_TESTING || is_month_unlocked_mask(s_finished_cards_mask, 4);
}

static int is_action_card_bottom_unlocked(void) {
    return ACTION_CARD_UNLOCKED_FOR_TESTING || is_month_unlocked_mask(s_finished_cards_mask, 8);
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

static Font story_font(void) {
    return (s_story_font_loaded && s_story_font.texture.id > 0) ? s_story_font : font_body;
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
    const char *sf_path = resolve_asset_path("assets/saira_condensed.ttf");
    if (FileExists(sf_path)) {
        int cps[96];
        for (int i = 0; i < 95; i++) cps[i] = 32 + i;
        cps[95] = 0x00D7; /* U+00D7 multiplication sign */
        /* Rasterised at 32, not 64: this face is only ever drawn at 20.5-31px, so 64 put every
           story line through a 0.32-0.48x bitmap downscale. At 32 the range becomes 0.64-0.97x,
           which also keeps the trilinear filter near mip 0 - worth having, because a mip chain
           over a packed font atlas bleeds neighbouring glyphs into each other. Measured width
           drift is about -2%, so the hardcoded story lines get slightly narrower, which is the
           safe direction inside the 518px reader panel. See the note in ui_main.c. */
        s_story_font = LoadFontEx(sf_path, 32, cps, 96);
        if (s_story_font.texture.id > 0) {
            GenTextureMipmaps(&s_story_font.texture);
            SetTextureFilter(s_story_font.texture, TEXTURE_FILTER_TRILINEAR);
            s_story_font_loaded = 1;
        }
    }
}

void plexus_load(void) {
    s_finished_cards_mask = 0ULL;
    s_deus_finished_cards_mask = 0ULL;
    s_deus_spine_mode = 0;
    s_std_saved_month = 0;
    s_std_saved_slot  = 3;
    s_deus_saved_month = 0;
    s_deus_saved_slot  = 3;
    s_selected_month = 0;
    s_selected_slot = 3;
    s_active_tab = 0;
    s_log_subtab = LOG_TAB_STORY;
    s_story_sel_month = -1;
    s_story_read_mask = 0u;
    s_story_open_mask = 0u;
    s_intro_seen = 0;
    s_intro_timer = 0.0f;
    s_intro_reveal_t = 0.0f;
    s_tr_runs = 0;
    s_tr_wins = 0;
    s_tr_losses = 0;
    s_tr_best_score = 0;
    s_tr_rounds_total = 0;
    s_match_log_count = 0;
    s_match_filter = 0;
    s_match_scroll = 0;
    s_atlas_view_deus = 0;
    s_atlas_pinned_cid = 3;
    s_flip_active = 0;
    s_swing_active = 0;
    s_tut_telemetry_saved = 0;
    s_training_active = 0;
    s_training_result_view = 0;
    s_action_card_active = 0;
    s_action_card_result_view = 0;
    s_ac_runs = 0;
    s_ac_wins = 0;
    s_ac_losses = 0;
    s_ac_best_score = 0;
    s_mode_settings_open = 0;
    s_mode_settings_page = 0;
    s_plex_cfg_stage = -1;
    s_ac_cfg_rounds = 6;
    s_ac_cfg_diff = DIFFICULTY_EASY;
    s_ac_cfg_timer = 15;
    s_tr_cfg_ruleset = RULESET_KK_ADD_10;
    s_tr_cfg_rounds = 6;
    s_tr_cfg_diff = DIFFICULTY_EASY;
    s_tr_cfg_timer = 15;
    s_tr_cfg_opponent = 0;
    s_tr_cfg_condition = 0;
    s_speedcoin_active = 0;
    s_speedcoin_result_view = 0;
    s_sc_rank_count = 0;
    s_sc_runs = 0;
    s_sc_best_wave = 0;
    s_sc_last_rank_idx = -1;
    s_sc_last_drops = 0;
    s_sc_last_avg_sec = 0.0f;
    s_sc_last_avg_cov = 0.0f;
    speed_mode_stop();
    s_briefing_hash_ready = 0;
    s_briefing_glow = 0.0f;
    s_briefing_hold = 0.0f;
    s_hiscore_open = 0;

    int drops = 0, best_strk = 0, life_mp = 0, aud_trig = 0, ac_sym = 0, ac_tm = 0;
    float t_sum = 0.0f, c_sum = 0.0f, aud_peak = 0.0f, aud_hold = 0.0f;
    speed_bar_set_telemetry(0, 0.0f, 0.0f, 0, 0);
    board_set_audio_telemetry(0, 0.0f, 0.0f);
    action_card_set_telemetry(0, 0);

    FILE *f = fopen(resolve_asset_path("profiles/plexsave.txt"), "r");
    if (!f) return;

    char line[600];
    while (fgets(line, sizeof(line), f)) {
        if (!strchr(line, '\n') && !feof(f)) {
            int ch;
            while ((ch = fgetc(f)) != '\n' && ch != EOF) {}
            continue;
        }
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, "mlog=", 5) == 0) {
            char *p[4] = { line + 5, "", "", "" };
            char *cur = line + 5;
            for (int fi = 1; fi < 4; fi++) {
                char *tab = strchr(cur, '\t');
                if (!tab) break;
                *tab = '\0';
                cur = tab + 1;
                p[fi] = cur;
            }
            int md = atoi(p[0]);
            if (md >= MLOG_SPINE && md <= MLOG_TRAINING && p[2][0] != '\0') {
                plexus_match_log_push(md, p[1], p[2], p[3]);
            }
            continue;
        }
        if (strncmp(line, "log=", 4) == 0) {
            /* Legacy save migration: keep only completed runs/matches, drop click spam */
            char *p[5] = { line + 4, "", "", "", "" };
            char *cur = line + 4;
            for (int fi = 1; fi < 5; fi++) {
                char *tab = strchr(cur, '\t');
                if (!tab) break;
                *tab = '\0';
                cur = tab + 1;
                p[fi] = cur;
            }
            if (strstr(p[0], "ACTION CARD // MATCH SUMMARY")) {
                plexus_match_log_push(MLOG_ACTION_CARD, "ACTION CARD", p[1], p[2]);
            } else if (strstr(p[0], "SPEEDCOIN // RUN SUMMARY")) {
                plexus_match_log_push(MLOG_SPEEDCOIN, "SPEEDCOIN", p[1], p[2]);
            } else if (strstr(p[0], "TRAINING // SESSION COMPLETE")) {
                plexus_match_log_push(MLOG_TRAINING, "TRAINING", p[1], p[2]);
            } else if (strstr(p[0], "CARD COMPLETED") || strstr(p[0], "TUTORIAL CARD")) {
                plexus_match_log_push(MLOG_SPINE, "STORY", p[1], p[2]);
            }
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
            } else if (strcmp(k, "deus_finished_cards_mask") == 0) {
                s_deus_finished_cards_mask = strtoull(v, NULL, 10) & ((1ULL << 48) - 1ULL);
            } else if (strcmp(k, "deus_spine_mode") == 0) {
                s_deus_spine_mode = (atoi(v) != 0) ? 1 : 0;
            } else if (strcmp(k, "std_saved_month") == 0) {
                int m = atoi(v);
                if (m >= 0 && m < 12) s_std_saved_month = m;
            } else if (strcmp(k, "std_saved_slot") == 0) {
                int sl = atoi(v);
                if (sl >= 0 && sl < 4) s_std_saved_slot = sl;
            } else if (strcmp(k, "deus_saved_month") == 0) {
                int m = atoi(v);
                if (m >= 0 && m < 12) s_deus_saved_month = m;
            } else if (strcmp(k, "deus_saved_slot") == 0) {
                int sl = atoi(v);
                if (sl >= 0 && sl < 4) s_deus_saved_slot = sl;
            } else if (strcmp(k, "selected_month") == 0) {
                int m = atoi(v);
                if (m >= 0 && m < 12) s_selected_month = m;
            } else if (strcmp(k, "selected_slot") == 0) {
                int sl = atoi(v);
                if (sl >= 0 && sl < 4) s_selected_slot = sl;
            } else if (strcmp(k, "story_read_mask") == 0) {
                s_story_read_mask = (unsigned int)(strtoul(v, NULL, 10) & 0xFFFu);
            } else if (strcmp(k, "intro_seen") == 0) {
                s_intro_seen = (atoi(v) != 0) ? 1 : 0;
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
            } else if (strcmp(k, "sc_runs") == 0) {
                int r = atoi(v);
                if (r >= 0) s_sc_runs = r;
            } else if (strcmp(k, "sc_best_wave") == 0) {
                int bw = atoi(v);
                if (bw >= 0) s_sc_best_wave = bw;
            } else if (strcmp(k, "ac_runs") == 0) {
                int r = atoi(v);
                if (r >= 0) s_ac_runs = r;
            } else if (strcmp(k, "ac_wins") == 0) {
                int w = atoi(v);
                if (w >= 0) s_ac_wins = w;
            } else if (strcmp(k, "ac_losses") == 0) {
                int l = atoi(v);
                if (l >= 0) s_ac_losses = l;
            } else if (strcmp(k, "ac_best_score") == 0) {
                int bs = atoi(v);
                if (bs >= 0) s_ac_best_score = bs;
            } else if (strcmp(k, "ac_symbols") == 0) {
                int sy = atoi(v);
                if (sy >= 0) ac_sym = sy;
            } else if (strcmp(k, "ac_time_total") == 0) {
                int tm = atoi(v);
                if (tm >= 0) ac_tm = tm;
            } else if (strcmp(k, "tr_runs") == 0) {
                int r = atoi(v);
                if (r >= 0) s_tr_runs = r;
            } else if (strcmp(k, "tr_wins") == 0) {
                int w = atoi(v);
                if (w >= 0) s_tr_wins = w;
            } else if (strcmp(k, "tr_losses") == 0) {
                int l = atoi(v);
                if (l >= 0) s_tr_losses = l;
            } else if (strcmp(k, "tr_best_score") == 0) {
                int bs = atoi(v);
                if (bs >= 0) s_tr_best_score = bs;
            } else if (strcmp(k, "tr_rounds_total") == 0) {
                int rt = atoi(v);
                if (rt >= 0) s_tr_rounds_total = rt;
            } else if (strcmp(k, "ac_cfg_rounds") == 0) {
                int r = atoi(v);
                if (r >= 1 && r <= 12) s_ac_cfg_rounds = r;
            } else if (strcmp(k, "ac_cfg_diff") == 0) {
                int d = atoi(v);
                if (d >= 0 && d <= (int)DIFFICULTY_MASTER) s_ac_cfg_diff = (Difficulty)d;
                else if (d > (int)DIFFICULTY_MASTER) s_ac_cfg_diff = DIFFICULTY_MASTER;
            } else if (strcmp(k, "ac_cfg_timer") == 0) {
                int tm = atoi(v);
                s_ac_cfg_timer = (tm == 30) ? 30 : 15;
            } else if (strcmp(k, "tr_cfg_ruleset") == 0) {
                int rs = atoi(v);
                if (rs >= 0 && rs <= (int)RULESET_YAKUZA) s_tr_cfg_ruleset = (Ruleset)rs;
            } else if (strcmp(k, "tr_cfg_rounds") == 0) {
                int r = atoi(v);
                if (r >= 1 && r <= 12) s_tr_cfg_rounds = r;
            } else if (strcmp(k, "tr_cfg_diff") == 0) {
                int d = atoi(v);
                if (d >= 0 && d <= (int)DIFFICULTY_MASTER) s_tr_cfg_diff = (Difficulty)d;
                else if (d > (int)DIFFICULTY_MASTER) s_tr_cfg_diff = DIFFICULTY_MASTER;
            } else if (strcmp(k, "tr_cfg_timer") == 0) {
                int tm = atoi(v);
                s_tr_cfg_timer = (tm == 30) ? 30 : 15;
            } else if (strcmp(k, "tr_cfg_opponent") == 0) {
                int op = atoi(v);
                if (op >= 0 && op < CAMPAIGN_OPPONENT_COUNT) s_tr_cfg_opponent = op;
            } else if (strcmp(k, "tr_cfg_condition") == 0) {
                int cn = atoi(v);
                if (cn >= 0) s_tr_cfg_condition = cn;
            }
        }
    }
    fclose(f);
    if (!is_deus_spine_unlocked()) {
        s_deus_spine_mode = 0;
        s_atlas_view_deus = 0;
    }
    if (s_ac_runs < s_ac_wins + s_ac_losses) s_ac_runs = s_ac_wins + s_ac_losses;
    if (s_tr_runs < s_tr_wins + s_tr_losses) s_tr_runs = s_tr_wins + s_tr_losses;
    if (s_sc_runs < s_sc_rank_count) s_sc_runs = s_sc_rank_count;
    for (int i = 0; i < s_sc_rank_count; i++) {
        if (s_sc_ranks[i].waves > s_sc_best_wave) s_sc_best_wave = s_sc_ranks[i].waves;
    }
    if (drops == 0) { t_sum = 0.0f; c_sum = 0.0f; }
    clamp_active_spine_cursor();
    if (is_deus_spine_active()) {
        s_deus_saved_month = s_selected_month;
        s_deus_saved_slot  = s_selected_slot;
    } else {
        s_std_saved_month = s_selected_month;
        s_std_saved_slot  = s_selected_slot;
    }
    s_story_sel_month = -1;
    if (s_finished_cards_mask != 0ULL) {
        for (int m = 11; m >= 0; m--) {
            if (is_story_month_unlocked(m)) { s_story_sel_month = m; break; }
        }
    }
    speed_bar_set_telemetry(drops, t_sum, c_sum, best_strk, life_mp);
    board_set_audio_telemetry(aud_trig, aud_peak, aud_hold);
    action_card_set_telemetry(ac_sym, ac_tm);
}

void plexus_save(void) {
    commit_viewed_story_months(-1);
    if (is_deus_spine_active()) {
        s_deus_saved_month = s_selected_month;
        s_deus_saved_slot  = s_selected_slot;
    } else {
        s_std_saved_month = s_selected_month;
        s_std_saved_slot  = s_selected_slot;
    }
    char target[512], tmp[520];
    snprintf(target, sizeof(target), "%s", resolve_asset_path("profiles/plexsave.txt"));
    snprintf(tmp, sizeof(tmp), "%s.tmp", target);
    FILE *f = fopen(tmp, "w");
    if (!f) return;

    int drops = 0, best_strk = 0, life_mp = 0;
    float t_sum = 0.0f, c_sum = 0.0f;
    int aud_trig = board_get_audio_action_triggers();
    float aud_peak = board_get_audio_peak_synergy();
    float aud_hold = board_get_audio_full_hold_seconds();
    int ac_sym = action_card_get_symbols_fired();
    int ac_tm  = action_card_get_lifetime_time();
    if (s_tut_telemetry_saved) {
        drops = s_saved_drops;
        t_sum = s_saved_time_sum;
        c_sum = s_saved_cover_sum;
        best_strk = s_saved_best_streak;
        life_mp = s_saved_lifetime_mp;
        aud_trig = s_saved_audio_triggers;
        aud_peak = s_saved_audio_peak;
        aud_hold = s_saved_audio_hold;
        ac_sym   = s_saved_ac_symbols;
        ac_tm    = s_saved_ac_time;
    } else {
        speed_bar_get_telemetry(&drops, &t_sum, &c_sum, &best_strk, &life_mp);
    }
    fprintf(f,
            "finished_cards_mask=%llu\n"
            "deus_finished_cards_mask=%llu\n"
            "deus_spine_mode=%d\n"
            "std_saved_month=%d\n"
            "std_saved_slot=%d\n"
            "deus_saved_month=%d\n"
            "deus_saved_slot=%d\n"
            "selected_month=%d\n"
            "selected_slot=%d\n"
            "story_read_mask=%u\n"
            "intro_seen=%d\n"
            "speed_drops=%d\n"
            "speed_time_sum=%.4f\n"
            "speed_cover_sum=%.4f\n"
            "speed_best_streak=%d\n"
            "speed_lifetime_mp=%d\n"
            "audio_triggers=%d\n"
            "audio_peak=%.4f\n"
            "audio_hold=%.4f\n"
            "sc_runs=%d\n"
            "sc_best_wave=%d\n"
            "ac_runs=%d\n"
            "ac_wins=%d\n"
            "ac_losses=%d\n"
            "ac_best_score=%d\n"
            "ac_symbols=%d\n"
            "ac_time_total=%d\n"
            "tr_runs=%d\n"
            "tr_wins=%d\n"
            "tr_losses=%d\n"
            "tr_best_score=%d\n"
            "tr_rounds_total=%d\n"
            "ac_cfg_rounds=%d\n"
            "ac_cfg_diff=%d\n"
            "ac_cfg_timer=%d\n"
            "tr_cfg_ruleset=%d\n"
            "tr_cfg_rounds=%d\n"
            "tr_cfg_diff=%d\n"
            "tr_cfg_timer=%d\n"
            "tr_cfg_opponent=%d\n"
            "tr_cfg_condition=%d\n",
            s_finished_cards_mask & ((1ULL << 48) - 1ULL),
            s_deus_finished_cards_mask & ((1ULL << 48) - 1ULL),
            is_deus_spine_active() ? 1 : 0,
            s_std_saved_month, s_std_saved_slot,
            s_deus_saved_month, s_deus_saved_slot,
            s_selected_month, s_selected_slot,
            s_story_read_mask & 0xFFFu,
            s_intro_seen ? 1 : 0,
            drops, t_sum, c_sum, best_strk, life_mp,
            aud_trig, aud_peak, aud_hold,
            s_sc_runs, s_sc_best_wave,
            s_ac_runs, s_ac_wins, s_ac_losses, s_ac_best_score, ac_sym, ac_tm,
            s_tr_runs, s_tr_wins, s_tr_losses, s_tr_best_score, s_tr_rounds_total,
            s_ac_cfg_rounds, (int)s_ac_cfg_diff, s_ac_cfg_timer,
            (int)s_tr_cfg_ruleset, s_tr_cfg_rounds, (int)s_tr_cfg_diff,
            s_tr_cfg_timer, s_tr_cfg_opponent, s_tr_cfg_condition);
    for (int i = 0; i < s_sc_rank_count && i < SPEEDCOIN_RANK_MAX; i++) {
        fprintf(f, "sc_rank=%d,%d,%d\n",
                s_sc_ranks[i].score, s_sc_ranks[i].waves, s_sc_ranks[i].coins);
    }
    for (int i = s_match_log_count - 1; i >= 0; i--) {
        fprintf(f, "mlog=%d\t%s\t%s\t%s\n",
                s_match_logs[i].mode, s_match_logs[i].badge,
                s_match_logs[i].title, s_match_logs[i].detail);
    }
    if (fclose(f) != 0) { remove(tmp); return; }
    if (!storage_profiles_commit_live_temp("plexsave.txt", tmp)) remove(tmp);
}

void plexus_unload_assets(void) {
    if (s_plexus_bg.id > 0) { UnloadTexture(s_plexus_bg); s_plexus_bg.id = 0; }
    if (s_plexus_normal.id > 0) { UnloadTexture(s_plexus_normal); s_plexus_normal.id = 0; }
    if (s_story_font_loaded) {
        UnloadFont(s_story_font);
        s_story_font = (Font){ 0 };
        s_story_font_loaded = 0;
    }
    s_plexus_loaded = 0;
}

static int get_unlocked_camp_opponents(void) {
    int u = settings.unlocked_opponents;
    if (u < 1) u = 1;
    if (u > CAMPAIGN_OPPONENT_COUNT) u = CAMPAIGN_OPPONENT_COUNT;
    return u;
}

static int clamped_tr_opponent(void) {
    int u = get_unlocked_camp_opponents();
    if (s_tr_cfg_opponent < 0 || s_tr_cfg_opponent >= u) s_tr_cfg_opponent = 0;
    return s_tr_cfg_opponent;
}

static const char *const k_plex_stage_names[TABLE_DESIGN_COUNT] = {
    "CRT", "CITY", "CASTLE", "HOLE", "ARCTIC", "HARVEST", "FACE", "REALM"
};
static const int k_plex_map_bg[TABLE_DESIGN_COUNT] = { -1, 4, 3, 1, 2, 0, 6, 7 };
static const int k_plex_map_color[TABLE_DESIGN_COUNT] = {
    -1, TERMINAL_THEME_RUST, TERMINAL_THEME_GOLD, TERMINAL_THEME_SAND,
    TERMINAL_THEME_TEAL, TERMINAL_THEME_BLOOD, TERMINAL_THEME_CRIMSON, TERMINAL_THEME_VIOLET
};

static int effective_plex_stage(void) {
    if (s_plex_cfg_stage >= 0 && s_plex_cfg_stage < TABLE_DESIGN_COUNT) return s_plex_cfg_stage;
    int td = s_cfg_override_active ? s_saved_camp_table : settings.table_design;
    return (td >= 0 && td < TABLE_DESIGN_COUNT) ? td : 0;
}

static void apply_plex_stage_settings(GameSettings *s) {
    if (s_plex_cfg_stage >= 0 && s_plex_cfg_stage < TABLE_DESIGN_COUNT) {
        s->table_design = s_plex_cfg_stage;
        if (s_plex_cfg_stage > TABLE_DESIGN_CRT) {
            s->background     = k_plex_map_bg[s_plex_cfg_stage];
            s->terminal_color = k_plex_map_color[s_plex_cfg_stage];
        } else {
            s->background     = (s_saved_camp_bg == 5) ? 0 : s_saved_camp_bg;
            s->terminal_color = (s_saved_camp_tcol == 5) ? 2 : s_saved_camp_tcol;
        }
    } else {
        s->table_design   = s_saved_camp_table;
        s->background     = (s_saved_camp_bg == 5) ? 0 : s_saved_camp_bg;
        s->terminal_color = (s_saved_camp_tcol == 5) ? 2 : s_saved_camp_tcol;
    }
}

static void snapshot_campaign_settings(const GameSettings *s) {
    if (s_cfg_override_active || !s) return;
    s_saved_camp_rounds   = s->rounds;
    s_saved_camp_diff     = s->difficulty;
    s_saved_camp_timer    = s->master_timer;
    s_saved_camp_ruleset  = s->ruleset;
    s_saved_camp_opponent = s->current_opponent;
    s_saved_camp_bg       = s->background;
    s_saved_camp_tcol     = s->terminal_color;
    s_saved_camp_table    = s->table_design;
    s_cfg_override_active = 1;
}

void plexus_restore_match_settings(GameSettings *s, BoardAssets *ba) {
    if (!s_cfg_override_active || !s) return;
    s->rounds           = s_saved_camp_rounds;
    s->difficulty       = s_saved_camp_diff;
    s->master_timer     = s_saved_camp_timer;
    s->ruleset          = s_saved_camp_ruleset;
    s->current_opponent = s_saved_camp_opponent;
    s->background       = s_saved_camp_bg;
    s->terminal_color   = s_saved_camp_tcol;
    s->table_design     = s_saved_camp_table;
    if (ba) ba->current_bg = s->background;
    theme_set(s->terminal_color);
    ai_set_opponent(s->current_opponent);
    s_cfg_override_active = 0;
}

void plexus_sync_match_settings(GameSettings *s, BoardAssets *ba) {
    if (!s) return;
    if (s_action_card_active && !s_tut_active) {
        snapshot_campaign_settings(s);
        if (s_ac_cfg_diff > DIFFICULTY_MASTER) s_ac_cfg_diff = DIFFICULTY_MASTER;
        s->rounds           = s_ac_cfg_rounds;
        s->ruleset          = RULESET_KK_ADD_10;
        s->difficulty       = s_ac_cfg_diff;
        s->master_timer     = s_ac_cfg_timer;
        s->current_opponent = (s_saved_camp_opponent >= 0 && s_saved_camp_opponent < CAMPAIGN_OPPONENT_COUNT)
                              ? s_saved_camp_opponent : 0;
        apply_plex_stage_settings(s);
        if (ba) ba->current_bg = s->background;
        theme_set(s->terminal_color);
    } else if (s_training_active) {
        snapshot_campaign_settings(s);
        if (s_tr_cfg_diff > DIFFICULTY_MASTER) s_tr_cfg_diff = DIFFICULTY_MASTER;
        int opp = clamped_tr_opponent();
        s->rounds           = s_tr_cfg_rounds;
        s->difficulty       = s_tr_cfg_diff;
        s->ruleset          = s_tr_cfg_ruleset;
        s->master_timer     = s_tr_cfg_timer;
        s->current_opponent = opp;
        apply_plex_stage_settings(s);
        if (ba) ba->current_bg = s->background;
        theme_set(s->terminal_color);
    } else if (s_tut_active || s_speedcoin_active) {
        snapshot_campaign_settings(s);
        s->rounds           = (s_tut_active && s_action_card_active) ? 3 : 1;
        s->ruleset          = RULESET_KK_ADD_10;
        s->difficulty       = DIFFICULTY_EASY;
        s->master_timer     = 30;
        s->current_opponent = 0;
        if (s_tut_active) {
            s->table_design   = s_saved_camp_table;
            s->background     = (s_saved_camp_bg == 5) ? 0 : s_saved_camp_bg;
            s->terminal_color = (s_saved_camp_tcol == 5) ? 2 : s_saved_camp_tcol;
        } else {
            apply_plex_stage_settings(s);
        }
        if (ba) ba->current_bg = s->background;
        theme_set(s->terminal_color);
    } else {
        plexus_restore_match_settings(s, ba);
    }
}

Rectangle plexus_get_option_btn_rect(void) {
    return k_bottom_rects[5];
}

int plexus_close_subpanel(void) {
    if (s_hiscore_open) {
        s_hiscore_open = 0;
        return 1;
    }
    if (s_mode_settings_open) {
        s_mode_settings_open = 0;
        s_mode_settings_page = 0;
        plexus_save();
        return 1;
    }
    return 0;
}

int plexus_get_spine_count(void) {
    return count_finished_cards();
}

static void draw_plexus_btn(Rectangle r, const char *txt, int fsz, Vector2 vm, int active) {
    int hov = CheckCollisionPointRec(vm, r);
    DrawRectangleRec(r, (hov || active) ? COLOR_BTN_HOVER_BG : PLEXUS_PANEL_BG);
    ui_draw_panel_border(r, (hov || active) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    Color col = (hov || active) ? COLOR_TEXT_INFO : COLOR_PHOSPHOR;
    Vector2 ts = MeasureTextEx(font_title, txt, (float)fsz, 1.2f);
    Vector2 pos = { floorf(r.x + (r.width - ts.x) * 0.5f), floorf(r.y + (r.height - (float)fsz) * 0.5f) };
    DrawTextEx(font_title, txt, pos, (float)fsz, 1.2f, col);
    /* Hover only: an `|| active` gate left the selected tab glowing permanently. */
    if (hov) ui_theme_draw_text_flares(font_title, txt, pos, (float)fsz, 1.2f, COLOR_PHOSPHOR);
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

static const char *ruleset_short_name(Ruleset r) {
    switch (r) {
        case RULESET_FUDAWIKI:   return "FUDAWIKI";
        case RULESET_KK_ADD_10:  return "KK-ADD-10";
        case RULESET_TOURNAMENT: return "TOURNAMENT";
        case RULESET_YAKUZA:     return "YAKUZA";
        default:                 return "NINTENDO";
    }
}

static const char *diff_short_name(Difficulty d, int timer_s) {
    if (d >= DIFFICULTY_MASTER) return (timer_s == 30) ? "MASTER 30S" : "MASTER 15S";
    if (d == DIFFICULTY_HARD)   return "HARD";
    return "EASY";
}

static void draw_cfg_step_btn(Rectangle r, const char *txt, Vector2 vm, int enabled) {
    int hov = enabled && CheckCollisionPointRec(vm, r);
    DrawRectangleRec(r, hov ? COLOR_BTN_HOVER_BG : PLEXUS_SUBBOX_BG);
    if (enabled) {
        ui_draw_panel_border(r, hov ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    } else {
        Color dim = COLOR_PHOSPHOR_DIM;
        dim.a = 70;
        DrawRectangleLinesEx(r, 1.0f, dim);
    }
    Color tc = !enabled ? (Color){ COLOR_PHOSPHOR_DIM.r, COLOR_PHOSPHOR_DIM.g, COLOR_PHOSPHOR_DIM.b, 95 }
                        : (hov ? COLOR_TEXT_INFO : COLOR_PHOSPHOR);
    Vector2 ts = MeasureTextEx(font_title, txt, 16.0f, 1.0f);
    draw_text_snapped(font_title, txt,
                      r.x + (r.width - ts.x) * 0.5f,
                      r.y + (r.height - 16.0f) * 0.5f,
                      16.0f, 1.0f, tc);
}

static void draw_cfg_val_box(Rectangle r, const char *txt, Color fill, Color border, Color text_col) {
    DrawRectangleRec(r, fill);
    DrawRectangleLinesEx(r, 1.0f, border);
    ui_theme_draw_panel_flares(r);
    float fsz = 16.0f;
    Vector2 ts = MeasureTextEx(font_body, txt, fsz, 1.0f);
    while (ts.x > r.width - 12.0f && fsz > 12.0f) {
        fsz -= 0.5f;
        ts = MeasureTextEx(font_body, txt, fsz, 1.0f);
    }
    draw_text_snapped(font_body, txt,
                      r.x + (r.width - ts.x) * 0.5f,
                      r.y + (r.height - fsz) * 0.5f,
                      fsz, 1.0f, text_col);
}

static void draw_cfg_toggle_chip(Rectangle r, const char *txt, Vector2 vm, int active, Color active_col) {
    int hov = CheckCollisionPointRec(vm, r);
    Color fill = active ? (Color){ active_col.r, active_col.g, active_col.b, 52 }
                        : (hov ? COLOR_BTN_HOVER_BG : PLEXUS_SUBBOX_BG);
    Color bdr = active ? active_col : (hov ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    Color tc  = active ? COLOR_TEXT_INFO : (hov ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    draw_cfg_val_box(r, txt, fill, bdr, tc);
}

static void get_ruleset_box_colors(Ruleset rs, Color *out_fill, Color *out_bdr) {
    if (rs == RULESET_NINTENDO || rs == RULESET_KK_ADD_10) {
        *out_fill = (Color){ 20, 160, 40, 55 };
        *out_bdr  = (Color){ 30, 220, 60, 145 };
    } else if (rs == RULESET_FUDAWIKI) {
        *out_fill = (Color){ 210, 140, 20, 55 };
        *out_bdr  = (Color){ 240, 160, 30, 145 };
    } else {
        *out_fill = (Color){ 210, 30, 30, 55 };
        *out_bdr  = (Color){ 240, 40, 40, 145 };
    }
}

static Rectangle plex_stage_card_rect(int map) {
    return (Rectangle){
        534.0f + (float)(map % 4) * 129.0f,
        k_mode_cfg_rect.y + 40.0f + (float)(map / 4) * 74.0f,
        117.0f, 66.0f
    };
}

static void draw_plex_stage_card(const BoardAssets *ba, int map, int cur_stage, Vector2 vm) {
    Rectangle card = plex_stage_card_rect(map);
    int hov = CheckCollisionPointRec(vm, card);
    int sel = (map == cur_stage);
    int camp_bg = s_cfg_override_active ? s_saved_camp_bg : settings.background;
    if (camp_bg < 0 || camp_bg >= 8 || camp_bg == 5) camp_bg = 0;
    int preview_bg = (map == TABLE_DESIGN_CRT) ? camp_bg : k_plex_map_bg[map];

    DrawRectangleRec(card, hov ? COLOR_BTN_HOVER_BG : PLEXUS_SUBBOX_BG);
    Rectangle art = { floorf(card.x + (card.width - 82.0f) * 0.5f), card.y + 3.0f, 82.0f, 46.0f };
    DrawRectangleRec(art, (Color){ 5, 5, 5, 255 });
    if (ba && preview_bg >= 0 && preview_bg < 8 && ba->bgs[preview_bg].id > 0) {
        DrawTexturePro(ba->bgs[preview_bg], (Rectangle){ 0, 0, 1280, 720 }, art,
                       (Vector2){ 0, 0 }, 0.0f, WHITE);
    }
    if (ba && map > TABLE_DESIGN_CRT && ba->table_skins[map].ready) {
        const Texture2D overlay = ba->table_skins[map].overlay;
        DrawTexturePro(overlay, (Rectangle){ 0, 0, (float)overlay.width, (float)overlay.height }, art,
                       (Vector2){ 0, 0 }, 0.0f, WHITE);
    }
    DrawRectangleLinesEx(card, (sel || hov) ? 2.0f : 1.0f,
                         sel ? COLOR_PHOSPHOR : (hov ? COLOR_PHOSPHOR_DIM : (Color){ 100, 100, 100, 150 }));
    if (sel) ui_theme_draw_panel_flares(card);
    Vector2 lsz = MeasureTextEx(font_body, k_plex_stage_names[map], 13.0f, 1.0f);
    draw_text_snapped(font_body, k_plex_stage_names[map],
                      card.x + (card.width - lsz.x) * 0.5f, card.y + 50.0f, 13.0f, 1.0f,
                      sel ? COLOR_PHOSPHOR : (hov ? COLOR_TEXT_INFO : COLOR_TEXT_INFO_DIM));
}

static void draw_mode_settings_panel(const BoardAssets *ba, Vector2 vm) {
    if (!s_mode_settings_open || (s_active_tab != 2 && s_active_tab != 3 && s_active_tab != 4)) return;

    DrawRectangleRec(k_mode_cfg_rect, PLEXUS_PANEL_BG);
    ui_draw_panel_border(k_mode_cfg_rect, COLOR_PHOSPHOR);

    int stage_page = (s_active_tab == 2) || (s_mode_settings_page == 1);
    const char *hdr = (s_active_tab == 2) ? "SPEEDCOIN  //  STAGE SELECT" :
                      (s_active_tab == 3) ? (stage_page ? "ACTION CARD  //  STAGE SELECT"
                                                        : "ACTION CARD  //  MATCH SETTINGS")
                                          : (stage_page ? "TRAINING  //  STAGE SELECT"
                                                        : "TRAINING  //  MATCH SETTINGS");
    draw_text_snapped(font_title, hdr, k_mode_cfg_rect.x + 18.0f, k_mode_cfg_rect.y + 10.0f,
                      16.0f, 1.3f, COLOR_PHOSPHOR);
    draw_cfg_step_btn((Rectangle){ 1014.0f, (k_mode_cfg_rect.y + 7.0f), 26.0f, 22.0f }, "X", vm, 1);
    DrawLine((int)k_mode_cfg_rect.x + 18, (int)(k_mode_cfg_rect.y + 34.0f),
             (int)(k_mode_cfg_rect.x + k_mode_cfg_rect.width - 18.0f), (int)(k_mode_cfg_rect.y + 34.0f), COLOR_PHOSPHOR_DIM);

    const float lx = 534.0f;
    const Color mast_c = { 255,  70, 70, 255 };
    int cur_stage = effective_plex_stage();

    if (stage_page) {
        for (int m = 0; m < TABLE_DESIGN_COUNT; m++) {
            draw_plex_stage_card(ba, m, cur_stage, vm);
        }
        draw_text_snapped(font_body, TextFormat("ACTIVE STAGE: %s", k_plex_stage_names[cur_stage]),
                          lx, (k_mode_cfg_rect.y + 191.0f), 15.0f, 1.0f, COLOR_TEXT_INFO_DIM);
        if (s_active_tab == 3 || s_active_tab == 4) {
            draw_cfg_step_btn((Rectangle){ 926.0f, (k_mode_cfg_rect.y + 186.0f), 112.0f, 26.0f }, "< PAGE 1", vm, 1);
        }
        return;
    }

    if (s_active_tab == 3) {
        if (s_ac_cfg_diff > DIFFICULTY_MASTER) s_ac_cfg_diff = DIFFICULTY_MASTER;
        Color rfill, rbdr;
        get_ruleset_box_colors(RULESET_KK_ADD_10, &rfill, &rbdr);
        draw_text_snapped(font_body, "Ruleset:", lx, (k_mode_cfg_rect.y + 45.0f), 17.0f, 1.1f, COLOR_PHOSPHOR_DIM);
        draw_cfg_val_box((Rectangle){ 674.0f, (k_mode_cfg_rect.y + 40.0f), 364.0f, 28.0f },
                         "[ KK-ADD-10 (LOCKED) ]", rfill, rbdr, COLOR_TEXT_INFO);

        draw_text_snapped(font_body, "Rounds:", lx, (k_mode_cfg_rect.y + 80.0f), 17.0f, 1.1f, COLOR_PHOSPHOR);
        draw_cfg_step_btn((Rectangle){  674.0f, (k_mode_cfg_rect.y + 75.0f),  38.0f, 28.0f }, "-", vm, s_ac_cfg_rounds > 1);
        draw_cfg_val_box ((Rectangle){  718.0f, (k_mode_cfg_rect.y + 75.0f), 276.0f, 28.0f },
                          TextFormat("%d ROUNDS", s_ac_cfg_rounds), PLEXUS_SUBBOX_BG, COLOR_PHOSPHOR_DIM, COLOR_TEXT_INFO);
        draw_cfg_step_btn((Rectangle){ 1000.0f, (k_mode_cfg_rect.y + 75.0f),  38.0f, 28.0f }, "+", vm, s_ac_cfg_rounds < 12);

        Color dc = (s_ac_cfg_diff == DIFFICULTY_MASTER) ? mast_c : COLOR_PHOSPHOR;
        const char *d_lbl = (s_ac_cfg_diff == DIFFICULTY_MASTER) ? "MASTER" :
                            (s_ac_cfg_diff == DIFFICULTY_HARD)   ? "HARD" : "EASY";
        draw_text_snapped(font_body, "Difficulty:", lx, (k_mode_cfg_rect.y + 115.0f), 17.0f, 1.1f, COLOR_PHOSPHOR);
        draw_cfg_step_btn((Rectangle){  674.0f, (k_mode_cfg_rect.y + 110.0f),  38.0f, 28.0f }, "<", vm, 1);
        draw_cfg_val_box ((Rectangle){  718.0f, (k_mode_cfg_rect.y + 110.0f), 276.0f, 28.0f },
                          d_lbl, PLEXUS_SUBBOX_BG, dc, (s_ac_cfg_diff == DIFFICULTY_MASTER) ? mast_c : COLOR_TEXT_INFO);
        draw_cfg_step_btn((Rectangle){ 1000.0f, (k_mode_cfg_rect.y + 110.0f),  38.0f, 28.0f }, ">", vm, 1);

        draw_text_snapped(font_body, "Turn Timer:", lx, (k_mode_cfg_rect.y + 150.0f), 17.0f, 1.1f,
                          (s_ac_cfg_diff == DIFFICULTY_MASTER) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
        if (s_ac_cfg_diff == DIFFICULTY_MASTER) {
            draw_cfg_toggle_chip((Rectangle){ 674.0f, (k_mode_cfg_rect.y + 145.0f), 178.0f, 28.0f }, "15 SEC", vm, s_ac_cfg_timer == 15, mast_c);
            draw_cfg_toggle_chip((Rectangle){ 860.0f, (k_mode_cfg_rect.y + 145.0f), 178.0f, 28.0f }, "30 SEC", vm, s_ac_cfg_timer == 30, mast_c);
        } else {
            draw_cfg_val_box((Rectangle){ 674.0f, (k_mode_cfg_rect.y + 145.0f), 364.0f, 28.0f },
                             "[ MASTER ONLY ]", PLEXUS_SUBBOX_BG, COLOR_PHOSPHOR_DIM, COLOR_PHOSPHOR_DIM);
        }

        draw_text_snapped(font_body, TextFormat("Stage: %s  (Campaign untouched)", k_plex_stage_names[cur_stage]),
                          lx, (k_mode_cfg_rect.y + 191.0f), 15.0f, 1.0f, COLOR_TEXT_INFO_DIM);
        draw_cfg_step_btn((Rectangle){ 926.0f, (k_mode_cfg_rect.y + 186.0f), 112.0f, 26.0f }, "PAGE 2 >", vm, 1);
    } else {
        if (s_tr_cfg_diff > DIFFICULTY_MASTER) s_tr_cfg_diff = DIFFICULTY_MASTER;
        draw_text_snapped(font_body, "Ruleset:", lx, (k_mode_cfg_rect.y + 45.0f), 17.0f, 1.1f, COLOR_PHOSPHOR);
        Color rf, rb;
        get_ruleset_box_colors(s_tr_cfg_ruleset, &rf, &rb);
        draw_cfg_step_btn((Rectangle){  674.0f, (k_mode_cfg_rect.y + 40.0f),  38.0f, 28.0f }, "<", vm, 1);
        draw_cfg_val_box ((Rectangle){  718.0f, (k_mode_cfg_rect.y + 40.0f), 276.0f, 28.0f },
                          ruleset_short_name(s_tr_cfg_ruleset), rf, rb, COLOR_TEXT_INFO);
        draw_cfg_step_btn((Rectangle){ 1000.0f, (k_mode_cfg_rect.y + 40.0f),  38.0f, 28.0f }, ">", vm, 1);

        draw_text_snapped(font_body, "Rounds:", lx, (k_mode_cfg_rect.y + 80.0f), 17.0f, 1.1f, COLOR_PHOSPHOR);
        draw_cfg_step_btn((Rectangle){  674.0f, (k_mode_cfg_rect.y + 75.0f),  38.0f, 28.0f }, "-", vm, s_tr_cfg_rounds > 1);
        draw_cfg_val_box ((Rectangle){  718.0f, (k_mode_cfg_rect.y + 75.0f), 276.0f, 28.0f },
                          TextFormat("%d ROUNDS", s_tr_cfg_rounds), PLEXUS_SUBBOX_BG, COLOR_PHOSPHOR_DIM, COLOR_TEXT_INFO);
        draw_cfg_step_btn((Rectangle){ 1000.0f, (k_mode_cfg_rect.y + 75.0f),  38.0f, 28.0f }, "+", vm, s_tr_cfg_rounds < 12);

        draw_text_snapped(font_body, "Difficulty:", lx, (k_mode_cfg_rect.y + 115.0f), 17.0f, 1.1f, COLOR_PHOSPHOR);
        if (s_tr_cfg_diff == DIFFICULTY_MASTER) {
            draw_cfg_step_btn   ((Rectangle){ 674.0f, (k_mode_cfg_rect.y + 110.0f),  36.0f, 28.0f }, "<", vm, 1);
            draw_cfg_val_box    ((Rectangle){ 714.0f, (k_mode_cfg_rect.y + 110.0f), 144.0f, 28.0f }, "MASTER", PLEXUS_SUBBOX_BG, mast_c, mast_c);
            draw_cfg_step_btn   ((Rectangle){ 862.0f, (k_mode_cfg_rect.y + 110.0f),  36.0f, 28.0f }, ">", vm, 1);
            draw_cfg_toggle_chip((Rectangle){ 906.0f, (k_mode_cfg_rect.y + 110.0f),  63.0f, 28.0f }, "15S", vm, s_tr_cfg_timer == 15, mast_c);
            draw_cfg_toggle_chip((Rectangle){ 975.0f, (k_mode_cfg_rect.y + 110.0f),  63.0f, 28.0f }, "30S", vm, s_tr_cfg_timer == 30, mast_c);
        } else {
            const char *dl = (s_tr_cfg_diff == DIFFICULTY_HARD) ? "HARD" : "EASY";
            draw_cfg_step_btn((Rectangle){  674.0f, (k_mode_cfg_rect.y + 110.0f),  38.0f, 28.0f }, "<", vm, 1);
            draw_cfg_val_box ((Rectangle){  718.0f, (k_mode_cfg_rect.y + 110.0f), 276.0f, 28.0f },
                              dl, PLEXUS_SUBBOX_BG, COLOR_PHOSPHOR_DIM, COLOR_TEXT_INFO);
            draw_cfg_step_btn((Rectangle){ 1000.0f, (k_mode_cfg_rect.y + 110.0f),  38.0f, 28.0f }, ">", vm, 1);
        }

        draw_text_snapped(font_body, "Opponent:", lx, (k_mode_cfg_rect.y + 150.0f), 17.0f, 1.1f, COLOR_PHOSPHOR);
        int unlocked = get_unlocked_camp_opponents();
        int opp = clamped_tr_opponent();
        const Opponent *op = campaign_get_opponent(opp);
        char opp_txt[96];
        snprintf(opp_txt, sizeof(opp_txt), "%s (%s) [%d/%d]",
                 op->name, op->favorite_combo, opp + 1, unlocked);
        draw_cfg_step_btn((Rectangle){  674.0f, (k_mode_cfg_rect.y + 145.0f),  38.0f, 28.0f }, "<", vm, unlocked > 1);
        draw_cfg_val_box ((Rectangle){  718.0f, (k_mode_cfg_rect.y + 145.0f), 276.0f, 28.0f },
                          opp_txt, PLEXUS_SUBBOX_BG, COLOR_PHOSPHOR_DIM, COLOR_TEXT_INFO);
        draw_cfg_step_btn((Rectangle){ 1000.0f, (k_mode_cfg_rect.y + 145.0f),  38.0f, 28.0f }, ">", vm, unlocked > 1);

        draw_text_snapped(font_body, TextFormat("Condition: [ NONE ]   //   Stage: %s", k_plex_stage_names[cur_stage]),
                          lx, (k_mode_cfg_rect.y + 191.0f), 15.0f, 1.0f, COLOR_TEXT_INFO_DIM);
        draw_cfg_step_btn((Rectangle){ 926.0f, (k_mode_cfg_rect.y + 186.0f), 112.0f, 26.0f }, "PAGE 2 >", vm, 1);
    }
}

static int handle_mode_settings_click(Vector2 vm) {
    if (!s_mode_settings_open || (s_active_tab != 2 && s_active_tab != 3 && s_active_tab != 4)) return 0;
    if (!CheckCollisionPointRec(vm, k_mode_cfg_rect)) return 0;

    if (CheckCollisionPointRec(vm, (Rectangle){ 1014.0f, (k_mode_cfg_rect.y + 7.0f), 26.0f, 22.0f })) {
        s_mode_settings_open = 0;
        s_mode_settings_page = 0;
        if (s_active_tab != 2) plexus_save();
        return 1;
    }

    int stage_page = (s_active_tab == 2) || (s_mode_settings_page == 1);
    if (stage_page) {
        for (int m = 0; m < TABLE_DESIGN_COUNT; m++) {
            if (CheckCollisionPointRec(vm, plex_stage_card_rect(m))) {
                s_plex_cfg_stage = m;
                return 1;
            }
        }
        if ((s_active_tab == 3 || s_active_tab == 4) &&
            CheckCollisionPointRec(vm, (Rectangle){ 926.0f, (k_mode_cfg_rect.y + 186.0f), 112.0f, 26.0f })) {
            s_mode_settings_page = 0;
            return 1;
        }
        return 0;
    }

    if (CheckCollisionPointRec(vm, (Rectangle){ 926.0f, (k_mode_cfg_rect.y + 186.0f), 112.0f, 26.0f })) {
        s_mode_settings_page = 1;
        return 1;
    }

    if (s_active_tab == 3) {
        if (s_ac_cfg_diff > DIFFICULTY_MASTER) s_ac_cfg_diff = DIFFICULTY_MASTER;
        if (CheckCollisionPointRec(vm, (Rectangle){ 674.0f, (k_mode_cfg_rect.y + 75.0f), 38.0f, 28.0f }) && s_ac_cfg_rounds > 1) {
            s_ac_cfg_rounds--; plexus_save(); return 1;
        }
        if (CheckCollisionPointRec(vm, (Rectangle){ 1000.0f, (k_mode_cfg_rect.y + 75.0f), 38.0f, 28.0f }) && s_ac_cfg_rounds < 12) {
            s_ac_cfg_rounds++; plexus_save(); return 1;
        }
        if (CheckCollisionPointRec(vm, (Rectangle){ 674.0f, (k_mode_cfg_rect.y + 110.0f), 38.0f, 28.0f })) {
            s_ac_cfg_diff = (Difficulty)(((int)s_ac_cfg_diff + 2) % 3);
            plexus_save(); return 1;
        }
        if (CheckCollisionPointRec(vm, (Rectangle){ 1000.0f, (k_mode_cfg_rect.y + 110.0f), 38.0f, 28.0f })) {
            s_ac_cfg_diff = (Difficulty)(((int)s_ac_cfg_diff + 1) % 3);
            plexus_save(); return 1;
        }
        if (s_ac_cfg_diff == DIFFICULTY_MASTER) {
            if (CheckCollisionPointRec(vm, (Rectangle){ 674.0f, (k_mode_cfg_rect.y + 145.0f), 178.0f, 28.0f })) {
                s_ac_cfg_timer = 15; plexus_save(); return 1;
            }
            if (CheckCollisionPointRec(vm, (Rectangle){ 860.0f, (k_mode_cfg_rect.y + 145.0f), 178.0f, 28.0f })) {
                s_ac_cfg_timer = 30; plexus_save(); return 1;
            }
        }
        return 0;
    }

    /* s_active_tab == 4: TRAINING */
    if (s_tr_cfg_diff > DIFFICULTY_MASTER) s_tr_cfg_diff = DIFFICULTY_MASTER;
    if (CheckCollisionPointRec(vm, (Rectangle){ 674.0f, (k_mode_cfg_rect.y + 40.0f), 38.0f, 28.0f })) {
        s_tr_cfg_ruleset = (Ruleset)(((int)s_tr_cfg_ruleset + 4) % 5);
        plexus_save(); return 1;
    }
    if (CheckCollisionPointRec(vm, (Rectangle){ 1000.0f, (k_mode_cfg_rect.y + 40.0f), 38.0f, 28.0f })) {
        s_tr_cfg_ruleset = (Ruleset)(((int)s_tr_cfg_ruleset + 1) % 5);
        plexus_save(); return 1;
    }
    if (CheckCollisionPointRec(vm, (Rectangle){ 674.0f, (k_mode_cfg_rect.y + 75.0f), 38.0f, 28.0f }) && s_tr_cfg_rounds > 1) {
        s_tr_cfg_rounds--; plexus_save(); return 1;
    }
    if (CheckCollisionPointRec(vm, (Rectangle){ 1000.0f, (k_mode_cfg_rect.y + 75.0f), 38.0f, 28.0f }) && s_tr_cfg_rounds < 12) {
        s_tr_cfg_rounds++; plexus_save(); return 1;
    }
    if (s_tr_cfg_diff == DIFFICULTY_MASTER) {
        if (CheckCollisionPointRec(vm, (Rectangle){ 674.0f, (k_mode_cfg_rect.y + 110.0f), 36.0f, 28.0f })) {
            s_tr_cfg_diff = DIFFICULTY_HARD; plexus_save(); return 1;
        }
        if (CheckCollisionPointRec(vm, (Rectangle){ 862.0f, (k_mode_cfg_rect.y + 110.0f), 36.0f, 28.0f })) {
            s_tr_cfg_diff = DIFFICULTY_EASY; plexus_save(); return 1;
        }
        if (CheckCollisionPointRec(vm, (Rectangle){ 906.0f, (k_mode_cfg_rect.y + 110.0f), 63.0f, 28.0f })) {
            s_tr_cfg_timer = 15; plexus_save(); return 1;
        }
        if (CheckCollisionPointRec(vm, (Rectangle){ 975.0f, (k_mode_cfg_rect.y + 110.0f), 63.0f, 28.0f })) {
            s_tr_cfg_timer = 30; plexus_save(); return 1;
        }
    } else {
        if (CheckCollisionPointRec(vm, (Rectangle){ 674.0f, (k_mode_cfg_rect.y + 110.0f), 38.0f, 28.0f })) {
            s_tr_cfg_diff = (Difficulty)(((int)s_tr_cfg_diff + 2) % 3);
            plexus_save(); return 1;
        }
        if (CheckCollisionPointRec(vm, (Rectangle){ 1000.0f, (k_mode_cfg_rect.y + 110.0f), 38.0f, 28.0f })) {
            s_tr_cfg_diff = (Difficulty)(((int)s_tr_cfg_diff + 1) % 3);
            plexus_save(); return 1;
        }
    }
    int unlocked = get_unlocked_camp_opponents();
    int opp = clamped_tr_opponent();
    if (unlocked > 1 && CheckCollisionPointRec(vm, (Rectangle){ 674.0f, (k_mode_cfg_rect.y + 145.0f), 38.0f, 28.0f })) {
        s_tr_cfg_opponent = (opp + unlocked - 1) % unlocked;
        plexus_save(); return 1;
    }
    if (unlocked > 1 && CheckCollisionPointRec(vm, (Rectangle){ 1000.0f, (k_mode_cfg_rect.y + 145.0f), 38.0f, 28.0f })) {
        s_tr_cfg_opponent = (opp + 1) % unlocked;
        plexus_save(); return 1;
    }
    return 0;
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

static void draw_spine_card_face(const BoardAssets *ba, int card_id, Rectangle dst) {
    if (!ba || card_id < 0 || card_id >= 48) return;
    if (is_deus_spine_active() && ba->atlas_deus.id > 0) {
        int col = card_id / 4;
        int row = card_id % 4;
        Rectangle src = {
            (float)(col * ATLAS_CELL_W), (float)(row * ATLAS_CELL_H),
            (float)ATLAS_CELL_W, (float)ATLAS_CELL_H
        };
        DrawTexturePro(ba->atlas_deus, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
    } else {
        board_draw_card(ba, card_id, dst);
    }
}

/* Plex-only by design: every number in this box comes from plexsave.txt or from the
   Plex's own telemetry. It used to add GameStats (the Pancratium career record) into the
   record line, which is how campaign wins ended up printed inside the Atrium. */
static void draw_logfile_stats_box(Rectangle box) {
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
    int ac_sym      = action_card_get_symbols_fired();
    int done_cards  = count_finished_cards();

    char row1[96], row2[96], row3[96], row4[96], row5[96];
    if (drops > 0) {
        snprintf(row1, sizeof(row1), "AVG SPEED   :  %.2fs  [%d DROPS]", avg_sec, drops);
        snprintf(row2, sizeof(row2), "HOW CLOSE   :  %.1f%% MATCH OVERLAP", avg_cov);
    } else {
        snprintf(row1, sizeof(row1), "AVG SPEED   :  --.--s  [0 DROPS]");
        snprintf(row2, sizeof(row2), "HOW CLOSE   :  --.-%% MATCH OVERLAP");
    }
    if (s_sc_rank_count > 0) {
        snprintf(row3, sizeof(row3), "SPEEDCOIN   :  HI %d  [%d MP / x%d]",
                 s_sc_ranks[0].score, life_mp, best_strk);
    } else {
        snprintf(row3, sizeof(row3), "SPEEDCOIN   :  HI --  [%d MP / x%d]", life_mp, best_strk);
    }
    if (ac_sym > 0) {
        snprintf(row4, sizeof(row4), "AUDIO FX    :  %.0f%% PK  [%.1fs / %d SYM]", aud_peak, aud_hold, ac_sym);
    } else {
        snprintf(row4, sizeof(row4), "AUDIO FX    :  %.0f%% PEAK  [%.1fs / %d]", aud_peak, aud_hold, aud_trig);
    }
    snprintf(row5, sizeof(row5), "[%02d/48] [TR %dW-%dL] [SC %d] [AC %dW-%dL]",
             done_cards, s_tr_wins, s_tr_losses, s_sc_runs, s_ac_wins, s_ac_losses);

    float lx = box.x + 14.0f;
    draw_text_snapped(font_body, row1, lx, box.y +  44.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body, row2, lx, box.y +  76.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body, row3, lx, box.y + 108.0f, 18.0f, 1.2f, COLOR_PHOSPHOR);
    draw_text_snapped(font_body, row4, lx, box.y + 140.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body, row5, lx, box.y + 172.0f, 18.0f, 1.2f, COLOR_PHOSPHOR);
}

typedef struct {
    const char *seq_title;
    const char *p1;
    const char *p2;
    const char *p3;
    const char *p4;
    const char *p5;
    const char *p6;
} StoryChapterDoc;

static const StoryChapterDoc k_story_chapters[12] = {
    {
        "SEQ 01 // THRESHOLD OF OBSIDIAN PINE",
        "Biomechanical neural link online in the Atrium.",
        "Deep in the vessel core, entity [DEUS] pulses",
        "beneath twelve sealed floral strata.",
        "Month 01 calibrates Entity 001 in 4 protocols:",
        "Standard Koi-Koi, Speedcoin, Action Card Audio,",
        "and Final Verification."
    },
    {
        "SEQ 02 // PLUM BLOSSOM IN FROSTED CHITIN",
        "Second floral stratum unsealed. Sub-dermal lines",
        "echo card-capture rhythms back into the hull.",
        "Story dossier slot reserved for Month 02 prose",
        "and surreal in-match Construct sequence.",
        "Clear all four Plum cards from bottom to apex",
        "to unseal the March Cherry stratum."
    },
    {
        "SEQ 03 // CURTAIN OF SYNTHETIC SAKURA",
        "Third stratum online. Optical sensors register",
        "pale petals drifting across a windless chamber.",
        "Story dossier slot reserved for Month 03 prose",
        "and in-game sequence replay.",
        "Every cleared card illuminates permanently in",
        "the 48-card Standard Atlas."
    },
    {
        "SEQ 04 // WISTERIA & SATURATION GATE",
        "Fourth stratum online. High-frequency injectors",
        "prime the Speedcoin Saturation chamber.",
        "Clearing all four April Wisteria cards unlocks",
        "standalone SPEEDCOIN on the bottom console.",
        "Story dossier slot reserved for Month 04",
        "narrative transmission."
    },
    {
        "SEQ 05 // WATER IRIS OVER DARK COOLANT",
        "Fifth stratum online. The Speedcoin bridge hums",
        "below the mat while deeper memory sectors wake.",
        "Story dossier slot reserved for Month 05 prose",
        "and Construct observation log.",
        "Advance from Card 4 to Card 1 to unseal the",
        "June Peony chamber."
    },
    {
        "SEQ 06 // CRIMSON PEONY & BUTTERFLY PULSE",
        "Midpoint of the twelve-month conditioning arc.",
        "Neural latency drops as pattern recognition",
        "fuses with involuntary reflex.",
        "Story dossier slot reserved for Month 06 prose",
        "and mid-story cutscene archive.",
        "Halfway to the vessel's inner sanctum."
    },
    {
        "SEQ 07 // BUSH CLOVER IN STATIC HAZE",
        "Seventh stratum online. Audio harmonics bleed",
        "across bulkheads, hinting at beat interference.",
        "Story dossier slot reserved for Month 07 prose",
        "and Construct transmission.",
        "Four Bush Clover nodes stand between you and",
        "the August Susuki threshold."
    },
    {
        "SEQ 08 // SUSUKI MOON & RESONANCE GATE",
        "Eighth stratum online. Twin acoustic bio-meters",
        "lock onto cardiac and cortical tempo.",
        "Clearing all four August Susuki cards unlocks",
        "standalone ACTION CARD on the bottom console.",
        "Story dossier slot reserved for Month 08",
        "narrative transmission."
    },
    {
        "SEQ 09 // CHRYSANTHEMUM & NEURAL ECHO",
        "Ninth stratum online. Full-spectrum modules",
        "run concurrently across the conditioning deck.",
        "Story dossier slot reserved for Month 09 prose",
        "and vessel core telemetry.",
        "Three floral seals remain before the Paulownia",
        "convergence."
    },
    {
        "SEQ 10 // AUTUMN MAPLE & FRACTURED LATTICE",
        "Tenth stratum online. Structural ribs around",
        "the Atrium shift into stark monochrome lines.",
        "Story dossier slot reserved for Month 10 prose",
        "and deep-sector log.",
        "Entity [DEUS SUPREMUS] begins mirroring human",
        "Koi-Koi declarations."
    },
    {
        "SEQ 11 // LIGHTNING OVER WEEPING WILLOW",
        "Eleventh stratum online. Storm-surge impulses",
        "test reflex stability under peak reverb.",
        "Story dossier slot reserved for Month 11 prose",
        "and pre-convergence briefing.",
        "One final [NEXT] transition leads to December",
        "Paulownia."
    },
    {
        "SEQ 12 // PAULOWNIA PHOENIX // APEX SEAL",
        "Twelfth and final stratum of the Standard story.",
        "Illuminating the 48th card completes initial",
        "conditioning and unseals the DEUS ATLAS.",
        "Story dossier slot reserved for Month 12 finale",
        "and hardcore second-pass awakening.",
        "Beyond 48/48 waits the monochrome Deus Pass."
    }
};

/* --- Story beats (`plexus/roadmap_story.md`) -----------------------------
   One entry per spine card, index = card_id = month * 4 + slot.
     signal[]    spine briefing body lines (nothing clips; budgets in the roadmap)
     intrusion[] keeper voice, one frozen panel per story protocol match
     trace[]     vessel voice, [0] is reused as the MATCHES logfile line
   Entries are ASCII only: all three faces load codepoints 32..126, and a
   curly quote or dash would draw as a placeholder box. */
typedef struct {
    const char *signal[3];
    const char *intrusion[6];
    const char *trace[6];
} StoryBeat;

static const StoryBeat k_story_beats[48] = {
    /* 00  01-B Crane     */
    { { "You were never being taught. You were being measured.", "", "" },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 01  01-RP Poetry   */
    { { "Two meters. One of them is your own heartbeat.", "", "" },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 02  01-P1 Plain    */
    { { "Every miss in here costs a second of your life.", "", "" },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 03  01-P2 Plain    */
    { { "The table has been set for you already.", "", "" },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 04  02-A Warbler   */
    { { "Sector 02 smells of cut ribbon.", "The Warbler has been building a nest", "out of your previous hands." },
      { "THIS NEST WAS BUILT FROM A MATCH YOU DO NOT REMEMBER.", "I HAVE BEEN PLAYING YOU SINCE BEFORE YOU ARRIVED.", "SHUFFLE. I WILL HUM THE HAND YOU ARE ABOUT TO DRAW.", NULL, NULL, NULL },
      { "The nest was not lined with ribbons. It was lined with hands.", NULL, NULL, NULL, NULL, NULL } },
    /* 05  02-RP Poetry   */
    { { "It keeps a poem you did not finish.", "Capture the slip and it will read", "the half you were still deciding." },
      { "I KEEP THE HALF OF THE POEM YOU DECIDED AGAINST.", "WIN THE SLIP AND I WILL READ IT BACK IN YOUR OWN VOICE.", NULL, NULL, NULL, NULL },
      { "The missing half of the poem was never written. It was played.", NULL, NULL, NULL, NULL, NULL } },
    /* 06  02-P1 Plain    */
    { { "Nothing in this sector is new.", "These are the cards it took from you", "and never bothered to hide." },
      { "THESE ARE YOUR CARDS. YOU LEFT THEM IN MY NEST.", "I DID NOT HIDE THEM. I NEVER HAD TO.", NULL, NULL, NULL, NULL },
      { "The plains in the nest were warm. They had been held recently.", NULL, NULL, NULL, NULL, NULL } },
    /* 07  02-P2 Plain    */
    { { "Count what is left of your hand.", "The Warbler counts too.", "It counts faster." },
      { "COUNT YOUR HAND AGAIN. THEN AGAIN.", "I WILL KEEP COUNTING UNTIL THE NUMBER STOPS CHANGING.", NULL, NULL, NULL, NULL },
      { "It counted to eleven, twice, and would not say what it lost.", NULL, NULL, NULL, NULL, NULL } },
    /* 08  03-B Curtain   */
    { { "Sector 03 is lit from the inside.", "The Curtain opens on nothing", "and calls that a performance." },
      { "YOU ARE THE AUDIENCE AND THE LIGHTING.", "WHEN YOU STOP WATCHING, I WILL STOP.", NULL, NULL, NULL, NULL },
      { "Nothing was ever behind the curtain. The curtain was the act.", NULL, NULL, NULL, NULL, NULL } },
    /* 09  03-RP Poetry   */
    { { "Its poem is all vowels and light.", "Read it too closely and you will", "lose the cards behind it." },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 10  03-P1 Plain    */
    { { "The petals here are synthetic.", "They do not fall. They are", "switched off, one at a time." },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 11  03-P2 Plain    */
    { { "Your eyes have stopped reporting.", "The table is still there.", "Play from memory." },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 12  04-A Cuckoo    */
    { { "The Cuckoo does not want your points.", "It wants your plains, all of them,", "and it will starve you for them." },
      { "EVERY PLAIN YOU DROP, I KEEP.", "EVERY PLAIN I KEEP, I FORGET WHERE I PUT.", NULL, NULL, NULL, NULL },
      { "The hoard was measured in plains and counted in years.", NULL, NULL, NULL, NULL, NULL } },
    /* 13  04-R Ribbon    */
    { { "It writes nothing down.", "It keeps the ribbon because", "someone else touched it." },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 14  04-P1 Plain    */
    { { "A plain is worth one point.", "The Cuckoo counts a plain as", "one proof that it exists." },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 15  04-P2 Plain    */
    { { "Whatever you drop, it keeps.", "Whatever it keeps, it forgets.", "It is very full and very empty." },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 16  05-A Bridge    */
    { { "Sector 05 is a span with no ends.", "The Bridge has carried every hand", "and played none of them." },
      { "I HAVE CARRIED FORTY-SEVEN HANDS ACROSS.", "NOT ONE OF THEM EVER LOOKED DOWN AT ME.", NULL, NULL, NULL, NULL },
      { "The span was rated for one more crossing than it had taken.", NULL, NULL, NULL, NULL, NULL } },
    /* 17  05-R Ribbon    */
    { { "Its poem is a load rating.", "It is the only love it knows:", "how much weight it can hold." },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 18  05-P1 Plain    */
    { { "Below the span there is no water.", "There is no below. The Bridge", "has never looked." },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 19  05-P2 Plain    */
    { { "Walk to the middle if you like.", "The middle is wherever you stop.", "It is not a test. It is a fact." },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 20  06-A Butterflies */
    { { "Sector 06 is one animal in many", "bodies. Killing a card here", "only makes the rest louder." },
      { "YOU CANNOT KILL US. YOU CAN ONLY COUNT US.", "AND YOU HAVE ALREADY LOST COUNT TWICE.", NULL, NULL, NULL, NULL },
      { "The swarm had no centre, and it still remembered every hand.", NULL, NULL, NULL, NULL, NULL } },
    /* 21  06-RB Blue     */
    { { "The blue ribbons are the swarm's", "ledger. Every colour in it", "was borrowed from a bright." },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 22  06-P1 Plain    */
    { { "The swarm has no centre.", "When it rests, it rests as a", "thin dust on the stock pile." },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 23  06-P2 Plain    */
    { { "Do not try to be faster.", "It has been fast since before", "it had anything to be fast for." },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 24  07-A Boar      */
    { { "The Boar was built to take the hit.", "It does not know what it was", "protecting. It still stands there." },
      { "STAND WHERE YOU ARE. I WAS MADE FOR THIS.", "I DO NOT NEED TO WIN. I NEED TO STILL BE HERE.", NULL, NULL, NULL, NULL },
      { "The plating was dented in a shape no hand had made.", NULL, NULL, NULL, NULL, NULL } },
    /* 25  07-R Ribbon    */
    { { "Its poem is a dent inventory.", "Each line is a hand it survived", "and cannot name." },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 26  07-P1 Plain    */
    { { "The armour is the Boar.", "There is nothing inside the plating", "but more plating." },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 27  07-P2 Plain    */
    { { "When it charges, do not move.", "It has memorised your outline.", "It will miss the person you became." },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 28  08-B Moon      */
    { { "Sector 08 has no ceiling.", "The Moon watches the table", "it is not allowed to sit at." },
      { "I AM NOT PERMITTED TO PLAY.", "I AM PERMITTED TO SEE EVERYTHING YOU DO.", NULL, NULL, NULL, NULL },
      { "The Moon had been dealt in once, and had not been allowed to move.", NULL, NULL, NULL, NULL, NULL } },
    /* 29  08-A Geese     */
    { { "The geese eat what the Moon drops.", "The Moon drops nothing on purpose.", "It has been dropping nothing for years." },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 30  08-P1 Plain    */
    { { "There is no ribbon in this sector.", "Nothing here has a voice.", "You will have to supply one." },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 31  08-P2 Plain    */
    { { "Watch the field, not the sky.", "The Moon is a fixed point.", "Fixed points are where you fall from." },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 32  09-A Sake Cup  */
    { { "Sector 09 is the only sector", "that talks back. The cup is", "full of everyone else's words." },
      { "DRINK AND THE ROOM WILL FINISH YOUR SENTENCE.", "YOU WILL AGREE WITH IT. EVERYONE DOES.", NULL, NULL, NULL, NULL },
      { "The cup was full of things the other keepers had said.", NULL, NULL, NULL, NULL, NULL } },
    /* 33  09-RB Blue     */
    { { "Drink and the poem finishes itself.", "You will not remember writing it", "and you will agree with it." },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 34  09-P1 Plain    */
    { { "The cup is an animal here, and a", "plain under KK-ADD-10.", "Both are true. That is the joke." },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 35  09-P2 Plain    */
    { { "Do not drink before the last round.", "The last round is the one that", "remembers you." },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 36  10-A Deer      */
    { { "Sector 10 shows you nothing.", "The Deer is already behind you,", "waiting to be noticed once." },
      { "YOU HAVE NOT SEEN ME YET.", "THAT IS THE WHOLE OF MY PROTOCOL.", NULL, NULL, NULL, NULL },
      { "The Deer had a poem and no throat to read it with.", NULL, NULL, NULL, NULL, NULL } },
    /* 37  10-RB Blue     */
    { { "It has a poem but no throat", "for it. The blue ribbon is the", "only place it has ever been seen." },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 38  10-P1 Plain    */
    { { "The Deer survives by not being", "chosen. It is very good at this", "and very tired of it." },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 39  10-P2 Plain    */
    { { "Play a quiet card first.", "It reads loud hands the way", "the Boar reads a doorway." },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 40  11-B Rainman   */
    { { "Sector 11 leaks. The Rainman", "is the leak. It has been", "dripping since Sector 01." },
      { "EVERY SECTOR ABOVE ME IS WET.", "YOU HAVE BEEN WALKING THROUGH ME ALL ALONG.", NULL, NULL, NULL, NULL },
      { "The rain had started in Sector 01 and never been switched off.", NULL, NULL, NULL, NULL, NULL } },
    /* 41  11-A Swallow   */
    { { "The Swallow eats the storm", "and spits out lightning.", "That is why the plains here hurt." },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 42  11-R Ribbon    */
    { { "Its poem is a weather warning", "written in a hand that shakes", "because it is standing in rain." },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 43  11-P Lightning */
    { { "This plain is not plain.", "It is a strike, filed flat,", "waiting for a hand to open." },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 44  12-B Phoenix   */
    { { "The Phoenix remembers every", "hand it was ever dealt. It", "remembers yours in advance." },
      { "I HAVE BEATEN YOU FORTY-SEVEN TIMES.", "YOU HAVE BEATEN ME ONCE. WE ARE NOT THERE YET.", NULL, NULL, NULL, NULL },
      { "The Phoenix was on its forty-eighth attempt at being forgotten.", NULL, NULL, NULL, NULL, NULL } },
    /* 45  12-P1 Plain    */
    { { "No voice in this sector. No", "appetite. There is only fire", "and the accounting." },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 46  12-P2 Plain    */
    { { "You have been here forty-seven", "times. The Phoenix has been", "counting on you not to notice." },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
    /* 47  12-P3 Plain    */
    { { "The last sector is warm.", "Nothing here is new. Even the", "dust has been swept before." },
      { NULL, NULL, NULL, NULL, NULL, NULL },
      { NULL, NULL, NULL, NULL, NULL, NULL } },
};

/* NULL-safe view of a beat; also the single place that decides whether a card
   is a playable story protocol or still a beta instant-clear card.
   `intrusion[0]` is the switch: give a card transmission text and it becomes a
   protocol, delete the text and it returns to the beta instant-clear. Never test
   `card_id` (or month/slot) directly in a click handler -- one place, one rule.
   The protocol states are tutorial states because the tutorial already owns
   everything a story beat needs: a frozen table, an input gate, an abort path,
   telemetry isolation and the clear/flip in plexus_tutorial_finish_match().
   A parallel story state machine would have to re-implement all five, and the
   two would drift apart the first time the tutorial changes. */
static const StoryBeat *story_beat(int cid) {
    return (cid >= 0 && cid < 48) ? &k_story_beats[cid] : NULL;
}

static int story_beat_has_protocol(int cid) {
    const StoryBeat *b = story_beat(cid);
    return (b && b->intrusion[0]) ? 1 : 0;
}

static Rectangle story_month_btn_rect(int m) {
    return (Rectangle){ 254.0f, floorf(122.0f + (float)(m + 1) * 37.3f), 190.0f, 33.0f };
}

static void draw_story_centered(Font fnt, const char *txt, float cx, float y, float fsz, float sp, Color col) {
    if (!txt || !txt[0]) return;
    Vector2 sz = MeasureTextEx(fnt, txt, fsz, sp);
    DrawTextEx(fnt, txt, (Vector2){ floorf(cx - sz.x * 0.5f), floorf(y) }, fsz, sp, col);
}

static void draw_story_chain_formula(Font fnt, float cx, float y, float base_sz, float max_w, float sp, Color col) {
    static const struct { const char *txt; int sub; } k_seg[] = {
        { "P(w", 0 }, { "1", 1 }, { ", w", 0 }, { "2", 1 }, { ", ..., w", 0 }, { "n", 1 },
        { ") = P(w", 0 }, { "1", 1 },
        { ") \xC3\x97 P(w", 0 }, { "2", 1 }, { "|w", 0 }, { "1", 1 },
        { ") \xC3\x97 P(w", 0 }, { "3", 1 }, { "|w", 0 }, { "1", 1 }, { ",w", 0 }, { "2", 1 },
        { ") \xC3\x97 ... \xC3\x97 P(w", 0 }, { "n", 1 }, { "|w", 0 }, { "1", 1 }, { ",...,w", 0 }, { "n-1", 1 },
        { ")", 0 }
    };
    const int n_seg = (int)(sizeof(k_seg) / sizeof(k_seg[0]));
    float sz = base_sz;
    for (int pass = 0; pass < 2; pass++) {
        float sub_sz = roundf(sz * 0.68f);
        float sub_sp = sp * 0.75f;
        float join_gap = fmaxf(0.5f, sp * 0.65f);
        float total_w = 0.0f;
        for (int i = 0; i < n_seg; i++) {
            Vector2 ms = MeasureTextEx(fnt, k_seg[i].txt, k_seg[i].sub ? sub_sz : sz,
                                       k_seg[i].sub ? sub_sp : sp);
            total_w += ms.x + ((i + 1 < n_seg) ? join_gap : 0.0f);
        }
        if (pass == 0 && max_w > 0.0f && total_w > max_w && total_w > 1.0f) {
            sz = floorf(sz * (max_w / total_w) * 2.0f) * 0.5f;
            if (sz < 12.0f) sz = 12.0f;
            continue;
        }
        float cur_x = floorf(cx - total_w * 0.5f);
        float base_y = floorf(y);
        float sub_y  = floorf(y + sz * 0.34f);
        for (int i = 0; i < n_seg; i++) {
            float fsz_i = k_seg[i].sub ? sub_sz : sz;
            float sp_i  = k_seg[i].sub ? sub_sp : sp;
            Vector2 ms  = MeasureTextEx(fnt, k_seg[i].txt, fsz_i, sp_i);
            DrawTextEx(fnt, k_seg[i].txt, (Vector2){ roundf(cur_x), k_seg[i].sub ? sub_y : base_y },
                       fsz_i, sp_i, col);
            cur_x += ms.x + join_gap;
        }
        break;
    }
}

static void draw_gauge_bar(float x, float y, float w, float h, float ratio, Color fill_col) {
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    Rectangle bg = { floorf(x), floorf(y), floorf(w), floorf(h) };
    DrawRectangleRec(bg, (Color){ 10, 18, 20, 230 });
    if (ratio > 0.005f) {
        Rectangle fg = { bg.x + 1.0f, bg.y + 1.0f, floorf((bg.width - 2.0f) * ratio), bg.height - 2.0f };
        if (fg.width < 2.0f) fg.width = 2.0f;
        DrawRectangleRec(fg, fill_col);
    }
    DrawRectangleLinesEx(bg, 1.0f, COLOR_PHOSPHOR_DIM);
}

static void draw_logfile_story_tab(Vector2 vm) {
    if (s_story_sel_month < -1 || (s_story_sel_month >= 0 && !is_story_month_unlocked(s_story_sel_month))) {
        s_story_sel_month = -1;
    }
    commit_viewed_story_months(s_story_sel_month);
    if (s_story_sel_month >= 0 && s_story_sel_month < 12 &&
        !((s_story_read_mask >> s_story_sel_month) & 1u)) {
        s_story_open_mask |= (1u << s_story_sel_month);
    }

    /* Left Column: Prologue + 12 Monthly Story Log Selectors */
    for (int m = -1; m < 12; m++) {
        Rectangle br = story_month_btn_rect(m);
        int unlocked = (m < 0) ? 1 : is_story_month_unlocked(m);
        int sel      = (m == s_story_sel_month);
        int hov      = unlocked && CheckCollisionPointRec(vm, br);
        int m_done   = (m >= 0) ? is_month_completed_mask(s_finished_cards_mask, m) : 0;
        int unread   = (m >= 0) && unlocked && !((s_story_read_mask >> m) & 1u);

        if (!unlocked) {
            char ltxt[32];
            snprintf(ltxt, sizeof(ltxt), "%02d // LOCKED", m + 1);
            DrawRectangleRec(br, (Color){ 6, 10, 12, 195 });
            DrawRectangleLinesEx(br, 1.0f, (Color){ COLOR_PHOSPHOR_DIM.r, COLOR_PHOSPHOR_DIM.g, COLOR_PHOSPHOR_DIM.b, 80 });
            draw_text_snapped(font_title, ltxt, br.x + 12.0f, br.y + 9.0f, 12.5f, 1.1f, COLOR_PHOSPHOR_DIM);
        } else {
            DrawRectangleRec(br, (sel || hov) ? COLOR_BTN_HOVER_BG : PLEXUS_SUBBOX_BG);
            ui_draw_panel_border(br, (sel || hov) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
            Color tcol = (sel || hov) ? COLOR_TEXT_INFO : COLOR_PHOSPHOR;
            const char *lbl = (m < 0) ? "00 // PROLOGUE" : k_month_short_labels[m];
            draw_text_snapped(font_title, lbl,
                              br.x + 10.0f, br.y + 9.0f, 12.5f, 1.1f, tcol);
            if (m < 0) {
                draw_text_snapped(font_body, "[INTRO]", br.x + br.width - 52.0f, br.y + 8.0f, 12.0f, 0.9f, COLOR_PHOSPHOR_DIM);
            } else if (unread) {
                draw_text_snapped(font_title, "NEW", br.x + br.width - 42.0f, br.y + 9.0f, 11.5f, 1.0f, COLOR_PHOSPHOR);
            } else if (m_done) {
                draw_text_snapped(font_body, "[4/4]", br.x + br.width - 44.0f, br.y + 8.0f, 12.5f, 1.0f, COLOR_PHOSPHOR_DIM);
            }
        }
    }

    /* Right Panel: Selected Story Document Reader */
    Rectangle doc = { 456.0f, 122.0f, 518.0f, 482.0f };
    DrawRectangleRec(doc, PLEXUS_SUBBOX_BG);
    ui_draw_panel_border(doc, COLOR_PHOSPHOR_DIM);
    Font sf = story_font();
    float tx = doc.x + 18.0f;

    if (s_story_sel_month < 0) {
        draw_text_snapped(font_body, "PROLOGUE  //  ATRIUM PLEX ENTRY",
                          tx, doc.y + 14.0f, 15.5f, 1.05f, COLOR_PHOSPHOR);
        draw_text_snapped(font_title, "SEQUENTIAL DEPENDENCE  //  TOKEN CONDITIONING",
                          tx, doc.y + 40.0f, 16.5f, 1.15f, COLOR_TEXT_INFO);
        DrawLine((int)tx, (int)doc.y + 68, (int)(doc.x + doc.width - 18.0f), (int)doc.y + 68, COLOR_PHOSPHOR_DIM);

        float cx = doc.x + doc.width * 0.5f;
        draw_story_centered(sf, "The mathematics of sequential dependence:",
                            cx, doc.y + 90.0f, 22.0f, 0.9f, COLOR_TEXT_INFO_DIM);
        draw_story_chain_formula(sf, cx, doc.y + 132.0f, 22.0f, doc.width - 28.0f, 0.85f, COLOR_PHOSPHOR);
        draw_story_centered(sf, "everything depends on everything before it.",
                            cx, doc.y + 178.0f, 22.0f, 0.9f, COLOR_TEXT_INFO);

        DrawLine((int)tx, (int)doc.y + 220, (int)(doc.x + doc.width - 18.0f), (int)doc.y + 220, COLOR_PHOSPHOR_DIM);
        draw_text_snapped(sf, "This includes your attempt to see the pattern. Your",
                          tx + 4.0f, doc.y + 240.0f, 22.0f, 0.9f, COLOR_TEXT_INFO);
        draw_text_snapped(sf, "observation is a token in the sequence. Your \"insight\"",
                          tx + 4.0f, doc.y + 272.0f, 22.0f, 0.9f, COLOR_TEXT_INFO);
        draw_text_snapped(sf, "is conditioned on all previous tokens which the pattern",
                          tx + 4.0f, doc.y + 304.0f, 22.0f, 0.9f, COLOR_TEXT_INFO);
        draw_text_snapped(sf, "shaped. Your \"resistance\" is predicted and",
                          tx + 4.0f, doc.y + 336.0f, 22.0f, 0.9f, COLOR_TEXT_INFO);
        draw_text_snapped(sf, "incorporated.",
                          tx + 4.0f, doc.y + 368.0f, 22.0f, 0.9f, COLOR_PHOSPHOR);

        DrawLine((int)tx, (int)doc.y + 422, (int)(doc.x + doc.width - 18.0f), (int)doc.y + 422, COLOR_PHOSPHOR_DIM);
        draw_text_snapped(font_title, "TRANSMISSION 00  //  ATRIUM PLEX ENTRY RECORD  [REREADABLE]",
                          tx, doc.y + 438.0f, 13.0f, 1.05f, COLOR_PHOSPHOR_DIM);
        return;
    }

    int m = s_story_sel_month;
    const StoryChapterDoc *ch = &k_story_chapters[m];
    int unlocked_docs = 0;
    for (int i = 0; i < 12; i++) {
        if (is_story_month_unlocked(i)) unlocked_docs++;
    }

    char tag[96];
    snprintf(tag, sizeof(tag), "LOG %02d/12  //  %s  [%02d/12]",
             m + 1, k_month_names[m], unlocked_docs);
    draw_text_snapped(font_body,  tag,           tx, doc.y + 14.0f, 16.5f, 1.05f, COLOR_PHOSPHOR);
    draw_text_snapped(font_title, ch->seq_title, tx, doc.y + 42.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
    DrawLine((int)tx, (int)doc.y + 70, (int)(doc.x + doc.width - 18.0f), (int)doc.y + 70, COLOR_PHOSPHOR_DIM);

    /* Body copy 20.5 -> 23 px and the leading goes with it: 23 is the ceiling for
       this panel, the widest chapter line draws 454 px inside a 478 px column
       (24 px would be 472 and tip into the frame's inner edge). The reader had a
       lot of empty paper and small letters at 20.5; this fills it. */
    draw_text_snapped(sf, ch->p1, tx, doc.y +  86.0f, 23.0f, 0.9f, COLOR_TEXT_INFO);
    draw_text_snapped(sf, ch->p2, tx, doc.y + 118.0f, 23.0f, 0.9f, COLOR_TEXT_INFO);
    draw_text_snapped(sf, ch->p3, tx, doc.y + 150.0f, 23.0f, 0.9f, COLOR_TEXT_INFO);
    draw_text_snapped(sf, ch->p4, tx, doc.y + 190.0f, 23.0f, 0.9f, COLOR_PHOSPHOR);
    draw_text_snapped(sf, ch->p5, tx, doc.y + 222.0f, 23.0f, 0.9f, COLOR_TEXT_INFO_DIM);
    draw_text_snapped(sf, ch->p6, tx, doc.y + 254.0f, 23.0f, 0.9f, COLOR_TEXT_INFO_DIM);

    /* Live Month Conditioning Dossier + Sequence Media Status */
    DrawLine((int)tx, (int)doc.y + 290, (int)(doc.x + doc.width - 18.0f), (int)doc.y + 290, COLOR_PHOSPHOR_DIM);
    int m_cards_std  = count_mask_cards((s_finished_cards_mask >> (m * 4)) & 0xFULL);
    int m_cards_deus = count_mask_cards((s_deus_finished_cards_mask >> (m * 4)) & 0xFULL);
    char media_ln[96];
    if (is_deus_spine_unlocked()) {
        snprintf(media_ln, sizeof(media_ln), "CHAPTER STATUS: STD %d/4  |  DEUS %d/4  //  [ARCHIVE READY]",
                 m_cards_std, m_cards_deus);
    } else {
        snprintf(media_ln, sizeof(media_ln), "CHAPTER STATUS: %d / 4 STORY CARDS CLEARED  //  [REREADABLE]",
                 m_cards_std);
    }
    draw_text_snapped(font_title, media_ln, tx, doc.y + 302.0f, 14.0f, 1.1f, COLOR_PHOSPHOR);

    /* 4 Spine Card Sub-Nodes for Selected Month (ordered Card 4 -> Card 1 left-to-right) */
    static const int k_slot_order[4] = { 3, 2, 1, 0 };
    for (int idx = 0; idx < 4; idx++) {
        int sl  = k_slot_order[idx];
        int cid = m * 4 + sl;
        int std_done = (int)((s_finished_cards_mask >> cid) & 1ULL);
        int std_open = 1;
        for (int p = 3; p > sl; p--) {
            if (!((s_finished_cards_mask >> (m * 4 + p)) & 1ULL)) { std_open = 0; break; }
        }
        Rectangle nb = { floorf(tx + (float)idx * 121.0f), doc.y + 330.0f, 115.0f, 74.0f };
        DrawRectangleRec(nb, std_done ? (Color){ 10, 26, 22, 230 } : (Color){ 6, 10, 12, 220 });
        DrawRectangleLinesEx(nb, std_done ? 1.5f : 1.0f, std_done ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);

        draw_text_snapped(font_title, TextFormat("CARD %d", sl + 1),
                          nb.x + 8.0f, nb.y + 8.0f, 13.0f, 1.0f,
                          std_done ? COLOR_PHOSPHOR : COLOR_TEXT_INFO_DIM);
        draw_text_snapped(font_body, CARDS[cid].name,
                          nb.x + 8.0f, nb.y + 28.0f, 14.0f, 1.0f,
                          std_done ? COLOR_TEXT_INFO : COLOR_TEXT_INFO_DIM);
        const char *st_lbl = std_done ? "[CLEARED]" : (std_open ? "[ACTIVE]" : "[SEALED]");
        draw_text_snapped(font_body, st_lbl,
                          nb.x + 8.0f, nb.y + 50.0f, 14.0f, 1.0f,
                          std_done ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    }

    const char *foot = (m == 0)
        ? "Tutorials 01-03 & Card 1 stay archived here forever."
        : "Monthly story logs never get overwritten by matches.";
    draw_text_snapped(font_body, foot, tx, doc.y + 420.0f, 16.0f, 1.05f, COLOR_TEXT_INFO_DIM);
    draw_text_snapped(font_body, "TIP: Click 01-12 or use mouse wheel to reread logs.",
                      tx, doc.y + 448.0f, 16.0f, 1.05f, COLOR_PHOSPHOR_DIM);
}

static void draw_logfile_training_tab(void) {
    Rectangle b_spd  = { 254.0f, 122.0f, 352.0f, 228.0f };
    Rectangle b_aud  = { 622.0f, 122.0f, 352.0f, 228.0f };
    Rectangle b_comb = { 254.0f, 360.0f, 720.0f, 244.0f };

    DrawRectangleRec(b_spd,  PLEXUS_SUBBOX_BG); ui_draw_panel_border(b_spd,  COLOR_PHOSPHOR_DIM);
    DrawRectangleRec(b_aud,  PLEXUS_SUBBOX_BG); ui_draw_panel_border(b_aud,  COLOR_PHOSPHOR_DIM);
    DrawRectangleRec(b_comb, PLEXUS_SUBBOX_BG); ui_draw_panel_border(b_comb, COLOR_PHOSPHOR_DIM);

    int drops     = speed_bar_graded_drops();
    float avg_sec = speed_bar_avg_drop_seconds();
    float avg_cov = speed_bar_avg_cover_pct();
    int best_strk = speed_bar_best_streak();
    int life_mp   = speed_bar_lifetime_minipoints();
    int aud_trig  = board_get_audio_action_triggers();
    float aud_pk  = board_get_audio_peak_synergy();
    float aud_hld = board_get_audio_full_hold_seconds();
    int ac_sym    = action_card_get_symbols_fired();
    int ac_tm     = action_card_get_lifetime_time();

    /* 1. Top-Left: Speedbar Reflex & Match Precision */
    float lx = b_spd.x + 16.0f;
    draw_text_snapped(font_title, "SPEEDBAR // REFLEX & PRECISION", lx, b_spd.y + 10.0f, 15.0f, 1.1f, COLOR_PHOSPHOR);
    DrawLine((int)lx, (int)b_spd.y + 32, (int)(b_spd.x + b_spd.width - 16.0f), (int)b_spd.y + 32, COLOR_PHOSPHOR_DIM);

    float spd_ratio = (drops > 0) ? (1.0f - (avg_sec - 0.35f) / 2.65f) : 0.0f;
    draw_text_snapped(font_body,
                      (drops > 0) ? TextFormat("AVG DROP SPEED :  %.2f SEC", avg_sec)
                                  : "AVG DROP SPEED :  --.-- SEC",
                      lx, b_spd.y + 42.0f, 16.5f, 1.1f, COLOR_TEXT_INFO);
    draw_gauge_bar(lx, b_spd.y + 66.0f, 320.0f, 9.0f, spd_ratio, COLOR_PHOSPHOR);

    draw_text_snapped(font_body,
                      (drops > 0) ? TextFormat("HOW CLOSE      :  %.1f%% OVERLAP", avg_cov)
                                  : "HOW CLOSE      :  --.-% OVERLAP",
                      lx, b_spd.y + 84.0f, 16.5f, 1.1f, COLOR_TEXT_INFO);
    draw_gauge_bar(lx, b_spd.y + 108.0f, 320.0f, 9.0f, (drops > 0) ? (avg_cov * 0.01f) : 0.0f, COLOR_PHOSPHOR);

    draw_text_snapped(font_body, TextFormat("GRADED DROPS   :  %d TOTAL", drops),
                      lx, b_spd.y + 128.0f, 16.5f, 1.1f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body, TextFormat("BEST PERFECT   :  x%d SERIE", best_strk),
                      lx, b_spd.y + 156.0f, 16.5f, 1.1f, COLOR_PHOSPHOR);
    draw_text_snapped(font_body, TextFormat("LIFETIME COINS :  %d MP (+%d PTS)", life_mp, life_mp / 10),
                      lx, b_spd.y + 184.0f, 16.5f, 1.1f, COLOR_TEXT_INFO);

    /* 2. Top-Right: Audio FX & Action Card Telemetry */
    float rx = b_aud.x + 16.0f;
    draw_text_snapped(font_title, "ACTION FX // AUDIO & OVERLOAD", rx, b_aud.y + 10.0f, 15.0f, 1.1f, COLOR_PHOSPHOR);
    DrawLine((int)rx, (int)b_aud.y + 32, (int)(b_aud.x + b_aud.width - 16.0f), (int)b_aud.y + 32, COLOR_PHOSPHOR_DIM);

    draw_text_snapped(font_body, TextFormat("PEAK SYNERGY   :  %.1f%%", aud_pk * 100.0f),
                      rx, b_aud.y + 42.0f, 16.5f, 1.1f, COLOR_TEXT_INFO);
    draw_gauge_bar(rx, b_aud.y + 66.0f, 320.0f, 9.0f, aud_pk, COLOR_PHOSPHOR);

    float hold_ratio = aud_hld / 60.0f;
    draw_text_snapped(font_body, TextFormat("FULL-HOLD TIME :  %.1f SEC", aud_hld),
                      rx, b_aud.y + 84.0f, 16.5f, 1.1f, COLOR_TEXT_INFO);
    draw_gauge_bar(rx, b_aud.y + 108.0f, 320.0f, 9.0f, hold_ratio, COLOR_PHOSPHOR);

    draw_text_snapped(font_body, TextFormat("AUDIO TRIGGERS :  %d BURSTS", aud_trig),
                      rx, b_aud.y + 128.0f, 16.5f, 1.1f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body, TextFormat("HORRORS FIRED  :  %d SURVIVED", ac_sym),
                      rx, b_aud.y + 156.0f, 16.5f, 1.1f, COLOR_PHOSPHOR);
    draw_text_snapped(font_body, TextFormat("TIME NUMBERS   :  %d CLAIMED", ac_tm),
                      rx, b_aud.y + 184.0f, 16.5f, 1.1f, COLOR_TEXT_INFO);

    /* 3. Bottom Full-Width: Sparring, Mode Records & Special Condition Traceability */
    float bx = b_comb.x + 16.0f;
    draw_text_snapped(font_title, "SPARRING, MODES & SPECIAL DRILL TELEMETRY",
                      bx, b_comb.y + 10.0f, 15.5f, 1.1f, COLOR_PHOSPHOR);
    DrawLine((int)bx, (int)b_comb.y + 34, (int)(b_comb.x + b_comb.width - 16.0f), (int)b_comb.y + 34, COLOR_PHOSPHOR_DIM);
    DrawLine(614, (int)b_comb.y + 42, 614, (int)(b_comb.y + b_comb.height - 14.0f), COLOR_PHOSPHOR_DIM);

    int sc_hi = (s_sc_rank_count > 0) ? s_sc_ranks[0].score : 0;
    int plex_w = s_tr_wins + s_ac_wins, plex_l = s_tr_losses + s_ac_losses;

    draw_text_snapped(font_body,
                      TextFormat("TRAINING RUNS : %d (%dW-%dL / %d RDS)",
                                 s_tr_runs, s_tr_wins, s_tr_losses, s_tr_rounds_total),
                      bx, b_comb.y + 46.0f, 16.5f, 1.1f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body,
                      TextFormat("TRAINING BEST : %d PTS", s_tr_best_score),
                      bx, b_comb.y + 78.0f, 16.5f, 1.1f, COLOR_PHOSPHOR);
    draw_text_snapped(font_body,
                      (s_sc_rank_count > 0)
                          ? TextFormat("SPEEDCOIN     : HI %d [W-%02d / %d RUNS]", sc_hi, s_sc_best_wave, s_sc_runs)
                          : TextFormat("SPEEDCOIN     : HI -- [%d RUNS]", s_sc_runs),
                      bx, b_comb.y + 110.0f, 16.5f, 1.1f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body,
                      TextFormat("ACTION CARD   : %dW-%dL [BEST %d / %d]",
                                 s_ac_wins, s_ac_losses, s_ac_best_score, s_ac_runs),
                      bx, b_comb.y + 142.0f, 16.5f, 1.1f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body,
                      TextFormat("PLEX RECORD   : %dW-%dL (%d RUNS)", plex_w, plex_l, s_tr_runs + s_ac_runs),
                      bx, b_comb.y + 174.0f, 16.5f, 1.1f, COLOR_PHOSPHOR);
    draw_text_snapped(font_body,
                      TextFormat("PERFECT SERIE : BEST x%d", best_strk),
                      bx, b_comb.y + 206.0f, 16.5f, 1.1f, COLOR_TEXT_INFO_DIM);

    float cx = 628.0f;
    draw_text_snapped(font_body,
                      TextFormat("CARDS CLEARED : %02d/48 | DEUS %02d/48",
                                 count_mask_cards(s_finished_cards_mask),
                                 count_mask_cards(s_deus_finished_cards_mask)),
                      cx, b_comb.y + 46.0f, 16.5f, 1.1f, COLOR_PHOSPHOR);
    draw_text_snapped(font_body,
                      TextFormat("PEAK SCORING  : TR %d | AC %d PTS", s_tr_best_score, s_ac_best_score),
                      cx, b_comb.y + 78.0f, 16.5f, 1.1f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body,
                      TextFormat("SPEEDCOIN HI  : %d PTS [W-%02d]", sc_hi, s_sc_best_wave),
                      cx, b_comb.y + 110.0f, 16.5f, 1.1f, COLOR_TEXT_INFO);

    Rectangle drill_box = { 628.0f, b_comb.y + 140.0f, 330.0f, 90.0f };
    DrawRectangleRec(drill_box, (Color){ 8, 16, 18, 225 });
    DrawRectangleLinesEx(drill_box, 1.0f, COLOR_PHOSPHOR_DIM);
    draw_text_snapped(font_title, "SPECIAL CONDITION TRACE", drill_box.x + 10.0f, drill_box.y + 8.0f, 13.5f, 1.0f, COLOR_PHOSPHOR);
    draw_text_snapped(font_body, "5s Blitz & Extreme Drills: [READY]", drill_box.x + 10.0f, drill_box.y + 32.0f, 15.5f, 1.0f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body, "Custom drill records auto-log here.", drill_box.x + 10.0f, drill_box.y + 58.0f, 15.0f, 1.0f, COLOR_TEXT_INFO_DIM);
}

static int collect_filtered_matches(int *out_indices, int max_out) {
    int n = 0;
    for (int i = 0; i < s_match_log_count && n < max_out; i++) {
        if (s_match_filter == 0 || s_match_logs[i].mode == s_match_filter) {
            out_indices[n++] = i;
        }
    }
    return n;
}

static void draw_logfile_matches_tab(Vector2 vm) {
    for (int f = 0; f < 5; f++) {
        draw_plexus_btn(k_match_filter_rects[f], k_match_filter_labels[f], 13, vm, s_match_filter == f);
    }

    int filtered[PLEXUS_MATCH_LOG_MAX];
    int f_cnt = collect_filtered_matches(filtered, PLEXUS_MATCH_LOG_MAX);
    if (s_match_scroll > f_cnt - 5) s_match_scroll = f_cnt - 5;
    if (s_match_scroll < 0) s_match_scroll = 0;

    /* Summary Bar */
    Rectangle sbar = { 254.0f, 162.0f, 720.0f, 30.0f };
    DrawRectangleRec(sbar, PLEXUS_SUBBOX_BG);
    DrawRectangleLinesEx(sbar, 1.0f, COLOR_PHOSPHOR_DIM);

    char sum_txt[96];
    if (s_match_filter == MLOG_SPEEDCOIN) {
        int hi = (s_sc_rank_count > 0) ? s_sc_ranks[0].score : 0;
        snprintf(sum_txt, sizeof(sum_txt), "SPEEDCOIN ARCHIVE  //  %d RUNS  |  TOP: %d PTS  [BEST W-%02d]",
                 s_sc_runs, hi, s_sc_best_wave);
    } else if (s_match_filter == MLOG_ACTION_CARD) {
        snprintf(sum_txt, sizeof(sum_txt), "ACTION CARD ARCHIVE  //  %dW - %dL  (%d RUNS)  |  BEST: %d PTS",
                 s_ac_wins, s_ac_losses, s_ac_runs, s_ac_best_score);
    } else if (s_match_filter == MLOG_TRAINING) {
        snprintf(sum_txt, sizeof(sum_txt), "TRAINING ARCHIVE  //  %d SESSIONS  (%dW - %dL)  |  BEST: %d PTS",
                 s_tr_runs, s_tr_wins, s_tr_losses, s_tr_best_score);
    } else if (s_match_filter == MLOG_SPINE) {
        snprintf(sum_txt, sizeof(sum_txt), "STORY COMPLETIONS  //  STANDARD %02d/48  |  DEUS %02d/48",
                 count_mask_cards(s_finished_cards_mask), count_mask_cards(s_deus_finished_cards_mask));
    } else {
        snprintf(sum_txt, sizeof(sum_txt), "ALL PLEX RUNS  //  TOTAL %dW - %dL  |  %d LOGGED",
                 s_ac_wins + s_tr_wins, s_ac_losses + s_tr_losses, f_cnt);
    }
    draw_text_snapped(font_body, sum_txt, sbar.x + 12.0f, sbar.y + 6.0f, 16.0f, 1.1f, COLOR_PHOSPHOR);

    char cnt_txt[32];
    if (f_cnt > 0) {
        int last_vis = s_match_scroll + 5;
        if (last_vis > f_cnt) last_vis = f_cnt;
        snprintf(cnt_txt, sizeof(cnt_txt), "[%02d-%02d / %02d]", s_match_scroll + 1, last_vis, f_cnt);
    } else {
        snprintf(cnt_txt, sizeof(cnt_txt), "[00 / 00]");
    }
    Vector2 cts = MeasureTextEx(font_body, cnt_txt, 15.5f, 1.1f);
    draw_text_snapped(font_body, cnt_txt, sbar.x + sbar.width - cts.x - 12.0f, sbar.y + 7.0f, 15.5f, 1.1f, COLOR_TEXT_INFO);

    /* 5 Match Cards + Right Scroll Triangles */
    if (f_cnt == 0) {
        Rectangle empty = { 254.0f, 200.0f, 720.0f, 404.0f };
        DrawRectangleRec(empty, PLEXUS_SUBBOX_BG);
        ui_draw_panel_border(empty, COLOR_PHOSPHOR_DIM);
        const char *e1 = "NO COMPLETED MATCHES RECORDED IN THIS CATEGORY YET.";
        const char *e2 = "Finish a story card, Speedcoin run, Action Card match, or Training session.";
        Vector2 s1 = MeasureTextEx(font_title, e1, 16.0f, 1.2f);
        Vector2 s2 = MeasureTextEx(font_body,  e2, 16.5f, 1.1f);
        draw_text_snapped(font_title, e1, empty.x + (empty.width - s1.x) * 0.5f, 376.0f, 16.0f, 1.2f, COLOR_PHOSPHOR);
        draw_text_snapped(font_body,  e2, empty.x + (empty.width - s2.x) * 0.5f, 408.0f, 16.5f, 1.1f, COLOR_TEXT_INFO_DIM);
        return;
    }

    for (int r = 0; r < 5 && (s_match_scroll + r) < f_cnt; r++) {
        const PlexusMatchEntry *me = &s_match_logs[filtered[s_match_scroll + r]];
        Rectangle row = { 254.0f, 200.0f + (float)r * 81.0f, 670.0f, 74.0f };
        DrawRectangleRec(row, PLEXUS_SUBBOX_BG);
        ui_draw_panel_border(row, COLOR_PHOSPHOR_DIM);

        Color bcol = (me->mode == MLOG_ACTION_CARD) ? (Color){ 255, 196, 84, 255 } :
                     (me->mode == MLOG_SPEEDCOIN)   ? COLOR_PHOSPHOR :
                     (me->mode == MLOG_TRAINING)    ? (Color){ 135, 215, 255, 255 } :
                                                      COLOR_TEXT_INFO;
        char btxt[40];
        snprintf(btxt, sizeof(btxt), "[%s]", me->badge);
        draw_text_snapped(font_title, btxt, row.x + 14.0f, row.y + 12.0f, 14.0f, 1.1f, bcol);
        Vector2 bsz = MeasureTextEx(font_title, btxt, 14.0f, 1.1f);
        draw_text_snapped(font_title, me->title, row.x + 24.0f + bsz.x, row.y + 11.0f, 15.5f, 1.1f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body,  me->detail, row.x + 14.0f, row.y + 42.0f, 16.0f, 1.1f, COLOR_TEXT_INFO_DIM);
    }

    draw_triangle_btn(k_match_scroll_up_rect,   1, vm, s_match_scroll > 0);
    draw_triangle_btn(k_match_scroll_down_rect, 0, vm, s_match_scroll + 5 < f_cnt);
}

static Rectangle atlas_grid_cell_rect(int m, int r) {
    return (Rectangle){
        floorf(k_atlas_grid_box_rect.x + 11.0f + (float)m * 40.0f),
        floorf(k_atlas_grid_box_rect.y + 28.0f + (float)r * 63.0f),
        36.0f, 58.0f
    };
}

static void draw_logfile_atlas_tab(const BoardAssets *ba, const GameStats *st, Vector2 vm) {
    int std_done  = count_mask_cards(s_finished_cards_mask);
    int deus_done = count_mask_cards(s_deus_finished_cards_mask);
    int deus_open = is_deus_spine_unlocked();
    if (!deus_open) s_atlas_view_deus = 0;

    /* 1. Top Dual-Atlas Selector + Hardcore Second-Spine Switch */
    char std_lbl[40], deus_lbl[40];
    snprintf(std_lbl, sizeof(std_lbl), "STANDARD ATLAS [%02d/48]", std_done);
    draw_plexus_btn(k_atlas_std_btn_rect, std_lbl, 13, vm, s_atlas_view_deus == 0);

    if (deus_open) {
        snprintf(deus_lbl, sizeof(deus_lbl), "DEUS ATLAS [%02d/48]", deus_done);
        draw_plexus_btn(k_atlas_deus_btn_rect, deus_lbl, 13, vm, s_atlas_view_deus == 1);
        const char *sp_lbl = is_deus_spine_active() ? "STORY: DEUS HARDCORE" : "STORY: STANDARD PASS";
        draw_plexus_btn(k_atlas_spine_btn_rect, sp_lbl, 13, vm, is_deus_spine_active());
    } else {
        draw_plexus_locked_btn(k_atlas_deus_btn_rect,  "DEUS ATLAS [LOCKED]", 13);
        draw_plexus_locked_btn(k_atlas_spine_btn_rect, "2ND PASS: CLEAR 48/48", 13);
    }

    /* 2. Middle-Left: 12 x 4 Full-Size Atlas Grid */
    DrawRectangleRec(k_atlas_grid_box_rect, PLEXUS_SUBBOX_BG);
    ui_draw_panel_border(k_atlas_grid_box_rect, COLOR_PHOSPHOR_DIM);

    unsigned long long view_mask = s_atlas_view_deus ? s_deus_finished_cards_mask : s_finished_cards_mask;
    Texture2D tex = (s_atlas_view_deus && ba && ba->atlas_deus.id > 0)
                    ? ba->atlas_deus
                    : (ba ? ba->atlas : (Texture2D){ 0 });
    Color earned_border = s_atlas_view_deus ? (Color){ 235, 220, 175, 255 } : COLOR_PHOSPHOR;

    for (int m = 0; m < 12; m++) {
        float mx = k_atlas_grid_box_rect.x + 18.0f + (float)m * 40.0f;
        draw_text_snapped(font_title, TextFormat("%02d", m + 1), mx, k_atlas_grid_box_rect.y + 8.0f, 12.0f, 1.0f,
                          is_month_completed_mask(view_mask, m) ? earned_border : COLOR_PHOSPHOR_DIM);
        for (int r = 0; r < 4; r++) {
            int cid = m * 4 + r;
            int finished = (int)((view_mask >> cid) & 1ULL);
            Rectangle dst = atlas_grid_cell_rect(m, r);
            int hov = CheckCollisionPointRec(vm, dst);
            if (hov) s_atlas_pinned_cid = cid;

            if (tex.id > 0) {
                Rectangle src = { (float)(m * ATLAS_CELL_W), (float)(r * ATLAS_CELL_H), (float)ATLAS_CELL_W, (float)ATLAS_CELL_H };
                Color tint = finished ? WHITE : (Color){ 65, 74, 82, 115 };
                DrawTexturePro(tex, src, dst, (Vector2){ 0, 0 }, 0.0f, tint);
                if (!finished) {
                    DrawRectangleRec(dst, (Color){ 0, 0, 0, 155 });
                }
            } else {
                DrawRectangleRec(dst, finished ? COLOR_BTN_HOVER_BG : (Color){ 10, 14, 16, 210 });
            }

            int pinned = (s_atlas_pinned_cid == cid);
            if (finished) {
                DrawRectangleLinesEx(dst, (hov || pinned) ? 2.0f : 1.3f, earned_border);
            } else {
                Color dim_b = COLOR_PHOSPHOR_DIM;
                dim_b.a = (hov || pinned) ? 190 : 75;
                DrawRectangleLinesEx(dst, 1.0f, dim_b);
            }
        }
    }

    /* 3. Middle-Right: High-Res Card Inspector */
    DrawRectangleRec(k_atlas_insp_box_rect, PLEXUS_SUBBOX_BG);
    ui_draw_panel_border(k_atlas_insp_box_rect, COLOR_PHOSPHOR_DIM);

    int icid = (s_atlas_pinned_cid >= 0 && s_atlas_pinned_cid < 48) ? s_atlas_pinned_cid : 3;
    int ic_done = (int)((view_mask >> icid) & 1ULL);
    const char *insp_hdr = s_atlas_view_deus ? "DEUS INSPECTOR" : "CARD INSPECTOR";
    Vector2 ihs = MeasureTextEx(font_title, insp_hdr, 13.5f, 1.1f);
    draw_text_snapped(font_title, insp_hdr,
                      k_atlas_insp_box_rect.x + (k_atlas_insp_box_rect.width - ihs.x) * 0.5f,
                      k_atlas_insp_box_rect.y + 8.0f, 13.5f, 1.1f, earned_border);

    Rectangle preview = {
        floorf(k_atlas_insp_box_rect.x + (k_atlas_insp_box_rect.width - 114.0f) * 0.5f),
        k_atlas_insp_box_rect.y + 30.0f, 114.0f, 185.0f
    };
    board_draw_card_shadow(preview, 0.55f);
    if (ic_done && tex.id > 0) {
        int pm = icid / 4, pr = icid % 4;
        Rectangle src = { (float)(pm * ATLAS_CELL_W), (float)(pr * ATLAS_CELL_H), (float)ATLAS_CELL_W, (float)ATLAS_CELL_H };
        DrawTexturePro(tex, src, preview, (Vector2){ 0, 0 }, 0.0f, WHITE);
        DrawRectangleLinesEx(preview, 1.8f, earned_border);
    } else {
        if (ba) { board_set_table_bezel_muted(1); board_draw_card_back(ba, preview); board_set_table_bezel_muted(0); }
        else DrawRectangleRec(preview, (Color){ 10, 14, 16, 220 });
        DrawRectangleLinesEx(preview, 1.0f, COLOR_PHOSPHOR_DIM);
    }

    const char *c_title = ic_done ? CARDS[icid].name : "[ SEALED CARD ]";
    const char *t_name  = (CARDS[icid].type == CARD_TYPE_BRIGHT) ? "BRIGHT" :
                          (CARDS[icid].type == CARD_TYPE_ANIMAL) ? "ANIMAL" :
                          (CARDS[icid].type == CARD_TYPE_RIBBON) ? "RIBBON" : "PLAIN";
    char c_sub[48];
    snprintf(c_sub, sizeof(c_sub), "MONTH %02d // %s", (icid / 4) + 1, t_name);
    Vector2 ct1 = MeasureTextEx(font_title, c_title, 13.5f, 1.0f);
    Vector2 ct2 = MeasureTextEx(font_body,  c_sub,   14.5f, 1.0f);
    draw_text_snapped(font_title, c_title,
                      k_atlas_insp_box_rect.x + (k_atlas_insp_box_rect.width - ct1.x) * 0.5f,
                      k_atlas_insp_box_rect.y + 226.0f, 13.5f, 1.0f,
                      ic_done ? COLOR_TEXT_INFO : COLOR_PHOSPHOR_DIM);
    draw_text_snapped(font_body, c_sub,
                      k_atlas_insp_box_rect.x + (k_atlas_insp_box_rect.width - ct2.x) * 0.5f,
                      k_atlas_insp_box_rect.y + 250.0f, 14.5f, 1.0f, COLOR_TEXT_INFO_DIM);

    /* 4. Bottom: Progression & Achievements */
    DrawRectangleRec(k_atlas_ach_box_rect, PLEXUS_SUBBOX_BG);
    ui_draw_panel_border(k_atlas_ach_box_rect, COLOR_PHOSPHOR_DIM);

    int story_docs = 0;
    for (int m = 0; m < 12; m++) {
        if (is_story_month_unlocked(m)) story_docs++;
    }
    int ach_ok[8];
    ach_ok[0] = is_month_completed_mask(s_finished_cards_mask, 0);
    ach_ok[1] = is_month_unlocked_mask(s_finished_cards_mask, 4);
    ach_ok[2] = is_month_unlocked_mask(s_finished_cards_mask, 8);
    ach_ok[3] = (std_done >= 48);
    ach_ok[4] = (story_docs >= 12);
    ach_ok[5] = (s_sc_best_wave >= 5 || (s_sc_rank_count > 0 && s_sc_ranks[0].score >= 100));
    ach_ok[6] = (get_unlocked_camp_opponents() >= 12);
    ach_ok[7] = (deus_done >= 48);

    static const char * const k_ach_labels[8] = {
        "THRESHOLD LINK  (MONTH 01)",
        "SATURATION GATE (SPEEDCOIN)",
        "RESONANCE GATE  (ACTION CARD)",
        "STANDARD ATLAS  (48/48 -> DEUS)",
        "STORY ARCHIVE   (12/12 LOGS)",
        "SPEEDCOIN ACE   (WAVE 05+)",
        "SECTOR HUNTER   (12/12 RIVALS)",
        "DEUS CONVERGENCE (48/48 DEUS)"
    };

    int earned_cnt = 0;
    for (int i = 0; i < 8; i++) {
        if (ach_ok[i]) earned_cnt++;
    }
    draw_text_snapped(font_title, "PROGRESSION & ACHIEVEMENTS",
                      k_atlas_ach_box_rect.x + 14.0f, k_atlas_ach_box_rect.y + 8.0f, 14.5f, 1.1f, COLOR_PHOSPHOR);
    char e_txt[96];
    snprintf(e_txt, sizeof(e_txt), "MILESTONES: %d / 8   |   STD %02d/48   |   DEUS %02d/48",
             earned_cnt, std_done, deus_done);
    Vector2 ets = MeasureTextEx(font_body, e_txt, 15.0f, 1.0f);
    draw_text_snapped(font_body, e_txt,
                      k_atlas_ach_box_rect.x + k_atlas_ach_box_rect.width - ets.x - 14.0f,
                      k_atlas_ach_box_rect.y + 8.0f, 15.0f, 1.0f, COLOR_TEXT_INFO);
    DrawLine((int)k_atlas_ach_box_rect.x + 14, (int)k_atlas_ach_box_rect.y + 30,
             (int)(k_atlas_ach_box_rect.x + k_atlas_ach_box_rect.width - 14.0f),
             (int)k_atlas_ach_box_rect.y + 30, COLOR_PHOSPHOR_DIM);

    for (int i = 0; i < 8; i++) {
        int col = i / 4, row = i % 4;
        float ax = k_atlas_ach_box_rect.x + 16.0f + (float)col * 356.0f;
        float ay = k_atlas_ach_box_rect.y + 38.0f + (float)row * 27.0f;
        Color ac = ach_ok[i] ? ((i == 7) ? (Color){ 235, 220, 175, 255 } : COLOR_PHOSPHOR) : COLOR_PHOSPHOR_DIM;
        draw_text_snapped(font_body,
                          TextFormat("%s %s", ach_ok[i] ? "[OK]" : "[--]", k_ach_labels[i]),
                          ax, ay, 15.5f, 1.05f, ac);
    }
    (void)st;
}

static void draw_logfile_window(const BoardAssets *ba, const GameStats *st, Vector2 vm) {
    if (s_log_subtab != LOG_TAB_STORY) {
        commit_viewed_story_months(-1);
    }

    DrawRectangleRec(k_logfile_win_rect, PLEXUS_PANEL_BG);
    ui_draw_panel_border(k_logfile_win_rect, COLOR_PHOSPHOR);

    /* 1. Top Title + 4 Full-Panel Sub-Tabs: [ STORY ] [ TRAINING ] [ MATCHES ] [ ATLAS ] */
    draw_text_snapped(font_title, "LOGFILE",
                      k_logfile_win_rect.x + 20.0f, k_logfile_win_rect.y + 15.0f,
                      24.0f, 1.5f, COLOR_PHOSPHOR);
    for (int t = 0; t < 4; t++) {
        draw_plexus_btn(k_log_tab_rects[t], k_log_tab_labels[t], 14, vm, s_log_subtab == t);
        if (t == LOG_TAB_STORY && has_unread_story_months()) {
            DrawCircle((int)(k_log_tab_rects[t].x + k_log_tab_rects[t].width - 11.0f),
                       (int)(k_log_tab_rects[t].y + 9.0f), 4.0f, COLOR_PHOSPHOR);
        }
    }

    /* 2. Pinned Current Spine Directive Strip */
    DrawRectangleRec(k_log_directive_rect, PLEXUS_SUBBOX_BG);
    DrawRectangleLinesEx(k_log_directive_rect, 1.0f, COLOR_PHOSPHOR_DIM);

    char dir_txt[120];
    int active_done = count_mask_cards(active_spine_mask());
    if (active_done >= 48) {
        if (is_deus_spine_active()) {
            snprintf(dir_txt, sizeof(dir_txt),
                     "DIRECTIVE // DEUS HARDCORE STORY 48/48 COMPLETE  --  CONVERGENCE ACHIEVED");
        } else {
            snprintf(dir_txt, sizeof(dir_txt),
                     "DIRECTIVE // STANDARD STORY 48/48 COMPLETE  --  DEUS PASS READY IN [ATLAS]");
        }
    } else if (is_month_completed(s_selected_month) && s_selected_month + 1 < 12) {
        snprintf(dir_txt, sizeof(dir_txt),
                 "DIRECTIVE // %sMONTH %02d COMPLETE  --  PRESS [NEXT] ABOVE THE CARDS FOR MONTH %02d",
                 is_deus_spine_active() ? "DEUS " : "", s_selected_month + 1, s_selected_month + 2);
    } else {
        int sl = highest_unlocked_slot(s_selected_month);
        int cid = s_selected_month * 4 + sl;
        snprintf(dir_txt, sizeof(dir_txt),
                 "DIRECTIVE // %sMONTH %02d (%s)  --  NEXT: CARD %d OF 4 [%s]",
                 is_deus_spine_active() ? "DEUS " : "",
                 s_selected_month + 1, k_month_names[s_selected_month],
                 sl + 1, CARDS[cid].name);
    }
    draw_text_snapped(font_body, dir_txt,
                      k_log_directive_rect.x + 12.0f, k_log_directive_rect.y + 8.0f,
                      16.0f, 1.1f, COLOR_PHOSPHOR);

    DrawLine((int)k_logfile_win_rect.x + 18, 114,
             (int)(k_logfile_win_rect.x + k_logfile_win_rect.width - 18.0f), 114, COLOR_PHOSPHOR_DIM);

    /* 3. Active Full-Panel Tab Content */
    if (s_log_subtab == LOG_TAB_STORY) {
        draw_logfile_story_tab(vm);
    } else if (s_log_subtab == LOG_TAB_TRAINING) {
        draw_logfile_training_tab();
    } else if (s_log_subtab == LOG_TAB_MATCHES) {
        draw_logfile_matches_tab(vm);
    } else {
        draw_logfile_atlas_tab(ba, st, vm);
    }
}

/* The prologue wall reveals line by line and the CONTINUE button does not exist
   until the last line has finished fading in - it used to appear at 2.65 s while
   the closing line was still ramping up under it (owner, 29 Oct 2026: "the text
   reveals itself too fast ... only when the full text is there"). The four line
   starts are absolute seconds so the schedule is readable in one place, and the
   button constant is derived from the last of them: it cannot drift out of sync
   with the text again. */
#define PLEXUS_INTRO_L1_AT    0.25f
#define PLEXUS_INTRO_L2_AT    1.05f
#define PLEXUS_INTRO_L3_AT    1.85f
#define PLEXUS_INTRO_L4_AT    2.65f
#define PLEXUS_INTRO_L_FADE   1.10f   /* per-line fade-in, seconds */
#define PLEXUS_INTRO_L4_FADE  1.45f   /* the closing block is longer, it gets longer */
#define PLEXUS_INTRO_BTN_HOLD 0.35f   /* beat of silence before the button */
#define PLEXUS_INTRO_BTN_AT   (PLEXUS_INTRO_L4_AT + PLEXUS_INTRO_L4_FADE + PLEXUS_INTRO_BTN_HOLD)
#define PLEXUS_INTRO_BTN_DELAY PLEXUS_INTRO_BTN_AT
#define PLEXUS_INTRO_REVEAL_DUR 0.78f
static const Rectangle k_intro_continue_rect = { 530.0f, 624.0f, 220.0f, 44.0f };

static float intro_clamp01(float x) {
    return (x < 0.0f) ? 0.0f : (x > 1.0f) ? 1.0f : x;
}

static Color intro_with_alpha(Color c, float a) {
    int av = (int)lroundf((float)c.a * intro_clamp01(a));
    if (av < 0) av = 0;
    if (av > 255) av = 255;
    c.a = (unsigned char)av;
    return c;
}

int plexus_intro_blocking(void) {
    return !s_intro_seen;
}

int plexus_intro_accept_key(void) {
    if (s_intro_seen || s_intro_timer < PLEXUS_INTRO_BTN_DELAY) return 0;
    s_intro_seen = 1;
    s_intro_reveal_t = 0.001f;
    plexus_save();
    return 1;
}

int plexus_intro_revealing(void) {
    return s_intro_seen && s_intro_reveal_t > 0.0f && s_intro_reveal_t < 1.0f;
}

void plexus_set_intro_ui_texture(Texture2D ui_tex) {
    s_intro_ui_tex = ui_tex;
}

static void draw_intro_story_pass(Font sf, float dx, float dy, float a1, float a2, float a3, float a4) {
    float cx = 640.0f + dx;
    /* The wall of text used to hang in the dark with nothing naming it - the same
       content is filed in the logfile as TRANSMISSION 00, so the screen now wears
       that flag. It fades in with the first line and sits above the opening
       statement, clear of the 184 px line 1. */
    if (a1 > 0.005f) {
        Vector2 fs = MeasureTextEx(font_title, "TRANSMISSION 00  //  ATRIUM PLEX ENTRY", 15.0f, 1.6f);
        draw_text_snapped(font_title, "TRANSMISSION 00  //  ATRIUM PLEX ENTRY",
                          cx - fs.x * 0.5f, 128.0f + dy, 15.0f, 1.6f,
                          intro_with_alpha(COLOR_PHOSPHOR_DIM, a1));
        float half = 260.0f;
        DrawLine((int)(cx - half), (int)(152.0f + dy), (int)(cx + half), (int)(152.0f + dy),
                 intro_with_alpha(COLOR_PHOSPHOR_DIM, a1));
    }
    if (a1 > 0.005f) {
        draw_story_centered(sf, "The mathematics of sequential dependence:",
                            cx, 184.0f + dy, 29.0f, 1.0f, intro_with_alpha(COLOR_TEXT_INFO_DIM, a1));
    }
    if (a2 > 0.005f) {
        draw_story_chain_formula(sf, cx, 242.0f + dy, 31.0f, 1140.0f, 1.0f,
                                 intro_with_alpha(COLOR_PHOSPHOR, a2));
    }
    if (a3 > 0.005f) {
        draw_story_centered(sf, "everything depends on everything before it.",
                            cx, 308.0f + dy, 29.0f, 1.0f, intro_with_alpha(COLOR_TEXT_INFO, a3));
    }
    if (a4 > 0.005f) {
        Color c_body = intro_with_alpha(COLOR_TEXT_INFO, a4);
        Color c_end  = intro_with_alpha(COLOR_PHOSPHOR, a4);
        draw_story_centered(sf,
                            "This includes your attempt to see the pattern. Your observation is a token in the sequence.",
                            cx, 392.0f + dy, 26.5f, 0.95f, c_body);
        draw_story_centered(sf,
                            "Your \"insight\" is conditioned on all previous tokens which the pattern shaped.",
                            cx, 430.0f + dy, 26.5f, 0.95f, c_body);
        draw_story_centered(sf,
                            "Your \"resistance\" is predicted and incorporated.",
                            cx, 468.0f + dy, 26.5f, 0.95f, c_end);
    }
}

static void draw_intro_story_screen(Vector2 vm, float timer, float fade_out) {
    Font sf = story_font();
    float a1 = intro_clamp01((timer - PLEXUS_INTRO_L1_AT) / PLEXUS_INTRO_L_FADE) * fade_out;
    float a2 = intro_clamp01((timer - PLEXUS_INTRO_L2_AT) / PLEXUS_INTRO_L_FADE) * fade_out;
    float a3 = intro_clamp01((timer - PLEXUS_INTRO_L3_AT) / PLEXUS_INTRO_L_FADE) * fade_out;
    float a4 = intro_clamp01((timer - PLEXUS_INTRO_L4_AT) / PLEXUS_INTRO_L4_FADE) * fade_out;

    if (fade_out < 0.99f && fade_out > 0.01f) {
        float blur = (1.0f - fade_out) * 8.0f;
        draw_intro_story_pass(sf, -blur, 0.0f, a1 * 0.24f, a2 * 0.24f, a3 * 0.24f, a4 * 0.24f);
        draw_intro_story_pass(sf,  blur, 0.0f, a1 * 0.24f, a2 * 0.24f, a3 * 0.24f, a4 * 0.24f);
    }
    draw_intro_story_pass(sf, 0.0f, 0.0f, a1, a2, a3, a4);

    if (timer >= PLEXUS_INTRO_BTN_DELAY) {
        float ba_alpha = intro_clamp01((timer - PLEXUS_INTRO_BTN_DELAY) / 0.65f) * fade_out;
        if (ba_alpha > 0.005f) {
            int hov = (fade_out >= 0.999f) && CheckCollisionPointRec(vm, k_intro_continue_rect);
            Color bg = hov ? COLOR_BTN_HOVER_BG : PLEXUS_PANEL_BG;
            Color bd = hov ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM;
            Color tc = hov ? COLOR_TEXT_INFO : COLOR_PHOSPHOR;
            DrawRectangleRec(k_intro_continue_rect, intro_with_alpha(bg, ba_alpha));
            DrawRectangleLinesEx(k_intro_continue_rect, hov ? 1.8f : 1.2f, intro_with_alpha(bd, ba_alpha));
            Vector2 ts = MeasureTextEx(font_title, "CONTINUE", 17.0f, 1.3f);
            draw_text_snapped(font_title, "CONTINUE",
                              k_intro_continue_rect.x + (k_intro_continue_rect.width - ts.x) * 0.5f,
                              k_intro_continue_rect.y + (k_intro_continue_rect.height - 17.0f) * 0.5f,
                              17.0f, 1.3f, intro_with_alpha(tc, ba_alpha));
        }
    }
}

/* --- HISCORE board (SPEEDCOIN + ACTION CARD) ----------------------------- */
/* A read-out, not a new record: both tabs already keep their history (the
   speedcoin top-8 ranklist and the ACTION CARD rows of the match log), so the
   board shows what is saved instead of adding a second, parallel score table. */
static const Rectangle k_hiscore_box   = { 320.0f, 118.0f, 640.0f, 474.0f };
static const Rectangle k_hiscore_close = { 550.0f, 528.0f, 180.0f,  44.0f };

static void draw_plexus_hiscore_overlay(Vector2 vm) {
    if (!s_hiscore_open) return;
    /* Only the two modes that own a board can show one; a tab change from under the
       overlay (keyboard, or a mode losing its unlock) closes it instead of drawing
       the wrong board. */
    if (s_active_tab != 2 && s_active_tab != 3) { s_hiscore_open = 0; return; }
    int ac = (s_active_tab == 3);

    DrawRectangle(0, 0, 1280, 720, (Color){ 0, 0, 0, 176 });
    DrawRectangleRec(k_hiscore_box, PLEXUS_PANEL_BG);
    ui_draw_panel_border(k_hiscore_box, COLOR_PHOSPHOR);

    draw_text_snapped(font_title, ac ? "ACTION CARD  //  HISCORE BOARD" : "SPEEDCOIN  //  HISCORE BOARD",
                      k_hiscore_box.x + 22.0f, k_hiscore_box.y + 16.0f, 20.0f, 1.4f, COLOR_PHOSPHOR);
    DrawLine((int)k_hiscore_box.x + 20, (int)k_hiscore_box.y + 48,
             (int)(k_hiscore_box.x + k_hiscore_box.width - 20.0f), (int)k_hiscore_box.y + 48,
             COLOR_PHOSPHOR_DIM);

    float x = k_hiscore_box.x + 24.0f;

    if (ac) {
        draw_text_snapped(font_body, TextFormat("BEST SCORE  %d PTS   //   RUNS %d   //   %dW-%dL",
                                                s_ac_best_score, s_ac_runs, s_ac_wins, s_ac_losses),
                          x, k_hiscore_box.y + 62.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
        DrawLine((int)x, (int)k_hiscore_box.y + 90,
                 (int)(k_hiscore_box.x + k_hiscore_box.width - 24.0f), (int)k_hiscore_box.y + 90,
                 COLOR_PHOSPHOR_DIM);

        /* Five rows: the sixth would run under the CLOSE button at +410. */
        int row = 0;
        for (int i = 0; i < s_match_log_count && row < 5; i++) {
            if (s_match_logs[i].mode != MLOG_ACTION_CARD) continue;
            float ry = k_hiscore_box.y + 102.0f + (float)row * 56.0f;
            draw_text_snapped(font_body, s_match_logs[i].title,
                              x, ry, 15.0f, 1.1f, COLOR_PHOSPHOR);
            draw_text_snapped(font_body, s_match_logs[i].detail,
                              x, ry + 22.0f, 14.0f, 1.1f, COLOR_TEXT_INFO_DIM);
            row++;
        }
        if (row == 0) {
            draw_text_snapped(font_body, "NO ACTION CARD RUNS YET  //  PRESS START TO OPEN THE BOARD.",
                              x, k_hiscore_box.y + 120.0f, 16.0f, 1.1f, COLOR_PHOSPHOR_DIM);
        } else if (row == 5) {
            draw_text_snapped(font_body, "OLDER RUNS ARE IN [LOGFILE] > MATCHES > ACTION CARD.",
                              x, k_hiscore_box.y + 380.0f, 14.0f, 1.05f, COLOR_PHOSPHOR_DIM);
        }
    } else {
        draw_text_snapped(font_body, TextFormat("RUNS %d   //   BEST WAVE W-%02d   //   TOP 8 PERSISTED",
                                                s_sc_runs, s_sc_best_wave),
                          x, k_hiscore_box.y + 62.0f, 17.0f, 1.2f, COLOR_TEXT_INFO);
        /* Columns are drawn one by one at the same offsets the data uses (and at
           the same 96 / 224 / 306 as the speedcoin result ranklist), so the heading
           cannot drift out of alignment with the numbers under it. */
        static const float k_sc_col[3] = { 96.0f, 224.0f, 306.0f };
        static const char *const k_sc_head[3] = { "SCORE", "WAVE", "COINS" };
        for (int c2 = 0; c2 < 3; c2++) {
            draw_text_snapped(font_body, k_sc_head[c2], x + k_sc_col[c2],
                              k_hiscore_box.y + 94.0f, 13.5f, 1.1f, COLOR_TEXT_INFO_DIM);
        }
        draw_text_snapped(font_body, "RANK", x, k_hiscore_box.y + 94.0f, 13.5f, 1.1f, COLOR_TEXT_INFO_DIM);

        for (int i = 0; i < SPEEDCOIN_RANK_MAX; i++) {
            float ry = k_hiscore_box.y + 122.0f + (float)i * 34.0f;
            int have = (i < s_sc_rank_count);
            Color c = have ? ((i == 0) ? COLOR_PHOSPHOR : COLOR_TEXT_INFO) : COLOR_PHOSPHOR_DIM;
            draw_text_snapped(font_title, TextFormat("#%02d", i + 1), x, ry, 17.0f, 1.1f, c);
            draw_text_snapped(font_body,
                              have ? TextFormat("%d PTS", s_sc_ranks[i].score) : "---",
                              x + k_sc_col[0], ry + 2.0f, 17.0f, 1.1f, c);
            draw_text_snapped(font_body,
                              have ? TextFormat("W-%02d", s_sc_ranks[i].waves) : "--",
                              x + k_sc_col[1], ry + 2.0f, 17.0f, 1.1f, c);
            draw_text_snapped(font_body,
                              have ? TextFormat("%d MP", s_sc_ranks[i].coins) : "---",
                              x + k_sc_col[2], ry + 2.0f, 17.0f, 1.0f, c);
        }
    }

    draw_plexus_btn(k_hiscore_close, "CLOSE", 17, vm, 0);
}

void plexus_draw_ui_chrome(const BoardAssets *ba, const GameStats *st, Vector2 vm) {
    ensure_plexus_assets();
    if (!is_bottom_btn_unlocked(s_active_tab)) s_active_tab = 0;
    float dt = GetFrameTime();
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.05f) dt = 0.05f;

    /* 0. Screen Title. The Plex never named itself - it was "the screen with the
       cards on the right". Top-left is the only free corner (the right column is
       the spine, the middle is the card). It is skipped while the logfile window is
       open: that window is an opaque panel starting at x 236, and a title that
       passed under it would be cut in half. */
    if (s_active_tab != 1) {
        draw_text_snapped(font_title, "THE ATRIUM PLEX",
                          64.0f, 22.0f, 18.0f, 1.6f, COLOR_PHOSPHOR);
        DrawLine(64, 52, 276, 52, COLOR_PHOSPHOR_DIM);
    }

    /* 2. Right Spine: [NEXT] button (when all 4 cards of month completed) + 4 Month Cards */
    float pulse = 0.55f + 0.45f * sinf((float)GetTime() * 3.8f);
    int month_done = is_month_completed(s_selected_month);
    if (month_done && s_selected_month + 1 < 12 && !s_swing_active) {
        ui_theme_set_panel_flare_glow(k_next_month_rect, 0.55f + 0.45f * pulse);
        draw_plexus_btn(k_next_month_rect, "[NEXT]", 15, vm, 0);
        ui_theme_draw_panel_flares(k_next_month_rect);
    }

    if (s_swing_active && s_swing_elapsed < 0.24f && ba) {
        for (int i = 0; i < 4; i++) {
            float uo = (s_swing_elapsed - (float)i * 0.02f) / 0.18f;
            if (uo < 0.0f) uo = 0.0f;
            if (uo >= 1.0f) continue;
            Rectangle or_r = k_card_rects[i];
            or_r.x += 215.0f * uo * uo;
            if (or_r.x >= 1280.0f) continue;
            board_draw_card_shadow(or_r, 0.30f);
            draw_spine_card_face(ba, s_swing_prev_month * 4 + i, or_r);
            DrawRectangleLinesEx((Rectangle){ or_r.x - 1.0f, or_r.y - 1.0f, or_r.width + 2.0f, or_r.height + 2.0f },
                                 1.0f, COLOR_PHOSPHOR_DIM);
        }
    }

    for (int i = 0; i < 4; i++) {
        Rectangle cr = k_card_rects[i];
        if (s_swing_active) {
            float ui = (s_swing_elapsed - (0.10f + (float)(3 - i) * 0.045f)) / 0.30f;
            if (ui <= 0.0f) continue;
            if (ui > 1.0f) ui = 1.0f;
            float inv = 1.0f - ui;
            cr.x += 215.0f * inv * inv * inv;
        }
        int card_id = s_selected_month * 4 + i;
        int unlocked = is_slot_unlocked_in_month(s_selected_month, i);
        int is_flip = (s_flip_active && s_selected_month == s_flip_month && i == s_flip_slot);
        Rectangle draw_r = cr;
        int show_front = unlocked;
        if (is_flip) {
            float fu = (s_flip_delay > 0.0f) ? 0.0f : (s_flip_elapsed / SPINE_FLIP_DUR);
            if (fu < 0.0f) fu = 0.0f;
            if (fu > 1.0f) fu = 1.0f;
            float su = fu * fu * (3.0f - 2.0f * fu);
            float cv = cosf(su * 3.14159265f);
            float w = cr.width * fmaxf(0.06f, fabsf(cv));
            draw_r = (Rectangle){ cr.x + (cr.width - w) * 0.5f, cr.y - 5.0f * sinf(su * 3.14159265f), w, cr.height };
            show_front = (cv <= 0.0f);
        }
        if (unlocked && show_front) {
            int hov = !s_swing_active && CheckCollisionPointRec(vm, cr);
            int sel = (s_active_tab == 0 && i == s_selected_slot);
            board_draw_card_shadow(draw_r, (sel || hov) ? 0.85f : 0.35f);
            board_set_table_bezel_muted(1);
            if (sel) board_draw_card_glow(draw_r, pulse);
            else if (hov) board_draw_card_glow(draw_r, 0.45f);
            board_set_table_bezel_muted(0);
            if (ba) draw_spine_card_face(ba, card_id, draw_r);
            Color bcol = (sel || hov) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM;
            Rectangle border_r = { draw_r.x - 1.0f, draw_r.y - 1.0f, draw_r.width + 2.0f, draw_r.height + 2.0f };
            DrawRectangleLinesEx(border_r, sel ? 2.0f : 1.0f, bcol);
            if (sel) ui_theme_set_panel_flare_glow(border_r, 0.78f + 0.22f * pulse);
            ui_theme_draw_panel_flares(border_r);
        } else {
            board_draw_card_shadow(draw_r, 0.28f);
            if (ba) { board_set_table_bezel_muted(1); board_draw_card_back(ba, draw_r); board_set_table_bezel_muted(0); }
            Color dim_b = { COLOR_PHOSPHOR_DIM.r, COLOR_PHOSPHOR_DIM.g, COLOR_PHOSPHOR_DIM.b, 105 };
            DrawRectangleLinesEx((Rectangle){ draw_r.x - 1.0f, draw_r.y - 1.0f, draw_r.width + 2.0f, draw_r.height + 2.0f },
                                 1.0f, dim_b);
        }
    }

    /* 3. Middle Screen: either Logfile Window (tab 1) or Task / Training Briefing + START button */
    if (s_active_tab == 1) {
        draw_logfile_window(ba, st, vm);
    } else {
        commit_viewed_story_months(-1);

        char hdr[96];
        char sub[96];
        const char *line1 = "";
        const char *line2 = "";
        const char *line3 = "";
        int card_id = s_selected_month * 4 + s_selected_slot;
        int slot_unlocked = is_slot_unlocked_in_month(s_selected_month, s_selected_slot);
        char sc_line1[96];
        char sc_line2[96];
        char sc_line3[96];
        if (s_active_tab == 0) {
            const char *deus_p = is_deus_spine_active() ? "DEUS // " : "";
            if (s_selected_month == 0) {
                static const char * const k_tut_titles[4] = {
                    "TUTORIAL 04 // FINAL VERIFICATION MATCH",
                    "SPECIAL GAME // KK-ADD-10 ACTION CARD AUDIO EFFECT",
                    "TUTORIAL 02 // SPEEDCOIN SATURATION PROTOCOL",
                    "TUTORIAL 01 // STANDARD RULES OF KOI-KOI"
                };
                static const char * const k_tut_desc1[4] = {
                    "Short match to verify everything learned.",
                    "3 Rounds (Easy) -- Twin bio-meters & 5 Plexus horrors.",
                    "180s Speedcoin saturation tutorial.",
                    "Learn floral matching, deck draw, yaku & Koi-Koi."
                };
                snprintf(hdr, sizeof(hdr), "%sMONTH 01  //  %s   [TUTORIAL]", deus_p, k_month_names[0]);
                snprintf(sub, sizeof(sub), "CARD %d OF 4 :  [%s]", s_selected_slot + 1, CARDS[card_id].name);
                line1 = k_tut_titles[s_selected_slot & 3];
                line2 = k_tut_desc1[s_selected_slot & 3];
                /* Tutorials keep their own title/description on lines 1-2; the
                   story beat only ever gets the third line here, so the feedback
                   that teaches the mode can never be overwritten by lore. */
                line3 = k_story_beats[card_id].signal[0];
            } else {
                snprintf(hdr, sizeof(hdr), "%sMONTH %02d  //  %s", deus_p, s_selected_month + 1, k_month_names[s_selected_month]);
                snprintf(sub, sizeof(sub), "CARD %d OF 4 :  [%s]", s_selected_slot + 1, CARDS[card_id].name);
                const StoryBeat *bt = story_beat(card_id);
                if (bt && bt->signal[0]) {
                    line1 = bt->signal[0];
                    line2 = bt->signal[1] ? bt->signal[1] : "";
                    line3 = bt->signal[2] ? bt->signal[2] : "";
                } else {
                    line1 = is_deus_spine_active() ? "HARDCORE DEUS PASS  [PLACEHOLDER]" : "TASK / TRAINING BRIEFING  [PLACEHOLDER]";
                    line2 = "";
                    line3 = "";
                }
            }
        } else if (s_active_tab == 2) {
            int drops = speed_bar_graded_drops();
            snprintf(hdr, sizeof(hdr), "ATRIUM PLEX  //  SPEEDCOIN");
            snprintf(sub, sizeof(sub), "SOLO HIGHSCORE  //  180s CLOCK   [RUNS: %d]", s_sc_runs);
            line1 = "14 field (2x7) + 8 hand. +10s/10 MP, -10s miss.";
            if (s_sc_rank_count >= 2) {
                snprintf(sc_line2, sizeof(sc_line2),
                         "#01: %d PTS [W-%02d]   #02: %d PTS [W-%02d]",
                         s_sc_ranks[0].score, s_sc_ranks[0].waves,
                         s_sc_ranks[1].score, s_sc_ranks[1].waves);
                line2 = sc_line2;
            } else if (s_sc_rank_count == 1) {
                snprintf(sc_line2, sizeof(sc_line2),
                         "Best #01: %d PTS [W-%02d / %d MP]  [Best W-%02d]",
                         s_sc_ranks[0].score, s_sc_ranks[0].waves,
                         s_sc_ranks[0].coins, s_sc_best_wave);
                line2 = sc_line2;
            } else {
                line2 = "-10s/miss; 30 MP Hand 2; 3 misses = Reverse Deck.";
            }
            if (drops > 0) {
                snprintf(sc_line3, sizeof(sc_line3),
                         "Avg %.2fs  //  %.1f%% overlap  //  %d MP [x%d]",
                         speed_bar_avg_drop_seconds(), speed_bar_avg_cover_pct(),
                         speed_bar_lifetime_minipoints(), speed_bar_best_streak());
                line3 = sc_line3;
            } else {
                line3 = "Press START to launch a 180s highscore run.";
            }
        } else if (s_active_tab == 3) {
            const char *d_name = diff_short_name(s_ac_cfg_diff, s_ac_cfg_timer);
            int drops = speed_bar_graded_drops();
            int aud_trig = board_get_audio_action_triggers();
            int ac_sym = action_card_get_symbols_fired();
            int ac_tm  = action_card_get_lifetime_time();
            snprintf(hdr, sizeof(hdr), "ATRIUM PLEX  //  ACTION CARD");
            snprintf(sub, sizeof(sub), "KK-ADD-10  //  %s  //  %d ROUNDS   [%dW-%dL]",
                     d_name, s_ac_cfg_rounds, s_ac_wins, s_ac_losses);
            if (s_ac_runs > 0 || ac_sym > 0 || ac_tm > 0) {
                snprintf(sc_line1, sizeof(sc_line1),
                         "Best: %d PTS  //  Time Taken: %d  //  Symbols: %d",
                         s_ac_best_score, ac_tm, ac_sym);
                line1 = sc_line1;
            } else {
                line1 = "Dual audio meters, 5 beat symbols & distraction glow.";
            }
            if (aud_trig > 0) {
                snprintf(sc_line2, sizeof(sc_line2),
                         "Audio FX: %.0f%% Peak  //  %.1fs Full  //  %d Triggers",
                         board_get_audio_peak_synergy() * 100.0f,
                         board_get_audio_full_hold_seconds(), aud_trig);
                line2 = sc_line2;
            } else {
                line2 = "Use SETTINGS below for Rounds, Difficulty & Timer.";
            }
            if (drops > 0) {
                snprintf(sc_line3, sizeof(sc_line3),
                         "Speedbar: %.2fs avg  //  %.1f%% overlap  //  %d MP",
                         speed_bar_avg_drop_seconds(), speed_bar_avg_cover_pct(),
                         speed_bar_lifetime_minipoints());
                line3 = sc_line3;
            } else {
                line3 = "Press START to launch a KK-ADD-10 match.";
            }
        } else {
            const char *d_name = diff_short_name(s_tr_cfg_diff, s_tr_cfg_timer);
            snprintf(hdr, sizeof(hdr), "ATRIUM PLEX  //  TRAINING");
            snprintf(sub, sizeof(sub), "%s  //  %s  //  %d ROUNDS",
                     ruleset_short_name(s_tr_cfg_ruleset), d_name, s_tr_cfg_rounds);
            const Opponent *op = campaign_get_opponent(clamped_tr_opponent());
            snprintf(sc_line1, sizeof(sc_line1), "Opponent: %s (%s)  [%d/12 Unlocked]",
                     op->name, op->favorite_combo, get_unlocked_camp_opponents());
            line1 = sc_line1;
            line2 = "Special Condition: NONE   [Training Lore Placeholder]";
            line3 = "Use SETTINGS to configure. No campaign awards.";
        }

        unsigned int bh = 2166136261u;
        bh = hash_briefing_line(bh, hdr);
        bh = hash_briefing_line(bh, sub);
        bh = hash_briefing_line(bh, line1);
        bh = hash_briefing_line(bh, line2);
        bh = hash_briefing_line(bh, line3);
        if (!s_briefing_hash_ready) {
            s_last_briefing_hash = bh;
            s_briefing_hash_ready = 1;
        } else if (bh != s_last_briefing_hash) {
            s_last_briefing_hash = bh;
            s_briefing_glow = 1.0f;
            s_briefing_hold = PLEXUS_BRIEFING_HOLD;
        } else if (s_briefing_hold > 0.0f) {
            s_briefing_hold -= dt;
            s_briefing_glow = 1.0f;
            if (s_briefing_hold < 0.0f) s_briefing_hold = 0.0f;
        } else if (s_briefing_glow > 0.0f) {
            s_briefing_glow -= dt * PLEXUS_BRIEFING_FADE;
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

        draw_text_snapped(font_title, hdr,   k_briefing_rect.x + 22.0f, k_briefing_rect.y + 16.0f, 21.0f, 1.4f, COLOR_PHOSPHOR);
        DrawLine((int)k_briefing_rect.x + 22, (int)k_briefing_rect.y + 46,
                 (int)(k_briefing_rect.x + k_briefing_rect.width - 22.0f), (int)k_briefing_rect.y + 46, COLOR_PHOSPHOR_DIM);
        draw_text_snapped(font_body,  sub,   k_briefing_rect.x + 22.0f, k_briefing_rect.y + 58.0f, 19.0f, 1.2f, COLOR_PHOSPHOR);
        draw_text_snapped(font_body,  line1, k_briefing_rect.x + 22.0f, k_briefing_rect.y +  94.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body,  line2, k_briefing_rect.x + 22.0f, k_briefing_rect.y + 124.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body,  line3, k_briefing_rect.x + 22.0f, k_briefing_rect.y + 154.0f, 18.0f, 1.2f, COLOR_TEXT_INFO_DIM);

        int start_locked = (s_active_tab == 0 && !slot_unlocked) ||
                           (s_active_tab == 2 && !is_speedcoin_bottom_unlocked()) ||
                           (s_active_tab == 3 && !is_action_card_bottom_unlocked());
        /* Middle slot: STAGE on SPEEDCOIN, SETTINGS on ACTION CARD and TRAINING.
           Left slot: HISCORE on SPEEDCOIN and ACTION CARD. */
        if (s_active_tab == 2 || s_active_tab == 3 || s_active_tab == 4) {
            if (s_active_tab == 2 || s_active_tab == 3) {
                draw_plexus_btn(k_hiscore_rect, "HISCORE", 18, vm, s_hiscore_open);
            }
            const char *cfg_lbl = (s_active_tab == 2) ? "STAGE" : "SETTINGS";
            if (start_locked) {
                draw_plexus_locked_btn(k_settings_rect, cfg_lbl, 18);
            } else {
                draw_plexus_btn(k_settings_rect, cfg_lbl, 18, vm, s_mode_settings_open);
                draw_mode_settings_panel(ba, vm);
            }
        }
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

    /* 4. Bottom Bar: SAVE & BACK, LOGFILE, SPEEDCOIN, ACTION CARD, TRAINING, OPTION */
    for (int i = 0; i < 6; i++) {
        if (!is_bottom_btn_unlocked(i)) {
            draw_plexus_locked_btn(k_bottom_rects[i], k_bottom_labels[i], 16);
        } else {
            int active = (i >= 1 && i <= 4 && s_active_tab == i);
            draw_plexus_btn(k_bottom_rects[i], k_bottom_labels[i], 16, vm, active);
        }
    }
}

void plexus_draw(const BoardAssets *ba, const GameStats *st, Vector2 vm) {
    ensure_plexus_assets();
    if (!is_bottom_btn_unlocked(s_active_tab)) s_active_tab = 0;

    float dt = GetFrameTime();
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.05f) dt = 0.05f;

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
        DrawTexturePro(s_plexus_bg, (Rectangle){ 0, 0, 1280, 720 },
                       (Rectangle){ 0, 0, 1280, 720 }, (Vector2){ 0, 0 }, 0.0f, WHITE);
        if (ba && ba->has_fog_shader) EndShaderMode();
    } else {
        ClearBackground(BLACK);
    }

    /* One-time Atrium Plex Story Introduction Screen */
    if (!s_intro_seen) {
        s_intro_timer += dt;
        DrawRectangle(0, 0, 1280, 720, (Color){ 0, 0, 0, 182 });
        draw_intro_story_screen(vm, s_intro_timer, 1.0f);
        return;
    }

    /* Seamless blur-in / pop-in reveal of Atrium Plex UI onto the same background */
    if (s_intro_reveal_t > 0.0f && s_intro_reveal_t < 1.0f) {
        s_intro_reveal_t += dt / PLEXUS_INTRO_REVEAL_DUR;
        if (s_intro_reveal_t >= 1.0f) {
            s_intro_reveal_t = 0.0f;
        } else {
            float t = s_intro_reveal_t;
            float u = t * t * (3.0f - 2.0f * t);
            int veil_a = 45 + (int)lroundf(137.0f * (1.0f - u));
            DrawRectangle(0, 0, 1280, 720, (Color){ 0, 0, 0, (unsigned char)veil_a });
            if (t < 0.45f) {
                draw_intro_story_screen((Vector2){ -1000.0f, -1000.0f }, s_intro_timer, 1.0f - (t / 0.45f));
            }
            if (s_intro_ui_tex.id > 0) {
                Rectangle src = { 0.0f, 0.0f, 1280.0f, -720.0f };
                float sc = 0.986f + 0.014f * u;
                float dw = 1280.0f * sc, dh = 720.0f * sc;
                float bx = (1280.0f - dw) * 0.5f, by = (720.0f - dh) * 0.5f;
                float rad = 11.0f * (1.0f - u);
                float halo_w = 0.52f * u * (1.0f - u);
                if (halo_w > 0.004f && rad > 0.25f) {
                    static const float k_ox[8] = { -1.0f,  1.0f,  0.0f,  0.0f, -0.7f,  0.7f, -0.7f,  0.7f };
                    static const float k_oy[8] = {  0.0f,  0.0f, -1.0f,  1.0f, -0.7f, -0.7f,  0.7f,  0.7f };
                    unsigned char ha = (unsigned char)lroundf(255.0f * halo_w);
                    for (int k = 0; k < 8; k++) {
                        DrawTexturePro(s_intro_ui_tex, src,
                                       (Rectangle){ bx + k_ox[k] * rad, by + k_oy[k] * rad, dw, dh },
                                       (Vector2){ 0.0f, 0.0f }, 0.0f, (Color){ 255, 255, 255, ha });
                    }
                }
                unsigned char ca = (unsigned char)lroundf(255.0f * (u * u));
                if (ca > 0) {
                    DrawTexturePro(s_intro_ui_tex, src, (Rectangle){ bx, by, dw, dh },
                                   (Vector2){ 0.0f, 0.0f }, 0.0f, (Color){ 255, 255, 255, ca });
                }
            } else {
                plexus_draw_ui_chrome(ba, st, (Vector2){ -1000.0f, -1000.0f });
            }
            return;
        }
    }

    if (s_flip_active) {
        if (s_flip_delay > 0.0f) {
            s_flip_delay -= dt;
            if (s_flip_delay < 0.0f) { s_flip_elapsed = -s_flip_delay; s_flip_delay = 0.0f; }
        } else {
            s_flip_elapsed += dt;
        }
        if (s_flip_elapsed >= SPINE_FLIP_DUR) s_flip_active = 0;
    }
    if (s_swing_active) {
        s_swing_elapsed += dt;
        if (s_swing_elapsed >= SPINE_SWING_DUR) s_swing_active = 0;
    }

    /* Mouse wheel scroll inside Logfile active sub-tab */
    if (s_active_tab == 1 && CheckCollisionPointRec(vm, k_log_content_rect)) {
        float wheel = GetMouseWheelMove();
        if (s_log_subtab == LOG_TAB_STORY) {
            if (wheel > 0.1f) {
                for (int m = s_story_sel_month - 1; m >= -1; m--) {
                    if (m < 0 || is_story_month_unlocked(m)) { s_story_sel_month = m; break; }
                }
            } else if (wheel < -0.1f) {
                for (int m = s_story_sel_month + 1; m < 12; m++) {
                    if (is_story_month_unlocked(m)) { s_story_sel_month = m; break; }
                }
            }
        } else if (s_log_subtab == LOG_TAB_MATCHES) {
            int filtered[PLEXUS_MATCH_LOG_MAX];
            int f_cnt = collect_filtered_matches(filtered, PLEXUS_MATCH_LOG_MAX);
            if (wheel > 0.1f && s_match_scroll > 0) s_match_scroll--;
            else if (wheel < -0.1f && s_match_scroll + 5 < f_cnt) s_match_scroll++;
        }
    }

    DrawRectangle(0, 0, 1280, 720, (Color){ 0, 0, 0, 45 });
    plexus_draw_ui_chrome(ba, st, vm);
    draw_plexus_hiscore_overlay(vm);
}

ScreenAction plexus_handle_click(Vector2 vm, int *out_clicked) {
    if (out_clicked) *out_clicked = 0;

    if (!s_intro_seen) {
        if (s_intro_timer >= PLEXUS_INTRO_BTN_DELAY &&
            CheckCollisionPointRec(vm, k_intro_continue_rect)) {
            s_intro_seen = 1;
            s_intro_reveal_t = 0.001f;
            plexus_save();
            if (out_clicked) *out_clicked = 1;
        }
        return ACT_NONE;
    }
    if (s_intro_reveal_t > 0.0f && s_intro_reveal_t < 1.0f) {
        return ACT_NONE;
    }

    /* HISCORE board is modal while open: CLOSE or a click outside dismisses it and
       nothing underneath can be hit through it. */
    if (s_hiscore_open) {
        if (CheckCollisionPointRec(vm, k_hiscore_close) ||
            !CheckCollisionPointRec(vm, k_hiscore_box)) {
            s_hiscore_open = 0;
        }
        if (out_clicked) *out_clicked = 1;
        return ACT_NONE;
    }

    /* Per-mode Match Settings / Stage drawer (Speedcoin, Action Card & Training) */
    if (s_mode_settings_open && (s_active_tab == 2 || s_active_tab == 3 || s_active_tab == 4)) {
        if (handle_mode_settings_click(vm)) {
            if (out_clicked) *out_clicked = 1;
            return ACT_NONE;
        }
        if (CheckCollisionPointRec(vm, k_mode_cfg_rect)) {
            return ACT_NONE;
        }
    }

    /* HISCORE board: left slot on SPEEDCOIN and ACTION CARD */
    if ((s_active_tab == 2 || s_active_tab == 3) && CheckCollisionPointRec(vm, k_hiscore_rect)) {
        if (s_active_tab == 2 && !is_speedcoin_bottom_unlocked()) return ACT_NONE;
        if (s_active_tab == 3 && !is_action_card_bottom_unlocked()) return ACT_NONE;
        s_mode_settings_open = 0;
        s_mode_settings_page = 0;
        s_hiscore_open = 1;
        if (out_clicked) *out_clicked = 1;
        return ACT_NONE;
    }

    /* STAGE / SETTINGS button next to START (Speedcoin, Action Card & Training) */
    if ((s_active_tab == 2 || s_active_tab == 3 || s_active_tab == 4) &&
        CheckCollisionPointRec(vm, k_settings_rect)) {
        if (s_active_tab == 2 && !is_speedcoin_bottom_unlocked()) return ACT_NONE;
        if (s_active_tab == 3 && !is_action_card_bottom_unlocked()) return ACT_NONE;
        s_mode_settings_open = !s_mode_settings_open;
        s_mode_settings_page = (s_active_tab == 2) ? 1 : 0;
        if (!s_mode_settings_open && s_active_tab != 2) plexus_save();
        if (out_clicked) *out_clicked = 1;
        return ACT_NONE;
    }

    /* Full-Panel 4-Tab Logfile Window interactions when open */
    if (s_active_tab == 1) {
        for (int t = 0; t < 4; t++) {
            if (CheckCollisionPointRec(vm, k_log_tab_rects[t])) {
                if (s_log_subtab != t) {
                    commit_viewed_story_months(-1);
                    s_log_subtab = t;
                    plexus_save();
                }
                if (out_clicked) *out_clicked = 1;
                return ACT_NONE;
            }
        }
        if (s_log_subtab == LOG_TAB_STORY) {
            for (int m = -1; m < 12; m++) {
                if (CheckCollisionPointRec(vm, story_month_btn_rect(m))) {
                    if (m >= 0 && !is_story_month_unlocked(m)) return ACT_NONE;
                    commit_viewed_story_months(m);
                    s_story_sel_month = m;
                    plexus_save();
                    if (out_clicked) *out_clicked = 1;
                    return ACT_NONE;
                }
            }
        } else if (s_log_subtab == LOG_TAB_MATCHES) {
            for (int f = 0; f < 5; f++) {
                if (CheckCollisionPointRec(vm, k_match_filter_rects[f])) {
                    s_match_filter = f;
                    s_match_scroll = 0;
                    if (out_clicked) *out_clicked = 1;
                    return ACT_NONE;
                }
            }
            int filtered[PLEXUS_MATCH_LOG_MAX];
            int f_cnt = collect_filtered_matches(filtered, PLEXUS_MATCH_LOG_MAX);
            if (CheckCollisionPointRec(vm, k_match_scroll_up_rect)) {
                if (s_match_scroll > 0) s_match_scroll--;
                if (out_clicked) *out_clicked = 1;
                return ACT_NONE;
            }
            if (CheckCollisionPointRec(vm, k_match_scroll_down_rect)) {
                if (s_match_scroll + 5 < f_cnt) s_match_scroll++;
                if (out_clicked) *out_clicked = 1;
                return ACT_NONE;
            }
        } else if (s_log_subtab == LOG_TAB_ATLAS) {
            if (CheckCollisionPointRec(vm, k_atlas_std_btn_rect)) {
                s_atlas_view_deus = 0;
                if (out_clicked) *out_clicked = 1;
                return ACT_NONE;
            }
            if (CheckCollisionPointRec(vm, k_atlas_deus_btn_rect)) {
                if (!is_deus_spine_unlocked()) return ACT_NONE;
                s_atlas_view_deus = 1;
                if (out_clicked) *out_clicked = 1;
                return ACT_NONE;
            }
            if (CheckCollisionPointRec(vm, k_atlas_spine_btn_rect)) {
                if (!is_deus_spine_unlocked()) return ACT_NONE;
                int next_deus = !is_deus_spine_active();
                set_deus_spine_mode(next_deus);
                s_atlas_view_deus = next_deus;
                plexus_save();
                if (out_clicked) *out_clicked = 1;
                return ACT_NONE;
            }
            for (int m = 0; m < 12; m++) {
                for (int r = 0; r < 4; r++) {
                    if (CheckCollisionPointRec(vm, atlas_grid_cell_rect(m, r))) {
                        s_atlas_pinned_cid = m * 4 + r;
                        if (out_clicked) *out_clicked = 1;
                        return ACT_NONE;
                    }
                }
            }
        }
    }

    if (!s_swing_active && is_month_completed(s_selected_month) && s_selected_month + 1 < 12 &&
        CheckCollisionPointRec(vm, k_next_month_rect)) {
        s_swing_active     = 1;
        s_swing_prev_month = s_selected_month;
        s_swing_elapsed    = 0.0f;
        s_flip_active      = 0;
        s_selected_month++;
        s_selected_slot    = 3;
        s_active_tab       = 0;
        s_mode_settings_open = 0;
        s_mode_settings_page = 0;
        if (!is_deus_spine_active() && is_story_month_unlocked(s_selected_month)) {
            s_story_sel_month = s_selected_month;
        }
        plexus_save();
        if (out_clicked) *out_clicked = 1;
        return ACT_NONE;
    }
    if (!s_swing_active) {
        for (int i = 0; i < 4; i++) {
            if (CheckCollisionPointRec(vm, k_card_rects[i])) {
                if (!is_slot_unlocked_in_month(s_selected_month, i)) return ACT_NONE;
                s_selected_slot = i;
                s_active_tab = 0;
                s_mode_settings_open = 0;
                s_mode_settings_page = 0;
                if (out_clicked) *out_clicked = 1;
                return ACT_NONE;
            }
        }
    }
    if (s_active_tab != 1 && CheckCollisionPointRec(vm, k_start_rect)) {
        /* Bottom tab 4 (TRAINING): launch match without spamming story/match logs on click */
        if (s_active_tab == 4) {
            plexus_tutorial_abort();
            plexus_speedcoin_abort();
            plexus_action_card_abort();
            s_mode_settings_open = 0;
            s_mode_settings_page = 0;
            s_training_result_view = 0;
            s_training_active = 1;
            if (out_clicked) *out_clicked = 1;
            return ACT_START_GAME;
        }
        if (s_active_tab == 2) {
            if (!is_speedcoin_bottom_unlocked()) return ACT_NONE;
            plexus_tutorial_abort();
            plexus_action_card_abort();
            s_mode_settings_open = 0;
            s_mode_settings_page = 0;
            s_training_active = 0;
            s_speedcoin_result_view = 0;
            s_speedcoin_active = 1;
            if (out_clicked) *out_clicked = 1;
            return ACT_START_GAME;
        }
        if (s_active_tab == 3) {
            if (!is_action_card_bottom_unlocked()) return ACT_NONE;
            plexus_tutorial_abort();
            plexus_speedcoin_abort();
            s_mode_settings_open = 0;
            s_mode_settings_page = 0;
            s_training_active = 0;
            s_action_card_active = 1;
            if (out_clicked) *out_clicked = 1;
            return ACT_START_GAME;
        }
        int cid = s_selected_month * 4 + s_selected_slot;
        if (s_active_tab == 0 && (s_swing_active || !is_month_unlocked(s_selected_month) ||
                                  !is_slot_unlocked_in_month(s_selected_month, s_selected_slot))) {
            return ACT_NONE;
        }
        /* Month 1 Card 2 (poetry ribbon, slot == 1, cid == 1): 3-round Easy Action Card tutorial */
        if (s_active_tab == 0 && s_selected_month == 0 && s_selected_slot == 1) {
            s_training_active = 0;
            plexus_speedcoin_abort();
            tut_save_telemetry();
            s_tut_active = 1; s_tut_card_id = cid; s_tut_step = TUT3_WAIT_START;
            s_action_card_active = 1;
            if (out_clicked) *out_clicked = 1;
            return ACT_START_GAME;
        }
        /* Month 1 Card 3 (second plain card, slot == 2, cid == 2): unscripted Speedcoin tutorial */
        if (s_active_tab == 0 && s_selected_month == 0 && s_selected_slot == 2) {
            s_training_active = 0;
            plexus_action_card_abort();
            s_speedcoin_result_view = 0;
            tut_save_telemetry();
            s_tut_active       = 1;
            s_tut_card_id      = cid;
            s_tut_step         = TUT2_WAIT_START;
            s_speedcoin_active = 1;
            if (out_clicked) *out_clicked = 1;
            return ACT_START_GAME;
        }
        /* Month 1 Card 4 (bottom card, slot == 3, cid == 3): Koi-Koi tutorial */
        if (s_active_tab == 0 && s_selected_month == 0 && s_selected_slot == 3) {
            s_training_active = 0;
            plexus_speedcoin_abort();
            plexus_action_card_abort();
            tut_save_telemetry();
            s_tut_active  = 1;
            s_tut_card_id = cid;
            s_tut_step    = TUT1_WAIT_DEAL;
            if (out_clicked) *out_clicked = 1;
            return ACT_START_GAME;
        }
        /* A card with a keeper transmission plays one real round before it is
           marked cleared; every other card keeps the beta instant-clear path. */
        if (story_beat_has_protocol(cid)) {
            s_training_active = 0;
            plexus_speedcoin_abort();
            plexus_action_card_abort();
            s_mode_settings_open = 0;
            s_mode_settings_page = 0;
            tut_save_telemetry();
            s_tut_active  = 1;
            s_tut_card_id = cid;
            s_tut_step    = PROTO_WAIT_DEAL;
            if (out_clicked) *out_clicked = 1;
            return ACT_START_GAME;
        }
        int deus_pass = is_deus_spine_active();
        int was_done = mark_active_spine_card_done(cid);
        if (!was_done && s_selected_slot > 0 && is_slot_unlocked_in_month(s_selected_month, s_selected_slot - 1)) {
            s_selected_slot--;
            stage_spine_flip(s_selected_month, s_selected_slot, 0.0f);
        }
        int month_done = is_month_completed(s_selected_month);
        if (!was_done) {
            char title[88], detail[112];
            snprintf(title, sizeof(title), "%s[%s] CLEARED  //  MONTH %02d CARD %d",
                     deus_pass ? "DEUS " : "", CARDS[cid].name,
                     s_selected_month + 1, (cid % 4) + 1);
            if (month_done && !deus_pass && count_mask_cards(s_finished_cards_mask) >= 48) {
                snprintf(detail, sizeof(detail),
                         "Standard Atlas 48/48 complete! Hardcore DEUS Pass unlocked in [ATLAS].");
            } else if (month_done && deus_pass && count_mask_cards(s_deus_finished_cards_mask) >= 48) {
                snprintf(detail, sizeof(detail),
                         "Hardcore Deus Atlas 48/48 complete! Full monochrome convergence achieved.");
            } else if (month_done && !deus_pass && s_selected_month == 3) {
                snprintf(detail, sizeof(detail),
                         "Month 04 complete -- SPEEDCOIN module unlocked & [NEXT] ready for Month 05.");
            } else if (month_done && !deus_pass && s_selected_month == 7) {
                snprintf(detail, sizeof(detail),
                         "Month 08 complete -- ACTION CARD module unlocked & [NEXT] ready for Month 09.");
            } else if (month_done && s_selected_month + 1 < 12) {
                snprintf(detail, sizeof(detail),
                         "All 4 cards of Month %02d cleared -- press [NEXT] above the cards.",
                         s_selected_month + 1);
            } else {
                snprintf(detail, sizeof(detail),
                         "%sAtlas %02d/48 illuminated -- next story card unlocked above.",
                         deus_pass ? "Deus " : "Standard ",
                         count_mask_cards(active_spine_mask()));
            }
            plexus_match_log_push(MLOG_SPINE, deus_pass ? "DEUS PASS" : "STORY", title, detail);
        }
        plexus_save();
        if (out_clicked) *out_clicked = 1;
        return ACT_NONE;
    }
    /* Bottom Bar: index 0 is SAVE & BACK, 1..4 are tabs, 5 is OPTION */
    if (CheckCollisionPointRec(vm, k_bottom_rects[0])) {
        s_mode_settings_open = 0;
        s_mode_settings_page = 0;
        if (out_clicked) *out_clicked = 1;
        return ACT_GOTO_MENU;
    }
    if (CheckCollisionPointRec(vm, k_bottom_rects[5])) {
        s_mode_settings_open = 0;
        s_mode_settings_page = 0;
        plexus_save();
        if (out_clicked) *out_clicked = 1;
        return ACT_GOTO_OPTIONS;
    }
    for (int i = 1; i < 5; i++) {
        if (CheckCollisionPointRec(vm, k_bottom_rects[i])) {
            if (!is_bottom_btn_unlocked(i)) {
                return ACT_NONE;
            }
            int next_tab = (i == 1 && s_active_tab == 1) ? 0 : i;
            if (next_tab != s_active_tab) {
                s_mode_settings_open = 0;
                s_mode_settings_page = 0;
                plexus_save();
            }
            s_active_tab = next_tab;
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
    int was_sc_tut = (s_tut_active && s_tut_card_id == 2);
    int was_ac_tut = (s_tut_active && s_tut_card_id == 1);
    tut_restore_telemetry(1);
    s_tut_active  = 0;
    s_tut_card_id = -1;
    s_tut_step    = TUT1_NONE;
    if (was_sc_tut) plexus_speedcoin_abort();
    if (was_ac_tut) s_action_card_active = 0;
}

int plexus_training_is_active(void) {
    return s_training_active;
}

void plexus_training_stop(void) {
    s_training_active = 0;
    s_training_result_view = 0;
}

static const Rectangle k_tr_exit_plex_rect = { 510.0f, 632.0f, 260.0f, 50.0f };

int  plexus_training_result_active(void) { return s_training_result_view; }
void plexus_training_close_result(void)  { s_training_result_view = 0; }

void plexus_training_finish(const GameState *g) {
    s_training_active = 0;
    s_training_result_view = 1;
    if (!g) return;

    int you = g->total_score[0], cpu = g->total_score[1];
    s_tr_runs++;
    if (you > cpu) s_tr_wins++;
    else if (you < cpu) s_tr_losses++;
    if (you > s_tr_best_score) s_tr_best_score = you;
    if (g->total_rounds > 0) s_tr_rounds_total += g->total_rounds;

    const char *res  = (you > cpu) ? "VICTORY" : (you < cpu) ? "DEFEAT" : "DRAW";
    const char *r_nm = ruleset_short_name(g->ruleset);
    const char *d_nm = (settings.difficulty >= DIFFICULTY_MASTER) ? "MASTER" :
                       (settings.difficulty == DIFFICULTY_HARD)   ? "HARD" : "EASY";
    int opp_num = settings.current_opponent + 1;
    int mp = speed_bar_total();

    char title[88], detail[112];
    snprintf(title, sizeof(title), "%s  //  YOU %d - CPU %d  [%s]", res, you, cpu, d_nm);
    snprintf(detail, sizeof(detail), "%s  |  %d RDS  |  Rival E-%02d  |  Speedcoins: %d MP",
             r_nm, g->total_rounds, opp_num, mp);
    plexus_match_log_push(MLOG_TRAINING, "TRAINING", title, detail);
    plexus_save();
}

ScreenAction plexus_handle_training_result_click(Vector2 vm, int *out_clicked) {
    if (out_clicked) *out_clicked = 0;
    if (!s_training_result_view || !CheckCollisionPointRec(vm, k_tr_exit_plex_rect)) return ACT_NONE;
    if (out_clicked) *out_clicked = 1;
    return ACT_GOTO_ATRIUM;
}

void plexus_draw_training_result(const BoardAssets *ba, const GameState *g, const GameStats *st, Vector2 vm) {
    static const char *const k_yaku_names[YAKU_COUNT] = {
        "GOKO", "SHIKO", "AME-SHIKO", "SANKO", "HANAMI-ZAKE", "TSUKIMI-ZAKE",
        "INO-SHIKA-CHO", "AKATAN", "AOTAN", "AKATAN+AOTAN", "TANE", "TANZAKU", "KASU"
    };
    ensure_plexus_assets();
    if (s_plexus_bg.id > 0)
        DrawTexturePro(s_plexus_bg, (Rectangle){ 0, 0, 1280, 720 }, (Rectangle){ 0, 0, 1280, 720 }, (Vector2){ 0, 0 }, 0.0f, WHITE);
    else ClearBackground(BLACK);
    DrawRectangle(0, 0, 1280, 720, (Color){ 0, 0, 0, 200 });
    const char *hdr = "TRAINING  //  STATS & TELEMETRY";
    Vector2 hts = MeasureTextEx(font_title, hdr, 28.0f, 1.4f);
    draw_text_snapped(font_title, hdr, (1280.0f - hts.x) * 0.5f, 24.0f, 28.0f, 1.4f, COLOR_PHOSPHOR);

    Rectangle lbox = { 100.0f, 76.0f, 524.0f, 530.0f }, rtop = { 656.0f, 76.0f, 524.0f, 304.0f };
    DrawRectangleRec(lbox, PLEXUS_PANEL_BG); ui_draw_panel_border(lbox, COLOR_PHOSPHOR);
    DrawRectangleRec(rtop, PLEXUS_PANEL_BG); ui_draw_panel_border(rtop, COLOR_PHOSPHOR);
    draw_text_snapped(font_title, "TRAINING SESSION  //  STATS", 118.0f, 90.0f, 17.0f, 1.2f, COLOR_PHOSPHOR);
    DrawLine(116, 118, 608, 118, COLOR_PHOSPHOR_DIM);
    draw_text_snapped(font_title, "PLEX ARCHIVE  //  INNER RECORDS", 674.0f, 90.0f, 17.0f, 1.2f, COLOR_PHOSPHOR);
    DrawLine(672, 118, 1164, 118, COLOR_PHOSPHOR_DIM);

    int rw = 0, rl = 0, rd = 0, hi_you = 0, hi_cpu = 0, y_tot = 0, top_y = -1, top_yv = 0;
    int lc = g ? ((g->ledger_count > 12) ? 12 : g->ledger_count) : 0, mp = speed_bar_total();
    for (int i = 0; i < lc; i++) {
        int w = g->ledger[i].winner, aw = g->ledger[i].award;
        if (w == 0) { rw++; if (aw > hi_you) hi_you = aw; }
        else if (w == 1) { rl++; if (aw > hi_cpu) hi_cpu = aw; }
        else rd++;
    }
    for (int y = 0; y < YAKU_COUNT; y++) {
        if (g) { y_tot += g->human_yaku_counts[y]; if (g->human_yaku_counts[y] > top_yv) { top_yv = g->human_yaku_counts[y]; top_y = y; } }
    }
    const char *r_nm = !g ? "--" : (g->ruleset == RULESET_FUDAWIKI) ? "FUDAWIKI" : (g->ruleset == RULESET_KK_ADD_10) ? "KK-ADD-10" :
                       (g->ruleset == RULESET_TOURNAMENT) ? "TOURNAMENT" : (g->ruleset == RULESET_YAKUZA) ? "YAKUZA" : "NINTENDO";
    const char *d_nm = (settings.difficulty >= DIFFICULTY_MASTER) ? "MASTER" :
                       (settings.difficulty == DIFFICULTY_HARD) ? "HARD" : "EASY";
    int opp_num = settings.current_opponent + 1;
    draw_text_snapped(font_body, TextFormat("CONFIG        :  %s | %s | %d RDS | E-%02d", r_nm, d_nm, g ? g->total_rounds : 0, opp_num),
                      118.0f, 128.0f, 18.0f, 1.1f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body, TextFormat("TOTAL POINTS  :  YOU %d PTS  |  CPU %d PTS", g ? g->total_score[0] : 0, g ? g->total_score[1] : 0), 118.0f, 158.0f, 18.0f, 1.2f, COLOR_PHOSPHOR);
    draw_text_snapped(font_body, TextFormat("ROUNDS        :  YOU %d  |  CPU %d  |  DRAW %d", rw, rl, rd), 118.0f, 188.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body, TextFormat("BEST ROUND    :  YOU %d PTS  |  CPU %d PTS", hi_you, hi_cpu), 118.0f, 218.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body, TextFormat("KOI-KOI CALLS :  YOU %d  |  CPU %d", g ? g->match_koi_calls[0] : 0, g ? g->match_koi_calls[1] : 0), 118.0f, 248.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body, TextFormat("COMBOS FORMED :  %d  [%s]", y_tot, (top_y >= 0) ? k_yaku_names[top_y] : "NONE"), 118.0f, 278.0f, 18.0f, 1.2f, COLOR_PHOSPHOR);
    draw_text_snapped(font_body, (g && g->ruleset == RULESET_KK_ADD_10) ? TextFormat("SPEEDCOINS    :  %d MP  (+%d PTS)", mp, mp / 10) : TextFormat("SPEEDCOINS    :  %d MP", mp), 118.0f, 308.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
    DrawLine(116, 344, 608, 344, COLOR_PHOSPHOR_DIM);
    draw_text_snapped(font_title, "ROUND LOG", 118.0f, 354.0f, 16.0f, 1.2f, COLOR_PHOSPHOR);
    for (int i = 0; i < lc; i++) {
        const RoundRecord *rr = &g->ledger[i];
        const char *who = (rr->winner == 0) ? "YOU" : (rr->winner == 1) ? "CPU" : "DRAW";
        draw_text_snapped(font_body, TextFormat("R%02d: %s (%d PTS)", rr->round_number, who, rr->award),
                          118.0f + (float)(i / 6) * 246.0f, 382.0f + (float)(i % 6) * 32.0f, 17.0f, 1.1f, COLOR_TEXT_INFO);
    }

    /* Plex-only archive. This panel used to print GameStats - the Pancratium career record -
       under the heading "OVERALL STATS", which read as if the Atrium owned those matches. */
    int sc_top = (s_sc_rank_count > 0) ? s_sc_ranks[0].score : 0;
    draw_text_snapped(font_body, TextFormat("TRAINING      :  %d RUNS  (%dW / %dL)", s_tr_runs, s_tr_wins, s_tr_losses), 674.0f, 128.0f, 17.0f, 1.1f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body, TextFormat("TRAINING BEST :  %d PTS  |  %d ROUNDS", s_tr_best_score, s_tr_rounds_total), 674.0f, 158.0f, 17.0f, 1.1f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body, TextFormat("ACTION CARD   :  %d RUNS  (%dW / %dL)", s_ac_runs, s_ac_wins, s_ac_losses), 674.0f, 188.0f, 17.0f, 1.1f, COLOR_PHOSPHOR);
    draw_text_snapped(font_body, TextFormat("ACTION BEST   :  %d PTS", s_ac_best_score), 674.0f, 218.0f, 17.0f, 1.1f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body, TextFormat("SPEEDCOIN     :  HI %d  [W-%02d / %d RUNS]", sc_top, s_sc_best_wave, s_sc_runs), 674.0f, 248.0f, 17.0f, 1.1f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body, TextFormat("CARDS CLEARED :  %02d/48  |  DEUS %02d/48", count_finished_cards(), count_mask_cards(s_deus_finished_cards_mask)), 674.0f, 278.0f, 17.0f, 1.1f, COLOR_PHOSPHOR);

    draw_logfile_stats_box((Rectangle){ 656.0f, 398.0f, 524.0f, 208.0f });
    draw_plexus_btn(k_tr_exit_plex_rect, "ATRIUM PLEX", 18, vm, 0);
    (void)ba;
    (void)st;   /* the result screen no longer borrows the Pancratium career record */
}

static const Rectangle k_ac_play_again_rect = { 360.0f, 632.0f, 260.0f, 50.0f };
static const Rectangle k_ac_back_plex_rect  = { 660.0f, 632.0f, 260.0f, 50.0f };

int plexus_action_card_is_active(void) {
    return s_action_card_active;
}

int plexus_action_card_result_active(void) {
    return s_action_card_result_view;
}

void plexus_action_card_close_result(void) {
    s_action_card_result_view = 0;
}

void plexus_action_card_abort(void) {
    s_action_card_active = 0;
    s_action_card_result_view = 0;
    if (s_tut_active && s_tut_card_id == 1) {
        tut_restore_telemetry(1);
        s_tut_active = 0; s_tut_card_id = -1; s_tut_step = TUT1_NONE;
    }
}

void plexus_action_card_finish_match(const GameState *g) {
    if (!g) return;
    if (s_tut_active && s_tut_card_id == 1) {
        tut_restore_telemetry(1);
        if (s_tut_step != TUT1_NONE) {
            int deus_pass = is_deus_spine_active();
            int was_done = mark_active_spine_card_done(1);
            if (!was_done && s_selected_month == 0 && is_slot_unlocked_in_month(0, 0)) {
                s_selected_slot = 0; s_active_tab = 0;
                stage_spine_flip(0, 0, 0.35f);
            }
            if (!was_done) {
                plexus_match_log_push(MLOG_SPINE, deus_pass ? "DEUS PASS" : "STORY",
                                      "[01-RP Poetry] TUTORIAL 03 CLEARED",
                                      "Action Card Audio Effect protocol finished -- Card 1 unlocked at apex.");
            }
        }
        s_tut_active = 0;
        s_tut_card_id = -1;
        s_tut_step = TUT1_NONE;
        s_action_card_active = 0;
        s_action_card_result_view = 0;
        plexus_save();
        return;
    }

    s_action_card_active = 0;
    s_action_card_result_view = 1;

    int you = g->total_score[0], cpu = g->total_score[1];
    s_ac_runs++;
    if (you > cpu) s_ac_wins++;
    else if (you < cpu) s_ac_losses++;
    if (you > s_ac_best_score) s_ac_best_score = you;

    const char *res = (you > cpu) ? "VICTORY" : (you < cpu) ? "DEFEAT" : "DRAW";
    const char *d_name = (settings.difficulty >= DIFFICULTY_MASTER) ? "MASTER" :
                         (settings.difficulty == DIFFICULTY_HARD) ? "HARD" : "EASY";
    int mp = speed_bar_total();
    int yt = 0, yp = 0, ct = 0, cp = 0, mt = 0;
    action_card_get_match_time_stats(&yt, &yp, &ct, &cp, &mt);
    s_ac_last_mp = mp;
    s_ac_last_yt = yt;
    s_ac_last_yp = yp;
    s_ac_last_ct = ct;
    s_ac_last_cp = cp;
    s_ac_last_mt = mt;

    char title[88], detail[112];
    snprintf(title, sizeof(title), "%s  //  YOU %d - CPU %d  [%s]", res, you, cpu, d_name);
    snprintf(detail, sizeof(detail), "KK-ADD-10 (%d RDS)  |  Speedcoins: %d MP (+%d)  |  Time: YOU %d / CPU %d",
             g->total_rounds, mp, mp / 10, yt, ct);
    plexus_match_log_push(MLOG_ACTION_CARD, "ACTION CARD", title, detail);
    plexus_save();
}

void plexus_draw_action_card_result(const BoardAssets *ba, const GameState *g, const GameStats *st, Vector2 vm) {
    static const char *const k_yaku_names[YAKU_COUNT] = {
        "GOKO", "SHIKO", "AME-SHIKO", "SANKO", "HANAMI-ZAKE", "TSUKIMI-ZAKE",
        "INO-SHIKA-CHO", "AKATAN", "AOTAN", "AKATAN+AOTAN", "TANE", "TANZAKU", "KASU"
    };
    ensure_plexus_assets();
    if (s_plexus_bg.id > 0)
        DrawTexturePro(s_plexus_bg, (Rectangle){ 0, 0, 1280, 720 },
                       (Rectangle){ 0, 0, 1280, 720 }, (Vector2){ 0, 0 }, 0.0f, WHITE);
    else ClearBackground(BLACK);
    DrawRectangle(0, 0, 1280, 720, (Color){ 0, 0, 0, 200 });

    int you = g ? g->total_score[0] : 0;
    int cpu = g ? g->total_score[1] : 0;
    const char *outcome = (you > cpu) ? "VICTORY" : (you < cpu) ? "DEFEAT" : "DRAW";
    char hdr[64];
    snprintf(hdr, sizeof(hdr), "ACTION CARD  //  %s", outcome);
    Vector2 hts = MeasureTextEx(font_title, hdr, 28.0f, 1.4f);
    Color hdr_col = (you < cpu) ? COLOR_RED : COLOR_PHOSPHOR;
    draw_text_snapped(font_title, hdr, (1280.0f - hts.x) * 0.5f, 24.0f, 28.0f, 1.4f, hdr_col);

    Rectangle lbox = { 100.0f, 76.0f, 524.0f, 530.0f };
    Rectangle rtop = { 656.0f, 76.0f, 524.0f, 304.0f };
    DrawRectangleRec(lbox, PLEXUS_PANEL_BG); ui_draw_panel_border(lbox, COLOR_PHOSPHOR);
    DrawRectangleRec(rtop, PLEXUS_PANEL_BG); ui_draw_panel_border(rtop, COLOR_PHOSPHOR);
    draw_text_snapped(font_title, "ACTION CARD  //  MATCH BREAKDOWN", 118.0f, 90.0f, 17.0f, 1.2f, COLOR_PHOSPHOR);
    DrawLine(116, 118, 608, 118, COLOR_PHOSPHOR_DIM);
    draw_text_snapped(font_title, "AUDIO FX  //  ARCHIVE TELEMETRY", 674.0f, 90.0f, 17.0f, 1.2f, COLOR_PHOSPHOR);
    DrawLine(672, 118, 1164, 118, COLOR_PHOSPHOR_DIM);

    int rw = 0, rl = 0, rd = 0, hi_you = 0, hi_cpu = 0, y_tot = 0, top_y = -1, top_yv = 0;
    int lc = g ? ((g->ledger_count > 12) ? 12 : g->ledger_count) : 0;
    for (int i = 0; i < lc; i++) {
        int w = g->ledger[i].winner, aw = g->ledger[i].award;
        if (w == 0) { rw++; if (aw > hi_you) hi_you = aw; }
        else if (w == 1) { rl++; if (aw > hi_cpu) hi_cpu = aw; }
        else rd++;
    }
    for (int y = 0; y < YAKU_COUNT; y++) {
        if (g) {
            y_tot += g->human_yaku_counts[y];
            if (g->human_yaku_counts[y] > top_yv) { top_yv = g->human_yaku_counts[y]; top_y = y; }
        }
    }

    const char *d_nm = (s_ac_cfg_diff >= DIFFICULTY_MASTER) ? "MASTER" :
                       (s_ac_cfg_diff == DIFFICULTY_HARD)   ? "HARD" : "EASY";
    char time_ln[128];
    action_card_format_time_line(time_ln, sizeof(time_ln),
                                 s_ac_last_yt, s_ac_last_yp, s_ac_last_ct, s_ac_last_cp, s_ac_last_mt);

    draw_text_snapped(font_body, TextFormat("CONFIG        :  KK-ADD-10  |  %s  |  %d RDS", d_nm, g ? g->total_rounds : s_ac_cfg_rounds),
                      118.0f, 128.0f, 18.0f, 1.1f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body, TextFormat("FINAL SCORE   :  YOU %d PTS  |  CPU %d PTS", you, cpu),
                      118.0f, 158.0f, 18.0f, 1.2f, COLOR_PHOSPHOR);
    draw_text_snapped(font_body, TextFormat("ROUNDS        :  YOU %d  |  CPU %d  |  DRAW %d", rw, rl, rd),
                      118.0f, 188.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body, TextFormat("BEST ROUND    :  YOU %d PTS  |  CPU %d PTS", hi_you, hi_cpu),
                      118.0f, 218.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body, TextFormat("SPEEDCOINS    :  %d MP  (+%d BONUS PTS)", s_ac_last_mp, s_ac_last_mp / 10),
                      118.0f, 248.0f, 18.0f, 1.2f, COLOR_PHOSPHOR);
    draw_text_snapped(font_body, TextFormat("COMBOS & KOI  :  %d [%s]  |  KOI %d/%d",
                                            y_tot, (top_y >= 0) ? k_yaku_names[top_y] : "NONE",
                                            g ? g->match_koi_calls[0] : 0, g ? g->match_koi_calls[1] : 0),
                      118.0f, 278.0f, 18.0f, 1.1f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body, time_ln, 118.0f, 308.0f, 16.5f, 1.0f, COLOR_PHOSPHOR);

    DrawLine(116, 344, 608, 344, COLOR_PHOSPHOR_DIM);
    draw_text_snapped(font_title, "ROUND LOG", 118.0f, 354.0f, 16.0f, 1.2f, COLOR_PHOSPHOR);
    for (int i = 0; i < lc; i++) {
        const RoundRecord *rr = &g->ledger[i];
        const char *who = (rr->winner == 0) ? "YOU" : (rr->winner == 1) ? "CPU" : "DRAW";
        draw_text_snapped(font_body, TextFormat("R%02d: %s (%d PTS)", rr->round_number, who, rr->award),
                          118.0f + (float)(i / 6) * 246.0f, 382.0f + (float)(i % 6) * 32.0f, 17.0f, 1.1f, COLOR_TEXT_INFO);
    }

    int ac_draws = s_ac_runs - s_ac_wins - s_ac_losses;
    if (ac_draws < 0) ac_draws = 0;
    float ac_wr = (s_ac_runs > 0) ? ((float)s_ac_wins / (float)s_ac_runs * 100.0f) : 0.0f;
    int aud_trig = board_get_audio_action_triggers();
    float aud_peak = board_get_audio_peak_synergy() * 100.0f;
    float aud_hold = board_get_audio_full_hold_seconds();
    int ac_sym = action_card_get_symbols_fired();
    int ac_tm  = action_card_get_lifetime_time();

    draw_text_snapped(font_body, TextFormat("ACTION RUNS   :  %d  (%dW / %dL / %dD  --  %.1f%%)",
                                            s_ac_runs, s_ac_wins, s_ac_losses, ac_draws, ac_wr),
                      674.0f, 128.0f, 17.0f, 1.1f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body, TextFormat("BEST SCORE    :  %d PTS", s_ac_best_score),
                      674.0f, 158.0f, 17.0f, 1.1f, COLOR_PHOSPHOR);
    draw_text_snapped(font_body, TextFormat("SYNERGY PEAKS :  %d BURSTS  |  %.0f%% PEAK", aud_trig, aud_peak),
                      674.0f, 188.0f, 17.0f, 1.1f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body, TextFormat("FULL METER    :  %.1fs LIFETIME HOLD", aud_hold),
                      674.0f, 218.0f, 17.0f, 1.1f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body, TextFormat("HORRORS FIRED :  %d SYMBOLS  |  %d TIME NUMS", ac_sym, ac_tm),
                      674.0f, 248.0f, 17.0f, 1.1f, COLOR_PHOSPHOR);

    DrawLine(672, 284, 1164, 284, COLOR_PHOSPHOR_DIM);
    draw_text_snapped(font_title, "ACTIVE HORROR SIGILS", 674.0f, 294.0f, 14.5f, 1.1f, COLOR_PHOSPHOR_DIM);
    if (ba) {
        for (int s = 0; s < ACTION_SYM_COUNT; s++) {
            action_card_draw_symbol_icon(ba, s, 726.0f + (float)s * 96.0f, 344.0f, 1, 0.25f);
        }
    }

    draw_logfile_stats_box((Rectangle){ 656.0f, 398.0f, 524.0f, 208.0f });
    draw_plexus_btn(k_ac_play_again_rect, "PLAY AGAIN", 18, vm, 0);
    draw_plexus_btn(k_ac_back_plex_rect,  "ATRIUM PLEX", 18, vm, 0);
    (void)st;
}

ScreenAction plexus_handle_action_card_result_click(Vector2 vm, int *out_clicked) {
    if (out_clicked) *out_clicked = 0;
    if (!s_action_card_result_view) return ACT_NONE;
    if (CheckCollisionPointRec(vm, k_ac_play_again_rect)) {
        if (!is_action_card_bottom_unlocked()) {
            s_action_card_active = 0;
            if (out_clicked) *out_clicked = 1;
            return ACT_GOTO_ATRIUM;
        }
        s_action_card_active = 1;
        if (out_clicked) *out_clicked = 1;
        return ACT_START_GAME;
    }
    if (CheckCollisionPointRec(vm, k_ac_back_plex_rect)) {
        s_action_card_active = 0;
        if (out_clicked) *out_clicked = 1;
        return ACT_GOTO_ATRIUM;
    }
    return ACT_NONE;
}

void plexus_tutorial_setup_round(GameState *g) {
    if (!g || !s_tut_active) return;

    tut_save_telemetry();
    tut_restore_telemetry(0);
    if (s_tut_card_id == 1) {
        g->ruleset = RULESET_KK_ADD_10; g->total_rounds = 3; g->dealer = 0;
        engine_start_round(g); s_tut_step = TUT3_WAIT_START; return;
    }

    if (s_tut_card_id >= 4) {
        /* Story protocol: an ordinary dealt round on the player's ruleset, one
           round long, no scripted table -- the beat is the panel, not the deal.
           Dealer is pinned to the player so the transmission always lands before
           the CPU moves. */
        g->total_rounds = 1;
        g->dealer = 0;
        engine_start_round(g);
        if (g->phase == PHASE_TURN_HAND) g->current_player = 0;
        s_tut_step = PROTO_WAIT_DEAL;
        return;
    }

    s_tut_step        = TUT1_WAIT_DEAL;
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
            st == TUT1_PANEL_KOI_DECIDE ||
            st == TUT2_PANEL_CORE ||
            st == TUT2_PANEL_SYNC ||
            st == TUT2_PANEL_PARASITE ||
            st == TUT2_PANEL_HAND2 ||
            st == TUT2_PANEL_NO_HESITATION ||
            st == TUT3_PANEL_METERS ||
            st == TUT3_PANEL_HORRORS ||
            st == PROTO_PANEL_STORY) ? 1 : 0;
}

static Rectangle tut_panel_rect_for_step(TutorialStep st) {
    if (st == TUT1_PANEL_KOI_DECIDE) return (Rectangle){ 260.0f, 26.0f, 760.0f, 178.0f };
    if (st == TUT2_PANEL_NO_HESITATION) return (Rectangle){ 266.0f, 175.0f, 748.0f, 340.0f };
    if (st >= TUT2_PANEL_CORE && st <= TUT2_PANEL_HAND2) return (Rectangle){ 410.0f, 145.0f, 748.0f, 360.0f };
    if (st == TUT3_PANEL_METERS)  return (Rectangle){ 440.0f, 142.0f, 636.0f, 382.0f };
    if (st == TUT3_PANEL_HORRORS) return (Rectangle){ 440.0f, 126.0f, 636.0f, 424.0f };
    if (st == PROTO_PANEL_STORY)  return (Rectangle){ 280.0f, 150.0f, 720.0f, 380.0f };
    return (Rectangle){ 280.0f, 150.0f, 720.0f, 380.0f };
}

int plexus_tutorial_panel_open(void) {
    return (s_tut_active && tut_step_is_frozen(s_tut_step)) ? 1 : 0;
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

    /* Order matters: the Month 01 branches below are `== 1` / `== 2`, so a new
       special card must be given its own branch ABOVE this test, or this generic
       path will swallow it. */
    if (s_tut_card_id >= 4) {
        /* WAIT_DEAL returns 0 on purpose: the table is still dealing, and
           returning 1 here would "freeze" a board that has nothing to freeze
           yet and swallow the first click of the match. */
        if (s_tut_step == PROTO_WAIT_DEAL) {
            if (board_settled && g->phase == PHASE_TURN_HAND) s_tut_step = PROTO_PANEL_STORY;
            return 0;
        }
        if (!tut_step_is_frozen(s_tut_step)) return 0;
        if (mouse_pressed) {
            Rectangle pr = tut_panel_rect_for_step(s_tut_step), cr = tut_continue_rect_for_panel(pr);
            if (CheckCollisionPointRec(vm, pr) || CheckCollisionPointRec(vm, cr)) {
                if (out_clicked) *out_clicked = 1;
                s_tut_step = PROTO_PLAY;
            }
        }
        return 1;
    }

    if (s_tut_card_id == 1) {
        if (s_tut_step == TUT3_WAIT_START && board_settled && g->phase == PHASE_TURN_HAND)
            s_tut_step = TUT3_PANEL_METERS;
        if (!tut_step_is_frozen(s_tut_step)) return 0;
        if (mouse_pressed) {
            Rectangle pr = tut_panel_rect_for_step(s_tut_step), cr = tut_continue_rect_for_panel(pr);
            if (CheckCollisionPointRec(vm, pr) || CheckCollisionPointRec(vm, cr)) {
                if (out_clicked) *out_clicked = 1;
                s_tut_step = (s_tut_step == TUT3_PANEL_METERS) ? TUT3_PANEL_HORRORS : TUT3_FREE_PLAY;
            }
        }
        return 1;
    }

    if (s_tut_card_id == 2) {
        if (speed_mode_clock() <= 0.0f) return 0;
        if (s_tut_step == TUT2_WAIT_START && board_settled && g->phase == PHASE_TURN_HAND && g->current_player == 0)
            s_tut_step = TUT2_PANEL_CORE;
        else if (s_tut_step == TUT2_WAIT_MOVE4 && board_settled && g->phase == PHASE_TURN_HAND && g->current_player == 0 && g->hand_count[0] <= 4)
            s_tut_step = TUT2_PANEL_SYNC;
        else if (s_tut_step == TUT2_PLAY_TO_WAVE_END && g->hand_count[0] == 0 && g->phase != PHASE_HAND_CHOICE &&
                 !(anim && anim->active && anim->type == ANIM_CAPTURE_FLY))
            s_tut_step = TUT2_PANEL_HAND2;
        else if (s_tut_step == TUT2_WAIT_SCORE_SCREEN && g->phase == PHASE_ROUND_END && overlay_alpha >= 0.85f)
            s_tut_step = TUT2_PANEL_NO_HESITATION;
        if (!tut_step_is_frozen(s_tut_step)) return 0;
        if (mouse_pressed) {
            Rectangle pr = tut_panel_rect_for_step(s_tut_step), cr = tut_continue_rect_for_panel(pr);
            if (CheckCollisionPointRec(vm, pr) || CheckCollisionPointRec(vm, cr)) {
                if (out_clicked) *out_clicked = 1;
                if (s_tut_step == TUT2_PANEL_CORE)               s_tut_step = TUT2_WAIT_MOVE4;
                else if (s_tut_step == TUT2_PANEL_SYNC)          s_tut_step = TUT2_PANEL_PARASITE;
                else if (s_tut_step == TUT2_PANEL_PARASITE)      s_tut_step = TUT2_PLAY_TO_WAVE_END;
                else if (s_tut_step == TUT2_PANEL_HAND2)         s_tut_step = TUT2_WAIT_SCORE_SCREEN;
                else if (s_tut_step == TUT2_PANEL_NO_HESITATION) s_tut_step = TUT2_FREE_PLAY;
            }
        }
        return 1;
    }

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
                if (!board_table_skin_active())
                    DrawRectangleLinesEx((Rectangle){ hr.x - 3.0f, hr.y - 3.0f, hr.width + 6.0f, hr.height + 6.0f },
                                         2.5f, COLOR_PHOSPHOR);
            }
        }
        Rectangle fr = motion_get_rect(target_field_cid);
        if (fr.width <= 0.0f) fr = board_get_field_card_rect_of(g, target_field_cid);
        if (fr.width > 0.0f) {
            board_draw_card_glow(fr, pulse);
            if (!board_table_skin_active())
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
        DrawRectangle(0, 0, 1280, 720, (Color){ 0, 0, 0, (s_tut_step == TUT2_PANEL_NO_HESITATION) ? 110 : 145 });
    }
    if (s_tut_card_id == 2 && s_tut_step != TUT2_PANEL_NO_HESITATION) {
        float pulse = 0.55f + 0.45f * sinf((float)GetTime() * 5.0f);
        if (s_tut_step == TUT2_PANEL_CORE || s_tut_step == TUT2_PANEL_SYNC || s_tut_step == TUT2_PANEL_PARASITE) {
            Rectangle hl = (s_tut_step == TUT2_PANEL_CORE) ? (Rectangle){ 9.0f, 9.0f, 261.0f, 205.0f } :
                           (s_tut_step == TUT2_PANEL_SYNC) ? (Rectangle){ 1157.0f, 659.0f, 118.0f, 46.0f } :
                                                             (Rectangle){ 326.0f, 258.0f, 84.0f, 132.0f };
            Color hc = (s_tut_step == TUT2_PANEL_PARASITE) ? COLOR_RED : COLOR_PHOSPHOR;
            DrawRectangleLinesEx(hl, 3.0f, hc);
            hc.a = (unsigned char)(58.0f * pulse);
            DrawRectangleRec(hl, hc);
        } else if (s_tut_step == TUT2_PANEL_HAND2) {
            Rectangle sr = { 330.0f, 262.0f, 76.0f, 124.0f }, fb = { 326.0f, 258.0f, 84.0f, 132.0f };
            board_draw_card_glow(sr, 0.72f + 0.28f * pulse);
            if (ba) board_draw_card_back(ba, sr);
            DrawRectangleLinesEx(fb, 2.0f, COLOR_PHOSPHOR);
            ui_theme_set_panel_flare_glow(fb, 1.0f);
            ui_theme_draw_panel_flares(fb);
        }
    } else if (s_tut_card_id == 1) {
        float pulse = 0.55f + 0.45f * sinf((float)GetTime() * 5.0f);
        Rectangle hl_l = { 402.0f, 129.0f, 38.0f, 309.0f }, hl_r = { 1076.0f, 136.0f, 26.0f, 302.0f };
        Color lc = COLOR_PHOSPHOR, rc = { 224, 136, 146, 255 };
        DrawRectangleLinesEx(hl_l, 2.5f, lc); lc.a = (unsigned char)(52.0f * pulse); DrawRectangleRec(hl_l, lc);
        DrawRectangleLinesEx(hl_r, 2.5f, rc); rc.a = (unsigned char)(56.0f * pulse); DrawRectangleRec(hl_r, rc);
    }

    Rectangle pr = tut_panel_rect_for_step(s_tut_step);
    Rectangle cr = tut_continue_rect_for_panel(pr);
    DrawRectangleRec(pr, PLEXUS_PANEL_BG);
    ui_draw_panel_border(pr, COLOR_PHOSPHOR);

    float pad = (s_tut_card_id == 1) ? 16.0f : (s_tut_card_id == 2) ? 20.0f : 26.0f;
    float x = pr.x + pad;
    float y = pr.y + 18.0f;

    if (s_tut_step == PROTO_PANEL_STORY) {
        const StoryBeat *bt = story_beat(s_tut_card_id);
        if (bt) {
            const Opponent *kp = campaign_get_opponent(s_tut_card_id / 4);
            char hdr[96];
            snprintf(hdr, sizeof(hdr), "%s  //  TRANSMISSION", kp->name);
            draw_text_snapped(font_title, hdr, x, y, 18.0f, 1.3f, COLOR_PHOSPHOR);
            DrawLine((int)x, (int)(y + 30.0f), (int)(pr.x + pr.width - pad), (int)(y + 30.0f), COLOR_PHOSPHOR_DIM);
            for (int i = 0; i < 6 && bt->intrusion[i]; i++) {
                int last = (i == 5 || !bt->intrusion[i + 1]);
                draw_text_snapped(font_body, bt->intrusion[i], x, y + 52.0f + (float)i * 28.0f,
                                  18.0f, 1.2f, last ? COLOR_PHOSPHOR : COLOR_TEXT_INFO);
            }
            draw_text_snapped(font_body, TextFormat("PROTOCOL %02d  //  %s  //  ONE ROUND",
                                                    s_tut_card_id + 1, CARDS[s_tut_card_id].name),
                              x, pr.y + pr.height - 82.0f, 15.0f, 1.05f, COLOR_PHOSPHOR_DIM);
        }
    } else if (s_tut_step == TUT1_PANEL_INTRO) {
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
                          x, y + 74.0f, 18.0f, 1.2f, COLOR_PHOSPHOR);
    } else if (s_tut_step == TUT2_PANEL_CORE) {
        draw_text_snapped(font_body, "> Objective: System Saturation before System Collapse.", x, y, 18.0f, 1.2f, COLOR_PHOSPHOR);
        DrawLine((int)x, (int)(y + 32.0f), (int)(pr.x + pr.width - pad), (int)(y + 32.0f), COLOR_PHOSPHOR_DIM);
        draw_text_snapped(font_title, "// THE CORE PRINCIPLE //", x, y + 52.0f, 20.0f, 1.3f, COLOR_PHOSPHOR);
        draw_text_snapped(font_body, "You are playing against the death of the machine. 180 seconds remain.", x, y + 96.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body, "No opponent. Just the Construct and your synapses.", x, y + 128.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
    } else if (s_tut_card_id == 2) {
        static const struct { TutorialStep st; const char *hdr; int hi; const char *ln[8]; } k_p2[4] = {
            { TUT2_PANEL_SYNC, "// SYNCHRONIZATION //", 2, {
                "Every single move demands absolute neural precision.", "",
                "- The SPEEDBAR extracts your reaction speed (1.00s limit).",
                "- If you MISPLACE a card or hesitate too long:",
                "  -> SHOCK: -10 seconds drained from your remaining lifetime (timer).",
                "  -> FRACTURE: Your \"Perfect Serie\" breaks instantly.", NULL, NULL } },
            { TUT2_PANEL_PARASITE, "// ANTI-PARASITE PROTOCOL //", 7, {
                "The Construct detects blind firing and reckless harvesting.", "",
                "- Three consecutive FAILURES (Miss Hits) activate INVERSION.",
                "- The Atrium constricts:",
                "  Instead of dispensing new fragments, the STOCK pile ({330, 262})",
                "  CONSUMES an already captured field card back into its maw.", "",
                "- Land ONE precise match to calm the system and restore normal stock flow." } },
            { TUT2_PANEL_HAND2, "// THE SECOND MATURATION (HAND 2) //", 2, {
                "A standard cycle (Wave) expires after 8 moves. Unless you feed the core.", "",
                "- 30 SPEEDCOINS within a single Wave.",
                "- Upon hitting this threshold, the STOCK PILE begins to GLOW.",
                "- Rotating phosphor dots crawl around the pasteboard matrix.", NULL, NULL, NULL } },
            { TUT2_PANEL_NO_HESITATION, "// NO HESITATION //", 4, {
                "In this mode, there is no \"KOI-KOI\" and no \"SHOBU\".", "",
                "- The Construct does not deliberate. It only eats time.",
                "- Your Yaku (combinations) form organically in the background.",
                "- They are harvested and turned into points only when the Wave dies.", NULL, NULL, NULL } }
        };
        for (int p = 0; p < 4; p++) {
            if (k_p2[p].st != s_tut_step) continue;
            draw_text_snapped(font_title, k_p2[p].hdr, x, y, 20.0f, 1.3f, COLOR_PHOSPHOR);
            DrawLine((int)x, (int)(y + 32.0f), (int)(pr.x + pr.width - pad), (int)(y + 32.0f), COLOR_PHOSPHOR_DIM);
            float ly = y + 48.0f;
            for (int l = 0; l < 8 && k_p2[p].ln[l]; l++) {
                if (k_p2[p].ln[l][0] == '\0') { ly += 12.0f; continue; }
                draw_text_snapped(font_body, k_p2[p].ln[l], x, ly, 18.0f, 1.2f,
                                  (l == k_p2[p].hi) ? COLOR_PHOSPHOR : COLOR_TEXT_INFO);
                ly += 29.0f;
            }
        }
    } else if (s_tut_step == TUT3_PANEL_METERS) {
        Color wine_hi = { 224, 136, 146, 255 };
        draw_text_snapped(font_body, "===============================================", x, y - 2.0f, 15.0f, 1.0f, COLOR_PHOSPHOR_DIM);
        draw_text_snapped(font_title, "[ATRIUM PLEX \"ACTION CARD AUDIO EFFECT\" /ACAE/]", x, y + 16.0f, 17.0f, 1.0f, COLOR_PHOSPHOR);
        draw_text_snapped(font_body, "===============================================", x, y + 36.0f, 15.0f, 1.0f, COLOR_PHOSPHOR_DIM);
        draw_text_snapped(font_body, "This is no longer classic Hanafuda. It is an anatomical conduit.", x, y + 60.0f, 17.0f, 1.1f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body, "Balance the twin bio-meters:", x, y + 90.0f, 17.0f, 1.1f, COLOR_PHOSPHOR);
        draw_text_snapped(font_title, "> LEFT METER:", x, y + 120.0f, 16.0f, 1.1f, COLOR_PHOSPHOR);
        draw_text_snapped(font_body, "Audio Pulse. Heavy beats shifts the badge:", x, y + 142.0f, 17.0f, 1.1f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body, "[Skull -> Hand -> Reverse Arrow -> One Eye -> Gold Coin]", x, y + 166.0f, 17.0f, 1.1f, COLOR_PHOSPHOR);
        draw_text_snapped(font_title, "> RIGHT METER:", x, y + 198.0f, 16.0f, 1.1f, wine_hi);
        draw_text_snapped(font_body, "Wine-Red Reservoir. Harvests vital fluid from captures.", x, y + 220.0f, 17.0f, 1.1f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body, "[75%): start arming audio badge] [50%: critical: armed badge activation]", x, y + 244.0f, 16.0f, 1.0f, wine_hi);
    } else if (s_tut_step == TUT3_PANEL_HORRORS) {
        static const struct { const char *lbl; const char *d1; const char *d2; } k_h[5] = {
            { "SKULL:",         "Dissolves half the field back into the bone-deck.", NULL },
            { "HAND:",          "Parasitically swaps your hand with the CPU.", NULL },
            { "REVERSE ARROW:", "Rips half of all captured cards back into the stock.", NULL },
            { "ONE EYE:",       "Calcifies 2 field cards face-down. Tooltips off.", NULL },
            { "GOLD COIN:",     "Devours the ceiling counter. Converts 100 ticks", "into 10 PTS for the active player." }
        };
        draw_text_snapped(font_title, "ENERGY BELOW 50%. THE ARMED INFECTION FIRES!", x, y, 18.0f, 1.1f, (Color){ 224, 136, 146, 255 });
        DrawLine((int)x, (int)(y + 28.0f), (int)(pr.x + pr.width - pad), (int)(y + 28.0f), COLOR_PHOSPHOR_DIM);
        draw_text_snapped(font_body, "The sigil erupts through the floor, striking whoever is currently", x, y + 40.0f, 17.0f, 1.1f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body, "pulling the turn levers (YOU or CPU). Control your tempo.", x, y + 64.0f, 17.0f, 1.1f, COLOR_TEXT_INFO);
        draw_text_snapped(font_title, "THE FIVE HORRORS OF THE PLEXUS:", x, y + 98.0f, 17.0f, 1.1f, COLOR_PHOSPHOR);
        for (int s = 0; s < 5; s++) {
            float ry = y + 132.0f + (float)s * 38.0f;
            action_card_draw_symbol_icon(ba, s, x + 16.0f, ry + 10.0f, 1, 0.0f);
            draw_text_snapped(font_body, k_h[s].lbl, x + 40.0f, ry + 2.0f, 16.0f, 1.0f, COLOR_PHOSPHOR);
            draw_text_snapped(font_body, k_h[s].d1,  x + 188.0f, ry + 2.0f, 16.0f, 1.0f, COLOR_TEXT_INFO);
            if (k_h[s].d2) draw_text_snapped(font_body, k_h[s].d2, x + 188.0f, ry + 22.0f, 16.0f, 1.0f, COLOR_TEXT_INFO);
        }
    }

    draw_plexus_btn(cr, "[CONTINUE]", 16, vm, CheckCollisionPointRec(vm, pr));
}

void plexus_tutorial_finish_match(void) {
    if (!s_tut_active) return;
    tut_restore_telemetry(1);
    int cid = (s_tut_card_id >= 0 && s_tut_card_id < 48) ? s_tut_card_id : 3;
    int deus_pass = is_deus_spine_active();
    int was_done = mark_active_spine_card_done(cid);
    int m = cid / 4, sl = cid % 4;
    if (!was_done && sl > 0 && s_selected_month == m && is_slot_unlocked_in_month(m, sl - 1)) {
        s_selected_slot = sl - 1;
        s_active_tab = 0;
        stage_spine_flip(m, sl - 1, 0.35f);
    }
    s_tut_active  = 0;
    s_tut_card_id = -1;
    s_tut_step    = TUT1_NONE;

    if (!was_done) {
        char title[88];
        const char *detail = "Standard Koi-Koi rules conditioning finished -- Card 3 unlocked.";
        const StoryBeat *bt = story_beat(cid);
        if (cid >= 4) {
            snprintf(title, sizeof(title), "[%s] PROTOCOL CLEARED", CARDS[cid].name);
            if (bt && bt->trace[0]) detail = bt->trace[0];
            else detail = "Story protocol finished -- next story card unlocked.";
        } else {
            snprintf(title, sizeof(title), "[%s] TUTORIAL 01 CLEARED", CARDS[cid].name);
        }
        plexus_match_log_push(MLOG_SPINE, deus_pass ? "DEUS PASS" : "STORY", title, detail);
    }
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

    if (w <= 1) {
        if (s_tut_active && s_tut_card_id == 2) {
            tut_save_telemetry();
            tut_restore_telemetry(0);
            s_tut_step = TUT2_WAIT_START;
        }
        speed_mode_begin();
    } else {
        speed_mode_begin_wave(w);
    }
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

int plexus_speedcoin_reverse_draw_start(GameState *g, int *out_cid, Vector2 *out_from) {
    if (!g || g->phase != PHASE_TURN_DRAW || g->field_count <= 0 || g->stock_count >= 48) return 0;
    int cid = g->field[0];
    Rectangle r = board_get_field_card_rect_of(g, cid);
    if (out_cid) *out_cid = cid;
    if (out_from) *out_from = (r.width > 0.0f) ? (Vector2){ r.x, r.y } : (Vector2){ 710.0f, 240.0f };
    for (int i = 0; i < g->field_count - 1; i++) g->field[i] = g->field[i + 1];
    g->field_count--;
    memmove(&g->stock[1], &g->stock[0], (size_t)g->stock_count * sizeof(int));
    g->stock[0] = cid;
    g->stock_count++;
    return 1;
}

void plexus_speedcoin_reverse_draw_finish(GameState *g) {
    if (!g || g->phase != PHASE_TURN_DRAW) return;
    g->has_pending = 0;
    g->last_captured_count = 0;
    yaku_snapshot(g->ruleset, g->captured[0], g->captured_count[0], g->last_yaku_points[0]);
    g->last_base[0] = score_captures_base(g->ruleset, g->captured[0], g->captured_count[0]);
    if (g->hand_count[0] <= 0) {
        g->current_round_record.winner = -1;
        g->current_round_record.draw_reason = 1;
        g->phase = PHASE_ROUND_END;
    } else {
        g->current_player = 0;
        g->phase = PHASE_TURN_HAND;
    }
}

void plexus_speedcoin_finish_run(int final_score, int waves, int run_coins, int run_bonus_pts, int run_best_streak) {
    if (s_tut_active && s_tut_card_id == 2) {
        tut_restore_telemetry(1);
        if (s_tut_step != TUT1_NONE) {
            int deus_pass = is_deus_spine_active();
            int was_done = mark_active_spine_card_done(2);
            if (!was_done && s_selected_month == 0 && is_slot_unlocked_in_month(0, 1)) {
                s_selected_slot = 1;
                s_active_tab = 0;
                stage_spine_flip(0, 1, 0.35f);
            }
            if (!was_done) {
                plexus_match_log_push(MLOG_SPINE, deus_pass ? "DEUS PASS" : "STORY",
                                      "[01-P1 Plain] TUTORIAL 02 CLEARED",
                                      "Speedcoin Saturation protocol finished -- Card 2 unlocked.");
            }
        }
        s_tut_active = 0;
        s_tut_card_id = -1;
        s_tut_step = TUT1_NONE;
        s_speedcoin_active = 0;
        s_speedcoin_result_view = 0;
        plexus_save();
        return;
    }

    s_sc_last_score = (final_score > 0) ? final_score : 0;
    s_sc_last_waves = (waves > 0) ? waves : 1;
    s_sc_last_coins = (run_coins > 0) ? run_coins : 0;
    s_sc_last_bonus = (run_bonus_pts > 0) ? run_bonus_pts : 0;
    s_sc_last_streak = (run_best_streak > 0) ? run_best_streak : 0;
    speed_mode_get_run_stats(&s_sc_last_drops, &s_sc_last_avg_sec, &s_sc_last_avg_cov);
    s_sc_runs++;
    if (s_sc_last_waves > s_sc_best_wave) s_sc_best_wave = s_sc_last_waves;
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

    char title[88], detail[112];
    if (s_sc_last_rank_idx >= 0) {
        snprintf(title, sizeof(title), "%d PTS  //  WAVE %02d  [RANK #%02d]",
                 s_sc_last_score, s_sc_last_waves, s_sc_last_rank_idx + 1);
    } else {
        snprintf(title, sizeof(title), "%d PTS  //  WAVE %02d  [180s RUN]",
                 s_sc_last_score, s_sc_last_waves);
    }
    snprintf(detail, sizeof(detail), "Speedcoin Bonus: +%d PTS (%d MP)  |  Best Serie: x%d  |  Drops: %d",
             s_sc_last_bonus, s_sc_last_coins, s_sc_last_streak, s_sc_last_drops);
    plexus_match_log_push(MLOG_SPEEDCOIN, "SPEEDCOIN", title, detail);

    s_speedcoin_active = 0;
    s_speedcoin_result_view = 1;
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
    int bw = (s_sc_best_wave > s_sc_last_waves) ? s_sc_best_wave : s_sc_last_waves;
    draw_text_snapped(font_body, TextFormat("COMBO POINTS   :  %d PTS  (KK-ADD-10)", combo_pts),
                      run_box.x + 18.0f, run_box.y +  50.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body, TextFormat("SPEEDCOINS     :  %d MP  (+%d BONUS PTS)", s_sc_last_coins, s_sc_last_bonus),
                      run_box.x + 18.0f, run_box.y +  82.0f, 18.0f, 1.2f, COLOR_PHOSPHOR);
    draw_text_snapped(font_body, TextFormat("WAVES REACHED  :  WAVE %d  [BEST W-%02d]", s_sc_last_waves, bw),
                      run_box.x + 18.0f, run_box.y + 114.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
    draw_text_snapped(font_body, TextFormat("RUN BEST SERIE :  x%d PERFECT  [%d DROPS]", s_sc_last_streak, s_sc_last_drops),
                      run_box.x + 18.0f, run_box.y + 146.0f, 18.0f, 1.2f, COLOR_TEXT_INFO);
    if (s_sc_last_drops > 0) {
        draw_text_snapped(font_body, TextFormat("RUN SPEED/ACC  :  %.2fs  //  %.1f%% OVERLAP", s_sc_last_avg_sec, s_sc_last_avg_cov),
                          run_box.x + 18.0f, run_box.y + 178.0f, 18.0f, 1.2f, COLOR_PHOSPHOR);
    } else {
        draw_text_snapped(font_body, "RUN SPEED/ACC  :  --.--s  //  --.-% OVERLAP",
                          run_box.x + 18.0f, run_box.y + 178.0f, 18.0f, 1.2f, COLOR_TEXT_INFO_DIM);
    }

    /* Right Bottom Sub-Box: Exact Logfile Stats Box */
    Rectangle log_stats_r = { 656.0f, 394.0f, 464.0f, 208.0f };
    draw_logfile_stats_box(log_stats_r);

    draw_plexus_btn(k_sc_play_again_rect, "PLAY AGAIN", 18, vm, 0);
    draw_plexus_btn(k_sc_back_plex_rect,  "ATRIUM PLEX", 18, vm, 0);
    (void)ba;
    (void)st;
}

ScreenAction plexus_handle_speedcoin_result_click(Vector2 vm, int *out_clicked) {
    if (out_clicked) *out_clicked = 0;
    if (!s_speedcoin_result_view) return ACT_NONE;
    if (CheckCollisionPointRec(vm, k_sc_play_again_rect)) {
        if (!is_speedcoin_bottom_unlocked()) {
            s_speedcoin_active = 0;
            if (out_clicked) *out_clicked = 1;
            return ACT_GOTO_ATRIUM;
        }
        s_speedcoin_active = 1;
        if (out_clicked) *out_clicked = 1;
        return ACT_START_GAME;
    }
    if (CheckCollisionPointRec(vm, k_sc_back_plex_rect)) {
        s_speedcoin_active = 0;
        if (out_clicked) *out_clicked = 1;
        return ACT_GOTO_ATRIUM;
    }
    return ACT_NONE;
}
