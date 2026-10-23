/*
 * test_koikoi_trigger.c — Verifies Koi-Koi decision screen triggers and
 * resolution across all wiki yakus, growth stages, and ruleset rules.
 *
 * Links the real src/cards.c, src/rules.c, and src/engine.c with zero copies
 * of scoring or phase logic.
 *
 * Build & run:
 *   gcc -std=c99 -O2 -Wall -Wextra -Isrc tools/test_koikoi_trigger.c \
 *       src/cards.c src/rules.c src/engine.c -o /tmp/test_koikoi_trigger
 *   /tmp/test_koikoi_trigger
 *
 * Exit code 0 = all checks passed.
 */
#include <stdio.h>
#include <string.h>
#include "cards.h"
#include "engine.h"
#include "rules.h"

static int failures = 0;

/* Run one turn ending with `pile` captured under `rs`, with the previous
   decision snapshot initialized from `snap`/`snap_n`. */
static GamePhase run_turn_snapshot_rs(Ruleset rs, int hand_cards_p0, int koi_p0, int koi_p1,
                                      const int *pile, int n,
                                      const int *snap, int snap_n, GameState *out) {
    GameState g;
    engine_init(&g, rs, 1, 12345);
    g.current_player = 0;
    g.dealer = 0;
    g.phase = PHASE_TURN_DRAW;
    g.koi_called[0] = koi_p0;
    g.koi_called[1] = koi_p1;

    memcpy(g.captured[0], pile, sizeof(int) * (size_t)n);
    g.captured_count[0] = n;
    g.last_base[0] = score_captures_base(rs, snap, snap_n);
    yaku_snapshot(rs, snap, snap_n, g.last_yaku_points[0]);

    /* Non-matching hand & stock cards drawn strictly outside `pile` so the
       stock reveal ends the turn naturally through finish_stock_turn(). */
    int used[48] = {0};
    for (int i = 0; i < n; i++) used[pile[i]] = 1;
    int next = 0;
    for (int k = 0; k < hand_cards_p0; k++) {
        while (used[next]) next++;
        g.hand[0][g.hand_count[0]++] = next;
        used[next] = 1;
    }
    for (int k = 0; k < 2; k++) {
        while (used[next]) next++;
        g.hand[1][g.hand_count[1]++] = next;
        used[next] = 1;
    }
    while (used[next]) next++;
    g.stock[g.stock_count++] = next;

    engine_reveal_stock(&g);
    *out = g;
    return g.phase;
}

static GamePhase run_turn_rs(Ruleset rs, const int *pile, int n, GameState *out) {
    return run_turn_snapshot_rs(rs, 2, 0, 0, pile, n, pile, n - 1, out);
}

static GamePhase run_turn(const int *pile, int n, GameState *out) {
    return run_turn_rs(RULESET_KK_ADD_10, pile, n, out);
}

static void check_growth_rs(Ruleset rs, const char *name,
                            const int *before_pile, int bn, const int *after_pile, int an) {
    GameState g;
    GamePhase phase = run_turn_snapshot_rs(rs, 2, 0, 0, after_pile, an, before_pile, bn, &g);
    int ok = (phase == PHASE_DECIDE);
    printf("%-20s grows (%2d -> %2d pts)  Koi-Koi screen: %s\n", name,
           score_captures_base(rs, before_pile, bn),
           score_captures_base(rs, after_pile, an),
           ok ? "YES" : "NO  <-- PROBLEM");
    if (!ok) failures++;
}

static void check_no_growth_rs(Ruleset rs, const char *name,
                               const int *before_pile, int bn, const int *after_pile, int an) {
    GameState g;
    GamePhase phase = run_turn_snapshot_rs(rs, 2, 0, 0, after_pile, an, before_pile, bn, &g);
    int ok = (phase != PHASE_DECIDE);
    printf("%-20s no growth (control)   Koi-Koi screen: %s\n", name,
           ok ? "no (correct)" : "YES  <-- PROBLEM");
    if (!ok) failures++;
}

