/*
 * wiki_tables.c — Documentation table dumper for the Xeno Koi wiki.
 *
 * Links against the REAL engine scoring code (src/rules.c + src/cards.c) and
 * prints markdown fragments to stdout. tools/gen_wiki_tables.py splices these
 * fragments into the marker blocks in docs/, so the wiki's tables can never
 * drift away from the engine.
 *
 * Build (handled by gen_wiki_tables.py):
 *   gcc -std=c99 -O2 -Isrc tools/wiki_tables.c src/cards.c src/rules.c -o wiki_tables
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cards.h"
#include "campaign.h"
#include "rules.h"

static const char *MONTH_NAMES[12] = {
    "January (Pine)", "February (Plum)", "March (Cherry)", "April (Wisteria)",
    "May (Iris)", "June (Peony)", "July (Clover)", "August (Pampas)",
    "September (Chrysanthemum)", "October (Maple)", "November (Willow)", "December (Paulownia)"
};

static const char *TYPE_NAMES[4] = { "Bright", "Animal", "Ribbon", "Plain" };

static const char *yaku_name_of(YakuId id) {
    switch (id) {
        case YAKU_GOKO:          return "Goko";
        case YAKU_SHIKO:         return "Shiko";
        case YAKU_AME_SHIKO:     return "Ame-shiko";
        case YAKU_SANKO:         return "Sanko";
        case YAKU_HANAMI_ZAKE:   return "Hanami-zake";
        case YAKU_TSUKIMI_ZAKE:  return "Tsukimi-zake";
        case YAKU_INO_SHIKA_CHO: return "Ino-shika-cho";
        case YAKU_AKATAN:        return "Akatan";
        case YAKU_AOTAN:         return "Aotan";
        case YAKU_AKATAN_AOTAN:  return "Akatan + Aotan";
        case YAKU_TANE:          return "Tane";
        case YAKU_TANZAKU:       return "Tanzaku";
        case YAKU_KASU:          return "Kasu";
        default:                 return "?";
    }
}

/* Print the ScoreResult of a capture pile as "Name (pts) + ... = base". */
static void print_score_cell(Ruleset rs, const int *pile, int n) {
    ScoreResult sr = score_captures(rs, pile, n);
    if (sr.yaku_count == 0) {
        printf("-- *(0)* | ");
        return;
    }
    for (int i = 0; i < sr.yaku_count; i++) {
        printf("%s %d%s", yaku_name_of(sr.yakus[i].id), sr.yakus[i].points,
               (i + 1 < sr.yaku_count) ? " + " : " ");
    }
    printf("= **%d** | ", sr.base_points);
}

/* Find yaku points in a score result; -1 if absent. */
static int find_yaku_points(const ScoreResult *sr, YakuId id) {
    for (int i = 0; i < sr->yaku_count; i++) {
        if (sr->yakus[i].id == id) return sr->yakus[i].points;
    }
    return -1;
}

/* One card thumbnail. Images are cut from assets/atlas.png by
 * tools/make_card_images.py into docs/assets/cards/card-<id>.webp.
 * `prefix` is the relative path from the page using the fragment. */
static void print_card_img(const char *prefix, int id, const char *cls) {
    printf("![%s](%scard-%02d.webp){ .%s }", CARDS[id].name, prefix, id, cls);
}

/* A row of card thumbnails for a capture pile. */
static void print_pile_imgs(const char *prefix, const int *pile, int n, const char *cls) {
    for (int i = 0; i < n; i++) {
        if (i) putchar(' ');
        print_card_img(prefix, pile[i], cls);
    }
}

static void frag_cards(void) {
    puts("@@FRAGMENT cards");
    puts("| ID | Card image | Month | Card | Type | Special |");
    puts("|---:|---|---|---|---|---|");
    for (int i = 0; i < 48; i++) {
        const Card *c = &CARDS[i];
        const char *sp = "";
        if (c->flags & CARD_FLAG_RAIN)   sp = "Rain Man";
        if (c->flags & CARD_FLAG_CUP)    sp = "Sake Cup";
        if (c->flags & CARD_FLAG_POETRY) sp = "Poetry ribbon";
        if (c->flags & CARD_FLAG_BLUE)   sp = "Blue ribbon";
        printf("| %d | ", c->id);
        print_card_img("../assets/cards/", i, "card-thumb");
        printf(" | %s | %s | %s | %s |\n",
               MONTH_NAMES[c->month - 1], c->name, TYPE_NAMES[c->type], sp);
    }
    puts("");
    printf("**Composition check (from `cards_validate()` invariants):** 5 Brights, 9 Animals, 10 Ribbons, 24 Plains = 48 cards.\n");
    puts("@@END");
}

