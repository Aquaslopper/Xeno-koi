#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "cards.h"
#include "rules.h"
#include "engine.h"
#include "ai.h"

static void assert_score(Ruleset r, const int *cards, int n, int expected_base, int expected_yakus) {
    ScoreResult sr = score_captures(r, cards, n);
    int fast_base = score_captures_base(r, cards, n);
    if (sr.base_points != expected_base || fast_base != expected_base ||
        (expected_yakus >= 0 && sr.yaku_count != expected_yakus)) {
        fprintf(stderr, "FAIL: ruleset=%d expected=(base:%d, yakus:%d) got=(base:%d, fast:%d, yakus:%d)\n",
                (int)r, expected_base, expected_yakus, sr.base_points, fast_base, sr.yaku_count);
        exit(1);
    }
}

static void test_deck_integrity(void) {
    int month_counts[13] = {0};
    int type_counts[4] = {0};
    for (int i = 0; i < 48; i++) {
        assert(CARDS[i].id == i);
        assert(CARDS[i].month >= 1 && CARDS[i].month <= 12);
        assert((int)CARDS[i].type >= 0 && (int)CARDS[i].type < 4);
        month_counts[CARDS[i].month]++;
        type_counts[CARDS[i].type]++;
    }
    for (int m = 1; m <= 12; m++) assert(month_counts[m] == 4);
    assert(type_counts[CARD_TYPE_BRIGHT] == 5);
    assert(type_counts[CARD_TYPE_ANIMAL] == 9);
    assert(type_counts[CARD_TYPE_RIBBON] == 10);
    assert(type_counts[CARD_TYPE_PLAIN]  == 24);
}

static void test_scoring_rulesets(void) {
    /* 1. Five Brights (Goko): 10 in Nintendo/KK-ADD-10/Tournament/Yakuza, 15 in FudaWiki */
    int goko[5] = { 0, 8, 28, 40, 44 };
    assert_score(RULESET_NINTENDO,   goko, 5, 10, 1);
    assert_score(RULESET_FUDAWIKI,   goko, 5, 15, 1);
    assert_score(RULESET_KK_ADD_10,  goko, 5, 10, 1);
    assert_score(RULESET_TOURNAMENT, goko, 5, 10, 1);
    assert_score(RULESET_YAKUZA,     goko, 5, 10, 1);

    /* 2. Four Brights (Shiko vs Ame-shiko) */
    int shiko[4] = { 0, 8, 28, 44 };
    int ame_shiko[4] = { 0, 8, 28, 40 };
    assert_score(RULESET_NINTENDO, shiko, 4, 8, 1);
    assert_score(RULESET_FUDAWIKI, shiko, 4, 8, 1);
    assert_score(RULESET_NINTENDO, ame_shiko, 4, 7, 1);
    assert_score(RULESET_FUDAWIKI, ame_shiko, 4, 7, 1);

    /* 3. Three Brights (Sanko vs Rainman block) */
    int sanko[3] = { 0, 8, 28 };
    int rainy3[3] = { 0, 8, 40 };
    assert_score(RULESET_NINTENDO, sanko, 3, 5, 1);
    assert_score(RULESET_FUDAWIKI, sanko, 3, 6, 1);
    assert_score(RULESET_NINTENDO, rainy3, 3, 0, 0);
    assert_score(RULESET_FUDAWIKI, rainy3, 3, 0, 0);

    /* 4. Viewings (Curtain 8 + Moon 28 + Sake Cup 32): disabled in Tournament & Yakuza */
    int viewings[3] = { 8, 28, 32 };
    assert_score(RULESET_NINTENDO,   viewings, 3, 10, 2);
    assert_score(RULESET_FUDAWIKI,   viewings, 3, 10, 2);
    assert_score(RULESET_KK_ADD_10,  viewings, 3, 10, 2);
    assert_score(RULESET_TOURNAMENT, viewings, 3,  0, 0);
    assert_score(RULESET_YAKUZA,     viewings, 3,  0, 0);

    /* 5. Ino-shika-cho (20, 24, 36) + 2 extra animals (4, 12) */
    int trio2[5] = { 20, 24, 36, 4, 12 };
    assert_score(RULESET_NINTENDO,   trio2, 5, 7, 1);
    assert_score(RULESET_KK_ADD_10,  trio2, 5, 6, 2); /* 5 + Tane(1) */
    assert_score(RULESET_TOURNAMENT, trio2, 5, 7, 1);

    /* 6. All 6 special ribbons (1, 5, 9, 21, 33, 37) */
    int rib6[6] = { 1, 5, 9, 21, 33, 37 };
    assert_score(RULESET_NINTENDO,   rib6, 6, 10, 1);
    assert_score(RULESET_KK_ADD_10,  rib6, 6, 13, 2); /* Akatan+Aotan(10) + Tanzaku(6-3=3) */
    assert_score(RULESET_TOURNAMENT, rib6, 6, 10, 1);

    /* 7. 4 ribbons in KK-ADD-10 vs Nintendo */
    int rib4[4] = { 1, 5, 13, 17 };
    assert_score(RULESET_NINTENDO,  rib4, 4, 0, 0);
    assert_score(RULESET_KK_ADD_10, rib4, 4, 1, 1);

    /* 8. 9 plains + Sake Cup (32): Kasu in KK-ADD-10 only */
    int plains9cup[10] = { 2, 3, 6, 7, 10, 11, 14, 15, 18, 32 };
    assert_score(RULESET_NINTENDO,   plains9cup, 10, 0, 0);
    assert_score(RULESET_KK_ADD_10,  plains9cup, 10, 1, 1);
    assert_score(RULESET_TOURNAMENT, plains9cup, 10, 0, 0);
}

