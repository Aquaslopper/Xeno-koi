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

/* ---------------------------------------------------------------------------
 * Combo -> Koi-Koi screen trigger tests (added 23 Oct 2026).
 *
 * Contract under test: when a player's captures complete a new combo (yaku),
 * the Koi-Koi / Shobu screen (PHASE_DECIDE) opens. Each case drives the real
 * engine: player 0 already holds the "have" cards in captures, the field is
 * left with exactly one card of the final card's month, and the stock top is
 * the final card. engine_reveal_stock() then plays a normal stock stage, and
 * the resulting phase is exactly what the game would show the player.
 * ------------------------------------------------------------------------- */

static int remove_card(GameState *g, int id) {
    for (int p = 0; p < 2; p++)
        for (int i = 0; i < g->hand_count[p]; i++)
            if (g->hand[p][i] == id) {
                for (int k = i; k < g->hand_count[p] - 1; k++) g->hand[p][k] = g->hand[p][k + 1];
                g->hand_count[p]--;
                return 1;
            }
    for (int i = 0; i < g->field_count; i++)
        if (g->field[i] == id) {
            for (int k = i; k < g->field_count - 1; k++) g->field[k] = g->field[k + 1];
            g->field_count--;
            return 1;
        }
    for (int i = 0; i < g->stock_count; i++)
        if (g->stock[i] == id) {
            for (int k = i; k < g->stock_count - 1; k++) g->stock[k] = g->stock[k + 1];
            g->stock_count--;
            return 1;
        }
    return 0;
}

static void run_combo_case(const char *name, Ruleset rs, const int *have, int n_have,
                           int final_card, int field_card, int expect_screen) {
    GameState g;
    engine_init(&g, rs, 1, 0xC0FFEEu);
    engine_start_round(&g);

    for (int i = 0; i < n_have; i++) {
        assert(remove_card(&g, have[i]));
        g.captured[0][g.captured_count[0]++] = have[i];
    }
    /* Leave only field_card on the field in the final card's month, so the
       stock draw produces one clean two-card capture (no draw choice). */
    int m = CARDS[final_card].month;
    for (int x = 0; x < 48; x++)
        if (CARDS[x].month == m && x != final_card)
            for (int i = 0; i < g.field_count; i++)
                if (g.field[i] == x) {
                    for (int k = i; k < g.field_count - 1; k++) g.field[k] = g.field[k + 1];
                    g.field_count--;
                    g.stock[g.stock_count++] = x;
                    break;
                }
    assert(remove_card(&g, field_card));
    g.field[g.field_count++] = field_card;
    assert(remove_card(&g, final_card));
    g.stock[g.stock_count++] = final_card;

    if (g.hand_count[0] == 0) {
        /* Keep one card in player 0's hand so an empty-hand auto-Shobu cannot
           mask the decision this test is checking. Stock bottom is never the
           final card (final card is the stock top). */
        int c = g.stock[0];
        for (int k = 0; k < g.stock_count - 1; k++) g.stock[k] = g.stock[k + 1];
        g.stock_count--;
        g.hand[0][g.hand_count[0]++] = c;
    }

    g.current_player = 0;
    g.phase = PHASE_TURN_DRAW;
    g.has_pending = 0;
    assert(engine_validate_zones(&g));

    assert(engine_reveal_stock(&g));
    int screen = (g.phase == PHASE_DECIDE);
    printf("    %-44s -> %s\n", name, screen ? "KOI-KOI SCREEN" : "no screen, play continues");
    if (screen != expect_screen) {
        fprintf(stderr, "FAIL: combo trigger case \"%s\": expected %s, got %s\n",
                name, expect_screen ? "KOI-KOI SCREEN" : "no screen",
                screen ? "KOI-KOI SCREEN" : "no screen");
        exit(1);
    }
}

static void test_hand_stage_probe(void) {
    /* The maintainer's exact scenario: the third red ribbon is captured by
       PLAYING A HAND CARD. The current flow asks the Koi-Koi question only at
       the stock stage (finish_stock_turn); this test pins that contract so a
       future "ask immediately" change is an explicit, reviewed edit. */
    GameState g;
    engine_init(&g, RULESET_KK_ADD_10, 1, 0xC0FFEEu);
    engine_start_round(&g);

    const int have[2] = { 1, 5 }; /* the first two red ribbons, already captured */
    for (int i = 0; i < 2; i++) {
        assert(remove_card(&g, have[i]));
        g.captured[0][g.captured_count[0]++] = have[i];
    }
    assert(remove_card(&g, 9)); /* third red ribbon goes into the hand */
    g.hand[0][g.hand_count[0]++] = 9;
    /* Month 3 on the field must be exactly the curtain (8). */
    for (int x = 10; x <= 11; x++)
        for (int i = 0; i < g.field_count; i++)
            if (g.field[i] == x) {
                for (int k = i; k < g.field_count - 1; k++) g.field[k] = g.field[k + 1];
                g.field_count--;
                g.stock[g.stock_count++] = x;
                break;
            }
    assert(remove_card(&g, 8));
    g.field[g.field_count++] = 8;

    g.current_player = 0;
    g.phase = PHASE_TURN_HAND;
    g.has_pending = 0;
    assert(engine_validate_zones(&g));

    assert(engine_play_hand(&g, 9, 8));
    int screen = (g.phase == PHASE_DECIDE);
    printf("    %-44s -> %s\n", "red poem finished with a HAND card (KK-ADD-10)",
           screen ? "KOI-KOI SCREEN immediately"
                  : "no screen yet - question comes at the stock stage");
    assert(!screen && g.phase == PHASE_TURN_DRAW);
}