static void frag_yaku_values(void) {
    /* Minimal / canonical pile for each yaku. */
    static const int p_goko[]    = { 0, 8, 28, 40, 44 };
    static const int p_shiko[]   = { 0, 8, 28, 44 };
    static const int p_ame[]     = { 0, 8, 28, 40 };
    static const int p_sanko[]   = { 0, 8, 28 };
    static const int p_hanami[]  = { 8, 32 };
    static const int p_tsukimi[] = { 28, 32 };
    static const int p_ino[]     = { 20, 24, 36 };
    static const int p_akatan[]  = { 1, 5, 9 };
    static const int p_aotan[]   = { 21, 33, 37 };
    static const int p_both[]    = { 1, 5, 9, 21, 33, 37 };
    static const int p_tane[]    = { 4, 12, 16, 29, 41 };
    static const int p_tanzaku[] = { 1, 13, 17, 25, 42 };
    static const int p_kasu[]    = { 2, 3, 6, 7, 10, 11, 14, 15, 18, 19 };

    struct { YakuId id; const char *pile_desc; const int *pile; int n; } rows[] = {
        { YAKU_GOKO,          "all 5 Brights",                          p_goko,    5 },
        { YAKU_SHIKO,         "4 Brights, no Rain Man",                 p_shiko,   4 },
        { YAKU_AME_SHIKO,     "4 Brights incl. Rain Man",               p_ame,     4 },
        { YAKU_SANKO,         "3 Brights, no Rain Man",                 p_sanko,   3 },
        { YAKU_HANAMI_ZAKE,   "Curtain + Sake Cup",                     p_hanami,  2 },
        { YAKU_TSUKIMI_ZAKE,  "Moon + Sake Cup",                        p_tsukimi, 2 },
        { YAKU_INO_SHIKA_CHO, "Butterflies + Boar + Deer",              p_ino,     3 },
        { YAKU_AKATAN,        "3 Poetry ribbons (Jan/Feb/Mar)",         p_akatan,  3 },
        { YAKU_AOTAN,         "3 Blue ribbons (Jun/Sep/Oct)",           p_aotan,   3 },
        { YAKU_AKATAN_AOTAN,  "Akatan + Aotan (6 ribbons)",             p_both,    6 },
        { YAKU_TANE,          "5 Animals, Ino-shika-cho incomplete",    p_tane,    5 },
        { YAKU_TANZAKU,       "5 Ribbons, no poetry/blue set complete", p_tanzaku, 5 },
        { YAKU_KASU,          "10 Plains",                              p_kasu,   10 },
    };

    puts("@@FRAGMENT yaku-values");
    puts("Points each yaku scores **with the exact capture pile listed** (evaluated by the engine's own `score_captures()`):");
    puts("");
    puts("| Yaku | Capture pile | Cards | Nintendo | FudaWiki | KK-ADD-10 | Tournament | Yakuza |");
    puts("|---|---|---|---:|---:|---:|---:|---:|");
    for (unsigned r = 0; r < sizeof(rows) / sizeof(rows[0]); r++) {
        printf("| **%s** | %s | ", yaku_name_of(rows[r].id), rows[r].pile_desc);
        print_pile_imgs("../assets/cards/", rows[r].pile, rows[r].n, "card-mini");
        printf(" | ");
        for (int rs = 0; rs < 5; rs++) {
            ScoreResult sr = score_captures((Ruleset)rs, rows[r].pile, rows[r].n);
            int pts = find_yaku_points(&sr, rows[r].id);
            if (pts < 0) printf("--%s", (rs < 4) ? " | " : " |\n");
            else printf("%d%s", pts, (rs < 4) ? " | " : " |\n");
        }
    }
    puts("");
    puts("*(a `--` means the yaku cannot score at all under that ruleset)*");
    puts("@@END");
}