static void test_multipliers_and_awards(void) {
    assert(koi_caller_multiplier(0) == 1);
    assert(koi_caller_multiplier(1) == 2);
    assert(koi_caller_multiplier(2) == 3);
    assert(koi_caller_multiplier(5) == 6);
    assert(koi_caller_multiplier(9) == KOI_CALLER_MULT_MAX);

    assert(calculate_award(0, 2, 1) == 0);
    assert(calculate_award(5, 0, 0) == 5);
    assert(calculate_award(5, 1, 0) == 10); /* caller x2 */
    assert(calculate_award(5, 0, 1) == 10); /* opponent x2 */
    assert(calculate_award(7, 0, 0) == 14); /* 7+ x2 */
    assert(calculate_award(7, 1, 1) == 56); /* 7 * 2(7+) * 2(opp) * 2(self) */
    assert(calculate_award(8, 2, 1) == 96); /* 8 * 2(7+) * 2(opp) * 3(self) */
}

static void test_simulated_matches(void) {
    GameState g;
    for (int m = 0; m < 500; m++) {
        Ruleset r = (Ruleset)(m % 5);
        Difficulty d = (Difficulty)(m % 4);
        uint32_t seed = (uint32_t)(m * 2654435761u + 17u);
        uint32_t ai_rng = seed ^ 0xA5A5A5A5u;
        ai_set_opponent(m % 13);
        engine_init(&g, r, 6, seed);
        engine_start_round(&g);

        int guard = 0;
        while (g.phase != PHASE_MATCH_END && guard++ < 2000) {
            assert(engine_validate_zones(&g));
            for (int p = 0; p < 2; p++) {
                int b1 = score_captures_base(g.ruleset, g.captured[p], g.captured_count[p]);
                int b2 = score_captures(g.ruleset, g.captured[p], g.captured_count[p]).base_points;
                assert(b1 == b2);
            }
            if (g.phase == PHASE_ROUND_END) {
                if (g.ruleset == RULESET_YAKUZA) {
                    assert(g.current_round_record.winner == 0 || g.current_round_record.winner == 1);
                }
                engine_next_round(&g);
            } else if (g.phase == PHASE_TURN_HAND) {
                int card = -1, target = -1;
                ai_choose_hand(&g, d, &ai_rng, &card, &target);
                int ok = engine_play_hand(&g, card, target);
                assert(ok);
            } else if (g.phase == PHASE_HAND_CHOICE) {
                int ok = engine_pick_hand_choice(&g, g.choice_targets[0]);
                assert(ok);
            } else if (g.phase == PHASE_TURN_DRAW) {
                engine_reveal_stock(&g);
            } else if (g.phase == PHASE_DRAW_CHOICE) {
                int target = ai_choose_stock_target(&g, d, &ai_rng);
                int ok = engine_pick_draw_choice(&g, target);
                assert(ok);
            } else if (g.phase == PHASE_DECIDE) {
                DecisionType dec = ai_choose_decision(&g, d, &ai_rng);
                int ok = engine_decide(&g, dec);
                assert(ok);
            } else {
                assert(0 && "Unexpected game phase");
            }
        }
        assert(g.phase == PHASE_MATCH_END);
    }
}

int main(void) {
    test_deck_integrity();
    test_scoring_rulesets();
    test_multipliers_and_awards();
    test_simulated_matches();
    printf("All engine & ruleset tests passed (500 full matches verified).\n");
    return 0;
}