static void check_yaku(const char *name, const int *pile, int n, int expected_after) {
    GameState g_koi, g_shobu;
    GamePhase phase = run_turn(pile, n, &g_koi);
    int before = score_captures_base(RULESET_KK_ADD_10, pile, n - 1);
    int after = score_captures_base(RULESET_KK_ADD_10, pile, n);
    int ok = (phase == PHASE_DECIDE && after == expected_after);
    printf("%-20s before=%2d after=%2d  Koi-Koi screen: %s\n",
           name, before, after, ok ? "YES" : "NO  <-- PROBLEM");
    if (!ok) { failures++; return; }

    /* Choosing Koi-Koi records the call, snapshots yakus, and continues play. */
    g_shobu = g_koi;
    engine_decide(&g_koi, DECISION_KOI_KOI);
    int cont = (g_koi.koi_called[0] == 1 && g_koi.match_koi_calls[0] == 1 && g_koi.phase == PHASE_TURN_HAND);
    printf("%-20s choosing Koi-Koi: %s\n", "", cont ? "call recorded, play continues" : "BROKEN");
    if (!cont) failures++;

    /* Choosing Shobu ends the round immediately and awards calculate_award(). */
    engine_decide(&g_shobu, DECISION_SHOBU);
    int want_award = calculate_award(after, 0, 0);
    int banked = (g_shobu.phase == PHASE_ROUND_END && g_shobu.current_round_record.winner == 0 &&
                  g_shobu.dealer == 0 && g_shobu.total_score[0] == want_award &&
                  g_shobu.current_round_record.award == want_award);
    if (!banked) {
        printf("%-20s choosing Shobu: BROKEN (got %d, want %d)\n", "", g_shobu.total_score[0], want_award);
        failures++;
    }
}

static void check_no_trigger_rs(Ruleset rs, const char *name, const int *pile, int n) {
    GameState g;
    GamePhase phase = run_turn_rs(rs, pile, n, &g);
    int ok = (phase != PHASE_DECIDE && g.phase == PHASE_TURN_HAND);
    printf("%-20s (control) Koi-Koi screen: %s\n", name, ok ? "no (correct)" : "YES  <-- PROBLEM");
    if (!ok) failures++;
}

