#include "rules.h"
#include <string.h>

static const int BRIGHT_IDS[5] = { 0, 8, 28, 40, 44 };

#define HAS_CARD(mask, id) (((mask) >> (id)) & 1ULL)

CaptureSummary rules_summarize_captures(const int *cards, int n) {
    CaptureSummary s = { 0, 0, 0, 0, 0 };
    for (int i = 0; i < n; i++) {
        int id = cards[i];
        if ((unsigned)id < 48u) {
            s.mask |= (1ULL << id);
            switch (CARDS[id].type) {
                case CARD_TYPE_BRIGHT: s.bright_count++; break;
                case CARD_TYPE_ANIMAL: s.animal_count++; break;
                case CARD_TYPE_RIBBON: s.ribbon_count++; break;
                case CARD_TYPE_PLAIN:  s.plain_count++;  break;
            }
        }
    }
    return s;
}

int score_summary_base(Ruleset ruleset, CaptureSummary s) {
    uint64_t mask = s.mask;
    int base = 0;
    int has_rain = (int)HAS_CARD(mask, 40);
    int has_cup  = (int)HAS_CARD(mask, 32);

    /* 1. Bright Family */
    if (s.bright_count == 5) {
        base += (ruleset == RULESET_FUDAWIKI) ? 15 : 10;
    } else if (s.bright_count == 4) {
        base += has_rain ? 7 : 8;
    } else if (s.bright_count == 3 && !has_rain) {
        base += (ruleset == RULESET_FUDAWIKI) ? 6 : 5;
    }

    /* 2. Viewings */
    if (ruleset != RULESET_TOURNAMENT && ruleset != RULESET_YAKUZA && has_cup) {
        if (HAS_CARD(mask, 8))  base += 5;
        if (HAS_CARD(mask, 28)) base += 5;
    }

    /* 3. Animals: Butterflies (20), Boar (24), Deer (36) */
    const uint64_t ino_mask = (1ULL << 20) | (1ULL << 24) | (1ULL << 36);
    int has_ino = ((mask & ino_mask) == ino_mask);
    if (ruleset == RULESET_KK_ADD_10) {
        if (has_ino) base += 5;
        if (s.animal_count >= 5) base += s.animal_count - 4;
    } else {
        if (has_ino) base += 5 + (s.animal_count - 3);
        else if (s.animal_count >= 5) base += s.animal_count - 4;
    }

    /* 4. Ribbons: Akatan (1, 5, 9), Aotan (21, 33, 37) */
    const uint64_t akatan_mask = (1ULL << 1) | (1ULL << 5) | (1ULL << 9);
    const uint64_t aotan_mask  = (1ULL << 21) | (1ULL << 33) | (1ULL << 37);
    int has_akatan = ((mask & akatan_mask) == akatan_mask);
    int has_aotan  = ((mask & aotan_mask) == aotan_mask);

    if (ruleset == RULESET_KK_ADD_10) {
        if (has_akatan) base += 5;
        if (has_aotan)  base += 5;
        if (s.ribbon_count >= 4) base += s.ribbon_count - 3;
    } else {
        if (has_akatan && has_aotan) base += 10 + (s.ribbon_count - 6);
        else if (has_akatan || has_aotan) base += 5 + (s.ribbon_count - 3);
        else if (s.ribbon_count >= 5) base += s.ribbon_count - 4;
    }

    /* 5. Plains (Kasu) */
    int effective_plains = s.plain_count + ((ruleset == RULESET_KK_ADD_10 && has_cup) ? 1 : 0);
    if (effective_plains >= 10) {
        base += effective_plains - 9;
    }

    return base;
}

int score_captures_base(Ruleset ruleset, const int *cards, int n) {
    return score_summary_base(ruleset, rules_summarize_captures(cards, n));
}