static void frag_stacking(void) {
    static const int p_ino_extra[]    = { 20, 24, 36, 4, 29 };            /* ino + 2 more animals */
    static const int p_akatan_extra[] = { 1, 5, 9, 13, 17 };              /* akatan + 2 more ribbons */
    static const int p_both_extra[]   = { 1, 5, 9, 21, 33, 37, 13, 17 };  /* both sets + 2 more ribbons */
    static const int p_tane6[]        = { 4, 12, 16, 20, 29, 41 };        /* 6 animals, no ino */
    static const int p_tanzaku4[]     = { 13, 17, 25, 42 };               /* 4 plain ribbons */
    static const int p_kasu9_cup[]    = { 2, 3, 6, 7, 10, 11, 14, 15, 18, 32 };      /* 9 plains + cup */
    static const int p_kasu10[]       = { 2, 3, 6, 7, 10, 11, 14, 15, 18, 19 };      /* 10 plains */
    static const int p_viewings[]     = { 8, 28, 32 };                    /* curtain + moon + cup */
    static const int p_sanko_rain[]   = { 0, 8, 40 };                     /* 3 brights but one is rain */
    static const int p_mixed[]        = { 0, 8, 28, 32, 20, 24, 36, 1, 5, 9, 2, 3 }; /* big mixed pile */
    /* proper 6-ribbon pile without any poetry/blue set: 1 + 4 plain ribbons + one more plain is impossible
       (only 4 plain ribbons exist), so use 2 poetry + 2 blue + 2 plain = no set complete */
    static const int p_tanzaku6_real[] = { 1, 5, 21, 33, 13, 17 };

    struct { const char *desc; const int *pile; int n; } rows[] = {
        { "Ino-shika-cho + 2 extra Animals (5 total)",        p_ino_extra,     5 },
        { "Akatan + 2 extra Ribbons (5 total)",               p_akatan_extra,  5 },
        { "Akatan + Aotan + 2 extra Ribbons (8 total)",       p_both_extra,    8 },
        { "6 Animals, no Ino-shika-cho",                      p_tane6,         6 },
        { "4 Ribbons, no poetry/blue set",                    p_tanzaku4,      4 },
        { "6 Ribbons, no poetry/blue set",                    p_tanzaku6_real, 6 },
        { "9 Plains + Sake Cup",                              p_kasu9_cup,    10 },
        { "10 Plains (no Sake Cup)",                          p_kasu10,       10 },
        { "Curtain + Moon + Sake Cup (both Viewings, no Sanko)", p_viewings,      3 },
        { "3 Brights but one is the Rain Man (no Sanko)",      p_sanko_rain,    3 },
        { "Mixed pile: 3 Brights + Cup + Ino-shika-cho + Akatan + 2 Plains", p_mixed, 12 },
    };

    puts("@@FRAGMENT stacking");
    puts("How counting yaku and special sets stack, per ruleset (same engine evaluation):");
    puts("");
    puts("| Capture pile | Cards | Nintendo | FudaWiki | KK-ADD-10 | Tournament | Yakuza |");
    puts("|---|---|---|---|---|---|---|");
    for (unsigned r = 0; r < sizeof(rows) / sizeof(rows[0]); r++) {
        printf("| %s | ", rows[r].desc);
        print_pile_imgs("../assets/cards/", rows[r].pile, rows[r].n, "card-tiny");
        printf(" | ");
        for (int rs = 0; rs < 5; rs++) {
            print_score_cell((Ruleset)rs, rows[r].pile, rows[r].n);
        }
        putchar('\n');
    }
    puts("");
    puts("*(a `--` = the pile scores nothing; `+` separates stacked yaku inside one cell)*");
    puts("@@END");
}

