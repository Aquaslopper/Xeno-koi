#ifndef ENGINE_H
#define ENGINE_H

#include <stdint.h>
#include "cards.h"
#include "rules.h"

typedef enum {
    PHASE_NONE = 0,
    PHASE_DEALER_SELECT,
    PHASE_DEAL,
    PHASE_LUCKY_CHECK,
    PHASE_TURN_HAND,
    PHASE_HAND_CHOICE,
    PHASE_TURN_DRAW,
    PHASE_DRAW_CHOICE,
    PHASE_DECIDE,
    PHASE_ROUND_END,
    PHASE_MATCH_END
} GamePhase;

typedef enum {
    DECISION_KOI_KOI = 0,
    DECISION_SHOBU   = 1
} DecisionType;

typedef struct {
    int round_number;
    int dealer;         /* 0: Human, 1: CPU */
    int winner;         /* 0: Human, 1: CPU, -1: Draw */
    int draw_reason;    /* 0: None, 1: Exhaustion, 2: Both Lucky */
    int lucky_hand;     /* 1 if won via Teshi/Kuttsuki */
    int base_points;
    int mult_seven;     /* 1 or 2 */
    int mult_opp_koi;   /* 1 or 2  - opponent called Koi-Koi and lost the round */
    int mult_self_koi;  /* 1..KOI_CALLER_MULT_MAX - escalating caller multiplier */
    int self_koi_calls; /* Koi-Koi calls made by the round winner */
    int opp_koi_calls;  /* Koi-Koi calls made by the round loser */
    int award;
    ScoreResult score_result;
} RoundRecord;

typedef struct {
    Ruleset ruleset;
    int total_rounds;       /* 1..12, default 6 */
    int current_round;      /* 1..total_rounds */
    int total_score[2];     /* 0: Human, 1: CPU */
    int dealer;             /* 0: Human, 1: CPU */
    int current_player;     /* 0: Human, 1: CPU */

    GamePhase phase;

    /* Zones: each card id 0..47 is in exactly one zone */
    int hand[2][48];
    int hand_count[2];
    int field[48];
    int field_count;
    int stock[48];
    int stock_count;
    int captured[2][48];
    int captured_count[2];

    int has_pending;
    int pending_card;

    /* Choice states */
    int played_hand_card;
    int choice_targets[4];
    int choice_count;

    /* Round tracking */
    int last_base[2];
    /* KK-ADD-10 only: points per yaku at the player's last Koi-Koi decision
       (0 = yaku absent). The Koi-Koi screen opens when a yaku is new or its
       points have grown past this snapshot. See finish_stock_turn(). */
    int last_yaku_points[2][YAKU_COUNT];
    int koi_called[2];      /* number of times koi-koi called this round */
    int match_koi_calls[2]; /* number of times koi-koi called this match (koi_called resets every round) */
    int human_yaku_counts[YAKU_COUNT]; /* human yaku tracking */

    /* Ledger */
    RoundRecord current_round_record;
    RoundRecord ledger[12];
    int ledger_count;

    /* PRNG */
    uint32_t rng_state;

    /* Last capture event (for UI animations / logging) */
    int last_capture_source;
    int last_captured[4];
    int last_captured_count;

    /* Oya-gime dealer selection */
    int oya_cards[2];
    int oya_player_choice;
    int oya_winner;
    float oya_timer;

    /* AAA Game Feel: Hitlag freeze-frame */
    float hitlag_timer;

    /* Master Mode: Ticking clock */
    float player_timer;
} GameState;

/* PRNG helper */
uint32_t engine_rand(uint32_t *state);

/* Initialize match state */
void engine_init(GameState *g, Ruleset ruleset, int total_rounds, uint32_t seed);

/* Determine first dealer by single card draw. Returns 0 or 1. */
int engine_determine_first_dealer(uint32_t *rng_state);

/* Deal a round. Handles misdeals automatically. Advances to LUCKY_CHECK or TURN_HAND. */
void engine_start_round(GameState *g);

/* Check lucky hands (Teshi / Kuttsuki). Advances to TURN_HAND or ROUND_END. */
void engine_check_lucky_hands(GameState *g);

/* Play a card from hand. target_id is matching field card, or -1 if none/unspecified. */
int engine_play_hand(GameState *g, int card_id, int target_id);

/* Pick target when in PHASE_HAND_CHOICE. */
int engine_pick_hand_choice(GameState *g, int target_id);

/* Cancel hand choice when 2 matches were available, returning to TURN_HAND. */
int engine_cancel_hand_choice(GameState *g);

/* Auto-plays a random card from player hand when timer expires in MASTER difficulty. */
int engine_timeout_autoplay(GameState *g);

/* Reveal stock card into pending zone and resolve 0/1/3 matches or enter DRAW_CHOICE. */
int engine_reveal_stock(GameState *g);

/* Pick target for stock card when 2 matches exist. Cannot be cancelled. */
int engine_pick_draw_choice(GameState *g, int target_id);

/* Make Koi-Koi or Shobu decision. */
int engine_decide(GameState *g, DecisionType decision);

/* Advance to next round or MATCH_END. */
void engine_next_round(GameState *g);

/* Conservation check: returns 1 if all 48 cards are in exactly one zone, 0 otherwise. */
int engine_validate_zones(const GameState *g);

#endif /* ENGINE_H */
