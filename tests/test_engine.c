/* Engine + rules tests: scorer expectations are taken from published
   Koi-Koi rules (FudaWiki yaku table, Nintendo values where FudaWiki notes
   they differ), plus the house definitions for KK-ADD-10 (additive fixed
   sets), Tournament (no Tsukimi-zake, Sake Cup strictly an animal) and
   Yakuza (Tournament scoring + Oya-ken: dealer takes an exhausted round
   for a flat 6). Run via: ./build.sh test */
#include <stdio.h>
#include <string.h>
#include "engine.h"
#include "ai.h"
#include "campaign.h"

static int failures = 0, checks = 0;
#define CHECK(cond) do { checks++; if (!(cond)) { failures++; \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static int base(Ruleset r, const int *c, int n) { return score_captures(r, c, n).base_points; }
static int base_ex(Ruleset r, const int *c, int n, int m) { return score_captures_ex(r, c, n, m).base_points; }
static int has_yaku(Ruleset r, const int *c, int n, YakuId id) {
    ScoreResult s = score_captures(r, c, n);
    for (int i = 0; i < s.yaku_count; i++) if (s.yakus[i].id == id) return 1;
    return 0;
}

static const Ruleset ALL[5] = { RULESET_NINTENDO, RULESET_FUDAWIKI, RULESET_KK_ADD_10,
                                RULESET_TOURNAMENT, RULESET_YAKUZA };

static void test_bright_family(void) {
    int goko[] = {0, 8, 28, 40, 44};
    CHECK(base(RULESET_NINTENDO, goko, 5) == 10);
    CHECK(base(RULESET_FUDAWIKI, goko, 5) == 15);
    CHECK(base(RULESET_KK_ADD_10, goko, 5) == 10);
    CHECK(base(RULESET_TOURNAMENT, goko, 5) == 10);
    CHECK(base(RULESET_YAKUZA, goko, 5) == 10);
    CHECK(has_yaku(RULESET_NINTENDO, goko, 5, YAKU_GOKO));
    int shiko[] = {0, 8, 28, 44};
    int ame[] = {40, 0, 8, 28};
    int sanko[] = {0, 8, 28};
    int rain3[] = {40, 0, 8}; /* 3 brights incl. Rain: no Sanko */
    for (int i = 0; i < 5; i++) {
        CHECK(base(ALL[i], shiko, 4) == 8);
        CHECK(base(ALL[i], ame, 4) == 7);
        CHECK(base(ALL[i], rain3, 3) == 0);
    }
    CHECK(base(RULESET_NINTENDO, sanko, 3) == 5);
    CHECK(base(RULESET_FUDAWIKI, sanko, 3) == 6);
    CHECK(base(RULESET_KK_ADD_10, sanko, 3) == 5);
    CHECK(base(RULESET_TOURNAMENT, sanko, 3) == 5);
    CHECK(base(RULESET_YAKUZA, sanko, 3) == 5);
}

static void test_viewings(void) {
    int hanami[] = {8, 32}, tsukimi[] = {28, 32}, both[] = {8, 28, 32};
    for (int i = 0; i < 5; i++) CHECK(base(ALL[i], hanami, 2) == 5); /* Hanami everywhere */
    CHECK(base(RULESET_NINTENDO, tsukimi, 2) == 5);
    CHECK(base(RULESET_FUDAWIKI, tsukimi, 2) == 5);
    CHECK(base(RULESET_KK_ADD_10, tsukimi, 2) == 5);
    CHECK(base(RULESET_TOURNAMENT, tsukimi, 2) == 0); /* removed: too strong */
    CHECK(base(RULESET_YAKUZA, tsukimi, 2) == 0);    /* Yakuza = Tournament scoring */
    CHECK(base(RULESET_NINTENDO, both, 3) == 10);
    CHECK(base(RULESET_TOURNAMENT, both, 3) == 5);   /* Hanami only */
    CHECK(base(RULESET_YAKUZA, both, 3) == 5);
}

