#ifndef STORAGE_H
#define STORAGE_H

#include "rules.h"
#include "ai.h"

#define MUSIC_FOLDER_CAP 1024

/* Card table skins, in picker order. 0 is the CRT table the game ships with and stays
   the default; 1.. are forged tables from tables/table_<name>/. Index 1 (CITY) is still
   a placeholder; index 2 (CASTLE) is forged and renders in ui_board.c (beta, in live
   test - tables/table_castle/). Names live in campaign.c next to the picker. */
#define TABLE_DESIGN_CRT    0
#define TABLE_DESIGN_CITY   1   /* placeholder - not forged yet, renders as CRT */
#define TABLE_DESIGN_CASTLE 2
#define TABLE_DESIGN_COUNT  3

typedef struct {
    char music_folder[MUSIC_FOLDER_CAP]; /* empty: built-in match music */
    int rounds;            /* 1..12, default 6 */
    Difficulty difficulty; /* EASY, HARD, or MASTER */
    int master_timer;      /* 15 or 30 (seconds), default 15 */
    Ruleset ruleset;       /* NINTENDO, FUDAWIKI, KK_ADD_10 */
    int background;        /* 0..4 or 6..7; 5 is reserved for Deus */
    float bgm_volume;      /* 0.0 .. 1.0, default 1.0f */
    float effect_chorus;   /* 0..1 capture/Koi-Koi effect controls */
    float effect_bass;
    float effect_reverb;
    float sfx_volume;      /* 0.0 .. 1.0, default 0.5f */
    int anim_speed;        /* 0: Slow (600ms), 1: Normal (400ms), 2: Fast (250ms) */
    int terminal_color;    /* 0: Green, 1: Amber, 2: Teal, 3: Crimson, 4: Violet */
    int fullscreen;        /* 0: Windowed, 1: Borderless */
    int bloom;             /* 0: OFF, 1: MID, 2: FULL (default 1) */
    int crt_filter;        /* 0: OFF, 1: LOW, 2: MID, 3: HIGH (default 2) */
    float gamma;           /* 0.5 .. 2.0, default 1.0f */
    int unlocked_opponents;/* 1..12, default 1 */
    int current_opponent;  /* 0..11, default 0 */
    unsigned long long cleared_mask; /* 3 bits per opponent: bit0=bronze(Easy), bit1=silver(Hard), bit2=gold(Master) */
    int table_design;      /* card table skin (TABLE_DESIGN_* above): 0 = CRT (shipped,
                              default). CASTLE renders in ui_board.c; CITY is not forged
                              yet and still renders as CRT. */
} GameSettings;

typedef enum {
    STORAGE_PROFILES_LEGACY = 0,
    STORAGE_PROFILES_NEW,
    STORAGE_PROFILES_READY
} StorageProfileMode;

typedef struct {
    int valid;
    Difficulty difficulty;
    Ruleset ruleset;
    int master_timer;
    int terminal_color;
    int background;
    int current_opponent;
} StorageDeusSessionSnapshot;

typedef struct {
    int matches_played;
    int matches_won;
    int matches_lost;
    int matches_drawn;
    int rounds_won_you;
    int rounds_won_cpu;
    int points_for;
    int points_against;
    int best_award;        /* Best win (highest total score in a match won) */
    int highest_round_score; /* Highest single-round awarded score */
    int koi_calls_you;
    int koi_calls_cpu;
    int easy_matches;
    int hard_matches;
    int master_matches;
    int current_streak;
    int best_streak;
    int yaku_counts[YAKU_COUNT];
} GameStats;

typedef struct {
    char lines[10][128];
    int count;
} MatchHistory;

const char *storage_get_favorite_combo(const GameStats *st);

/* Load settings / stats / history with safe fallbacks */
void storage_load_settings(GameSettings *s);
void storage_save_settings(const GameSettings *s);
void storage_set_deus_session_snapshot(const StorageDeusSessionSnapshot *snapshot);

/* Profile slots are owned by storage; live files remain the existing loaders' interface. */
StorageProfileMode storage_profiles_init(void);
StorageProfileMode storage_profiles_mode(void);
int storage_profiles_active_slot(void);
int storage_profiles_slot_exists(int slot);
int storage_profiles_create(int slot);
int storage_profiles_switch(int slot);
int storage_profiles_erase(int slot);
int storage_profiles_flush_active(void);
int storage_profiles_commit_live_temp(const char *filename, const char *temp_path);

void storage_load_stats(GameStats *st);
void storage_save_stats(const GameStats *st);
void storage_reset_stats(GameStats *st);

void storage_load_history(MatchHistory *h);
void storage_add_history(MatchHistory *h, const char *record_line);

/* Saves all files and writes stats_report.txt */
void storage_save_all(const GameSettings *s, const GameStats *st, const MatchHistory *h);
void storage_write_report(const GameStats *st, const MatchHistory *h);
void storage_record_match(const GameState *g, const GameSettings *s, GameStats *st, MatchHistory *h);

#endif /* STORAGE_H */
