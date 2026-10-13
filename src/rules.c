#include "rules.h"
#include <string.h>

int ruleset_has_hanami_zake(Ruleset ruleset) {
    (void)ruleset; /* scored in every ruleset */
    return 1;
}

int ruleset_has_tsukimi_zake(Ruleset ruleset) {
    /* Removed only in Tournament (and Yakuza, which is Tournament scoring
       plus Oya-ken): Moon + Sake Cup was judged too strong. */
    return ruleset != RULESET_TOURNAMENT && ruleset != RULESET_YAKUZA;
}

int ruleset_cup_counts_as_kasu(Ruleset ruleset) {
    /* In Tournament/Yakuza the Sake Cup is strictly an animal card. */
    return ruleset != RULESET_TOURNAMENT && ruleset != RULESET_YAKUZA;
}

int ruleset_one_koi_call(Ruleset ruleset) {
    return ruleset == RULESET_NINTENDO;
}

int ruleset_round_month(int current_round, int total_rounds) {
    if (total_rounds != 12) return 0;
    if (current_round < 1 || current_round > 12) return 0;
    return current_round;
}

ScoreResult score_captures(Ruleset ruleset, const int *cards, int n) {
    return score_captures_ex(ruleset, cards, n, 0);
}

ScoreResult score_captures_ex(Ruleset ruleset, const int *cards, int n, int round_month) {
    ScoreResult res;
    memset(&res, 0, sizeof(res));

    int has[48] = {0};
    int bright_count = 0;
    int animal_count = 0;
    int ribbon_count = 0;
    int plain_count = 0;

    for (int i = 0; i < n; i++) {
        int id = cards[i];
        if (id >= 0 && id < 48) {
            has[id] = 1;
            switch (CARDS[id].type) {
                case CARD_TYPE_BRIGHT: bright_count++; break;
                case CARD_TYPE_ANIMAL: animal_count++; break;
                case CARD_TYPE_RIBBON: ribbon_count++; break;
                case CARD_TYPE_PLAIN:  plain_count++; break;
            }
        }
    }

    int has_rain = has[40]; /* 11-B Rainman */
    int has_cup = has[32];  /* 09-A Sake Cup */

    /* 1. Bright Family */
    if (bright_count == 5) {
        ScoredYaku *y = &res.yakus[res.yaku_count++];
        y->id = YAKU_GOKO;
        y->name = "Goko (Five Brights)";
        y->points = (ruleset == RULESET_FUDAWIKI) ? 15 : 10;
        int idx = 0;
        for (int i = 0; i < 48; i++) {
            if (has[i] && CARDS[i].type == CARD_TYPE_BRIGHT) {
                y->card_ids[idx++] = i;
            }
        }
        y->card_count = idx;
    } else if (bright_count == 4) {
        ScoredYaku *y = &res.yakus[res.yaku_count++];
        if (has_rain) {
            y->id = YAKU_AME_SHIKO;
            y->name = "Ame-shiko (Rainy Four Brights)";
            y->points = 7;
        } else {
            y->id = YAKU_SHIKO;
            y->name = "Shiko (Four Brights)";
            y->points = 8;
        }
        int idx = 0;
        for (int i = 0; i < 48; i++) {
            if (has[i] && CARDS[i].type == CARD_TYPE_BRIGHT) {
                y->card_ids[idx++] = i;
            }
        }
        y->card_count = idx;
    } else if (bright_count == 3) {
        if (!has_rain) {
            ScoredYaku *y = &res.yakus[res.yaku_count++];
            y->id = YAKU_SANKO;
            y->name = "Sanko (Three Brights)";
            y->points = (ruleset == RULESET_FUDAWIKI) ? 6 : 5;
            int idx = 0;
            for (int i = 0; i < 48; i++) {
                if (has[i] && CARDS[i].type == CARD_TYPE_BRIGHT) {
                    y->card_ids[idx++] = i;
                }
            }
            y->card_count = idx;
        }
    }

    /* 2. Viewings. Tsukimi-zake is removed in Tournament/Yakuza only;
       Hanami-zake is scored in every ruleset. */
    if (ruleset_has_hanami_zake(ruleset)) {
        if (has[8] && has_cup) { /* Curtain 03-B + Sake Cup 09-A */
            ScoredYaku *y = &res.yakus[res.yaku_count++];
            y->id = YAKU_HANAMI_ZAKE;
            y->name = "Hanami-zake (Cherry Viewing)";
            y->points = 5;
            y->card_ids[0] = 8;
            y->card_ids[1] = 32;
            y->card_count = 2;
        }
    }
    if (ruleset_has_tsukimi_zake(ruleset)) {
        if (has[28] && has_cup) { /* Moon 08-B + Sake Cup 09-A */
            ScoredYaku *y = &res.yakus[res.yaku_count++];
            y->id = YAKU_TSUKIMI_ZAKE;
            y->name = "Tsukimi-zake (Moon Viewing)";
            y->points = 5;
            y->card_ids[0] = 28;
            y->card_ids[1] = 32;
            y->card_count = 2;
        }
    }

    /* 3. Animals */
    int has_ino = (has[20] && has[24] && has[36]); /* Butterflies, Boar, Deer */

    if (ruleset == RULESET_KK_ADD_10) {
        if (has_ino) {
            ScoredYaku *y = &res.yakus[res.yaku_count++];
            y->id = YAKU_INO_SHIKA_CHO;
            y->name = "Ino-shika-cho (Boar-Deer-Butterfly)";
            y->points = 5;
            y->card_ids[0] = 20;
            y->card_ids[1] = 24;
            y->card_ids[2] = 36;
            y->card_count = 3;
        }
        if (animal_count >= 5) {
            ScoredYaku *y = &res.yakus[res.yaku_count++];
            y->id = YAKU_TANE;
            y->name = "Tane (Animals)";
            y->points = animal_count - 4;
            y->card_count = 0;
        }
    } else {
        /* Nintendo, FudaWiki, and Tournament */
        if (has_ino) {
            ScoredYaku *y = &res.yakus[res.yaku_count++];
            y->id = YAKU_INO_SHIKA_CHO;
            y->name = "Ino-shika-cho (Boar-Deer-Butterfly)";
            y->points = 5 + (animal_count - 3);
            y->card_ids[0] = 20;
            y->card_ids[1] = 24;
            y->card_ids[2] = 36;
            y->card_count = 3;
        } else if (animal_count >= 5) {
            ScoredYaku *y = &res.yakus[res.yaku_count++];
            y->id = YAKU_TANE;
            y->name = "Tane (Animals)";
            y->points = animal_count - 4;
            y->card_count = 0;
        }
    }

    /* 4. Ribbons */
    int has_akatan = (has[1] && has[5] && has[9]);      /* 01, 02, 03 Poetry */
    int has_aotan  = (has[21] && has[33] && has[37]);   /* 06, 09, 10 Blue */

    if (ruleset == RULESET_KK_ADD_10) {
        if (has_akatan) {
            ScoredYaku *y = &res.yakus[res.yaku_count++];
            y->id = YAKU_AKATAN;
            y->name = "Akatan (Red Poetry Ribbons)";
            y->points = 5;
            y->card_ids[0] = 1; y->card_ids[1] = 5; y->card_ids[2] = 9;
            y->card_count = 3;
        }
        if (has_aotan) {
            ScoredYaku *y = &res.yakus[res.yaku_count++];
            y->id = YAKU_AOTAN;
            y->name = "Aotan (Blue Ribbons)";
            y->points = 5;
            y->card_ids[0] = 21; y->card_ids[1] = 33; y->card_ids[2] = 37;
            y->card_count = 3;
        }
        if (ribbon_count >= 5) {
            ScoredYaku *y = &res.yakus[res.yaku_count++];
            y->id = YAKU_TANZAKU;
            y->name = "Tanzaku (Ribbons)";
            y->points = ribbon_count - 4;
            y->card_count = 0;
        }
    } else {
        /* Nintendo, FudaWiki, and Tournament */
        if (has_akatan && has_aotan) {
            ScoredYaku *y = &res.yakus[res.yaku_count++];
            y->id = YAKU_AKATAN_AOTAN;
            y->name = "Akatan + Aotan (All Ribbons)";
            y->points = 10 + (ribbon_count - 6);
            y->card_ids[0] = 1;  y->card_ids[1] = 5;  y->card_ids[2] = 9;   /* 01, 02, 03 Red Poetry */
            y->card_ids[3] = 21; y->card_ids[4] = 33; y->card_ids[5] = 37;  /* 06, 09, 10 Blue Ribbons */
            y->card_count = 6;
        } else if (has_akatan) {
            ScoredYaku *y = &res.yakus[res.yaku_count++];
            y->id = YAKU_AKATAN;
            y->name = "Akatan (Red Poetry Ribbons)";
            y->points = 5 + (ribbon_count - 3);
            y->card_ids[0] = 1; y->card_ids[1] = 5; y->card_ids[2] = 9;
            y->card_count = 3;
        } else if (has_aotan) {
            ScoredYaku *y = &res.yakus[res.yaku_count++];
            y->id = YAKU_AOTAN;
            y->name = "Aotan (Blue Ribbons)";
            y->points = 5 + (ribbon_count - 3);
            y->card_ids[0] = 21; y->card_ids[1] = 33; y->card_ids[2] = 37;
            y->card_count = 3;
        } else if (ribbon_count >= 5) {
            ScoredYaku *y = &res.yakus[res.yaku_count++];
            y->id = YAKU_TANZAKU;
            y->name = "Tanzaku (Ribbons)";
            y->points = ribbon_count - 4;
            y->card_count = 0;
        }
    }

    /* 5. Plains (Kasu). The Sake Cup also counts as a chaff card, except
       in Tournament/Yakuza where it is strictly an animal. */
    int effective_plains = plain_count;
    if (ruleset_cup_counts_as_kasu(ruleset) && has_cup) {
        effective_plains++;
    }
    if (effective_plains >= 10) {
        ScoredYaku *y = &res.yakus[res.yaku_count++];
        y->id = YAKU_KASU;
        y->name = "Kasu (Plains)";
        y->points = effective_plains - 9;
        y->card_count = 0;
    }

    /* 6. Tsukifuda: all four cards of the current round's month. */
    if (round_month >= 1 && round_month <= 12) {
        int b = (round_month - 1) * 4;
        if (has[b] && has[b + 1] && has[b + 2] && has[b + 3]) {
            ScoredYaku *y = &res.yakus[res.yaku_count++];
            y->id = YAKU_TSUKIFUDA;
            y->name = "Tsukifuda (Cards of the Month)";
            y->points = 4;
            y->card_ids[0] = b; y->card_ids[1] = b + 1;
            y->card_ids[2] = b + 2; y->card_ids[3] = b + 3;
            y->card_count = 4;
        }
    }

    res.bright_count = bright_count;
    res.animal_count = animal_count;
    res.ribbon_count = ribbon_count;
    res.plain_count = plain_count;
    res.effective_plain_count = effective_plains;

    /* Sum base points */
    int base = 0;
    for (int i = 0; i < res.yaku_count; i++) {
        base += res.yakus[i].points;
    }
    res.base_points = base;

    return res;
}