static void frag_award_grid(void) {
    static const int bases[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 15, 20 };
    puts("@@FRAGMENT award-grid");
    for (int opp = 0; opp <= 1; opp++) {
        printf("**Opponent Koi-Koi calls: %s**\n\n", opp ? "1 or more (x2)" : "none (x1)");
        puts("| Base points | no self Koi (x1) | 1 call (x2) | 2 calls (x3) | 3 calls (x4) | 4 calls (x5) | 5+ calls (x6) |");
        puts("|---:|---:|---:|---:|---:|---:|---:|");
        for (unsigned b = 0; b < sizeof(bases) / sizeof(bases[0]); b++) {
            printf("| %d |", bases[b]);
            for (int self = 0; self <= 5; self++) {
                int a = calculate_award(bases[b], self, opp);
                printf(" %d |", a);
            }
            putchar('\n');
        }
        puts("");
    }
    puts("*(computed by `calculate_award()` in `src/rules.c`; base <= 0 always awards 0)*");
    puts("@@END");
}

static void frag_ruleset_facts(void) {
    static const int p_goko[]       = { 0, 8, 28, 40, 44 };
    static const int p_shiko[]      = { 0, 8, 28, 44 };
    static const int p_ame[]        = { 0, 8, 28, 40 };
    static const int p_sanko[]      = { 0, 8, 28 };
    static const int p_hanami[]     = { 8, 32 };
    static const int p_tsukimi[]    = { 28, 32 };
    static const int p_kasu9_cup[]  = { 2, 3, 6, 7, 10, 11, 14, 15, 18, 32 };
    static const int p_ino_extra[]  = { 20, 24, 36, 4, 29 };
    static const int p_both6[]      = { 1, 5, 9, 21, 33, 37 };
    static const int p_tanzaku4[]   = { 13, 17, 25, 42 };
    static const int p_tanzaku5[]   = { 1, 13, 17, 25, 42 };

    puts("@@FRAGMENT ruleset-facts");
    puts("| Probe (capture pile) | Nintendo | FudaWiki | KK-ADD-10 | Tournament | Yakuza |");
    puts("|---|---:|---:|---:|---:|---:|");
    struct { const char *desc; const int *pile; int n; } rows[] = {
        { "Goko value (5 Brights)",                 p_goko,      5 },
        { "Shiko value (4 Brights, no Rain)",       p_shiko,     4 },
        { "Ame-shiko value (4 Brights w/ Rain)",    p_ame,       4 },
        { "Sanko value (3 Brights, no Rain)",       p_sanko,     3 },
    };
    for (unsigned r = 0; r < sizeof(rows) / sizeof(rows[0]); r++) {
        printf("| %s | ", rows[r].desc);
        for (int rs = 0; rs < 5; rs++) {
            ScoreResult sr = score_captures((Ruleset)rs, rows[r].pile, rows[r].n);
            printf("**%d**%s", sr.base_points, (rs < 4) ? " | " : " |\n");
        }
    }
    /* Viewings enabled? */
    printf("| Hanami-zake enabled (Curtain + Cup) | ");
    for (int rs = 0; rs < 5; rs++) {
        ScoreResult sr = score_captures((Ruleset)rs, p_hanami, 2);
        printf("%s%s", find_yaku_points(&sr, YAKU_HANAMI_ZAKE) >= 0 ? "yes" : "**no**",
               (rs < 4) ? " | " : " |\n");
    }
    printf("| Tsukimi-zake enabled (Moon + Cup) | ");
    for (int rs = 0; rs < 5; rs++) {
        ScoreResult sr = score_captures((Ruleset)rs, p_tsukimi, 2);
        printf("%s%s", find_yaku_points(&sr, YAKU_TSUKIMI_ZAKE) >= 0 ? "yes" : "**no**",
               (rs < 4) ? " | " : " |\n");
    }
    /* Sake cup as plain? */
    printf("| Sake Cup counts as a Plain for Kasu (9 Plains + Cup) | ");
    for (int rs = 0; rs < 5; rs++) {
        ScoreResult sr = score_captures((Ruleset)rs, p_kasu9_cup, 10);
        printf("%s%s", find_yaku_points(&sr, YAKU_KASU) >= 0 ? "yes" : "**no**",
               (rs < 4) ? " | " : " |\n");
    }
    /* Ino + Tane stacking */
    printf("| Ino-shika-cho and Tane score separately (Ino + 5 Animals) | ");
    for (int rs = 0; rs < 5; rs++) {
        ScoreResult sr = score_captures((Ruleset)rs, p_ino_extra, 5);
        int stacked = (find_yaku_points(&sr, YAKU_INO_SHIKA_CHO) >= 0 && find_yaku_points(&sr, YAKU_TANE) >= 0);
        printf("%s (total %d)%s", stacked ? "yes" : "**no, folded**", sr.base_points,
               (rs < 4) ? " | " : " |\n");
    }
    /* Akatan+Aotan combined set + tanzaku stacking */
    printf("| Akatan + Aotan as one 10-pt set (6 Ribbons) | ");
    for (int rs = 0; rs < 5; rs++) {
        ScoreResult sr = score_captures((Ruleset)rs, p_both6, 6);
        printf("total %d%s", sr.base_points, (rs < 4) ? " | " : " |\n");
    }
    /* Tanzaku threshold */
    printf("| Tanzaku window opens at 4 Ribbons (no sets) | ");
    for (int rs = 0; rs < 5; rs++) {
        ScoreResult sr4 = score_captures((Ruleset)rs, p_tanzaku4, 4);
        ScoreResult sr5 = score_captures((Ruleset)rs, p_tanzaku5, 5);
        int t4 = find_yaku_points(&sr4, YAKU_TANZAKU) >= 0;
        int t5 = find_yaku_points(&sr5, YAKU_TANZAKU) >= 0;
        if (t4)      printf("yes (4 = %d pt, 5 = %d pt)%s", sr4.base_points, sr5.base_points, (rs < 4) ? " | " : " |\n");
        else if (t5) printf("**no, needs 5** (5 = %d pt)%s", sr5.base_points, (rs < 4) ? " | " : " |\n");
        else         printf("no%s", (rs < 4) ? " | " : " |\n");
    }
    puts("");
    puts("*(all cells evaluated live from `score_captures()` in `src/rules.c`)*");
    puts("@@END");
}