static void test_combo_triggers(void) {
    printf("Combo -> Koi-Koi screen trigger (real engine, stock stage):\n");
    printf("  [KK-ADD-10]\n");
    run_combo_case("Akatan - 3 red ribbons (the poem)", RULESET_KK_ADD_10, (int[]){1, 5}, 2, 9, 8, 1);
    run_combo_case("Aotan - 3 blue ribbons (the poem)", RULESET_KK_ADD_10, (int[]){21, 33}, 2, 37, 38, 1);
    run_combo_case("Tanzaku - 4th ribbon, mixed (no poem)", RULESET_KK_ADD_10, (int[]){1, 13, 17}, 3, 21, 22, 1);
    run_combo_case("Tanzaku grows - 5th ribbon", RULESET_KK_ADD_10, (int[]){1, 5, 9, 13}, 4, 21, 22, 1);
    run_combo_case("Hanami-zake - cherry + sake cup", RULESET_KK_ADD_10, (int[]){8}, 1, 32, 34, 1);
    run_combo_case("Tsukimi-zake - moon + sake cup", RULESET_KK_ADD_10, (int[]){28}, 1, 32, 34, 1);
    run_combo_case("Sanko - 3 brights", RULESET_KK_ADD_10, (int[]){0, 8}, 2, 28, 30, 1);
    run_combo_case("Shiko - 4 brights", RULESET_KK_ADD_10, (int[]){0, 8, 28}, 3, 44, 45, 1);
    run_combo_case("Goko - 5 brights", RULESET_KK_ADD_10, (int[]){0, 8, 28, 44}, 4, 40, 41, 1);
    run_combo_case("Ino-shika-cho - boar, deer, butterfly", RULESET_KK_ADD_10, (int[]){20, 24}, 2, 36, 38, 1);
    run_combo_case("Tane - 5 animals", RULESET_KK_ADD_10, (int[]){4, 12, 16, 20}, 4, 24, 26, 1);
    run_combo_case("Kasu - 9 plains + sake cup", RULESET_KK_ADD_10, (int[]){2, 3, 6, 7, 10, 11, 14, 15, 18}, 9, 32, 33, 1);
    run_combo_case("Kasu - 10 plains", RULESET_KK_ADD_10, (int[]){2, 3, 6, 7, 10, 11, 14, 15, 18, 34}, 10, 45, 46, 1);
    run_combo_case("control: 2 plains, no combo", RULESET_KK_ADD_10, NULL, 0, 2, 3, 0);
    run_combo_case("control: rain man blocks 3 brights", RULESET_KK_ADD_10, (int[]){0, 8}, 2, 40, 41, 0);
    run_combo_case("control: 3 mixed ribbons, no combo", RULESET_KK_ADD_10, (int[]){1, 13, 17}, 3, 12, 14, 0);

    printf("  [NINTENDO]\n");
    run_combo_case("Akatan - 3 red ribbons (the poem)", RULESET_NINTENDO, (int[]){1, 5}, 2, 9, 8, 1);
    run_combo_case("Aotan - 3 blue ribbons (the poem)", RULESET_NINTENDO, (int[]){21, 33}, 2, 37, 38, 1);
    run_combo_case("control: 4 mixed ribbons, needs 5", RULESET_NINTENDO, (int[]){1, 13, 17}, 3, 21, 22, 0);
    run_combo_case("red poem grows - 6th ribbon (Akatan 5->8)", RULESET_NINTENDO, (int[]){1, 5, 9, 13, 17}, 5, 21, 22, 1);
    run_combo_case("Hanami-zake - cherry + sake cup", RULESET_NINTENDO, (int[]){8}, 1, 32, 34, 1);
    run_combo_case("Tsukimi-zake - moon + sake cup", RULESET_NINTENDO, (int[]){28}, 1, 32, 34, 1);
    run_combo_case("Sanko - 3 brights", RULESET_NINTENDO, (int[]){0, 8}, 2, 28, 30, 1);
    run_combo_case("Shiko - 4 brights", RULESET_NINTENDO, (int[]){0, 8, 28}, 3, 44, 45, 1);
    run_combo_case("Goko - 5 brights", RULESET_NINTENDO, (int[]){0, 8, 28, 44}, 4, 40, 41, 1);
    run_combo_case("Ino-shika-cho - boar, deer, butterfly", RULESET_NINTENDO, (int[]){20, 24}, 2, 36, 38, 1);
    run_combo_case("Tane - 5 animals", RULESET_NINTENDO, (int[]){4, 12, 16, 20}, 4, 24, 26, 1);
    run_combo_case("Kasu - 10 plains", RULESET_NINTENDO, (int[]){2, 3, 6, 7, 10, 11, 14, 15, 18, 34}, 10, 45, 46, 1);
    run_combo_case("control: 9 plains + cup, no cup bonus", RULESET_NINTENDO, (int[]){2, 3, 6, 7, 10, 11, 14, 15, 18}, 9, 32, 33, 0);
    run_combo_case("control: rain man blocks 3 brights", RULESET_NINTENDO, (int[]){0, 8}, 2, 40, 41, 0);

    printf("  [TOURNAMENT]\n");
    run_combo_case("sake + moon: viewings are DISABLED", RULESET_TOURNAMENT, (int[]){28}, 1, 32, 34, 0);
    run_combo_case("Sanko - 3 brights", RULESET_TOURNAMENT, (int[]){0, 8}, 2, 28, 30, 1);

    printf("  [Hand-stage probe]\n");
    test_hand_stage_probe();
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
    test_combo_triggers();
    test_simulated_matches();
    printf("All engine & ruleset tests passed (500 full matches verified).\n");
    return 0;
}
