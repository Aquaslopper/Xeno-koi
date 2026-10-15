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
    puts("| # | Opponent | Unit classification | Favorite card | Focus |");
    puts("|---:|---|---|---|---|");
    for (int i = 0; i <= 12; i++) {
        const Opponent *o = campaign_get_opponent(i);
        const Card *fc = &CARDS[o->favorite_card_id];
        if (i == CAMPAIGN_OPPONENT_COUNT)
            printf("| %d | **%s** ([dossier](deus-supremus.md)) | %s | ", i, o->name, o->title);
        else
            printf("| %d | **%s** | %s | ", i, o->name, o->title);
        printf("![%s](../assets/cards/%scard-%02d.webp){ .card-mini }", fc->name,
               card_subdir_for_opponent(i), o->favorite_card_id);
        printf(" %s (id %d) | %s |\n", fc->name, o->favorite_card_id, o->favorite_combo);
    }
    puts("");
    puts("Taunt lines:");
    puts("");
    puts("| # | Match taunt | Koi-Koi call |");
    puts("|---:|---|---|");
    for (int i = 0; i <= 12; i++) {
        const Opponent *o = campaign_get_opponent(i);
        printf("| %d | *%s* | *%s* |\n", i, o->taunt_text, o->koi_taunt);
    }
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

/* His own dossier, quoted from the roster instead of paraphrased: while the mode is being tuned,
   the wiki should show what the game actually says. */
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
    puts("");
    printf("> %s\n", o->lore_text);
    puts("");
    puts("*(generated from `g_opponents[12]` in `src/cards.c` - his text, not a paraphrase)*");
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
    return 0;
}
