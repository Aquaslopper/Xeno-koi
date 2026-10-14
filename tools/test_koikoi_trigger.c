/*
 * test_koikoi_trigger.c — Does each wiki yaku open the Koi-Koi screen
 * under KK-ADD-10?
 *
 * For each yaku's smallest capture pile (the same piles the wiki tables
 * use in tools/wiki_tables.c), this test:
 *   1. Puts the pile in the player's captured zone, with last_base set to
 *      the score of the pile WITHOUT its last card (as if that last card
 *      was just captured in the hand stage of this turn).
 *   2. Ends the turn the way a real turn ends: engine_reveal_stock() with
 *      a stock card that matches nothing, which runs finish_stock_turn().
 *   3. Checks that the engine is now in PHASE_DECIDE — the phase that
 *      shows the Koi-Koi / Shobu screen — and that choosing Koi-Koi
 *      records the call and play continues.
 *
 * Two negative controls (no yaku / Rain Man blocking Sanko) must NOT
 * reach PHASE_DECIDE.
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

/* Score of the pile with its last card removed. */
static int base_without_last(const int *pile, int n) {
    return score_captures_base(RULESET_KK_ADD_10, pile, n - 1);
}

/* Run one turn ending with `pile` captured, with the combo snapshot set
   from `snap`/`snap_n` (the pile at the player's last Koi-Koi decision,
   or the pile without its last card for a first decision). */
static GamePhase run_turn_snapshot(const int *pile, int n, const int *snap, int snap_n, GameState *out) {
    GameState g;
    engine_init(&g, RULESET_KK_ADD_10, 1, 12345);
    g.current_player = 0;
    g.dealer = 0;
    g.phase = PHASE_TURN_DRAW;

    memcpy(g.captured[0], pile, sizeof(int) * n);
    g.captured_count[0] = n;
    g.last_base[0] = score_captures_base(RULESET_KK_ADD_10, snap, snap_n);
    {
        ScoreResult sr = score_captures(RULESET_KK_ADD_10, snap, snap_n);
        for (int i = 0; i < sr.yaku_count; i++)
            g.last_yaku_points[0][sr.yakus[i].id] = sr.yakus[i].points;
    }

    /* Give both players cards in hand (needed so play can continue),
       and one stock card that matches nothing on the empty field.
       Cards are picked so nothing overlaps the pile. */
    int used[48] = {0};
    for (int i = 0; i < n; i++) used[pile[i]] = 1;
    int next = 0;
    for (int k = 0; k < 2; k++) { while (used[next]) next++; g.hand[0][g.hand_count[0]++] = next; used[next] = 1; }
    for (int k = 0; k < 2; k++) { while (used[next]) next++; g.hand[1][g.hand_count[1]++] = next; used[next] = 1; }
    while (used[next]) next++;
    g.stock[g.stock_count++] = next;

    engine_reveal_stock(&g);
    *out = g;
    return g.phase;
}

/* A turn ending with `pile` captured, snapshot from the pile minus its last card. */
static GamePhase run_turn(const int *pile, int n, GameState *out) {
    return run_turn_snapshot(pile, n, pile, n - 1, out);
}

/* A combo that was already on the board at the last Koi-Koi decision grows. */
static void check_growth(const char *name, const int *before_pile, int bn, const int *after_pile, int an) {
    GameState g;
    GamePhase phase = run_turn_snapshot(after_pile, an, before_pile, bn, &g);
    int ok = (phase == PHASE_DECIDE);
    printf("%-16s grows (%d -> %d pts)  Koi-Koi screen: %s\n", name,
           score_captures_base(RULESET_KK_ADD_10, before_pile, bn),
           score_captures_base(RULESET_KK_ADD_10, after_pile, an),
           ok ? "YES" : "NO  <-- PROBLEM");
    if (!ok) failures++;
}

static void check_no_growth(const char *name, const int *before_pile, int bn, const int *after_pile, int an) {
    GameState g;
    GamePhase phase = run_turn_snapshot(after_pile, an, before_pile, bn, &g);
    int ok = (phase != PHASE_DECIDE);
    printf("%-16s no growth (control)  Koi-Koi screen: %s\n", name, ok ? "no (correct)" : "YES  <-- PROBLEM");
    if (!ok) failures++;
}

static void check_yaku(const char *name, const int *pile, int n) {
    GameState g;
    GamePhase phase = run_turn(pile, n, &g);
    int before = base_without_last(pile, n);
    int after = score_captures_base(RULESET_KK_ADD_10, pile, n);
    int ok = (phase == PHASE_DECIDE);
    printf("%-16s before=%2d after=%2d  Koi-Koi screen: %s\n",
           name, before, after, ok ? "YES" : "NO  <-- PROBLEM");
    if (!ok) { failures++; return; }

    /* Choosing Koi-Koi must record the call and let play continue. */
    engine_decide(&g, DECISION_KOI_KOI);
    int cont = (g.koi_called[0] == 1 && g.phase == PHASE_TURN_HAND);
    printf("%-16s choosing Koi-Koi: %s\n", "", cont ? "call recorded, play continues" : "BROKEN");
    if (!cont) failures++;
}

