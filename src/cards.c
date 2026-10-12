#include "cards.h"

const Card CARDS[48] = {
    /* 00-03: Month 1 (Jan Pine) */
    { 0,  1, 0, CARD_TYPE_BRIGHT, CARD_FLAG_NONE,   20, "01-B Crane" },
    { 1,  1, 1, CARD_TYPE_RIBBON, CARD_FLAG_POETRY,  5, "01-RP Poetry" },
    { 2,  1, 2, CARD_TYPE_PLAIN,  CARD_FLAG_NONE,    1, "01-P1 Plain" },
    { 3,  1, 3, CARD_TYPE_PLAIN,  CARD_FLAG_NONE,    1, "01-P2 Plain" },

    /* 04-07: Month 2 (Feb Plum) */
    { 4,  2, 0, CARD_TYPE_ANIMAL, CARD_FLAG_NONE,   10, "02-A Warbler" },
    { 5,  2, 1, CARD_TYPE_RIBBON, CARD_FLAG_POETRY,  5, "02-RP Poetry" },
    { 6,  2, 2, CARD_TYPE_PLAIN,  CARD_FLAG_NONE,    1, "02-P1 Plain" },
    { 7,  2, 3, CARD_TYPE_PLAIN,  CARD_FLAG_NONE,    1, "02-P2 Plain" },

    /* 08-11: Month 3 (Mar Cherry) */
    { 8,  3, 0, CARD_TYPE_BRIGHT, CARD_FLAG_NONE,   20, "03-B Curtain" },
    { 9,  3, 1, CARD_TYPE_RIBBON, CARD_FLAG_POETRY,  5, "03-RP Poetry" },
    { 10, 3, 2, CARD_TYPE_PLAIN,  CARD_FLAG_NONE,    1, "03-P1 Plain" },
    { 11, 3, 3, CARD_TYPE_PLAIN,  CARD_FLAG_NONE,    1, "03-P2 Plain" },

    /* 12-15: Month 4 (Apr Wisteria) */
    { 12, 4, 0, CARD_TYPE_ANIMAL, CARD_FLAG_NONE,   10, "04-A Cuckoo" },
    { 13, 4, 1, CARD_TYPE_RIBBON, CARD_FLAG_NONE,    5, "04-R Ribbon" },
    { 14, 4, 2, CARD_TYPE_PLAIN,  CARD_FLAG_NONE,    1, "04-P1 Plain" },
    { 15, 4, 3, CARD_TYPE_PLAIN,  CARD_FLAG_NONE,    1, "04-P2 Plain" },

    /* 16-19: Month 5 (May Iris) */
    { 16, 5, 0, CARD_TYPE_ANIMAL, CARD_FLAG_NONE,   10, "05-A Bridge" },
    { 17, 5, 1, CARD_TYPE_RIBBON, CARD_FLAG_NONE,    5, "05-R Ribbon" },
    { 18, 5, 2, CARD_TYPE_PLAIN,  CARD_FLAG_NONE,    1, "05-P1 Plain" },
    { 19, 5, 3, CARD_TYPE_PLAIN,  CARD_FLAG_NONE,    1, "05-P2 Plain" },

    /* 20-23: Month 6 (Jun Peony) */
    { 20, 6, 0, CARD_TYPE_ANIMAL, CARD_FLAG_NONE,   10, "06-A Butterflies" },
    { 21, 6, 1, CARD_TYPE_RIBBON, CARD_FLAG_BLUE,    5, "06-RB Blue" },
    { 22, 6, 2, CARD_TYPE_PLAIN,  CARD_FLAG_NONE,    1, "06-P1 Plain" },
    { 23, 6, 3, CARD_TYPE_PLAIN,  CARD_FLAG_NONE,    1, "06-P2 Plain" },

    /* 24-27: Month 7 (Jul Clover) */
    { 24, 7, 0, CARD_TYPE_ANIMAL, CARD_FLAG_NONE,   10, "07-A Boar" },
    { 25, 7, 1, CARD_TYPE_RIBBON, CARD_FLAG_NONE,    5, "07-R Ribbon" },
    { 26, 7, 2, CARD_TYPE_PLAIN,  CARD_FLAG_NONE,    1, "07-P1 Plain" },
    { 27, 7, 3, CARD_TYPE_PLAIN,  CARD_FLAG_NONE,    1, "07-P2 Plain" },

    /* 28-31: Month 8 (Aug Pampas) */
    { 28, 8, 0, CARD_TYPE_BRIGHT, CARD_FLAG_NONE,   20, "08-B Moon" },
    { 29, 8, 1, CARD_TYPE_ANIMAL, CARD_FLAG_NONE,   10, "08-A Geese" },
    { 30, 8, 2, CARD_TYPE_PLAIN,  CARD_FLAG_NONE,    1, "08-P1 Plain" },
    { 31, 8, 3, CARD_TYPE_PLAIN,  CARD_FLAG_NONE,    1, "08-P2 Plain" },

    /* 32-35: Month 9 (Sep Chrysanthemum) */
    { 32, 9, 0, CARD_TYPE_ANIMAL, CARD_FLAG_CUP,    10, "09-A Sake Cup" },
    { 33, 9, 1, CARD_TYPE_RIBBON, CARD_FLAG_BLUE,    5, "09-RB Blue" },
    { 34, 9, 2, CARD_TYPE_PLAIN,  CARD_FLAG_NONE,    1, "09-P1 Plain" },
    { 35, 9, 3, CARD_TYPE_PLAIN,  CARD_FLAG_NONE,    1, "09-P2 Plain" },

    /* 36-39: Month 10 (Oct Maple) */
    { 36, 10, 0, CARD_TYPE_ANIMAL, CARD_FLAG_NONE,  10, "10-A Deer" },
    { 37, 10, 1, CARD_TYPE_RIBBON, CARD_FLAG_BLUE,   5, "10-RB Blue" },
    { 38, 10, 2, CARD_TYPE_PLAIN,  CARD_FLAG_NONE,   1, "10-P1 Plain" },
    { 39, 10, 3, CARD_TYPE_PLAIN,  CARD_FLAG_NONE,   1, "10-P2 Plain" },

    /* 40-43: Month 11 (Nov Willow) */
    { 40, 11, 0, CARD_TYPE_BRIGHT, CARD_FLAG_RAIN,  20, "11-B Rainman" },
    { 41, 11, 1, CARD_TYPE_ANIMAL, CARD_FLAG_NONE,  10, "11-A Swallow" },
    { 42, 11, 2, CARD_TYPE_RIBBON, CARD_FLAG_NONE,   5, "11-R Ribbon" },
    { 43, 11, 3, CARD_TYPE_PLAIN,  CARD_FLAG_NONE,   1, "11-P Lightning" },

    /* 44-47: Month 12 (Dec Paulownia) */
    { 44, 12, 0, CARD_TYPE_BRIGHT, CARD_FLAG_NONE,  20, "12-B Phoenix" },
    { 45, 12, 1, CARD_TYPE_PLAIN,  CARD_FLAG_NONE,   1, "12-P1 Plain" },
    { 46, 12, 2, CARD_TYPE_PLAIN,  CARD_FLAG_NONE,   1, "12-P2 Plain" },
    { 47, 12, 3, CARD_TYPE_PLAIN,  CARD_FLAG_NONE,   1, "12-P2 Plain" }
};

