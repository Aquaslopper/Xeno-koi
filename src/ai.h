#ifndef AI_H
#define AI_H

#include <stdint.h>
#include "engine.h"

typedef enum {
    DIFFICULTY_EASY = 0,
    DIFFICULTY_HARD = 1,
    DIFFICULTY_MASTER = 2,
    DIFFICULTY_DEUS = 3
} Difficulty;

/* PRNG for AI (kept separate from engine deal PRNG) */
uint32_t ai_rand(uint32_t *state);

/* Chooses a card from hand to play and the target field card (or -1 if no target / capture all / discard). */
void ai_choose_hand(const GameState *g, Difficulty diff, uint32_t *ai_rng, int *out_card, int *out_target);

/* Chooses one of the 2 targets when stock reveal produces 2 matches. */
int ai_choose_stock_target(const GameState *g, Difficulty diff, uint32_t *ai_rng);

/* Chooses between KOI-KOI and SHOBU. */
DecisionType ai_choose_decision(const GameState *g, Difficulty diff, uint32_t *ai_rng);

void ai_set_opponent(int opp_id);
int ai_get_opponent(void);

#endif /* AI_H */