static void check_no_trigger(const char *name, const int *pile, int n) {
    GameState g;
    GamePhase phase = run_turn(pile, n, &g);
    int ok = (phase != PHASE_DECIDE);
    printf("%-16s (control) Koi-Koi screen: %s\n", name, ok ? "no (correct)" : "YES  <-- PROBLEM");
    if (!ok) failures++;
}

int main(void) {
    static const int p_goko[]    = { 0, 8, 28, 40, 44 };
    static const int p_shiko[]   = { 0, 8, 28, 44 };
    static const int p_ame[]     = { 0, 8, 28, 40 };
    static const int p_sanko[]   = { 0, 8, 28 };
    static const int p_hanami[]  = { 8, 32 };
    static const int p_tsukimi[] = { 28, 32 };
    static const int p_ino[]     = { 20, 24, 36 };
    static const int p_akatan[]  = { 1, 5, 9 };
    static const int p_aotan[]   = { 21, 33, 37 };
    static const int p_both[]    = { 1, 5, 9, 21, 33, 37 };
    static const int p_tane[]    = { 4, 12, 16, 29, 41 };
    static const int p_tanzaku[] = { 1, 13, 17, 25, 42 };
    static const int p_kasu[]    = { 2, 3, 6, 7, 10, 11, 14, 15, 18, 19 };

    static const int p_rain_sanko[] = { 0, 8, 40 };  /* 3 Brights incl. Rain Man: no Sanko */
    static const int p_nothing[]    = { 2, 3 };      /* two Plains: no yaku */

    if (!cards_validate()) { printf("cards_validate() failed\n"); return 1; }

    printf("KK-ADD-10: does completing each wiki yaku open the Koi-Koi screen?\n\n");
    check_yaku("Goko", p_goko, 5);
    check_yaku("Shiko", p_shiko, 4);
    check_yaku("Ame-shiko", p_ame, 4);
    check_yaku("Sanko", p_sanko, 3);
    check_yaku("Hanami-zake", p_hanami, 2);
    check_yaku("Tsukimi-zake", p_tsukimi, 2);
    check_yaku("Ino-shika-cho", p_ino, 3);
    check_yaku("Akatan", p_akatan, 3);
    check_yaku("Aotan", p_aotan, 3);
    check_yaku("Akatan+Aotan", p_both, 6);
    check_yaku("Tane", p_tane, 5);
    check_yaku("Tanzaku", p_tanzaku, 5);
    check_yaku("Kasu", p_kasu, 10);
    check_no_trigger("Rain-Sanko", p_rain_sanko, 3);
    check_no_trigger("No yaku", p_nothing, 2);

    /* Growth: the combo was already banked as a Koi-Koi decision point,
       then it grows into its next stage and must open the screen again. */
    static const int g_sanko[]      = { 0, 8, 28 };
    static const int g_shiko[]      = { 0, 8, 28, 44 };
    static const int g_goko[]       = { 0, 8, 28, 40, 44 };
    static const int g_tane5[]      = { 4, 12, 16, 29, 41 };
    static const int g_tane6[]      = { 4, 12, 16, 29, 41, 20 };
    static const int g_tanzaku5[]   = { 1, 13, 17, 25, 42 };
    static const int g_tanzaku6[]   = { 1, 13, 17, 25, 42, 5 };
    static const int g_kasu11[]     = { 2, 3, 6, 7, 10, 11, 14, 15, 18, 19, 22 };
    static const int g_ino_tane[]   = { 20, 24, 36, 4, 29 };   /* Ino, then Tane appears on top */
    static const int g_sanko_idle[] = { 0, 8, 28, 2 };         /* Sanko + an unrelated Plain */

    printf("\n");
    check_growth("Sanko->Shiko", g_sanko, 3, g_shiko, 4);
    check_growth("Shiko->Goko", g_shiko, 4, g_goko, 5);
    check_growth("Tane 5->6", g_tane5, 5, g_tane6, 6);
    check_growth("Tanzaku 5->6", g_tanzaku5, 5, g_tanzaku6, 6);
    check_growth("Kasu 10->11", p_kasu, 10, g_kasu11, 11);
    check_growth("Ino->+Tane", p_ino, 3, g_ino_tane, 5);
    check_no_growth("Sanko+Plain", g_sanko, 3, g_sanko_idle, 4);

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
