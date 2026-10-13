/* ============================================================
   XENO/KOI — Engine Integrity Harness
   Runs 15,000 headless AI-vs-AI simulation matches across all
   rulesets and difficulties, validating:
     * Card conservation (all 48 cards in exactly one zone)
     * Zone/counter bounds and phase legality at every step
     * Score ledger bookkeeping and award math
     * Dealer progression rules
     * Deterministic replay (zero desyncs: same seed => same trace)
   Exit code 0 = PASS, 1 = FAIL.
   ============================================================ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include "engine.h"
#include "ai.h"

#define TOTAL_MATCHES   15000
#define MAX_STEPS       100000

static long g_failures = 0;
static long g_zones_checked = 0;
static long g_steps_total = 0;
static long g_koi_calls = 0;
static long g_draws = 0;
static long g_decisive = 0;

static uint32_t mix32(uint32_t h, uint32_t v) {
    h ^= v;
    h *= 16777619u;
    return h;
}

typedef struct {
    uint32_t trace;          /* FNV hash over every phase transition */
    int      total_score[2];
    int      ledger_count;
    int      match_winner;
    uint32_t final_rng;
    long     steps;
} MatchReport;

static void fail(int match, long step, const char *fmt, ...) {
    g_failures++;
    if (g_failures <= 20) {
        va_list ap;
        fprintf(stderr, "[FAIL] match %d step %ld: ", match, step);
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
        fprintf(stderr, "\n");
    }
}

/* Full-coverage zone + bookkeeping validation. Returns 1 if OK. */
static int check_invariants(const GameState *g, int match, long step) {
    g_zones_checked++;
    if (!engine_validate_zones(g)) {
        fail(match, step, "card conservation violated (zones)");
        return 0;
    }
    int ok = 1;
    if (g->hand_count[0] < 0 || g->hand_count[0] > 8)  { fail(match, step, "hand_count[0]=%d out of range", g->hand_count[0]); ok = 0; }
    if (g->hand_count[1] < 0 || g->hand_count[1] > 8)  { fail(match, step, "hand_count[1]=%d out of range", g->hand_count[1]); ok = 0; }
    if (g->field_count < 0 || g->field_count > 48)     { fail(match, step, "field_count=%d out of range", g->field_count); ok = 0; }
    if (g->stock_count < 0 || g->stock_count > 24)     { fail(match, step, "stock_count=%d out of range", g->stock_count); ok = 0; }
    if (g->captured_count[0] < 0 || g->captured_count[0] > 48) { fail(match, step, "captured[0]=%d", g->captured_count[0]); ok = 0; }
    if (g->captured_count[1] < 0 || g->captured_count[1] > 48) { fail(match, step, "captured[1]=%d", g->captured_count[1]); ok = 0; }
    if (g->phase <= PHASE_NONE || g->phase > PHASE_MATCH_END) { fail(match, step, "illegal phase %d", (int)g->phase); ok = 0; }
    if (g->current_player < 0 || g->current_player > 1) { fail(match, step, "current_player=%d", g->current_player); ok = 0; }
    if (g->dealer < 0 || g->dealer > 1)                 { fail(match, step, "dealer=%d", g->dealer); ok = 0; }
    if (g->current_round < 1 || g->current_round > g->total_rounds) { fail(match, step, "current_round=%d/%d", g->current_round, g->total_rounds); ok = 0; }
    if (g->total_score[0] < 0 || g->total_score[1] < 0) { fail(match, step, "negative total score"); ok = 0; }
    if (g->koi_called[0] < 0 || g->koi_called[1] < 0)   { fail(match, step, "negative koi counter"); ok = 0; }
    if (g->ledger_count < 0 || g->ledger_count > 12)    { fail(match, step, "ledger_count=%d", g->ledger_count); ok = 0; }
    if (g->phase == PHASE_TURN_HAND && g->hand_count[g->current_player] <= 0) {
        fail(match, step, "turn with empty hand"); ok = 0;
    }
    if ((g->phase == PHASE_HAND_CHOICE || g->phase == PHASE_DRAW_CHOICE) && g->choice_count != 2) {
        fail(match, step, "choice phase without 2 targets"); ok = 0;
    }
    if (g->phase == PHASE_DRAW_CHOICE && !g->has_pending) { fail(match, step, "draw choice without pending card"); ok = 0; }
    return ok;
}