static void test_animals_ribbons(void) {
    int ino5[] = {20, 24, 36, 4, 12};       /* Ino trio + 2 animals */
    int rib6[] = {1, 5, 9, 21, 33, 37};     /* both ribbon trios */
    CHECK(base(RULESET_NINTENDO, ino5, 5) == 7);  /* 5 +1 per extra animal */
    CHECK(base(RULESET_FUDAWIKI, ino5, 5) == 7);
    CHECK(base(RULESET_KK_ADD_10, ino5, 5) == 6); /* fixed 5 + Tane 1 */
    CHECK(base(RULESET_NINTENDO, rib6, 6) == 10); /* Chofuku */
    CHECK(base(RULESET_FUDAWIKI, rib6, 6) == 10);
    CHECK(base(RULESET_KK_ADD_10, rib6, 6) == 12);/* 5 + 5 + Tanzaku 2 */
    int tane5[] = {4, 12, 16, 29, 41};
    int tan5[] = {13, 17, 25, 42, 1};
    for (int i = 0; i < 5; i++) {
        CHECK(base(ALL[i], tane5, 5) == 1);
        CHECK(base(ALL[i], tan5, 5) == 1);
    }
}

static void test_kasu_and_cup(void) {
    int nine_cup[] = {2, 3, 6, 7, 10, 11, 14, 15, 18, 32}; /* 9 plains + Sake Cup */
    CHECK(base(RULESET_NINTENDO, nine_cup, 10) == 1);  /* cup counts as chaff */
    CHECK(base(RULESET_FUDAWIKI, nine_cup, 10) == 1);
    CHECK(base(RULESET_KK_ADD_10, nine_cup, 10) == 1);
    CHECK(base(RULESET_TOURNAMENT, nine_cup, 10) == 0); /* cup strictly animal */
    CHECK(base(RULESET_YAKUZA, nine_cup, 10) == 0);
    int ten[] = {2, 3, 6, 7, 10, 11, 14, 15, 18, 19};
    for (int i = 0; i < 5; i++) CHECK(base(ALL[i], ten, 10) == 1);
}

static void test_tsukifuda(void) {
    int jan[] = {0, 1, 2, 3}; /* all of January */
    CHECK(base_ex(RULESET_NINTENDO, jan, 4, 1) == 4);
    CHECK(base_ex(RULESET_NINTENDO, jan, 4, 2) == 0); /* wrong month */
    CHECK(base_ex(RULESET_NINTENDO, jan, 4, 0) == 0); /* disabled */
    CHECK(base_ex(RULESET_YAKUZA, jan, 4, 1) == 4);
    CHECK(ruleset_round_month(1, 12) == 1);
    CHECK(ruleset_round_month(12, 12) == 12);
    CHECK(ruleset_round_month(1, 6) == 0);
}

/* Every capture that grows a scored category after a trio is complete
   must count as an improvement (earns a Koi-Koi/Shobu decision), even
   where the base does not move (KK-ADD-10 fixed-value trios). */
static void test_improvement_detection(void) {
    int red[] = {1, 5, 9, 13, 17, 25, 21};       /* Akatan first, then +ribbons */
    int ino[] = {20, 24, 36, 4, 12, 16};         /* Ino first, then +animals */
    for (int r = 0; r < 5; r++) {
        for (int n = 3; n < 7; n++) {
            ScoreResult a = score_captures(ALL[r], red, n);
            ScoreResult b = score_captures(ALL[r], red, n + 1);
            CHECK(score_is_improved(&a, &b));
        }
        for (int n = 3; n < 6; n++) {
            ScoreResult a = score_captures(ALL[r], ino, n);
            ScoreResult b = score_captures(ALL[r], ino, n + 1);
            CHECK(score_is_improved(&a, &b));
        }
    }
    /* No capture / unrelated capture: not an improvement. */
    int plain1[] = {1, 5, 9}, plain2[] = {1, 5, 9};
    ScoreResult a = score_captures(RULESET_KK_ADD_10, plain1, 3);
    ScoreResult b = score_captures(RULESET_KK_ADD_10, plain2, 3);
    CHECK(!score_is_improved(&a, &b));
}

/* Craft a mid-turn state: player 0 already called Koi-Koi holding the
   given pile, plays hand card onto its matching field card, then draws
   the stock card (which matches nothing). Returns the final phase. */
