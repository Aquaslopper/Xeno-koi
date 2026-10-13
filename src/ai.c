#include "ai.h"
#include "campaign.h"
#include <stdbool.h>
#include <string.h>

static int s_current_opponent = 0;

void ai_set_opponent(int opp_id) {
    if (opp_id >= 0 && opp_id <= 12) s_current_opponent = opp_id;
}

int ai_get_opponent(void) {
    return s_current_opponent;
}

uint32_t ai_rand(uint32_t *state) {
    if (*state == 0) *state = 0x87654321;
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static uint64_t build_capture_mask(const GameState *g, int p) {
    uint64_t mask = 0;
    int n = g->captured_count[p];
    for (int i = 0; i < n; i++) {
        int id = g->captured[p][i];
        if ((unsigned)id < 48u) mask |= (1ULL << id);
    }
    return mask;
}

#define MASK_BIT(m, id) ((int)(((m) >> (id)) & 1ULL))

static int blocks_opponent_mask(uint64_t opp_mask, int card_id) {
    if ((unsigned)card_id >= 48u) return 0;
    if (MASK_BIT(opp_mask, card_id)) return 0;

    /* Hanami: 03-B Curtain (8) + 09-A Cup (32) */
    if ((card_id == 32 && MASK_BIT(opp_mask, 8)) || (card_id == 8 && MASK_BIT(opp_mask, 32))) return 1;

    /* Tsukimi: 08-B Moon (28) + 09-A Cup (32) */
    if ((card_id == 32 && MASK_BIT(opp_mask, 28)) || (card_id == 28 && MASK_BIT(opp_mask, 32))) return 1;

    /* Ino-shika-cho: 20, 24, 36 */
    if ((card_id == 20 || card_id == 24 || card_id == 36) &&
        (MASK_BIT(opp_mask, 20) + MASK_BIT(opp_mask, 24) + MASK_BIT(opp_mask, 36) == 2)) return 1;

    /* Akatan: 1, 5, 9 */
    if ((card_id == 1 || card_id == 5 || card_id == 9) &&
        (MASK_BIT(opp_mask, 1) + MASK_BIT(opp_mask, 5) + MASK_BIT(opp_mask, 9) == 2)) return 1;

    /* Aotan: 21, 33, 37 */
    if ((card_id == 21 || card_id == 33 || card_id == 37) &&
        (MASK_BIT(opp_mask, 21) + MASK_BIT(opp_mask, 33) + MASK_BIT(opp_mask, 37) == 2)) return 1;

    /* Brights: 1 card away from Sanko (2 non-rain), Shiko/Ame-Shiko (3 total), or Goko (4 total) */
    if (CARDS[card_id].type == CARD_TYPE_BRIGHT) {
        int non_rain = MASK_BIT(opp_mask, 0) + MASK_BIT(opp_mask, 8) + MASK_BIT(opp_mask, 28) + MASK_BIT(opp_mask, 44);
        int total_b = non_rain + MASK_BIT(opp_mask, 40);
        if (card_id != 40 && non_rain == 2) return 1;
        if (total_b >= 3) return 1;
    }
    return 0;
}

static int bundle_raises_base_fast(Ruleset ruleset, CaptureSummary base_cs, int old_base, const int *new_cards, int new_count) {
    CaptureSummary cs = base_cs;
    for (int i = 0; i < new_count; i++) {
        int id = new_cards[i];
        if ((unsigned)id < 48u) {
            cs.mask |= (1ULL << id);
            switch (CARDS[id].type) {
                case CARD_TYPE_BRIGHT: cs.bright_count++; break;
                case CARD_TYPE_ANIMAL: cs.animal_count++; break;
                case CARD_TYPE_RIBBON: cs.ribbon_count++; break;
                case CARD_TYPE_PLAIN:  cs.plain_count++;  break;
            }
        }
    }
    return score_summary_base(ruleset, cs) > old_base;
}

static int eval_card_value_fast(int card_id, const int *new_cards, int new_count, int me,
                                Difficulty diff, int raises_base, uint64_t opp_mask, const Opponent *opp_info) {
    int val = CARDS[card_id].dealer_rank;
    if (raises_base) val += 15;

    if (diff == DIFFICULTY_DEUS) {
        if (blocks_opponent_mask(opp_mask, card_id)) val += 100;
        for (int i = 0; i < new_count; i++) {
            if (new_cards[i] != card_id && blocks_opponent_mask(opp_mask, new_cards[i])) {
                val += 100;
                break;
            }
        }
    } else if (diff == DIFFICULTY_MASTER) {
        if (blocks_opponent_mask(opp_mask, card_id)) val += 50;
        for (int i = 0; i < new_count; i++) {
            if (new_cards[i] != card_id && blocks_opponent_mask(opp_mask, new_cards[i])) {
                val += 50;
                break;
            }
        }
    } else {
        if (blocks_opponent_mask(opp_mask, card_id)) val += 6;
    }

    if (me == 1 && opp_info) {
        int fav = opp_info->favorite_card_id;
        if (card_id == fav) val += 35;
        for (int i = 0; i < new_count; i++) {
            if (new_cards[i] == fav) { val += 35; break; }
        }
        CardType ctype = CARDS[card_id].type;
        int fy = opp_info->favorite_yaku;
        if (fy == YAKU_GOKO || fy == YAKU_SANKO || fy == YAKU_SHIKO || fy == YAKU_AME_SHIKO) {
            if (ctype == CARD_TYPE_BRIGHT) val += 24;
        } else if (fy == YAKU_TANZAKU || fy == YAKU_AKATAN || fy == YAKU_AOTAN) {
            if (ctype == CARD_TYPE_RIBBON) val += 22;
        } else if (fy == YAKU_TANE) {
            if (ctype == CARD_TYPE_ANIMAL) val += 22;
        } else if (fy == YAKU_INO_SHIKA_CHO) {
            if (card_id == 20 || card_id == 24 || card_id == 36) val += 32;
            else if (ctype == CARD_TYPE_ANIMAL) val += 14;
        } else if (fy == YAKU_KASU) {
            if (ctype == CARD_TYPE_PLAIN) val += 20;
        } else if (fy == YAKU_HANAMI_ZAKE || fy == YAKU_TSUKIMI_ZAKE) {
            if (card_id == 32 || card_id == 8 || card_id == 28) val += 24;
        }
    }

    return val;
}

static int ai_get_accuracy(Difficulty diff, int boss_id) {
    if (diff == DIFFICULTY_DEUS) return 100;
    if (boss_id < 0) boss_id = 0;
    if (boss_id > 11) boss_id = 11;

    if (diff == DIFFICULTY_EASY) {
        return 20 + (boss_id * 40) / 11;
    } else if (diff == DIFFICULTY_HARD) {
        return 50 + (boss_id * 30) / 11;
    } else {
        return 78 + boss_id * 2;
    }
}

void ai_choose_hand(const GameState *g, Difficulty diff, uint32_t *ai_rng, int *out_card, int *out_target) {
    int me = g->current_player;
    int n = g->hand_count[me];
    if (n <= 0) {
        *out_card = -1;
        *out_target = -1;
        return;
    }

    int target_acc = ai_get_accuracy(diff, s_current_opponent);
    int roll = (int)(ai_rand(ai_rng) % 100);
    bool play_optimally = (roll < target_acc);

    /* If suboptimal roll: simulate a casual human blunder / inattentive play */
    if (!play_optimally) {
        int idx = (int)(ai_rand(ai_rng) % n);
        int card = g->hand[me][idx];
        int matches[4], mc = 0;
        for (int i = 0; i < g->field_count; i++) {
            if (CARDS[g->field[i]].month == CARDS[card].month) matches[mc++] = g->field[i];
        }
        *out_card = card;
        if (mc == 2) *out_target = (ai_rand(ai_rng) % 2) ? matches[1] : matches[0];
        else if (mc == 1) *out_target = matches[0];
        else *out_target = -1;
        return;
    }

    /* 1. Campaign signature card & preferred yaku priority */
    const Opponent *opp_info = campaign_get_opponent(s_current_opponent);
    if (opp_info) {
        int fav_id = opp_info->favorite_card_id;
        for (int i = 0; i < n; i++) {
            int card = g->hand[me][i];
            int cm = CARDS[card].month;
            for (int j = 0; j < g->field_count; j++) {
                int fc = g->field[j];
                if (CARDS[fc].month == cm && (card == fav_id || fc == fav_id)) {
                    *out_card = card;
                    *out_target = fc;
                    return;
                }
            }
        }
        int fy = opp_info->favorite_yaku;
        for (int i = 0; i < n; i++) {
            int card = g->hand[me][i];
            int cm = CARDS[card].month;
            CardType ct = CARDS[card].type;
            for (int j = 0; j < g->field_count; j++) {
                int fc = g->field[j];
                if (CARDS[fc].month == cm) {
                    CardType ft = CARDS[fc].type;
                    int m = 0;
                    if (fy == YAKU_GOKO && (ct == CARD_TYPE_BRIGHT || ft == CARD_TYPE_BRIGHT)) m = 1;
                    else if (fy == YAKU_TANZAKU && (ct == CARD_TYPE_RIBBON || ft == CARD_TYPE_RIBBON)) m = 1;
                    else if (fy == YAKU_TANE && (ct == CARD_TYPE_ANIMAL || ft == CARD_TYPE_ANIMAL)) m = 1;
                    else if (fy == YAKU_INO_SHIKA_CHO && (card == 20 || card == 24 || card == 36 || fc == 20 || fc == 24 || fc == 36)) m = 1;
                    else if (fy == YAKU_KASU && (ct == CARD_TYPE_PLAIN && ft == CARD_TYPE_PLAIN)) m = 1;
                    if (m) { *out_card = card; *out_target = fc; return; }
                }
            }
        }
    }

    /* Precompute public month visibility and capture summary once per turn */
    int known_in_month[13] = {0};
    for (int i = 0; i < n; i++) known_in_month[CARDS[g->hand[me][i]].month]++;
    for (int i = 0; i < g->field_count; i++) known_in_month[CARDS[g->field[i]].month]++;
    for (int p = 0; p < 2; p++) {
        for (int i = 0; i < g->captured_count[p]; i++) known_in_month[CARDS[g->captured[p][i]].month]++;
    }

    CaptureSummary me_cs = rules_summarize_captures(g->captured[me], g->captured_count[me]);
    int old_base = score_summary_base(g->ruleset, me_cs);
    uint64_t opp_mask = build_capture_mask(g, 1 - me);

    /* 2. Full heuristic evaluation (capture value, combo building, and blocking) */
    int best_score = -999999;
    int best_card = g->hand[me][0];
    int best_target = -1;

    for (int i = 0; i < n; i++) {
        int card = g->hand[me][i];
        int cm = CARDS[card].month;
        int matches[4], mc = 0;
        for (int j = 0; j < g->field_count; j++) {
            if (CARDS[g->field[j]].month == cm) matches[mc++] = g->field[j];
        }

        if (mc == 0) {
            int raises = bundle_raises_base_fast(g->ruleset, me_cs, old_base, &card, 1);
            int val = eval_card_value_fast(card, &card, 1, me, diff, raises, opp_mask, opp_info);
            int score = -val;
            int has_unseen = (known_in_month[cm] < 4);
            if (has_unseen) score -= 4;
            if (diff == DIFFICULTY_MASTER && blocks_opponent_mask(opp_mask, card)) score -= 100;
            if (diff == DIFFICULTY_DEUS) {
                if (blocks_opponent_mask(opp_mask, card)) score -= 150;
                if (has_unseen) score -= 8;
            }
            if (score > best_score) { best_score = score; best_card = card; best_target = -1; }
        } else if (mc == 1) {
            int bundle[2] = { card, matches[0] };
            int raises = bundle_raises_base_fast(g->ruleset, me_cs, old_base, bundle, 2);
            int score = eval_card_value_fast(card, bundle, 2, me, diff, raises, opp_mask, opp_info) +
                        eval_card_value_fast(matches[0], bundle, 2, me, diff, raises, opp_mask, opp_info);
            if (score > best_score) { best_score = score; best_card = card; best_target = matches[0]; }
        } else if (mc == 2) {
            int b0[2] = { card, matches[0] };
            int r0 = bundle_raises_base_fast(g->ruleset, me_cs, old_base, b0, 2);
            int s0 = eval_card_value_fast(card, b0, 2, me, diff, r0, opp_mask, opp_info) +
                     eval_card_value_fast(matches[0], b0, 2, me, diff, r0, opp_mask, opp_info);
            int b1[2] = { card, matches[1] };
            int r1 = bundle_raises_base_fast(g->ruleset, me_cs, old_base, b1, 2);
            int s1 = eval_card_value_fast(card, b1, 2, me, diff, r1, opp_mask, opp_info) +
                     eval_card_value_fast(matches[1], b1, 2, me, diff, r1, opp_mask, opp_info);
            int chosen_target = (s1 > s0) ? matches[1] : matches[0];
            int chosen_score = (s1 > s0) ? s1 : s0;
            if (chosen_score > best_score) { best_score = chosen_score; best_card = card; best_target = chosen_target; }
        } else if (mc == 3) {
            int bundle[4] = { card, matches[0], matches[1], matches[2] };
            int raises = bundle_raises_base_fast(g->ruleset, me_cs, old_base, bundle, 4);
            int score = eval_card_value_fast(card, bundle, 4, me, diff, raises, opp_mask, opp_info) +
                        eval_card_value_fast(matches[0], bundle, 4, me, diff, raises, opp_mask, opp_info) +
                        eval_card_value_fast(matches[1], bundle, 4, me, diff, raises, opp_mask, opp_info) +
                        eval_card_value_fast(matches[2], bundle, 4, me, diff, raises, opp_mask, opp_info);
            if (score > best_score) { best_score = score; best_card = card; best_target = -1; }
        }
    }
    *out_card = best_card;
    *out_target = best_target;
}

int ai_choose_stock_target(const GameState *g, Difficulty diff, uint32_t *ai_rng) {
    if (diff == DIFFICULTY_EASY) {
        return (ai_rand(ai_rng) % 2) ? g->choice_targets[1] : g->choice_targets[0];
    }
    int me = g->current_player;
    int t0 = g->choice_targets[0];
    int t1 = g->choice_targets[1];
    int b0[2] = { g->pending_card, t0 };
    int b1[2] = { g->pending_card, t1 };
    CaptureSummary me_cs = rules_summarize_captures(g->captured[me], g->captured_count[me]);
    int old_base = score_summary_base(g->ruleset, me_cs);
    uint64_t opp_mask = build_capture_mask(g, 1 - me);
    const Opponent *opp_info = campaign_get_opponent(s_current_opponent);
    int r0 = bundle_raises_base_fast(g->ruleset, me_cs, old_base, b0, 2);
    int r1 = bundle_raises_base_fast(g->ruleset, me_cs, old_base, b1, 2);
    int v0 = eval_card_value_fast(t0, b0, 2, me, diff, r0, opp_mask, opp_info);
    int v1 = eval_card_value_fast(t1, b1, 2, me, diff, r1, opp_mask, opp_info);
    return (v1 > v0) ? t1 : t0;
}

DecisionType ai_choose_decision(const GameState *g, Difficulty diff, uint32_t *ai_rng) {
    int me = g->current_player;
    int opp = 1 - me;

    if (diff == DIFFICULTY_EASY) {
        if (g->hand_count[me] >= 3 && (ai_rand(ai_rng) % 100 < 50)) {
            return DECISION_KOI_KOI;
        }
        return DECISION_SHOBU;
    }

    CaptureSummary opp_cs = rules_summarize_captures(g->captured[opp], g->captured_count[opp]);
    int opp_base = score_summary_base(g->ruleset, opp_cs);

    if (diff == DIFFICULTY_DEUS || diff == DIFFICULTY_MASTER) {
        if (opp_base > 0) return DECISION_SHOBU;
        static const int SPECIAL_CARDS[15] = {
            0, 1, 5, 8, 9, 20, 21, 24, 28, 32, 33, 36, 37, 40, 44
        };
        for (int i = 0; i < 15; i++) {
            if (blocks_opponent_mask(opp_cs.mask, SPECIAL_CARDS[i])) return DECISION_SHOBU;
        }
        int me_base = score_captures_base(g->ruleset, g->captured[me], g->captured_count[me]);
        if (me_base >= 7 || g->hand_count[me] < 3) return DECISION_SHOBU;
        return DECISION_KOI_KOI;
    }

    /* Hard: Koi-Koi only if base < 7, computer hand >= 4 and opponent base = 0; otherwise SHOBU */
    int me_base = score_captures_base(g->ruleset, g->captured[me], g->captured_count[me]);
    if (me_base < 7 && g->hand_count[me] >= 4 && opp_base == 0) {
        return DECISION_KOI_KOI;
    }
    return DECISION_SHOBU;
}