/* Validate a finished round's record against recomputed rules math.
   expected_scores[] must ALREADY include this round's award, since the
   engine applies points before entering PHASE_ROUND_END. */
static void check_round_record(const GameState *g, int match, long step,
                               const int expected_scores[2], int start_dealer) {
    const RoundRecord *r = &g->current_round_record;
    if (r->winner < -1 || r->winner > 1) { fail(match, step, "winner=%d", r->winner); return; }
    if (r->round_number != g->current_round)
        { fail(match, step, "record round mismatch"); }
    if (r->dealer != start_dealer)
        { fail(match, step, "record dealer %d != round-start dealer %d", r->dealer, start_dealer); }
    if (r->lucky_hand) {
        if (r->winner < 0) {
            if (r->draw_reason != 2) fail(match, step, "lucky draw reason=%d (want 2)", r->draw_reason);
            if (r->award != 0) fail(match, step, "both-lucky draw awarded %d", r->award);
        } else if (r->base_points != 6 || r->award != 6) {
            fail(match, step, "lucky hand award=%d base=%d (want 6/6)", r->award, r->base_points);
        }
    } else if (r->winner < 0) {
        if (r->draw_reason == 1 && (g->hand_count[0] != 0 || g->hand_count[1] != 0))
            fail(match, step, "exhaustion draw with cards in hand");
        if (r->award != 0) fail(match, step, "draw awarded %d points", r->award);
    } else if (r->score_result.yaku_count == 0 && r->base_points == 6) {
        /* Yakuza oya-ken exhaustion win: start dealer takes 6 (captures ignored) */
        if (g->ruleset != RULESET_YAKUZA) fail(match, step, "unearned 6-pt win outside yakuza rules");
        if (r->winner != start_dealer) fail(match, step, "oya-ken winner is not start dealer");
        if (g->hand_count[0] != 0 || g->hand_count[1] != 0) fail(match, step, "oya-ken without exhaustion");
        if (g->koi_called[0] != 0 || g->koi_called[1] != 0) fail(match, step, "oya-ken after koi call");
        if (r->award != calculate_award(6, r->mult_koi == 2)) fail(match, step, "oya-ken award math");
    } else {
        ScoreResult sr = score_captures(g->ruleset, g->captured[r->winner],
                                        g->captured_count[r->winner]);
        if (sr.base_points != r->base_points)
            fail(match, step, "recomputed base %d != record %d", sr.base_points, r->base_points);
        if (r->mult_seven != ((r->base_points >= 7) ? 2 : 1))
            fail(match, step, "mult_seven=%d for base %d", r->mult_seven, r->base_points);
        int award = calculate_award(r->base_points, r->mult_koi == 2);
        if (r->award != award)
            fail(match, step, "record award %d != computed %d", r->award, award);
    }
    for (int p = 0; p < 2; p++)
        if (g->total_score[p] != expected_scores[p])
            fail(match, step, "ledger drift p%d: expected %d got %d",
                 p, expected_scores[p], g->total_score[p]);
}

