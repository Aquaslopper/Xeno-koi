#include "rules.h"
#include <string.h>

ScoreResult score_captures(Ruleset ruleset, const int *cards, int n) {
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

    /* 2. Viewings (Completely DISABLED in Tournament and Yakuza rulesets) */
    if (ruleset != RULESET_TOURNAMENT && ruleset != RULESET_YAKUZA) {
        if (has[8] && has_cup) { /* Curtain 03-B + Sake Cup 09-A */
            ScoredYaku *y = &res.yakus[res.yaku_count++];
            y->id = YAKU_HANAMI_ZAKE;
            y->name = "Hanami-zake (Cherry Viewing)";
            y->points = 5;
            y->card_ids[0] = 8;
            y->card_ids[1] = 32;
            y->card_count = 2;
        }
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
        if (has_akatan && has_aotan) {
            /* KK-ADD: the combined set exists as its own combo (10, replacing the two 5s);
               Tanzaku still stacks on top of it */
            ScoredYaku *y = &res.yakus[res.yaku_count++];
            y->id = YAKU_AKATAN_AOTAN;
            y->name = "Akatan + Aotan (All Ribbons)";
            y->points = 10;
            y->card_ids[0] = 1;  y->card_ids[1] = 5;  y->card_ids[2] = 9;
            y->card_ids[3] = 21; y->card_ids[4] = 33; y->card_ids[5] = 37;
            y->card_count = 6;
        } else if (has_akatan) {
            ScoredYaku *y = &res.yakus[res.yaku_count++];
            y->id = YAKU_AKATAN;
            y->name = "Akatan (Red Poetry Ribbons)";
            y->points = 5;
            y->card_ids[0] = 1; y->card_ids[1] = 5; y->card_ids[2] = 9;
            y->card_count = 3;
        } else if (has_aotan) {
            ScoredYaku *y = &res.yakus[res.yaku_count++];
            y->id = YAKU_AOTAN;
            y->name = "Aotan (Blue Ribbons)";
            y->points = 5;
            y->card_ids[0] = 21; y->card_ids[1] = 33; y->card_ids[2] = 37;
            y->card_count = 3;
        }
        /* KK-ADD: count the ribbons from the 4th card, so every extra ribbon is a real +1
           and the Koi-Koi window opens on the 3rd, 4th, 5th ... ribbon (was: >= 5, n-4) */
        if (ribbon_count >= 4) {
            ScoredYaku *y = &res.yakus[res.yaku_count++];
            y->id = YAKU_TANZAKU;
            y->name = "Tanzaku (Ribbons)";
            y->points = ribbon_count - 3;
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

    /* 5. Plains (Kasu) */
    int effective_plains = plain_count;
    if (ruleset == RULESET_KK_ADD_10 && has_cup) {
        effective_plains++;
    }
    if (effective_plains >= 10) {
        ScoredYaku *y = &res.yakus[res.yaku_count++];
        y->id = YAKU_KASU;
        y->name = "Kasu (Plains)";
        y->points = effective_plains - 9;
        y->card_count = 0;
    }

    /* Sum base points */
    int base = 0;
    for (int i = 0; i < res.yaku_count; i++) {
        base += res.yakus[i].points;
    }
    res.base_points = base;

    return res;
}

int koi_multiplier(int winner_koi_calls, int opponent_called_koi) {
    if (winner_koi_calls > 0) return winner_koi_calls + 1;
    if (opponent_called_koi) return 2;
    return 1;
}

int calculate_award(int base, int winner_koi_calls, int opponent_called_koi) {
    if (base <= 0) return 0;
    int mult = 1;
    if (base >= 7) mult *= 2;
    mult *= koi_multiplier(winner_koi_calls, opponent_called_koi);
    return base * mult;
}