/* Which atlas an opponent's art comes from: Entity 13 draws from assets/atlas_deus.png, the
   same choice board_draw_card() makes in src/ui_board.c. Returns the subdirectory under
   docs/assets/cards/, empty for the twelve months. Keeps the roster, the difficulty chart and
   the image files agreeing on whose art is whose. */
static const char *card_subdir_for_opponent(int i) {
    return (i == CAMPAIGN_OPPONENT_COUNT) ? "deus/" : "";
}

static void frag_opponents(void) {
    puts("@@FRAGMENT opponents");
    puts("| Opponent | Unit classification | Favorite card | Focus |");
    puts("|---|---|---|---|");
    for (int i = 0; i <= 12; i++) {
        const Opponent *o = campaign_get_opponent(i);
        const Card *fc = &CARDS[o->favorite_card_id];
        if (i == CAMPAIGN_OPPONENT_COUNT)
            printf("| **%s** ([dossier](deus-supremus.md)) | %s | ", o->name, o->title);
        else
            printf("| **%s** | %s | ", o->name, o->title);
        printf("![%s](../assets/cards/%scard-%02d.webp){ .card-mini }", fc->name,
               card_subdir_for_opponent(i), o->favorite_card_id);
        printf(" %s (id %d) | %s |\n", fc->name, o->favorite_card_id, o->favorite_combo);
    }
    puts("");

    puts("@@END");
}

/* ---------------------------------------------------------------------------
 * The tutorial's worked round.
 *
 * One round, played out step by step, so docs/tutorial.md can show a hand instead of
 * describing one. Nothing here is typed in: the turn list is checked against the real
 * CARDS[] table and then fed through the same score_captures() and calculate_award()
 * the game calls, so a page cannot claim points the engine would not pay.
 *
 * The deal is chosen so that one sequence teaches all three capture rules and ends on
 * the only decision that matters -- bank it or push. Field months are 1,1,2,3,4,5,6,8:
 * one pair and no four-of-a-kind, which is legal, because the deal the engine accepts is
 * the one where has_four_of_month_or_four_pairs() (src/engine.c:36-44) says no. The same
 * mirror of that predicate below refuses to generate the page if a future CARDS[] change
 * turns this into a misdeal, an illegal hand, or a capture that never happened.
 */