static GamePhase play_capture_turn(Ruleset rs, const int *pile, int pn,
                                   int hand_card, int field_card, int stock_card) {
    GameState g;
    memset(&g, 0, sizeof(g));
    g.ruleset = rs;
    g.total_rounds = 6;
    g.current_round = 1;
    g.phase = PHASE_TURN_HAND;
    g.current_player = 0;
    g.dealer = 0;
    for (int i = 0; i < pn; i++) g.captured[0][i] = pile[i];
    g.captured_count[0] = pn;
    g.last_score[0] = score_captures(rs, pile, pn);
    g.last_base[0] = g.last_score[0].base_points;
    g.koi_called[0] = 1;
    g.hand[0][0] = hand_card; g.hand[0][1] = 44; g.hand_count[0] = 2;
    g.field[0] = field_card; g.field_count = 1;
    g.stock[0] = stock_card; g.stock_count = 1;
    engine_play_hand(&g, hand_card, -1);
    engine_reveal_stock(&g);
    return g.phase;
}

static void test_engine_decide_trigger(void) {
    int akatan[] = {1, 5, 9};
    /* KK-ADD-10: Akatan held, a 4th ribbon is captured. Base stays 5,
       but the player must still get the Koi-Koi screen. */
    CHECK(play_capture_turn(RULESET_KK_ADD_10, akatan, 3, 14, 13, 30) == PHASE_DECIDE);
    /* Tournament: same capture improves Akatan (5->6). Koi was already
       called once; Tournament has no one-call limit any more. */
    CHECK(play_capture_turn(RULESET_TOURNAMENT, akatan, 3, 14, 13, 30) == PHASE_DECIDE);
    CHECK(play_capture_turn(RULESET_YAKUZA, akatan, 3, 14, 13, 30) == PHASE_DECIDE);
    CHECK(play_capture_turn(RULESET_FUDAWIKI, akatan, 3, 14, 13, 30) == PHASE_DECIDE);
    /* Nintendo keeps its published one-call rule: the next improvement
       after a Koi-Koi call is an automatic Shobu (round ends). */
    CHECK(play_capture_turn(RULESET_NINTENDO, akatan, 3, 14, 13, 30) == PHASE_ROUND_END);
}

static void test_yakuza_exhaustion(void) {
    GameState g;
    memset(&g, 0, sizeof(g));
    g.ruleset = RULESET_YAKUZA;
    g.total_rounds = 6;
    g.current_round = 1;
    g.phase = PHASE_TURN_DRAW;
    g.current_player = 0;
    g.dealer = 1;
    g.koi_called[0] = 1; /* a Koi-Koi was called earlier this round */
    engine_reveal_stock(&g); /* no stock left: exhaustion */
    CHECK(g.phase == PHASE_ROUND_END);
    CHECK(g.current_round_record.winner == 1); /* dealer */
    CHECK(g.current_round_record.award == 6);
    CHECK(g.total_score[1] == 6);
    CHECK(g.dealer == 1);

    /* Non-Yakuza exhaustion stays a 0-0 draw. */
    memset(&g, 0, sizeof(g));
    g.ruleset = RULESET_TOURNAMENT;
    g.phase = PHASE_TURN_DRAW;
    g.dealer = 1;
    engine_reveal_stock(&g);
    CHECK(g.current_round_record.winner == -1);
    CHECK(g.total_score[0] == 0 && g.total_score[1] == 0);
}