ScoreResult score_captures(Ruleset ruleset, const int *cards, int n) {
    ScoreResult res;
    res.yaku_count = 0;
    res.base_points = 0;

    CaptureSummary cs = rules_summarize_captures(cards, n);
    uint64_t mask = cs.mask;
    int bright_count = cs.bright_count;
    int animal_count = cs.animal_count;
    int ribbon_count = cs.ribbon_count;
    int plain_count  = cs.plain_count;

    int has_rain = (int)HAS_CARD(mask, 40); /* 11-B Rainman */
    int has_cup  = (int)HAS_CARD(mask, 32); /* 09-A Sake Cup */

    /* 1. Bright Family */
    if (bright_count == 5) {
        ScoredYaku *y = &res.yakus[res.yaku_count++];
        y->id = YAKU_GOKO;
        y->name = "Goko (Five Brights)";
        y->points = (ruleset == RULESET_FUDAWIKI) ? 15 : 10;
        for (int i = 0; i < 5; i++) y->card_ids[i] = BRIGHT_IDS[i];
        y->card_count = 5;
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
        for (int i = 0; i < 5; i++) {
            int bid = BRIGHT_IDS[i];
            if (HAS_CARD(mask, bid)) y->card_ids[idx++] = bid;
        }
        y->card_count = idx;
    } else if (bright_count == 3 && !has_rain) {
        ScoredYaku *y = &res.yakus[res.yaku_count++];
        y->id = YAKU_SANKO;
        y->name = "Sanko (Three Brights)";
        y->points = (ruleset == RULESET_FUDAWIKI) ? 6 : 5;
        int idx = 0;
        for (int i = 0; i < 5; i++) {
            int bid = BRIGHT_IDS[i];
            if (HAS_CARD(mask, bid)) y->card_ids[idx++] = bid;
        }
        y->card_count = idx;
    }

    /* 2. Viewings (Completely DISABLED in Tournament and Yakuza rulesets) */
    if (ruleset != RULESET_TOURNAMENT && ruleset != RULESET_YAKUZA && has_cup) {
        if (HAS_CARD(mask, 8)) { /* Curtain 03-B + Sake Cup 09-A */
            ScoredYaku *y = &res.yakus[res.yaku_count++];
            y->id = YAKU_HANAMI_ZAKE;
            y->name = "Hanami-zake (Cherry Viewing)";
            y->points = 5;
            y->card_ids[0] = 8;
            y->card_ids[1] = 32;
            y->card_count = 2;
        }
        if (HAS_CARD(mask, 28)) { /* Moon 08-B + Sake Cup 09-A */
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
    const uint64_t ino_mask = (1ULL << 20) | (1ULL << 24) | (1ULL << 36);
    int has_ino = ((mask & ino_mask) == ino_mask);

    if (ruleset == RULESET_KK_ADD_10) {
        if (has_ino) {
            ScoredYaku *y = &res.yakus[res.yaku_count++];
            y->id = YAKU_INO_SHIKA_CHO;
            y->name = "Ino-shika-cho (Boar-Deer-Butterfly)";
            y->points = 5;
            y->card_ids[0] = 20; y->card_ids[1] = 24; y->card_ids[2] = 36;
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
        if (has_ino) {
            ScoredYaku *y = &res.yakus[res.yaku_count++];
            y->id = YAKU_INO_SHIKA_CHO;
            y->name = "Ino-shika-cho (Boar-Deer-Butterfly)";
            y->points = 5 + (animal_count - 3);
            y->card_ids[0] = 20; y->card_ids[1] = 24; y->card_ids[2] = 36;
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
    const uint64_t akatan_mask = (1ULL << 1) | (1ULL << 5) | (1ULL << 9);
    const uint64_t aotan_mask  = (1ULL << 21) | (1ULL << 33) | (1ULL << 37);
    int has_akatan = ((mask & akatan_mask) == akatan_mask);
    int has_aotan  = ((mask & aotan_mask) == aotan_mask);

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
        if (has_akatan && has_aotan) {
            ScoredYaku *y = &res.yakus[res.yaku_count++];
            y->id = YAKU_AKATAN_AOTAN;
            y->name = "Akatan + Aotan (All Ribbons)";
            y->points = 10 + (ribbon_count - 6);
            y->card_ids[0] = 1;  y->card_ids[1] = 5;  y->card_ids[2] = 9;
            y->card_ids[3] = 21; y->card_ids[4] = 33; y->card_ids[5] = 37;
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
    int effective_plains = plain_count + ((ruleset == RULESET_KK_ADD_10 && has_cup) ? 1 : 0);
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

int koi_caller_multiplier(int self_koi_calls) {
    if (self_koi_calls <= 0) return 1;
    int mult = 1 + self_koi_calls;
    return (mult > KOI_CALLER_MULT_MAX) ? KOI_CALLER_MULT_MAX : mult;
}

int calculate_award(int base, int self_koi_calls, int opponent_koi_calls) {
    if (base <= 0) return 0;
    int mult = 1;
    if (base >= 7) mult *= 2;
    if (opponent_koi_calls > 0) mult *= 2;
    mult *= koi_caller_multiplier(self_koi_calls);
    return base * mult;
}