static int run_match(int match_id, MatchReport *rep) {
    uint32_t seed = (uint32_t)match_id * 2654435761u + 123457u;
    if (seed == 0) seed = 0x12345678;
    Ruleset rs       = (Ruleset)(match_id % 5);
    Difficulty d0    = (Difficulty)((match_id / 5) % 3);
    Difficulty d1    = (Difficulty)((match_id / 15) % 3);
    int total_rounds = (match_id % 23 == 0) ? 1 + (match_id % 12) : 6;

    GameState g;
    uint32_t ai_rng = seed + 999;
    ai_set_opponent(match_id % 12);
    engine_init(&g, rs, total_rounds, seed);
    engine_start_round(&g);

    memset(rep, 0, sizeof(*rep));
    rep->trace = 2166136261u;
    int expected_scores[2] = { 0, 0 };
    /* record.dealer is the dealt-in dealer, captured before any lucky reassignment */
    int start_dealer = g.current_round_record.dealer;
    long step = 0;

    while (g.phase != PHASE_MATCH_END) {
        if (++step > MAX_STEPS) { fail(match_id, step, "step limit exceeded (state machine hang)"); return 0; }
        if (!check_invariants(&g, match_id, step)) return 0;

        switch (g.phase) {
        case PHASE_TURN_HAND: {
            int p = g.current_player;
            Difficulty d = (p == 0) ? d0 : d1;
            if (d == DIFFICULTY_MASTER && p == 0 && (ai_rand(&ai_rng) % 97) == 0) {
                if (!engine_timeout_autoplay(&g)) fail(match_id, step, "timeout autoplay rejected");
            } else {
                int card = -1, target = -1;
                ai_choose_hand(&g, d, &ai_rng, &card, &target);
                int r = engine_play_hand(&g, card, target);
                if (!r && g.phase != PHASE_HAND_CHOICE)
                    fail(match_id, step, "play_hand rejected card=%d target=%d", card, target);
            }
            break;
        }
        case PHASE_HAND_CHOICE: {
            int t = g.choice_targets[ai_rand(&ai_rng) % 2];
            if (!engine_pick_hand_choice(&g, t))
                fail(match_id, step, "hand choice rejected target=%d", t);
            break;
        }
        case PHASE_TURN_DRAW:
            engine_reveal_stock(&g);
            break;
        case PHASE_DRAW_CHOICE: {
            Difficulty d = (g.current_player == 0) ? d0 : d1;
            int t = ai_choose_stock_target(&g, d, &ai_rng);
            if (!engine_pick_draw_choice(&g, t)) {
                t = g.choice_targets[0];
                if (!engine_pick_draw_choice(&g, t))
                    fail(match_id, step, "draw choice rejected fallback target");
            }
            break;
        }
        case PHASE_DECIDE: {
            Difficulty d = (g.current_player == 0) ? d0 : d1;
            DecisionType dec = ai_choose_decision(&g, d, &ai_rng);
            if (!engine_decide(&g, dec)) fail(match_id, step, "decide rejected");
            if (dec == DECISION_KOI_KOI && g.phase != PHASE_ROUND_END) g_koi_calls++;
            break;
        }
        case PHASE_ROUND_END: {
            const RoundRecord *r = &g.current_round_record;
            int winner = r->winner, award = r->award;
            if (winner >= 0) {
                expected_scores[winner] += award;   /* engine applies points pre-ROUND_END */
                g_decisive++;
            } else g_draws++;
            check_round_record(&g, match_id, step, expected_scores, start_dealer);
            int last = (g.current_round >= g.total_rounds);
            engine_next_round(&g);
            if (!last) {
                int expect_dealer = (winner >= 0) ? winner : start_dealer;
                /* record.dealer captures the dealt-in dealer BEFORE any lucky-hand
                   reassignment inside engine_start_round, so it is the rotation truth. */
                if (g.current_round_record.dealer != expect_dealer)
                    fail(match_id, step, "dealer rotation: record %d want %d",
                         g.current_round_record.dealer, expect_dealer);
                start_dealer = g.current_round_record.dealer;
            }
            break;
        }
        default:
            fail(match_id, step, "unexpected phase %d in match loop", (int)g.phase);
            return 0;
        }

        /* Trace every transition for desync detection */
        rep->trace = mix32(rep->trace, (uint32_t)g.phase);
        rep->trace = mix32(rep->trace, (uint32_t)g.current_player);
        rep->trace = mix32(rep->trace, (uint32_t)g.hand_count[0] * 64u + (uint32_t)g.hand_count[1]);
        rep->trace = mix32(rep->trace, (uint32_t)g.field_count * 64u + (uint32_t)g.stock_count);
        rep->trace = mix32(rep->trace, (uint32_t)g.captured_count[0] * 64u + (uint32_t)g.captured_count[1]);
        rep->trace = mix32(rep->trace, (uint32_t)g.total_score[0] * 4096u + (uint32_t)g.total_score[1]);
        rep->trace = mix32(rep->trace, (uint32_t)g.current_round * 4u + (uint32_t)g.dealer);
    }

    if (!check_invariants(&g, match_id, step)) return 0;
    if (g.ledger_count != g.total_rounds)
        { fail(match_id, step, "ledger_count=%d != rounds=%d", g.ledger_count, g.total_rounds); return 0; }
    if (g.total_score[0] != expected_scores[0] || g.total_score[1] != expected_scores[1])
        { fail(match_id, step, "final score drift"); return 0; }
    for (int i = 0; i < g.ledger_count; i++) {
        if (g.ledger[i].round_number != i + 1)
            { fail(match_id, step, "ledger entry %d round_number=%d", i, g.ledger[i].round_number); return 0; }
    }

    rep->total_score[0] = g.total_score[0];
    rep->total_score[1] = g.total_score[1];
    rep->ledger_count   = g.ledger_count;
    rep->final_rng      = g.rng_state;
    rep->steps          = step;
    rep->match_winner   = (g.total_score[0] > g.total_score[1]) ? 0 :
                          (g.total_score[1] > g.total_score[0]) ? 1 : -1;
    g_steps_total += step;
    return 1;
}