static const ScoredYaku *find_yaku(const ScoreResult *sr, YakuId id) {
    for (int i = 0; i < sr->yaku_count; i++) {
        if (sr->yakus[i].id == id) return &sr->yakus[i];
    }
    return NULL;
}

int score_is_improved(const ScoreResult *old_sr, const ScoreResult *new_sr) {
    if (!old_sr || !new_sr) return 0;
    if (new_sr->base_points > old_sr->base_points) return 1;

    /* A yaku that was not there before, or that pays more than before. */
    for (int i = 0; i < new_sr->yaku_count; i++) {
        const ScoredYaku *o = find_yaku(old_sr, new_sr->yakus[i].id);
        if (!o || new_sr->yakus[i].points > o->points) return 1;
    }

    /* Fixed-value yaku (KK-ADD-10 trios) do not pay more when their
       category grows, but capturing another card of that category is
       still an improvement the player must be offered a decision on:
       e.g. Akatan held with 3 ribbons, then a 4th ribbon is captured. */
    if (new_sr->ribbon_count > old_sr->ribbon_count &&
        (find_yaku(new_sr, YAKU_AKATAN) || find_yaku(new_sr, YAKU_AOTAN) ||
         find_yaku(new_sr, YAKU_AKATAN_AOTAN) || find_yaku(new_sr, YAKU_TANZAKU))) return 1;
    if (new_sr->animal_count > old_sr->animal_count &&
        (find_yaku(new_sr, YAKU_INO_SHIKA_CHO) || find_yaku(new_sr, YAKU_TANE))) return 1;
    if (new_sr->bright_count > old_sr->bright_count &&
        (find_yaku(new_sr, YAKU_GOKO) || find_yaku(new_sr, YAKU_SHIKO) ||
         find_yaku(new_sr, YAKU_AME_SHIKO) || find_yaku(new_sr, YAKU_SANKO))) return 1;
    if (new_sr->effective_plain_count > old_sr->effective_plain_count &&
        find_yaku(new_sr, YAKU_KASU)) return 1;

    return 0;
}

int calculate_award(int base, int opponent_called_koi) {
    if (base <= 0) return 0;
    int mult = 1;
    if (base >= 7) mult *= 2;
    if (opponent_called_koi) mult *= 2;
    return base * mult;
}