static void test_boss_personalities(void) {
    /* Campaign favourites are the bosses' personalities: they must be
       exactly what campaign.h defines, in order, after the rules fix. */
    static const int fav_yaku[13] = {
        YAKU_GOKO, YAKU_TANZAKU, YAKU_GOKO, YAKU_KASU, YAKU_TANE, YAKU_TANE,
        YAKU_INO_SHIKA_CHO, YAKU_GOKO, YAKU_TANE, YAKU_INO_SHIKA_CHO,
        YAKU_TANZAKU, YAKU_GOKO, YAKU_GOKO
    };
    static const int fav_card[13] = { 0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 44 };
    for (int i = 0; i <= 12; i++) {
        const Opponent *o = campaign_get_opponent(i);
        CHECK(o->favorite_yaku == fav_yaku[i]);
        CHECK(o->favorite_card_id == fav_card[i]);
    }
    /* Yaku enum indices are persisted in stats files: they must not move. */
    CHECK(YAKU_GOKO == 0 && YAKU_KASU == 12 && YAKU_TSUKIFUDA == 13 && YAKU_COUNT == 14);

    /* The AI must still chase a boss's signature card first (DEUS plays
       optimally, so no RNG blunder interferes). Boss 0 loves card 0. */
    GameState g;
    memset(&g, 0, sizeof(g));
    g.ruleset = RULESET_NINTENDO;
    g.total_rounds = 6; g.current_round = 1;
    g.current_player = 1;
    g.hand[1][0] = 0; g.hand[1][1] = 45; g.hand_count[1] = 2;
    g.field[0] = 2; g.field[1] = 30; g.field_count = 2;
    ai_set_opponent(0);
    uint32_t rng = 12345;
    int card = -1, target = -1;
    ai_choose_hand(&g, DIFFICULTY_DEUS, &rng, &card, &target);
    CHECK(card == 0 && target == 2);

    /* Boss 6 loves Ino-shika-cho / the Boar (24). */
    memset(&g, 0, sizeof(g));
    g.ruleset = RULESET_NINTENDO;
    g.total_rounds = 6; g.current_round = 1;
    g.current_player = 1;
    g.hand[1][0] = 24; g.hand[1][1] = 45; g.hand_count[1] = 2;
    g.field[0] = 26; g.field[1] = 30; g.field_count = 2;
    ai_set_opponent(6);
    rng = 999;
    card = target = -1;
    ai_choose_hand(&g, DIFFICULTY_DEUS, &rng, &card, &target);
    CHECK(card == 24 && target == 26);
}

/* Random full matches driven by the AI for both seats: the engine must
   always terminate and conserve all 48 cards. */
static void test_random_matches(void) {
    for (int seed = 1; seed <= 60; seed++) {
        GameState g;
        engine_init(&g, ALL[seed % 5], 3, (uint32_t)(seed * 7919));
        engine_start_round(&g);
        uint32_t rng = (uint32_t)(seed * 104729 + 7);
        int steps = 0;
        while (g.phase != PHASE_MATCH_END && steps < 20000) {
            steps++;
            CHECK(engine_validate_zones(&g));
            switch (g.phase) {
                case PHASE_TURN_HAND: {
                    int card, target;
                    ai_choose_hand(&g, DIFFICULTY_HARD, &rng, &card, &target);
                    if (card < 0) { CHECK(0); goto next_seed; }
                    engine_play_hand(&g, card, target);
                    break;
                }
                case PHASE_HAND_CHOICE:
                    engine_pick_hand_choice(&g, g.choice_targets[0]);
                    break;
                case PHASE_TURN_DRAW:
                    engine_reveal_stock(&g);
                    break;
                case PHASE_DRAW_CHOICE:
                    engine_pick_draw_choice(&g, ai_choose_stock_target(&g, DIFFICULTY_HARD, &rng));
                    break;
                case PHASE_DECIDE:
                    engine_decide(&g, ai_choose_decision(&g, DIFFICULTY_HARD, &rng));
                    break;
                case PHASE_ROUND_END:
                    engine_next_round(&g);
                    break;
                default:
                    CHECK(0); goto next_seed;
            }
        }
        CHECK(g.phase == PHASE_MATCH_END);
        next_seed:;
    }
}

int main(void) {
    CHECK(cards_validate());
    test_bright_family();
    test_viewings();
    test_animals_ribbons();
    test_kasu_and_cup();
    test_tsukifuda();
    test_improvement_detection();
    test_engine_decide_trigger();
    test_yakuza_exhaustion();
    test_boss_personalities();
    test_random_matches();
    printf("%s: %d checks, %d failures\n", failures ? "FAIL" : "PASS", checks, failures);
    return failures ? 1 : 0;
}
