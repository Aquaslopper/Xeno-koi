#ifndef RULES_H
#define RULES_H

#include <stdint.h>
#include "cards.h"

typedef enum {
    RULESET_NINTENDO = 0,
    RULESET_FUDAWIKI = 1,
    RULESET_KK_ADD_10 = 2,
    RULESET_TOURNAMENT = 3,
    RULESET_YAKUZA = 4
} Ruleset;

typedef enum {
    YAKU_GOKO = 0,
    YAKU_SHIKO,
    YAKU_AME_SHIKO,
    YAKU_SANKO,
    YAKU_HANAMI_ZAKE,
    YAKU_TSUKIMI_ZAKE,
    YAKU_INO_SHIKA_CHO,
    YAKU_AKATAN,
    YAKU_AOTAN,
    YAKU_AKATAN_AOTAN,
    YAKU_TANE,
    YAKU_TANZAKU,
    YAKU_KASU,
    YAKU_COUNT
} YakuId;

typedef struct {
    YakuId id;
    const char *name;
    int points;
    int card_ids[10];
    int card_count;
} ScoredYaku;

typedef struct {
    int yaku_count;
    ScoredYaku yakus[YAKU_COUNT];
    int base_points;
} ScoreResult;

/* Pre-aggregated capture counts and bitmask for O(1) incremental evaluation. */
typedef struct {
    uint64_t mask;
    int bright_count;
    int animal_count;
    int ribbon_count;
    int plain_count;
} CaptureSummary;

/* Build summary from capture array and evaluate base points in O(1). */
CaptureSummary rules_summarize_captures(const int *cards, int n);
int score_summary_base(Ruleset ruleset, CaptureSummary s);

/* Scores a capture pile according to the specified ruleset. */
ScoreResult score_captures(Ruleset ruleset, const int *cards, int n);

/* Fast-path calculation of base points only (no struct formatting overhead). */
int score_captures_base(Ruleset ruleset, const int *cards, int n);

/* Calculates the final round award given the base and opponent koi status. */
int calculate_award(int base, int opponent_called_koi);

#endif /* RULES_H */
