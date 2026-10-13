#include "ai.h"
#include "campaign.h"
#include <stdbool.h>
#include <string.h>

static int s_current_opponent = 0;

void ai_set_opponent(int opp_id) {
    if (opp_id >= 0 && opp_id < 12) s_current_opponent = opp_id;
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

static int unseen_in_month(const GameState *g, int month, int me) {
    int known = 0;
    for (int i = 0; i < g->hand_count[me]; i++) {
        if (CARDS[g->hand[me][i]].month == month) known++;
    }
    for (int i = 0; i < g->field_count; i++) {
        if (CARDS[g->field[i]].month == month) known++;
    }
    for (int p = 0; p < 2; p++) {
        for (int i = 0; i < g->captured_count[p]; i++) {
            if (CARDS[g->captured[p][i]].month == month) known++;
        }
    }
    return (4 - known) > 0;
}

static int blocks_opponent_set(const GameState *g, int card_id, int opp) {
    if (card_id < 0 || card_id >= 48) return 0;
    int opp_has[48] = {0};
    for (int i = 0; i < g->captured_count[opp]; i++) opp_has[g->captured[opp][i]] = 1;
    if (opp_has[card_id]) return 0;

    /* Hanami: 08-B Curtain (8) + 09-A Cup (32) */
    if ((card_id == 32 && opp_has[8]) || (card_id == 8 && opp_has[32])) return 1;

    /* Tsukimi: 08-B Moon (28) + 09-A Cup (32) */
    if ((card_id == 32 && opp_has[28]) || (card_id == 28 && opp_has[32])) return 1;

    /* Ino-shika-cho: 20, 24, 36 */
    if ((card_id == 20 || card_id == 24 || card_id == 36) &&
        (opp_has[20] + opp_has[24] + opp_has[36] == 2)) return 1;

    /* Akatan: 1, 5, 9 */
    if ((card_id == 1 || card_id == 5 || card_id == 9) &&
        (opp_has[1] + opp_has[5] + opp_has[9] == 2)) return 1;

    /* Aotan: 21, 33, 37 */
    if ((card_id == 21 || card_id == 33 || card_id == 37) &&
        (opp_has[21] + opp_has[33] + opp_has[37] == 2)) return 1;

    /* Brights: 1 card away from Sanko (2 non-rain), Shiko/Ame-Shiko (3 total), or Goko (4 total) */
    if (CARDS[card_id].type == CARD_TYPE_BRIGHT) {
        int non_rain = opp_has[0] + opp_has[8] + opp_has[28] + opp_has[44];
        int total_b = non_rain + opp_has[40];
        if (card_id != 40 && non_rain == 2) return 1;
        if (total_b >= 3) return 1;
    }
    return 0;
}

static int eval_card_value(const GameState *g, int card_id, const int *new_cards, int new_count, int me, Difficulty diff) {
    int val = CARDS[card_id].dealer_rank;
    int opp = 1 - me;

    /* Check if taking it raises current player's base */
    int temp[48];
    int tc = g->captured_count[me];
    for (int i = 0; i < tc; i++) temp[i] = g->captured[me][i];
    for (int i = 0; i < new_count; i++) temp[tc++] = new_cards[i];

    ScoreResult old_sr = score_captures(g->ruleset, g->captured[me], g->captured_count[me]);
    ScoreResult new_sr = score_captures(g->ruleset, temp, tc);
    if (new_sr.base_points > old_sr.base_points) {
        val += 15;
    }

    if (diff == DIFFICULTY_MASTER) {
        /* Master AI: massive +50 priority to block player major yaku */
        if (blocks_opponent_set(g, card_id, opp)) val += 50;
        for (int i = 0; i < new_count; i++) {
            if (new_cards[i] != card_id && blocks_opponent_set(g, new_cards[i], opp)) {
                val += 50;
                break;
            }
        }
    } else {
        if (blocks_opponent_set(g, card_id, opp)) val += 6;
    }

    if (me == 1) {
        const Opponent *opp_info = campaign_get_opponent(s_current_opponent);
        if (opp_info) {
            int fav = opp_info->favorite_card_id;
            if (card_id == fav) val += 35;
            for (int i = 0; i < new_count; i++) {
                if (new_cards[i] == fav) { val += 35; break; }
            }
            if (opp_info->favorite_yaku == YAKU_GOKO || opp_info->favorite_yaku == YAKU_SANKO ||
                opp_info->favorite_yaku == YAKU_SHIKO || opp_info->favorite_yaku == YAKU_AME_SHIKO) {
                if (CARDS[card_id].type == CARD_TYPE_BRIGHT) val += 24;
            } else if (opp_info->favorite_yaku == YAKU_TANZAKU || opp_info->favorite_yaku == YAKU_AKATAN || opp_info->favorite_yaku == YAKU_AOTAN) {
                if (CARDS[card_id].type == CARD_TYPE_RIBBON) val += 22;
            } else if (opp_info->favorite_yaku == YAKU_TANE) {
                if (CARDS[card_id].type == CARD_TYPE_ANIMAL) val += 22;
            } else if (opp_info->favorite_yaku == YAKU_INO_SHIKA_CHO) {
                if (card_id == 20 || card_id == 24 || card_id == 36) val += 32;
                else if (CARDS[card_id].type == CARD_TYPE_ANIMAL) val += 14;
            } else if (opp_info->favorite_yaku == YAKU_KASU) {
                if (CARDS[card_id].type == CARD_TYPE_PLAIN) val += 20;
            } else if (opp_info->favorite_yaku == YAKU_HANAMI_ZAKE || opp_info->favorite_yaku == YAKU_TSUKIMI_ZAKE) {
                if (card_id == 32 || card_id == 8 || card_id == 28) val += 24;
            }
        }
    }

    return val;
}

static int ai_get_accuracy(Difficulty diff, int boss_id) {
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

    /* If optimal roll: execute smart strategy */
    /* 1. Campaign signature card & preferred yaku priority */
    const Opponent *opp_info = campaign_get_opponent(s_current_opponent);
    if (opp_info) {
        for (int i = 0; i < n; i++) {
            int card = g->hand[me][i];
            for (int j = 0; j < g->field_count; j++) {
                int fc = g->field[j];
                if (CARDS[fc].month == CARDS[card].month &&
                    (card == opp_info->favorite_card_id || fc == opp_info->favorite_card_id)) {
                    *out_card = card;
                    *out_target = fc;
                    return;
                }
            }
        }
        for (int i = 0; i < n; i++) {
            int card = g->hand[me][i];
            for (int j = 0; j < g->field_count; j++) {
                int fc = g->field[j];
                if (CARDS[fc].month == CARDS[card].month) {
                    int m = 0;
                    if (opp_info->favorite_yaku == YAKU_GOKO && (CARDS[card].type == CARD_TYPE_BRIGHT || CARDS[fc].type == CARD_TYPE_BRIGHT)) m = 1;
                    else if (opp_info->favorite_yaku == YAKU_TANZAKU && (CARDS[card].type == CARD_TYPE_RIBBON || CARDS[fc].type == CARD_TYPE_RIBBON)) m = 1;
                    else if (opp_info->favorite_yaku == YAKU_TANE && (CARDS[card].type == CARD_TYPE_ANIMAL || CARDS[fc].type == CARD_TYPE_ANIMAL)) m = 1;
                    else if (opp_info->favorite_yaku == YAKU_INO_SHIKA_CHO && (card == 20 || card == 24 || card == 36 || fc == 20 || fc == 24 || fc == 36)) m = 1;
                    else if (opp_info->favorite_yaku == YAKU_KASU && (CARDS[card].type == CARD_TYPE_PLAIN && CARDS[fc].type == CARD_TYPE_PLAIN)) m = 1;
                    if (m) { *out_card = card; *out_target = fc; return; }
                }
            }
        }
    }

    /* 2. Full heuristic evaluation (capture value, combo building, and blocking) */
    int best_score = -999999;
    int best_card = g->hand[me][0];
    int best_target = -1;

    for (int i = 0; i < n; i++) {
        int card = g->hand[me][i];
        int matches[4], mc = 0;
        for (int j = 0; j < g->field_count; j++) {
            if (CARDS[g->field[j]].month == CARDS[card].month) matches[mc++] = g->field[j];
        }

        if (mc == 0) {
            int val = eval_card_value(g, card, &card, 1, me, diff);
            int score = -val;
            if (unseen_in_month(g, CARDS[card].month, me)) score -= 4;
            if (diff == DIFFICULTY_MASTER && blocks_opponent_set(g, card, 1 - me)) score -= 100;
            if (score > best_score) { best_score = score; best_card = card; best_target = -1; }
        } else if (mc == 1) {
            int bundle[2] = { card, matches[0] };
            int score = eval_card_value(g, card, bundle, 2, me, diff) + eval_card_value(g, matches[0], bundle, 2, me, diff);
            if (score > best_score) { best_score = score; best_card = card; best_target = matches[0]; }
        } else if (mc == 2) {
            int b0[2] = { card, matches[0] };
            int s0 = eval_card_value(g, card, b0, 2, me, diff) + eval_card_value(g, matches[0], b0, 2, me, diff);
            int b1[2] = { card, matches[1] };
            int s1 = eval_card_value(g, card, b1, 2, me, diff) + eval_card_value(g, matches[1], b1, 2, me, diff);
            int chosen_target = (s1 > s0) ? matches[1] : matches[0];
            int chosen_score = (s1 > s0) ? s1 : s0;
            if (chosen_score > best_score) { best_score = chosen_score; best_card = card; best_target = chosen_target; }
        } else if (mc == 3) {
            int bundle[4] = { card, matches[0], matches[1], matches[2] };
            int score = eval_card_value(g, card, bundle, 4, me, diff) +
                        eval_card_value(g, matches[0], bundle, 4, me, diff) +
                        eval_card_value(g, matches[1], bundle, 4, me, diff) +
                        eval_card_value(g, matches[2], bundle, 4, me, diff);
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
    int v0 = eval_card_value(g, t0, b0, 2, me, diff);
    int v1 = eval_card_value(g, t1, b1, 2, me, diff);
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

    if (diff == DIFFICULTY_MASTER) {
        ScoreResult sr_opp = score_captures(g->ruleset, g->captured[opp], g->captured_count[opp]);
        if (sr_opp.base_points > 0) return DECISION_SHOBU;
        for (int i = 0; i < 48; i++) {
            if (blocks_opponent_set(g, i, opp)) return DECISION_SHOBU;
        }
        ScoreResult sr_me = score_captures(g->ruleset, g->captured[me], g->captured_count[me]);
        if (sr_me.base_points >= 7 || g->hand_count[me] < 3) return DECISION_SHOBU;
        return DECISION_KOI_KOI;
    }

    /* Hard: Koi-Koi only if base < 7, computer hand >= 4 and opponent base = 0; otherwise SHOBU */
    ScoreResult sr_me = score_captures(g->ruleset, g->captured[me], g->captured_count[me]);
    ScoreResult sr_opp = score_captures(g->ruleset, g->captured[opp], g->captured_count[opp]);

    if (sr_me.base_points < 7 && g->hand_count[me] >= 4 && sr_opp.base_points == 0) {
        return DECISION_KOI_KOI;
    }
    return DECISION_SHOBU;
}