/* A turn, as it would be played: one card from the hand, what it took, the card the stock
   turned over, what that took. `choice` marks a turn where the field held two matches, so the
   player had to pick -- the one thing of Koi-Koi's that a paragraph cannot teach. */
typedef struct {
    int played;
    int took[2];            /* field cards taken by the played card, -1 padding */
    int stock;              /* the top card of the stock                         */
    int stock_took[2];      /* what the stock card then matched on its own       */
    int choice;
} TutTurn;

static const int TUT_FIELD[8] = { 0, 1, 5, 10, 13, 17, 20, 28 };
static const int TUT_HAND[8]  = { 3, 4, 8, 12, 16, 29, 32, 40 };

/* turn 1  03-B Curtain takes 03-P1; the stock's 07-A Boar finds no July and stays face-up
   turn 2  08-A Geese takes 08-B Moon; the stock's 06-RB Blue does find 06-A Butterflies, so
           it captures without being asked -- you never choose the stock card, and it never
           comes into your hand
   turn 3  01-P2 Plain matches two January cards, so you pick one. Both picks are scored below. */
static const TutTurn TUT_TURNS[3] = {
    { 8,  { 10, -1 }, 24, { -1, -1 }, 0 },
    { 29, { 28, -1 }, 21, { 20, -1 }, 0 },
    { 3,  { 0,  -1 }, 31, { -1, -1 }, 1 },
};
static const int TUT_TAKE_THE_OTHER = 1;    /* turn 3: take the 01-RP Poetry ribbon instead */

/* engine.c:36-44, mirrored so the guard can run while the docs are generated. */
static int tut_is_void_deal(const int *cards, int n) {
    if (n != 8) return 0;
    int mc[13] = {0}, pairs = 0;
    for (int i = 0; i < 8; i++) mc[CARDS[cards[i]].month]++;
    for (int m = 1; m <= 12; m++) {
        if (mc[m] == 4) return 1;
        if (mc[m] == 2) pairs++;
    }
    return pairs == 4;
}

static int tut_in(const int *set, int n, int id) {
    for (int i = 0; i < n; i++) if (set[i] == id) return 1;
    return 0;
}

static void tut_fail(int turn, const char *what) {
    fprintf(stderr, "tutorial: turn %d %s - the example no longer matches the engine. Fix the "
                    "ids in tools/wiki_tables.c rather than the page.\n", turn, what);
    exit(1);
}

/* How many cards a played card may take: one match takes it, three take all three, two take
   exactly one. That is the rule, so the example is checked against it rather than described. */
static int tut_takes_allowed(int matches) { return matches == 2 ? 1 : matches; }

static int tut_count_matches(const int *field, int nf, int card) {
    int n = 0;
    for (int f = 0; f < nf; f++) if (CARDS[field[f]].month == CARDS[card].month) n++;
    return n;
}

static void tut_remove(int *field, int *nf, int id) {
    for (int f = 0; f < *nf; f++)
        if (field[f] == id) { field[f] = field[*nf - 1]; (*nf)--; return; }
}

