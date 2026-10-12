#ifndef CARDS_H
#define CARDS_H

#include <stddef.h>

typedef enum {
    CARD_TYPE_BRIGHT = 0,
    CARD_TYPE_ANIMAL = 1,
    CARD_TYPE_RIBBON = 2,
    CARD_TYPE_PLAIN  = 3
} CardType;

#define CARD_FLAG_NONE    0
#define CARD_FLAG_RAIN    (1 << 0)
#define CARD_FLAG_CUP     (1 << 1)
#define CARD_FLAG_POETRY  (1 << 2)
#define CARD_FLAG_BLUE    (1 << 3)

typedef struct {
    int id;             /* 0..47 = (month-1)*4 + slot */
    int month;          /* 1..12 */
    int slot;           /* 0..3 */
    CardType type;      /* Bright, Animal, Ribbon, Plain */
    int flags;          /* Bitmask of CARD_FLAG_* */
    int dealer_rank;    /* 20 (Bright), 10 (Animal), 5 (Ribbon), 1 (Plain) */
    const char *name;   /* Short descriptive name */
    float shimmer_alpha;/* Specular highlight opacity (0.0 .. 23.0) */
} Card;

extern const Card CARDS[48];

/* Validates the CARDS array structure and counts. Returns 1 if valid, 0 otherwise. */
int cards_validate(void);

#endif /* CARDS_H */
