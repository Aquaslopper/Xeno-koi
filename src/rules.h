#ifndef RULES_H
#define RULES_H

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
    /* Appended last: existing indices are persisted in stats files and
       referenced by campaign opponents' favorite_yaku. Do not reorder. */
    YAKU_TSUKIFUDA,
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
    /* Category totals of the scored pile (used for improvement checks). */
    int bright_count;
    int animal_count;
    int ribbon_count;
    int plain_count;          /* raw chaff cards */
    int effective_plain_count;/* chaff count incl. Sake Cup where it counts */
} ScoreResult;

/* --- Ruleset behaviour (single source of truth for scorer/engine/AI) --- */
/* Hanami-zake (Curtain + Cup) is scored. */
int ruleset_has_hanami_zake(Ruleset ruleset);
/* Tsukimi-zake (Moon + Cup) is scored. Removed in Tournament/Yakuza:
   Moon + Sake Cup was judged too strong for those rulesets. */
int ruleset_has_tsukimi_zake(Ruleset ruleset);
/* The Sake Cup also counts as a chaff card for Kasu. In Tournament/Yakuza
   the cup is treated strictly as an animal, so it does not. */
int ruleset_cup_counts_as_kasu(Ruleset ruleset);
/* Only Nintendo limits a player to one Koi-Koi call per round; the next
   improvement after that call is an automatic Shobu. */
int ruleset_one_koi_call(Ruleset ruleset);
/* Month (1..12) whose four cards form Tsukifuda this round, or 0 when the
   yaku does not apply (it is only used when a match lasts 12 rounds, so
   round n is month n). */
int ruleset_round_month(int current_round, int total_rounds);

/* Scores a capture pile according to the specified ruleset. */
ScoreResult score_captures(Ruleset ruleset, const int *cards, int n);

/* As above, with the Tsukifuda month (1..12, or 0 to disable that yaku). */
ScoreResult score_captures_ex(Ruleset ruleset, const int *cards, int n, int round_month);

/* 1 if new_sr forms a yaku old_sr lacked, raises a yaku's points, raises
   the base, or grows the card count of a category an already-held yaku
   scores from (e.g. a 4th ribbon under a fixed-value Akatan in
   KK-ADD-10, where the base does not move). This is the condition that
   earns a Koi-Koi / Shobu decision. */
int score_is_improved(const ScoreResult *old_sr, const ScoreResult *new_sr);

/* Calculates the final round award given the base and opponent koi status. */
int calculate_award(int base, int opponent_called_koi);

#endif /* RULES_H */