static void tut_validate(void) {
    if (tut_is_void_deal(TUT_FIELD, 8)) {
        fprintf(stderr, "tutorial: the example field is a misdeal\n");
        exit(1);
    }
    if (tut_is_void_deal(TUT_HAND, 8)) {
        fprintf(stderr, "tutorial: the example hand is a lucky hand (Teshi or Kuttsuki)\n");
        exit(1);
    }
    int used[48] = {0};
    for (int i = 0; i < 8; i++) {
        if (used[TUT_FIELD[i]] || used[TUT_HAND[i]]) {
            fprintf(stderr, "tutorial: a card appears twice in the example deal\n");
            exit(1);
        }
        used[TUT_FIELD[i]] = 1;
        used[TUT_HAND[i]] = 1;
    }

    int field[8], nf = 8;
    memcpy(field, TUT_FIELD, sizeof field);
    for (int t = 0; t < 3; t++) {
        int played = TUT_TURNS[t].played;
        int took = 0, matches;
        for (int k = 0; k < 2; k++) if (TUT_TURNS[t].took[k] >= 0) took++;
        if (!tut_in(TUT_HAND, 8, played)) tut_fail(t + 1, "plays a card that is not in the hand");
        matches = tut_count_matches(field, nf, played);
        if (took != tut_takes_allowed(matches)) {
            char m[96];
            snprintf(m, sizeof m, "takes %d card(s) when the field has %d match(es) of month %d "
                                  "(allowed: %d)", took, matches, CARDS[played].month,
                     tut_takes_allowed(matches));
            tut_fail(t + 1, m);
        }
        if (TUT_TURNS[t].choice && matches != 2) tut_fail(t + 1, "was meant to offer a choice");
        for (int k = 0; k < 2; k++) {
            int id = TUT_TURNS[t].took[k];
            if (id < 0) continue;
            if (!tut_in(field, nf, id)) tut_fail(t + 1, "captures a card that is not on the field");
            if (CARDS[id].month != CARDS[played].month) tut_fail(t + 1, "captures a different month");
            tut_remove(field, &nf, id);
        }
        used[played] = 1;
        if (took == 0) field[nf++] = played;           /* no match: it stays face-up */

        int stock = TUT_TURNS[t].stock;
        int stok = 0;
        for (int k = 0; k < 2; k++) if (TUT_TURNS[t].stock_took[k] >= 0) stok++;
        if (used[stock]) tut_fail(t + 1, "turns over a card that is already somewhere else");
        used[stock] = 1;
        matches = tut_count_matches(field, nf, stock);
        if (stok != tut_takes_allowed(matches)) {
            char m[96];
            snprintf(m, sizeof m, "stock card takes %d but the field has %d match(es)", stok, matches);
            tut_fail(t + 1, m);
        }
        for (int k = 0; k < 2; k++) {
            int id = TUT_TURNS[t].stock_took[k];
            if (id < 0) continue;
            if (CARDS[id].month != CARDS[stock].month) tut_fail(t + 1, "stock capture spans months");
            tut_remove(field, &nf, id);
        }
        if (stok == 0) field[nf++] = stock;
    }
}

static void tut_pile_cells(const int *pile, int n) {
    for (int i = 0; i < n; i++) {
        printf("![%s](assets/cards/card-%02d.webp){ .card-mini }", CARDS[pile[i]].name, pile[i]);
        if (i + 1 < n) printf(" ");
    }
}

/* Prints the yaku the engine found and hands back the base points it scored them at. */
static int tut_verdict(Ruleset rs, const int *pile, int n) {
    ScoreResult sr = score_captures(rs, pile, n);
    if (sr.yaku_count == 0) {
        printf("*nothing to score yet* = **0**");
        return 0;
    }
    for (int i = 0; i < sr.yaku_count; i++)
        printf("%s %d%s", yaku_name_of(sr.yakus[i].id), sr.yakus[i].points,
               (i + 1 < sr.yaku_count) ? " + " : " ");
    printf("= **%d**", sr.base_points);
    return sr.base_points;
}

/* The cards a player holds after `turns` turns. last_take overrides which of the two January
   cards turn 3 keeps, so the other choice is scored from the same source of truth. */
static int tut_build(int turns, int last_take, int *out) {
    int n = 0;
    for (int t = 0; t < turns; t++) {
        out[n++] = TUT_TURNS[t].played;
        int take = (t == turns - 1 && last_take >= 0) ? last_take : TUT_TURNS[t].took[0];
        if (take >= 0) out[n++] = take;
        for (int k = 0; k < 2; k++)
            if (TUT_TURNS[t].stock_took[k] >= 0) out[n++] = TUT_TURNS[t].stock_took[k];
    }
    return n;
}

static void tut_row(const char *when, int turns, int last_take) {
    int pile[24], n = tut_build(turns, last_take, pile);
    printf("| %s | ", when);
    tut_pile_cells(pile, n);
    printf(" | ");
    int base = tut_verdict(RULESET_KK_ADD_10, pile, n);
    printf(" | %d | %d | %d |\n", calculate_award(base, 0, 0), calculate_award(base, 1, 0),
           calculate_award(base, 0, 1));
}