static void check_cross_ruleset_koi_rules(void) {
    static const int s_sanko[]    = { 0, 8, 28 };
    static const int s_shiko[]    = { 0, 8, 28, 44 };
    static const int s_hanami[]   = { 8, 32 };
    static const int s_tsukimi[]  = { 28, 32 };
    static const int s_tanzaku4[] = { 1, 13, 17, 25 };
    static const int s_kasu_cup[] = { 2, 3, 6, 7, 10, 11, 14, 15, 18, 32 };
    static const int s_ino3[]     = { 20, 24, 36 };
    static const int s_ino4[]     = { 20, 24, 36, 4 };

    printf("\nCross-ruleset wiki Koi-Koi rules:\n");

    /* 1. 1-call cap + forced auto-Shobu in Nintendo, Tournament, Yakuza;
          unlimited Koi-Koi calls in FudaWiki and KK-ADD-10. */
    for (int r = 0; r < 5; r++) {
        GameState g;
        Ruleset rs = (Ruleset)r;
        int capped = (rs == RULESET_NINTENDO || rs == RULESET_TOURNAMENT || rs == RULESET_YAKUZA);
        GamePhase ph = run_turn_snapshot_rs(rs, 2, 1, 0, s_shiko, 4, s_sanko, 3, &g);
        int ok = capped ? (ph == PHASE_ROUND_END && g.current_round_record.winner == 0 &&
                           g.current_round_record.award == calculate_award(8, 1, 0))
                        : (ph == PHASE_DECIDE);
        if (!ok) {
            printf("FAIL: ruleset %d after 1 Koi-Koi call got phase=%d award=%d\n",
                   r, (int)ph, g.current_round_record.award);
            failures++;
        }
    }
    printf("1-call auto-Shobu (Nintendo/Tournament/Yakuza) vs unlimited (FudaWiki/KK-ADD-10): YES\n");

    /* 2. Empty hand (0 cards left) skips Koi-Koi screen and auto-banks Shobu in all 5 rulesets. */
    for (int r = 0; r < 5; r++) {
        GameState g;
        Ruleset rs = (Ruleset)r;
        int expected_sanko = (rs == RULESET_FUDAWIKI) ? 6 : 5;
        GamePhase ph = run_turn_snapshot_rs(rs, 0, 0, 1, s_sanko, 3, s_sanko, 2, &g);
        int want = calculate_award(expected_sanko, 0, 1); /* opponent called Koi-Koi -> x2 */
        if (ph != PHASE_ROUND_END || g.current_round_record.winner != 0 || g.total_score[0] != want) {
            printf("FAIL: empty-hand auto-Shobu broken in ruleset %d (phase=%d score=%d want=%d)\n",
                   r, (int)ph, g.total_score[0], want);
            failures++;
        }
    }
    printf("Last-card empty hand forces automatic Shobu across all 5 rulesets: YES\n");

    /* 3. Viewings trigger PHASE_DECIDE in Nintendo/FudaWiki/KK-ADD-10 and never in Tournament/Yakuza. */
    for (int r = 0; r < 5; r++) {
        GameState g1, g2;
        Ruleset rs = (Ruleset)r;
        int viewings_on = (rs != RULESET_TOURNAMENT && rs != RULESET_YAKUZA);
        GamePhase p1 = run_turn_rs(rs, s_hanami, 2, &g1);
        GamePhase p2 = run_turn_rs(rs, s_tsukimi, 2, &g2);
        int ok = viewings_on ? (p1 == PHASE_DECIDE && p2 == PHASE_DECIDE)
                             : (p1 != PHASE_DECIDE && p2 != PHASE_DECIDE);
        if (!ok) {
            printf("FAIL: viewing Koi-Koi trigger mismatch in ruleset %d\n", r);
            failures++;
        }
    }
    printf("Viewings open Koi-Koi only in Nintendo/FudaWiki/KK-ADD-10 (disabled in Tournament/Yakuza): YES\n");

    /* 4. 4 Ribbons and 9 Plains + Sake Cup open PHASE_DECIDE exclusively in KK-ADD-10. */
    for (int r = 0; r < 5; r++) {
        GameState g1, g2;
        Ruleset rs = (Ruleset)r;
        GamePhase p1 = run_turn_rs(rs, s_tanzaku4, 4, &g1);
        GamePhase p2 = run_turn_rs(rs, s_kasu_cup, 10, &g2);
        int ok = (rs == RULESET_KK_ADD_10) ? (p1 == PHASE_DECIDE && p2 == PHASE_DECIDE)
                                           : (p1 != PHASE_DECIDE && p2 != PHASE_DECIDE);
        if (!ok) {
            printf("FAIL: KK-ADD-10 exclusive threshold leaked in ruleset %d\n", r);
            failures++;
        }
    }
    printf("4-ribbon Tanzaku & 9-Plain+Cup Kasu open Koi-Koi only in KK-ADD-10: YES\n");

    /* 5. Score-based vs combo-based: 4th animal after Ino-shika-cho opens Koi-Koi in FudaWiki
          (score 5 -> 6), while in KK-ADD-10 fixed Ino stays 5 and waits for 5th animal (Tane). */
    check_growth_rs(RULESET_FUDAWIKI, "FudaWiki Ino+4th", s_ino3, 3, s_ino4, 4);
    check_no_growth_rs(RULESET_KK_ADD_10, "KK-ADD-10 Ino+4th", s_ino3, 3, s_ino4, 4);
}

