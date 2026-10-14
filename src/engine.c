#include "engine.h"
#include <string.h>

uint32_t engine_rand(uint32_t *state) {
    if (*state == 0) *state = 0x12345678;
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

int engine_determine_first_dealer(uint32_t *rng_state) {
    while (1) {
        int c0 = engine_rand(rng_state) % 48;
        int c1 = engine_rand(rng_state) % 48;
        if (c0 == c1) continue;
        int m0 = CARDS[c0].month;
        int m1 = CARDS[c1].month;
        if (m0 < m1) return 0; /* Earlier month deals */
        if (m1 < m0) return 1;
        /* Same month: tie-break by rank */
        if (CARDS[c0].dealer_rank > CARDS[c1].dealer_rank) return 0;
        if (CARDS[c1].dealer_rank > CARDS[c0].dealer_rank) return 1;
        /* Identical month and rank: redraw */
    }
}

static int has_four_of_month_or_four_pairs(const int *cards, int n) {
    if (n != 8) return 0;
    int mc[13] = {0}, pairs = 0;
    for (int i = 0; i < 8; i++) mc[CARDS[cards[i]].month]++;
    for (int m = 1; m <= 12; m++) {
        if (mc[m] == 4) return 1;
        if (mc[m] == 2) pairs++;
    }
    return (pairs == 4);
}

int engine_validate_zones(const GameState *g) {
    uint64_t seen = 0;
    for (int p = 0; p < 2; p++) {
        for (int i = 0; i < g->hand_count[p]; i++) {
            int id = g->hand[p][i];
            if ((unsigned)id >= 48u || (seen & (1ULL << id))) return 0;
            seen |= (1ULL << id);
        }
        for (int i = 0; i < g->captured_count[p]; i++) {
            int id = g->captured[p][i];
            if ((unsigned)id >= 48u || (seen & (1ULL << id))) return 0;
            seen |= (1ULL << id);
        }
    }
    for (int i = 0; i < g->field_count; i++) {
        int id = g->field[i];
        if ((unsigned)id >= 48u || (seen & (1ULL << id))) return 0;
        seen |= (1ULL << id);
    }
    for (int i = 0; i < g->stock_count; i++) {
        int id = g->stock[i];
        if ((unsigned)id >= 48u || (seen & (1ULL << id))) return 0;
        seen |= (1ULL << id);
    }
    if (g->has_pending) {
        int id = g->pending_card;
        if ((unsigned)id >= 48u || (seen & (1ULL << id))) return 0;
        seen |= (1ULL << id);
    }
    return (seen == ((1ULL << 48) - 1ULL));
}

void engine_init(GameState *g, Ruleset ruleset, int total_rounds, uint32_t seed) {
    memset(g, 0, sizeof(*g));
    g->ruleset = ruleset;
    g->total_rounds = total_rounds;
    g->current_round = 1;
    g->rng_state = seed ? seed : 0x12345678;
    g->dealer = engine_determine_first_dealer(&g->rng_state);
    g->phase = PHASE_DEALER_SELECT;
    g->oya_player_choice = -1;
    g->oya_winner = -1;
    int c0 = engine_rand(&g->rng_state) % 48;
    int c1 = engine_rand(&g->rng_state) % 48;
    while (c1 == c0 || (CARDS[c0].month == CARDS[c1].month && CARDS[c0].dealer_rank == CARDS[c1].dealer_rank)) {
        c1 = engine_rand(&g->rng_state) % 48;
    }
    g->oya_cards[0] = c0;
    g->oya_cards[1] = c1;
}

void engine_start_round(GameState *g) {
    while (1) {
        g->hand_count[0] = 8;
        g->hand_count[1] = 8;
        g->field_count = 8;
        g->stock_count = 24;
        g->captured_count[0] = 0;
        g->captured_count[1] = 0;
        g->has_pending = 0;
        g->pending_card = -1;
        g->last_base[0] = 0;
        g->last_base[1] = 0;
        g->koi_called[0] = 0;
        g->koi_called[1] = 0;
        memset(&g->current_round_record, 0, sizeof(g->current_round_record));
        g->current_round_record.round_number = g->current_round;
        g->current_round_record.dealer = g->dealer;

        int deck[48];
        for (int i = 0; i < 48; i++) deck[i] = i;
        for (int i = 47; i > 0; i--) {
            int j = engine_rand(&g->rng_state) % (i + 1);
            int tmp = deck[i]; deck[i] = deck[j]; deck[j] = tmp;
        }

        memcpy(g->hand[0], &deck[0], 8 * sizeof(int));
        memcpy(g->hand[1], &deck[8], 8 * sizeof(int));
        memcpy(g->field, &deck[16], 8 * sizeof(int));
        memcpy(g->stock, &deck[24], 24 * sizeof(int));

        if (!has_four_of_month_or_four_pairs(g->field, 8)) break;
        /* Misdeal on field: redeal with same dealer */
    }

    g->phase = PHASE_LUCKY_CHECK;
    engine_check_lucky_hands(g);
}

/* YAKUZA (Oya-ken) = TOURNAMENT + dealer privilege: every draw (exhaustion or
   both-lucky) pays the dealer a flat 6 points. */
static int apply_dealer_privilege(GameState *g) {
    int d = g->dealer;
    g->total_score[d] += 6;
    g->current_round_record.winner = d;
    g->current_round_record.draw_reason = 0;
    g->current_round_record.lucky_hand = 0;
    g->current_round_record.base_points = 6;
    g->current_round_record.mult_seven = 1;
    g->current_round_record.mult_opp_koi = 1;  /* flat 6, no multipliers */
    g->current_round_record.mult_self_koi = 1;
    g->current_round_record.self_koi_calls = 0;
    g->current_round_record.opp_koi_calls = 0;
    g->current_round_record.award = 6;
    memset(&g->current_round_record.score_result, 0, sizeof(ScoreResult));
    g->dealer = d;                             /* the dealer keeps the deal */
    g->phase = PHASE_ROUND_END;
    return 1;
}

void engine_check_lucky_hands(GameState *g) {
    if (g->phase != PHASE_LUCKY_CHECK) return;
    int lucky0 = has_four_of_month_or_four_pairs(g->hand[0], 8);
    int lucky1 = has_four_of_month_or_four_pairs(g->hand[1], 8);

    if (lucky0 && lucky1) {
        if (g->ruleset == RULESET_YAKUZA) { apply_dealer_privilege(g); return; }
        g->current_round_record.winner = -1;
        g->current_round_record.draw_reason = 2; /* Both lucky */
        g->current_round_record.lucky_hand = 1;
        g->phase = PHASE_ROUND_END;
        return;
    }
    if (lucky0 || lucky1) {
        int w = lucky0 ? 0 : 1;
        g->current_round_record.winner = w;
        g->current_round_record.lucky_hand = 1;
        g->current_round_record.base_points = 6;
        g->current_round_record.mult_seven = 1;
        g->current_round_record.mult_opp_koi = 1;
        g->current_round_record.mult_self_koi = 1;
        g->current_round_record.self_koi_calls = 0;
        g->current_round_record.opp_koi_calls = 0;
        g->current_round_record.award = 6;
        g->total_score[w] += 6;
        g->dealer = w;
        g->phase = PHASE_ROUND_END;
        return;
    }

    g->current_player = g->dealer;
    g->phase = PHASE_TURN_HAND;
}

static void remove_from_zone(int *zone, int *count, int card_id) {
    int n = *count;
    for (int i = 0; i < n; i++) {
        if (zone[i] == card_id) {
            for (int j = i; j < n - 1; j++) zone[j] = zone[j + 1];
            *count = n - 1;
            return;
        }
    }
}

int engine_pick_hand_choice(GameState *g, int target_id) {
    if (g->phase != PHASE_HAND_CHOICE) return 0;
    if (target_id != g->choice_targets[0] && target_id != g->choice_targets[1]) return 0;

    int p = g->current_player;
    int card_id = g->played_hand_card;
    remove_from_zone(g->hand[p], &g->hand_count[p], card_id);
    remove_from_zone(g->field, &g->field_count, target_id);
    g->captured[p][g->captured_count[p]++] = card_id;
    g->captured[p][g->captured_count[p]++] = target_id;
    g->last_capture_source = card_id;
    g->last_captured[0] = card_id;
    g->last_captured[1] = target_id;
    g->last_captured_count = 2;
    g->hitlag_timer = 0.15f;
    g->played_hand_card = -1;
    g->choice_count = 0;
    g->phase = PHASE_TURN_DRAW;
    return 1;
}

int engine_play_hand(GameState *g, int card_id, int target_id) {
    if (g->phase == PHASE_HAND_CHOICE) {
        if (card_id == g->played_hand_card) {
            return engine_pick_hand_choice(g, target_id);
        }
        return 0;
    }
    if (g->phase != PHASE_TURN_HAND) return 0;
    int p = g->current_player;
    int in_hand = 0;
    for (int i = 0; i < g->hand_count[p]; i++) {
        if (g->hand[p][i] == card_id) { in_hand = 1; break; }
    }
    if (!in_hand) return 0;

    int matches[4];
    int match_count = 0;
    int card_month = CARDS[card_id].month;
    for (int i = 0; i < g->field_count; i++) {
        if (CARDS[g->field[i]].month == card_month) {
            matches[match_count++] = g->field[i];
        }
    }

    if (match_count == 0) {
        remove_from_zone(g->hand[p], &g->hand_count[p], card_id);
        g->field[g->field_count++] = card_id;
        g->last_captured_count = 0;
        g->phase = PHASE_TURN_DRAW;
        return 1;
    }
    if (match_count == 1) {
        remove_from_zone(g->hand[p], &g->hand_count[p], card_id);
        remove_from_zone(g->field, &g->field_count, matches[0]);
        g->captured[p][g->captured_count[p]++] = card_id;
        g->captured[p][g->captured_count[p]++] = matches[0];
        g->last_capture_source = card_id;
        g->last_captured[0] = card_id;
        g->last_captured[1] = matches[0];
        g->last_captured_count = 2;
        g->hitlag_timer = 0.15f;
        g->phase = PHASE_TURN_DRAW;
        return 1;
    }
    if (match_count == 2) {
        if (target_id == matches[0] || target_id == matches[1]) {
            remove_from_zone(g->hand[p], &g->hand_count[p], card_id);
            remove_from_zone(g->field, &g->field_count, target_id);
            g->captured[p][g->captured_count[p]++] = card_id;
            g->captured[p][g->captured_count[p]++] = target_id;
            g->last_capture_source = card_id;
            g->last_captured[0] = card_id;
            g->last_captured[1] = target_id;
            g->last_captured_count = 2;
            g->hitlag_timer = 0.15f;
            g->phase = PHASE_TURN_DRAW;
            return 1;
        }
        g->played_hand_card = card_id;
        g->choice_targets[0] = matches[0];
        g->choice_targets[1] = matches[1];
        g->choice_count = 2;
        g->phase = PHASE_HAND_CHOICE;
        return 0;
    }
    if (match_count == 3) {
        remove_from_zone(g->hand[p], &g->hand_count[p], card_id);
        remove_from_zone(g->field, &g->field_count, matches[0]);
        remove_from_zone(g->field, &g->field_count, matches[1]);
        remove_from_zone(g->field, &g->field_count, matches[2]);
        g->captured[p][g->captured_count[p]++] = card_id;
        g->captured[p][g->captured_count[p]++] = matches[0];
        g->captured[p][g->captured_count[p]++] = matches[1];
        g->captured[p][g->captured_count[p]++] = matches[2];
        g->last_capture_source = card_id;
        g->last_captured[0] = card_id;
        g->last_captured[1] = matches[0];
        g->last_captured[2] = matches[1];
        g->last_captured[3] = matches[2];
        g->last_captured_count = 4;
        g->hitlag_timer = 0.15f;
        g->phase = PHASE_TURN_DRAW;
        return 1;
    }
    return 0;
}

int engine_cancel_hand_choice(GameState *g) {
    if (g->phase != PHASE_HAND_CHOICE) return 0;
    g->played_hand_card = -1;
    g->choice_count = 0;
    g->phase = PHASE_TURN_HAND;
    return 1;
}

int engine_timeout_autoplay(GameState *g) {
    if (g->current_player != 0 || g->hand_count[0] <= 0) return 0;
    if (g->phase == PHASE_HAND_CHOICE) {
        return engine_pick_hand_choice(g, g->choice_targets[0]);
    }
    if (g->phase != PHASE_TURN_HAND) return 0;
    int r_idx = (int)(engine_rand(&g->rng_state) % (uint32_t)g->hand_count[0]);
    int card_id = g->hand[0][r_idx], target_id = -1;
    for (int i = 0; i < g->field_count; i++) {
        if (CARDS[g->field[i]].month == CARDS[card_id].month) { target_id = g->field[i]; break; }
    }
    return engine_play_hand(g, card_id, target_id);
}

static int apply_exhaustion(GameState *g) {
    if (g->ruleset == RULESET_YAKUZA) return apply_dealer_privilege(g);
    g->current_round_record.winner = -1;
    g->current_round_record.draw_reason = 1; /* Exhaustion */
    g->phase = PHASE_ROUND_END;
    return 1;
}

static int advance_after_turn(GameState *g) {
    if (g->hand_count[0] == 0 && g->hand_count[1] == 0) return apply_exhaustion(g);
    int next_p = 1 - g->current_player;
    if (g->hand_count[next_p] == 0) next_p = g->current_player;
    if (g->hand_count[next_p] == 0) return apply_exhaustion(g);
    g->current_player = next_p;
    g->phase = PHASE_TURN_HAND;
    return 1;
}

static int apply_shobu(GameState *g) {
    int p = g->current_player;
    int opp = 1 - p;
    int self_koi = g->koi_called[p];
    int opp_koi = g->koi_called[opp];
    ScoreResult sr = score_captures(g->ruleset, g->captured[p], g->captured_count[p]);
    int award = calculate_award(sr.base_points, self_koi, opp_koi);
    g->total_score[p] += award;
    g->current_round_record.winner = p;
    g->current_round_record.base_points = sr.base_points;
    g->current_round_record.mult_seven = (sr.base_points >= 7) ? 2 : 1;
    g->current_round_record.mult_opp_koi = (opp_koi > 0) ? 2 : 1;
    g->current_round_record.mult_self_koi = koi_caller_multiplier(self_koi);
    g->current_round_record.self_koi_calls = self_koi;
    g->current_round_record.opp_koi_calls = opp_koi;
    g->current_round_record.award = award;
    g->current_round_record.score_result = sr;
    if (p == 0) {
        for (int i = 0; i < sr.yaku_count; i++) {
            if (sr.yakus[i].id >= 0 && sr.yakus[i].id < YAKU_COUNT) g->human_yaku_counts[sr.yakus[i].id]++;
        }
    }
    g->dealer = p;
    g->phase = PHASE_ROUND_END;
    return 1;
}

static int finish_stock_turn(GameState *g) {
    int p = g->current_player;
    int base = score_captures_base(g->ruleset, g->captured[p], g->captured_count[p]);
    if (base > g->last_base[p]) {
        if (g->hand_count[p] == 0) {
            return apply_shobu(g);
        }
        if ((g->ruleset == RULESET_NINTENDO || g->ruleset == RULESET_TOURNAMENT || g->ruleset == RULESET_YAKUZA) && g->koi_called[p] >= 1) {
            return apply_shobu(g);
        }
        g->phase = PHASE_DECIDE;
        return 1;
    }
    return advance_after_turn(g);
}

int engine_reveal_stock(GameState *g) {
    if (g->phase != PHASE_TURN_DRAW) return 0;
    if (g->stock_count <= 0) return apply_exhaustion(g);
    int stock_card = g->stock[--g->stock_count];
    g->pending_card = stock_card;
    g->has_pending = 1;

    int matches[4];
    int match_count = 0;
    int stock_month = CARDS[stock_card].month;
    for (int i = 0; i < g->field_count; i++) {
        if (CARDS[g->field[i]].month == stock_month) {
            matches[match_count++] = g->field[i];
        }
    }

    if (match_count == 0) {
        g->field[g->field_count++] = stock_card;
        g->has_pending = 0;
        g->last_captured_count = 0;
        return finish_stock_turn(g);
    }
    if (match_count == 1) {
        int p = g->current_player;
        remove_from_zone(g->field, &g->field_count, matches[0]);
        g->captured[p][g->captured_count[p]++] = stock_card;
        g->captured[p][g->captured_count[p]++] = matches[0];
        g->has_pending = 0;
        g->last_capture_source = stock_card;
        g->last_captured[0] = stock_card;
        g->last_captured[1] = matches[0];
        g->last_captured_count = 2;
        g->hitlag_timer = 0.15f;
        return finish_stock_turn(g);
    }
    if (match_count == 2) {
        g->choice_targets[0] = matches[0];
        g->choice_targets[1] = matches[1];
        g->choice_count = 2;
        g->phase = PHASE_DRAW_CHOICE;
        return 0;
    }
    if (match_count == 3) {
        int p = g->current_player;
        remove_from_zone(g->field, &g->field_count, matches[0]);
        remove_from_zone(g->field, &g->field_count, matches[1]);
        remove_from_zone(g->field, &g->field_count, matches[2]);
        g->captured[p][g->captured_count[p]++] = stock_card;
        g->captured[p][g->captured_count[p]++] = matches[0];
        g->captured[p][g->captured_count[p]++] = matches[1];
        g->captured[p][g->captured_count[p]++] = matches[2];
        g->has_pending = 0;
        g->last_capture_source = stock_card;
        g->last_captured[0] = stock_card;
        g->last_captured[1] = matches[0];
        g->last_captured[2] = matches[1];
        g->last_captured[3] = matches[2];
        g->last_captured_count = 4;
        g->hitlag_timer = 0.15f;
        return finish_stock_turn(g);
    }
    return 0;
}

int engine_pick_draw_choice(GameState *g, int target_id) {
    if (g->phase != PHASE_DRAW_CHOICE) return 0;
    if (target_id != g->choice_targets[0] && target_id != g->choice_targets[1]) return 0;
    int p = g->current_player;
    int stock_card = g->pending_card;
    remove_from_zone(g->field, &g->field_count, target_id);
    g->captured[p][g->captured_count[p]++] = stock_card;
    g->captured[p][g->captured_count[p]++] = target_id;
    g->has_pending = 0;
    g->last_capture_source = stock_card;
    g->last_captured[0] = stock_card;
    g->last_captured[1] = target_id;
    g->last_captured_count = 2;
    g->hitlag_timer = 0.15f;
    return finish_stock_turn(g);
}

int engine_decide(GameState *g, DecisionType decision) {
    if (g->phase != PHASE_DECIDE) return 0;
    if (decision == DECISION_SHOBU) {
        return apply_shobu(g);
    } else {
        int p = g->current_player;
        g->last_base[p] = score_captures_base(g->ruleset, g->captured[p], g->captured_count[p]);
        g->koi_called[p]++;
        g->match_koi_calls[p]++;
        return advance_after_turn(g);
    }
}

void engine_next_round(GameState *g) {
    if (g->phase != PHASE_ROUND_END) return;
    if (g->ledger_count < 12) {
        g->ledger[g->ledger_count++] = g->current_round_record;
    }
    if (g->current_round >= g->total_rounds) {
        g->phase = PHASE_MATCH_END;
    } else {
        g->current_round++;
        engine_start_round(g);
    }
}