static void frag_tutorial_worked(void) {
    tut_validate();
    puts("@@FRAGMENT tutorial-worked");
    puts("| After | You have captured | What `score_captures()` makes of it | Bank it now | "
         "You called Koi-Koi once | He called it, you won |");
    puts("|:--|:--|:--|--:|--:|--:|");
    tut_row("**turn 1**", 1, -1);
    tut_row("**turn 2**", 2, -1);
    tut_row("**turn 3**, you take the Crane", 3, -1);
    tut_row("**turn 3**, you take the ribbon", 3, TUT_TAKE_THE_OTHER);
    puts("");
    puts("*Base points and every award above come from this build's `score_captures()` and "
         "`calculate_award()` in `src/rules.c`, on the card ids in `src/cards.c`, under "
         "KK-ADD-10 -- the ruleset the game defaults to (`src/storage.c:35`). Nothing in this "
         "table is typed: the generator stops with an error if a capture in the example stops "
         "matching by month, if the deal turns into a misdeal, or if the third turn stops "
         "offering a real choice.*");
    puts("@@END");
}

/* Card names and their atlas cells, for the DEUS SUPREMUS study grid. Data only - the generator
   places no part of it on a page, it just must not guess where a card sits. Cell mapping is
   board_draw_card()'s: column id >> 2 (month - 1), row id & 3 (slot). */
static void frag_card_cells(void) {
    puts("@@FRAGMENT card-cells");
    for (int id = 0; id < 48; id++)
        printf("%d | %s | %d | %d\n", id, CARDS[id].name, id >> 2, id & 3);
    puts("@@END");
}

/* His dossier table is generated from the roster; the wiki's narrative voice is maintained
   separately in the page prose. */
static void frag_deus_dossier(void) {
    const Opponent *o = campaign_get_opponent(CAMPAIGN_OPPONENT_COUNT);
    const Card *fc = &CARDS[o->favorite_card_id];
    puts("@@FRAGMENT deus-dossier");
    puts("| | DEUS SUPREMUS |");
    puts("|---|---|");
    printf("| **Designation** | %s |\n", o->name);
    printf("| **Classification** | %s |\n", o->title);
    printf("| **Priority yaku** | %s |\n", o->favorite_combo);
    printf("| **Signature target** | ![%s](../assets/cards/%scard-%02d.webp){ .card-mini } "
           "%s (id %d) |\n", fc->name, card_subdir_for_opponent(CAMPAIGN_OPPONENT_COUNT),
           o->favorite_card_id, fc->name, o->favorite_card_id);
    printf("| **Match taunt** | *%s* |\n", o->taunt_text);
    printf("| **On a Koi-Koi call** | *%s* |\n", o->koi_taunt);
    puts("@@END");
}

/* Data for the difficulty chart in gen_wiki_tables.py: which card belongs to which
   opponent. Kept here so the chart cannot drift from the roster it draws. */
static void frag_roster_meta(void) {
    puts("@@FRAGMENT roster-meta");
    for (int i = 0; i <= 12; i++) {
        const Opponent *o = campaign_get_opponent(i);
        printf("%d | %s | %d | %s | %s | %s\n", i, o->name, o->favorite_card_id,
               CARDS[o->favorite_card_id].name, o->favorite_combo, card_subdir_for_opponent(i));
    }
    puts("@@END");
}

int main(void) {
    if (!cards_validate()) {
        fprintf(stderr, "cards_validate() failed - refusing to generate tables\n");
        return 1;
    }
    /* Consistency: fast path must agree with the full scorer for every ruleset. */
    {
        int probe[] = { 0, 8, 28, 32, 20, 24, 36, 1, 5, 9, 2, 3, 13, 17, 43 };
        for (int rs = 0; rs < 5; rs++) {
            int a = score_captures_base((Ruleset)rs, probe, 15);
            int b = score_captures((Ruleset)rs, probe, 15).base_points;
            if (a != b) {
                fprintf(stderr, "score path mismatch in ruleset %d: %d vs %d\n", rs, a, b);
                return 1;
            }
        }
    }
    frag_cards();
    frag_yaku_values();
    frag_stacking();
    frag_award_grid();
    frag_ruleset_facts();
    frag_opponents();
    frag_deus_dossier();
    frag_card_cells();
    frag_roster_meta();
    frag_tutorial_worked();
    return 0;
}
