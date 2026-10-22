#include "storage.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void get_app_path(const char *filename, char *out_path, size_t max_len) {
    const char *base = GetApplicationDirectory();
    if (base && base[0] != '\0') {
        snprintf(out_path, max_len, "%s%s", base, filename);
        if (FileExists(out_path)) return;
    }
    snprintf(out_path, max_len, "./%s", filename);
}

static void safe_write_text(const char *filename, const char *content) {
    char target[512], tmp[520];
    get_app_path(filename, target, sizeof(target));
    snprintf(tmp, sizeof(tmp), "%s.tmp", target);

    FILE *f = fopen(tmp, "w");
    if (!f) return;
    fputs(content, f);
    fclose(f);

    remove(target);
    rename(tmp, target);
}

void storage_load_settings(GameSettings *s) {
    s->music_folder[0] = '\0';
    s->rounds = 2;
    s->difficulty = DIFFICULTY_EASY;
    s->master_timer = 15;
    s->ruleset = RULESET_KK_ADD_10;
    s->background = 0;
#ifdef __EMSCRIPTEN__
    s->bgm_volume = 0.50f; /* Quieter fresh browser profile; saved settings below still win. */
#else
    s->bgm_volume = 1.00f;
#endif
    s->sfx_volume = 0.50f;
    s->effect_chorus = 0.0f;
    s->effect_bass = 0.0f;
    s->effect_reverb = 0.50f;
    s->anim_speed = 0;
    s->terminal_color = 2;
    s->fullscreen = 0;
    s->bloom = 1;
    s->crt_filter = 2;
    s->gamma = 1.00f;
    s->unlocked_opponents = 1;
    s->current_opponent = 0;
    s->cleared_mask = 0;

    char path[512];
    get_app_path("settings.txt", path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[MUSIC_FOLDER_CAP + 64];
    while (fgets(line, sizeof(line), f)) {
        if (!strchr(line, '\n') && !feof(f)) {
            int ch;
            while ((ch = fgetc(f)) != '\n' && ch != EOF) {}
            continue; /* Ignore overlong settings rather than truncating a path. */
        }
        if (strncmp(line, "music_folder=", 13) == 0) {
            line[strcspn(line, "\r\n")] = '\0';
            if (strlen(line + 13) < sizeof(s->music_folder))
                strcpy(s->music_folder, line + 13);
            continue;
        }
        char k[64], v[64];
        if (sscanf(line, "%63[^=]=%63s", k, v) == 2) {
            if (strcmp(k, "rounds") == 0) {
                int r = atoi(v);
                if (r >= 1 && r <= 12) s->rounds = r;
            } else if (strcmp(k, "difficulty") == 0) {
                if (strcmp(v, "deus") == 0) s->difficulty = DIFFICULTY_DEUS;
                else if (strcmp(v, "master") == 0) s->difficulty = DIFFICULTY_MASTER;
                else if (strcmp(v, "hard") == 0) s->difficulty = DIFFICULTY_HARD;
                else s->difficulty = DIFFICULTY_EASY;
            } else if (strcmp(k, "master_timer") == 0) {
                int mt = atoi(v);
                s->master_timer = (mt == 30) ? 30 : 15;
            } else if (strcmp(k, "ruleset") == 0) {
                if (strcmp(v, "fudawiki") == 0) s->ruleset = RULESET_FUDAWIKI;
                else if (strcmp(v, "kk_add_10") == 0 || strcmp(v, "kk-add-10") == 0) s->ruleset = RULESET_KK_ADD_10;
                else if (strcmp(v, "tournament") == 0) s->ruleset = RULESET_TOURNAMENT;
                else if (strcmp(v, "yakuza") == 0) s->ruleset = RULESET_YAKUZA;
                else s->ruleset = RULESET_NINTENDO;
            } else if (strcmp(k, "background") == 0) {
                int bg = atoi(v);
                if (bg >= 0 && bg < 8) s->background = bg;
            } else if (strcmp(k, "bgm_volume") == 0) {
                float vol = (float)atof(v);
                s->bgm_volume = (vol < 0.0f) ? 0.0f : (vol > 1.0f) ? 1.0f : vol;
            } else if (strcmp(k, "effect_chorus") == 0 || strcmp(k, "effect_bass") == 0 || strcmp(k, "effect_reverb") == 0) {
                float value = strtof(v, NULL);
                if (!(value >= 0.0f)) value = 0.0f; /* Includes NaN. */
                if (value > 1.0f) value = 1.0f;
                if (strcmp(k, "effect_chorus") == 0) s->effect_chorus = value;
                else if (strcmp(k, "effect_bass") == 0) s->effect_bass = value;
                else s->effect_reverb = value;
            } else if (strcmp(k, "sfx_volume") == 0) {
                float vol = (float)atof(v);
                s->sfx_volume = (vol < 0.0f) ? 0.0f : (vol > 1.0f) ? 1.0f : vol;
            } else if (strcmp(k, "anim_speed") == 0) {
                int sp = atoi(v);
                if (sp >= 0 && sp <= 2) s->anim_speed = sp;
            } else if (strcmp(k, "terminal_color") == 0 || strcmp(k, "theme") == 0) {
                if (strcmp(v, "amber") == 0 || strcmp(v, "1") == 0) s->terminal_color = 1;
                else if (strcmp(v, "teal") == 0 || strcmp(v, "2") == 0) s->terminal_color = 2;
                else if (strcmp(v, "crimson") == 0 || strcmp(v, "3") == 0) s->terminal_color = 3;
                else if (strcmp(v, "violet") == 0 || strcmp(v, "4") == 0) s->terminal_color = 4;
                else if (strcmp(v, "dark") == 0 || strcmp(v, "5") == 0) s->terminal_color = 5;
                else s->terminal_color = 0;
            } else if (strcmp(k, "fullscreen") == 0) {
                s->fullscreen = (atoi(v) == 1) ? 1 : 0;
            } else if (strcmp(k, "bloom") == 0) {
                int bl = atoi(v);
                if (bl >= 0 && bl <= 2) s->bloom = bl;
            } else if (strcmp(k, "crt_filter") == 0 || strcmp(k, "crt") == 0) {
                int crt = atoi(v);
                if (crt >= 0 && crt <= 3) s->crt_filter = crt;
            } else if (strcmp(k, "gamma") == 0) {
                float gm = (float)atof(v);
                if (gm >= 0.5f && gm <= 2.0f) s->gamma = gm;
            } else if (strcmp(k, "unlocked_opponents") == 0) {
                int uo = atoi(v);
                if (uo >= 1 && uo <= 12) s->unlocked_opponents = uo;
            } else if (strcmp(k, "current_opponent") == 0) {
                int co = atoi(v);
                if (co >= 0 && co <= 12) s->current_opponent = co;
            } else if (strcmp(k, "cleared_mask") == 0) {
                unsigned long long cm = strtoull(v, NULL, 10);
                s->cleared_mask = cm;
            }
        }
    }
    fclose(f);
    if (s->difficulty == DIFFICULTY_DEUS) {
        s->ruleset = RULESET_YAKUZA;
        s->master_timer = 15;
        s->terminal_color = 5;
        s->background = 5;
    }
}

void storage_save_settings(const GameSettings *s) {
    char buf[MUSIC_FOLDER_CAP + 1024];
    const char *diff_str = (s->difficulty == DIFFICULTY_DEUS) ? "deus" :
                           (s->difficulty == DIFFICULTY_MASTER) ? "master" :
                           (s->difficulty == DIFFICULTY_HARD) ? "hard" : "easy";
    const char *rule_str = (s->ruleset == RULESET_FUDAWIKI) ? "fudawiki" :
                           (s->ruleset == RULESET_KK_ADD_10) ? "kk_add_10" :
                           (s->ruleset == RULESET_TOURNAMENT) ? "tournament" :
                           (s->ruleset == RULESET_YAKUZA) ? "yakuza" : "nintendo";
    const char *col_str = (s->terminal_color == 1) ? "amber" :
                          (s->terminal_color == 2) ? "teal" :
                          (s->terminal_color == 3) ? "crimson" :
                          (s->terminal_color == 4) ? "violet" :
                          (s->terminal_color == 5) ? "dark" : "green";

    snprintf(buf, sizeof(buf),
             "music_folder=%s\n"
             "rounds=%d\n"
             "difficulty=%s\n"
             "master_timer=%d\n"
             "ruleset=%s\n"
             "background=%d\n"
             "bgm_volume=%.2f\n"
             "sfx_volume=%.2f\n"
             "effect_chorus=%.2f\n"
             "effect_bass=%.2f\n"
             "effect_reverb=%.2f\n"
             "anim_speed=%d\n"
             "terminal_color=%s\n"
             "fullscreen=%d\n"
             "bloom=%d\n"
             "crt_filter=%d\n"
             "gamma=%.2f\n"
             "unlocked_opponents=%d\n"
             "current_opponent=%d\n"
             "cleared_mask=%llu\n",
             s->music_folder, s->rounds, diff_str, s->master_timer, rule_str, s->background,
             s->bgm_volume, s->sfx_volume, s->effect_chorus, s->effect_bass, s->effect_reverb, s->anim_speed, col_str,
             s->fullscreen, s->bloom, s->crt_filter, s->gamma,
             s->unlocked_opponents, s->current_opponent, (unsigned long long)s->cleared_mask);
    safe_write_text("settings.txt", buf);
}

void storage_load_stats(GameStats *st) {
    memset(st, 0, sizeof(*st));
    char path[512];
    get_app_path("stats.txt", path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char k[64], v[64];
        if (sscanf(line, "%63[^=]=%63s", k, v) == 2) {
            int val = atoi(v);
            if (strcmp(k, "matches_played") == 0) st->matches_played = val;
            else if (strcmp(k, "matches_won") == 0) st->matches_won = val;
            else if (strcmp(k, "matches_lost") == 0) st->matches_lost = val;
            else if (strcmp(k, "matches_drawn") == 0) st->matches_drawn = val;
            else if (strcmp(k, "rounds_won_you") == 0) st->rounds_won_you = val;
            else if (strcmp(k, "rounds_won_cpu") == 0) st->rounds_won_cpu = val;
            else if (strcmp(k, "points_for") == 0) st->points_for = val;
            else if (strcmp(k, "points_against") == 0) st->points_against = val;
            else if (strcmp(k, "highest_round_score") == 0) st->highest_round_score = val;
            else if (strcmp(k, "best_award") == 0) st->best_award = val;
            else if (strcmp(k, "koi_calls_you") == 0) st->koi_calls_you = val;
            else if (strcmp(k, "koi_calls_cpu") == 0) st->koi_calls_cpu = val;
            else if (strcmp(k, "easy_matches") == 0) st->easy_matches = val;
            else if (strcmp(k, "hard_matches") == 0) st->hard_matches = val;
            else if (strcmp(k, "master_matches") == 0) st->master_matches = val;
            else if (strcmp(k, "current_streak") == 0) st->current_streak = val;
            else if (strcmp(k, "best_streak") == 0 || strcmp(k, "max_win_streak") == 0) st->best_streak = val;
            else if (strncmp(k, "yaku_", 5) == 0) {
                int yidx = atoi(k + 5);
                if (yidx >= 0 && yidx < YAKU_COUNT) st->yaku_counts[yidx] = val;
            }
        }
    }
    fclose(f);
}

const char *storage_get_favorite_combo(const GameStats *st) {
    static const char *const names[YAKU_COUNT] = {
        "GOKO", "SHIKO", "AME-SHIKO", "SANKO",
        "HANAMI-ZAKE", "TSUKIMI-ZAKE", "INO-SHIKA-CHO",
        "AKATAN", "AOTAN", "AKATAN + AOTAN",
        "TANE", "TANZAKU", "KASU"
    };
    if (!st) return "NONE";
    int best = -1, max_val = 0;
    for (int i = 0; i < YAKU_COUNT; i++) {
        if (st->yaku_counts[i] > max_val) {
            max_val = st->yaku_counts[i];
            best = i;
        }
    }
    return (best >= 0 && max_val > 0) ? names[best] : "NONE";
}

void storage_save_stats(const GameStats *st) {
    char buf[2048];
    int n = snprintf(buf, sizeof(buf),
             "matches_played=%d\n"
             "matches_won=%d\n"
             "matches_lost=%d\n"
             "matches_drawn=%d\n"
             "rounds_won_you=%d\n"
             "rounds_won_cpu=%d\n"
             "points_for=%d\n"
             "points_against=%d\n"
             "highest_round_score=%d\n"
             "best_award=%d\n"
             "koi_calls_you=%d\n"
             "koi_calls_cpu=%d\n"
             "easy_matches=%d\n"
             "hard_matches=%d\n"
             "current_streak=%d\n"
             "max_win_streak=%d\n"
             "best_streak=%d\n"
             "favorite_combo=%s\n",
             st->matches_played, st->matches_won, st->matches_lost, st->matches_drawn,
             st->rounds_won_you, st->rounds_won_cpu, st->points_for, st->points_against,
             st->highest_round_score, st->best_award,
             st->koi_calls_you, st->koi_calls_cpu,
             st->easy_matches, st->hard_matches,
             st->current_streak, st->best_streak, st->best_streak,
             storage_get_favorite_combo(st));
    size_t pos = (n > 0 && (size_t)n < sizeof(buf)) ? (size_t)n : 0;

    if (st->master_matches > 0 && pos < sizeof(buf)) {
        int w = snprintf(buf + pos, sizeof(buf) - pos, "master_matches=%d\n", st->master_matches);
        if (w > 0 && pos + (size_t)w < sizeof(buf)) pos += (size_t)w;
    }

    for (int i = 0; i < YAKU_COUNT && pos < sizeof(buf); i++) {
        int w = snprintf(buf + pos, sizeof(buf) - pos, "yaku_%d=%d\n", i, st->yaku_counts[i]);
        if (w > 0 && pos + (size_t)w < sizeof(buf)) pos += (size_t)w;
    }
    safe_write_text("stats.txt", buf);
}

void storage_reset_stats(GameStats *st) {
    memset(st, 0, sizeof(*st));
    storage_save_stats(st);

    char hist_path[512], rep_path[512];
    get_app_path("history.txt", hist_path, sizeof(hist_path));
    get_app_path("stats_report.txt", rep_path, sizeof(rep_path));
    remove(hist_path);
    remove(rep_path);
}

void storage_load_history(MatchHistory *h) {
    h->count = 0;
    char path[512];
    get_app_path("history.txt", path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[128];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';
        if (len > 0 && h->count < 10) {
            snprintf(h->lines[h->count++], sizeof(h->lines[0]), "%s", line);
        }
    }
    fclose(f);
}

void storage_add_history(MatchHistory *h, const char *record_line) {
    if (h->count >= 10) {
        memmove(&h->lines[0], &h->lines[1], 9 * sizeof(h->lines[0]));
        h->count = 9;
    }
    snprintf(h->lines[h->count++], sizeof(h->lines[0]), "%s", record_line);

    char buf[2048];
    size_t pos = 0;
    buf[0] = '\0';
    for (int i = 0; i < h->count && pos < sizeof(buf); i++) {
        int w = snprintf(buf + pos, sizeof(buf) - pos, "%s\n", h->lines[i]);
        if (w > 0 && pos + (size_t)w < sizeof(buf)) pos += (size_t)w;
    }
    safe_write_text("history.txt", buf);
}

void storage_write_report(const GameStats *st, const MatchHistory *h) {
    char buf[4096];
    float win_rate = (st->matches_played > 0) ? ((float)st->matches_won / st->matches_played * 100.0f) : 0.0f;

    int n = snprintf(buf, sizeof(buf),
             "====================================================\n"
             "        HANAFUDA // KOI-KOI - STATS REPORT          \n"
             "====================================================\n\n"
             "MATCHES\n"
             "  Played:              %d\n"
             "  Won / Lost:          %d / %d (Drawn: %d)\n"
             "  Win Rate:            %.1f%%\n\n"
             "ROUNDS & SCORING\n"
             "  Rounds Won:          YOU %d | CPU %d\n"
             "  Total Points:        FOR %d | AGAINST %d\n"
             "  Highest Round Score: %d pts\n"
             "  Best Award (Best Win): %d pts\n"
             "  Koi-Koi Calls:       YOU %d | CPU %d\n\n"
             "DIFFICULTY & STREAKS\n"
             "  Easy / Hard:         %d / %d\n"
             "  Current Streak:      %d\n"
             "  Max Win Streak:      %d\n"
             "  Favorite Combo:      %s\n\n"
             "LAST 10 MATCHES\n",
             st->matches_played, st->matches_won, st->matches_lost, st->matches_drawn,
             win_rate, st->rounds_won_you, st->rounds_won_cpu,
             st->points_for, st->points_against, st->highest_round_score, st->best_award,
             st->koi_calls_you, st->koi_calls_cpu,
             st->easy_matches, st->hard_matches,
             st->current_streak, st->best_streak,
             storage_get_favorite_combo(st));
    size_t pos = (n > 0 && (size_t)n < sizeof(buf)) ? (size_t)n : 0;

    for (int i = 0; i < h->count && pos < sizeof(buf); i++) {
        int w = snprintf(buf + pos, sizeof(buf) - pos, "  [%d] %s\n", i + 1, h->lines[i]);
        if (w > 0 && pos + (size_t)w < sizeof(buf)) pos += (size_t)w;
    }
    if (pos < sizeof(buf)) {
        snprintf(buf + pos, sizeof(buf) - pos, "\n====================================================\n");
    }
    safe_write_text("stats_report.txt", buf);
}

void storage_save_all(const GameSettings *s, const GameStats *st, const MatchHistory *h) {
    storage_save_settings(s);
    storage_save_stats(st);
    storage_write_report(st, h);
}

void storage_record_match(const GameState *g, const GameSettings *s, GameStats *st, MatchHistory *h) {
    st->matches_played++;
    if (s->difficulty == DIFFICULTY_MASTER || s->difficulty == DIFFICULTY_DEUS) st->master_matches++;
    else if (s->difficulty == DIFFICULTY_HARD) st->hard_matches++;
    else st->easy_matches++;
    int you = g->total_score[0], cpu = g->total_score[1];
    st->points_for += you; st->points_against += cpu;
    const char *outcome = (you > cpu) ? (st->matches_won++, st->current_streak++, "WIN") :
                          (you < cpu) ? (st->matches_lost++, st->current_streak = 0, "LOSS") : (st->matches_drawn++, "DRAW");
    if (st->current_streak > st->best_streak) st->best_streak = st->current_streak;
    if (you > cpu && you > st->best_award) st->best_award = you;
    for (int i = 0; i < g->ledger_count; i++) {
        if (g->ledger[i].winner == 0) st->rounds_won_you++;
        else if (g->ledger[i].winner == 1) st->rounds_won_cpu++;
        if (g->ledger[i].award > st->highest_round_score) st->highest_round_score = g->ledger[i].award;
    }
    for (int y = 0; y < YAKU_COUNT; y++) st->yaku_counts[y] += g->human_yaku_counts[y];
    st->koi_calls_you += g->match_koi_calls[0]; st->koi_calls_cpu += g->match_koi_calls[1];
    time_t now = time(NULL); char time_str[32], record_line[160];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", localtime(&now));
    const char *r_str = (s->ruleset == RULESET_FUDAWIKI) ? "fudawiki" : (s->ruleset == RULESET_KK_ADD_10) ? "kk-add-10" :
                        (s->ruleset == RULESET_TOURNAMENT) ? "tournament" : (s->ruleset == RULESET_YAKUZA) ? "yakuza" : "nintendo";
    const char *d_str = (s->difficulty == DIFFICULTY_DEUS) ? "deus" : (s->difficulty == DIFFICULTY_MASTER) ? "master" :
                        (s->difficulty == DIFFICULTY_HARD) ? "hard" : "easy";
    snprintf(record_line, sizeof(record_line), "%s;rounds=%d;%s;%s;you=%d;cpu=%d;%s", time_str, s->rounds, r_str, d_str, you, cpu, outcome);
    storage_add_history(h, record_line); storage_save_stats(st); storage_write_report(st, h);
}