int cards_validate(void) {
    int bright_count = 0;
    int animal_count = 0;
    int ribbon_count = 0;
    int plain_count = 0;
    int poetry_count = 0;
    int blue_count = 0;
    int rain_count = 0;
    int cup_count = 0;

    for (int i = 0; i < 48; i++) {
        const Card *c = &CARDS[i];
        if (c->id != i) return 0;
        if (c->month != (i / 4) + 1) return 0;
        if (c->slot != (i % 4)) return 0;

        switch (c->type) {
            case CARD_TYPE_BRIGHT:
                bright_count++;
                if (c->dealer_rank != 20) return 0;
                break;
            case CARD_TYPE_ANIMAL:
                animal_count++;
                if (c->dealer_rank != 10) return 0;
                break;
            case CARD_TYPE_RIBBON:
                ribbon_count++;
                if (c->dealer_rank != 5) return 0;
                break;
            case CARD_TYPE_PLAIN:
                plain_count++;
                if (c->dealer_rank != 1) return 0;
                break;
            default:
                return 0;
        }

        if (c->flags & CARD_FLAG_RAIN) rain_count++;
        if (c->flags & CARD_FLAG_CUP) cup_count++;
        if (c->flags & CARD_FLAG_POETRY) poetry_count++;
        if (c->flags & CARD_FLAG_BLUE) blue_count++;
    }

    if (bright_count != 5) return 0;
    if (animal_count != 9) return 0;
    if (ribbon_count != 10) return 0;
    if (plain_count != 24) return 0;

    if (rain_count != 1 || CARDS[40].flags != CARD_FLAG_RAIN) return 0;
    if (cup_count != 1 || CARDS[32].flags != CARD_FLAG_CUP) return 0;
    if (poetry_count != 3) return 0;
    if (blue_count != 3) return 0;

    return 1;
}