int main(void) {
    static const int p_goko[]     = { 0, 8, 28, 40, 44 };
    static const int p_shiko[]    = { 0, 8, 28, 44 };
    static const int p_ame[]      = { 0, 8, 28, 40 };
    static const int p_sanko[]    = { 0, 8, 28 };
    static const int p_hanami[]   = { 8, 32 };
    static const int p_tsukimi[]  = { 28, 32 };
    static const int p_ino[]      = { 20, 24, 36 };
    static const int p_akatan[]   = { 1, 5, 9 };
    static const int p_aotan[]    = { 21, 33, 37 };
    static const int p_both[]     = { 1, 5, 9, 21, 33, 37 };
    static const int p_tane[]     = { 4, 12, 16, 29, 41 };
    static const int p_tanzaku4[] = { 1, 13, 17, 25 };
    static const int p_kasu[]     = { 2, 3, 6, 7, 10, 11, 14, 15, 18, 19 };
    static const int p_kasu_cup[] = { 2, 3, 6, 7, 10, 11, 14, 15, 18, 32 };

    static const int p_rain_sanko[] = { 0, 8, 40 };  /* 3 Brights incl. Rain Man: no Sanko */
    static const int p_nothing[]    = { 2, 3 };      /* two Plains: no yaku */
    static const int p_rib3[]       = { 1, 13, 17 }; /* 3 ungrouped ribbons: below 4-ribbon threshold */
    static const int p_plains8cup[] = { 2, 3, 6, 7, 10, 11, 14, 15, 32 }; /* 8 Plains + Cup = 9 < 10 */

    if (!cards_validate()) { printf("cards_validate() failed\n"); return 1; }

    printf("KK-ADD-10: does completing each wiki yaku open the Koi-Koi screen?\n\n");
    check_yaku("Goko", p_goko, 5, 10);
    check_yaku("Shiko", p_shiko, 4, 8);
    check_yaku("Ame-shiko", p_ame, 4, 7);
    check_yaku("Sanko", p_sanko, 3, 5);
    check_yaku("Hanami-zake", p_hanami, 2, 5);
    check_yaku("Tsukimi-zake", p_tsukimi, 2, 5);
    check_yaku("Ino-shika-cho", p_ino, 3, 5);
    check_yaku("Akatan", p_akatan, 3, 5);
    check_yaku("Aotan", p_aotan, 3, 5);
    check_yaku("Akatan+Aotan", p_both, 6, 13);
    check_yaku("Tane (5 animals)", p_tane, 5, 1);
    check_yaku("Tanzaku (4 ribbons)", p_tanzaku4, 4, 1);
    check_yaku("Kasu (10 plains)", p_kasu, 10, 1);
    check_yaku("Kasu (9+Sake Cup)", p_kasu_cup, 10, 1);
    check_no_trigger_rs(RULESET_KK_ADD_10, "Rain-Sanko", p_rain_sanko, 3);
    check_no_trigger_rs(RULESET_KK_ADD_10, "3 Ribbons (no set)", p_rib3, 3);
    check_no_trigger_rs(RULESET_KK_ADD_10, "8 Plains+Cup", p_plains8cup, 9);
    check_no_trigger_rs(RULESET_KK_ADD_10, "No yaku", p_nothing, 2);

    /* Growth: the combo was already banked at an earlier Koi-Koi decision,
       then upgrades or gains a card and must open the Koi-Koi screen again. */
    static const int g_tane6[]      = { 4, 12, 16, 29, 41, 20 };
    static const int g_tanzaku5[]   = { 1, 13, 17, 25, 42 };
    static const int g_tanzaku6[]   = { 1, 13, 17, 25, 42, 5 };
    static const int g_kasu11[]     = { 2, 3, 6, 7, 10, 11, 14, 15, 18, 19, 22 };
    static const int g_kasu_cup11[] = { 2, 3, 6, 7, 10, 11, 14, 15, 18, 19, 32 };
    static const int g_ino_tane[]   = { 20, 24, 36, 4, 29 }; /* Ino, then Tane appears on top */
    static const int g_sanko_idle[] = { 0, 8, 28, 2 };       /* Sanko + an unrelated Plain */

    printf("\n");
    check_growth_rs(RULESET_KK_ADD_10, "Sanko->Shiko", p_sanko, 3, p_shiko, 4);
    check_growth_rs(RULESET_KK_ADD_10, "Shiko->Goko", p_shiko, 4, p_goko, 5);
    check_growth_rs(RULESET_KK_ADD_10, "Sanko->Ame-shiko", p_sanko, 3, p_ame, 4);
    check_growth_rs(RULESET_KK_ADD_10, "Ame-shiko->Goko", p_ame, 4, p_goko, 5);
    check_growth_rs(RULESET_KK_ADD_10, "Akatan->Both", p_akatan, 3, p_both, 6);
    check_growth_rs(RULESET_KK_ADD_10, "Aotan->Both", p_aotan, 3, p_both, 6);
    check_growth_rs(RULESET_KK_ADD_10, "Tane 5->6", p_tane, 5, g_tane6, 6);
    check_growth_rs(RULESET_KK_ADD_10, "Tanzaku 4->5", p_tanzaku4, 4, g_tanzaku5, 5);
    check_growth_rs(RULESET_KK_ADD_10, "Tanzaku 5->6", g_tanzaku5, 5, g_tanzaku6, 6);
    check_growth_rs(RULESET_KK_ADD_10, "Kasu 10->11", p_kasu, 10, g_kasu11, 11);
    check_growth_rs(RULESET_KK_ADD_10, "Kasu Cup 10->11", p_kasu_cup, 10, g_kasu_cup11, 11);
    check_growth_rs(RULESET_KK_ADD_10, "Ino->+Tane", p_ino, 3, g_ino_tane, 5);
    check_no_growth_rs(RULESET_KK_ADD_10, "Sanko+Plain", p_sanko, 3, g_sanko_idle, 4);

    check_cross_ruleset_koi_rules();

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