int main(void) {
    printf("XENO/KOI engine integrity harness\n");
    printf("=================================\n");
    if (!cards_validate()) {
        fprintf(stderr, "[FATAL] static CARDS table failed validation\n");
        return 1;
    }
    printf("cards_validate(): 48-card table OK\n");
    printf("Running %d simulation matches (each replayed for desync check)...\n", TOTAL_MATCHES);

    long desyncs = 0;
    for (int m = 0; m < TOTAL_MATCHES; m++) {
        MatchReport a, b;
        int ok1 = run_match(m, &a);
        int ok2 = run_match(m, &b);
        if (ok1 && ok2 && memcmp(&a, &b, sizeof(a)) != 0) {
            desyncs++;
            g_failures++;
            if (desyncs <= 5)
                fprintf(stderr, "[DESYNC] match %d: trace %08X vs %08X, scores %d-%d vs %d-%d\n",
                        m, a.trace, b.trace, a.total_score[0], a.total_score[1],
                        b.total_score[0], b.total_score[1]);
        }
        if ((m + 1) % 3000 == 0)
            printf("  %6d / %d matches validated (%ld failures, %ld desyncs)\n",
                   m + 1, TOTAL_MATCHES, g_failures, desyncs);
    }

    printf("---------------------------------\n");
    printf("Matches:        %d (rulesets 0-4, difficulties EASY/HARD/MASTER)\n", TOTAL_MATCHES);
    printf("Engine steps:   %ld\n", g_steps_total);
    printf("Zone checks:    %ld (48-card conservation verified each step)\n", g_zones_checked);
    printf("Rounds won:     %ld   Rounds drawn: %ld   Koi-koi calls: %ld\n",
           g_decisive, g_draws, g_koi_calls);
    printf("Desyncs:        %ld\n", desyncs);
    printf("Failures:       %ld\n", g_failures);
    if (g_failures == 0) {
        printf("RESULT: PASS — %d/%d matches, zero desyncs, full card conservation.\n",
               TOTAL_MATCHES, TOTAL_MATCHES);
        return 0;
    }
    printf("RESULT: FAIL\n");
    return 1;
}
