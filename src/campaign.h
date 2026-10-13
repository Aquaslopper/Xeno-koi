#ifndef CAMPAIGN_H
#define CAMPAIGN_H

#include "rules.h"

#define CAMPAIGN_OPPONENT_COUNT 12

typedef struct {
    int id;                       /* 0..11 */
    const char *name;             /* e.g. "01: JANUARY CRANE" */
    const char *title;            /* Subtitle / Unit classification */
    const char *lore_text;        /* Lore backstory */
    int favorite_card_id;         /* 0..47 */
    int favorite_yaku;            /* YAKU_* */
    const char *favorite_combo;   /* e.g. "Brights" */
    const char *taunt_text;       /* CPU Taunt */
    const char *koi_taunt;        /* CPU Koi-Koi call taunt */
} Opponent;

static const Opponent s_opponents[CAMPAIGN_OPPONENT_COUNT] = {
    {
        0, "01: JANUARY CRANE", "Obsidian Sentinel // Sector 01",
        "Ancient biomechanical sentinel encased in obsidian plating. Hunts divine Brights with relentless precision.",
        0, YAKU_GOKO, "Brights",
        "THE CRANE SEES ALL FUTURES.",
        "SOLAR RESERVES PEAKING. KOI-KOI!"
    },
    {
        1, "02: FEB. WARBLER", "Avian Construct // Sector 02",
        "An elegant but deadly avian construct. Obsessed with collecting ribbons to weave its synthetic nests.",
        4, YAKU_TANZAKU, "Ribbons",
        "A BEAUTIFUL, SYMMETRICAL DEATH.",
        "THE WEAVE EXPANDS. KOI-KOI!"
    },
    {
        2, "03: MARCH CURTAIN", "Sensory Overload Unit // Sector 03",
        "A sensory-overload unit. It feeds on the high-energy output of cherry blossoms and neon light.",
        8, YAKU_GOKO, "Brights",
        "REVEL IN THE NEON BLOSSOMS.",
        "ENERGY SURGING. KOI-KOI!"
    },
    {
        3, "04: APRIL CUCKOO", "Parasitic Vine-Mind // Sector 04",
        "A parasitic vine-mind. It suffocates its targets by hoarding low-yield plains to drain their resources.",
        12, YAKU_KASU, "Plains/Kasu",
        "DEATH BY A THOUSAND CUTS.",
        "STRANGLING ALL PROTOCOLS. KOI-KOI!"
    },
    {
        4, "05: MAY BRIDGE", "Structural Intelligence // Sector 05",
        "A structural intelligence spanning the gap between organic and synthetic lifeforms.",
        16, YAKU_TANE, "Animals",
        "CROSS THE VOID.",
        "SPANNING THE ABYSS. KOI-KOI!"
    },
    {
        5, "06: JUNE SWARM", "Hive-Mind Vector // Sector 06",
        "A hive-mind of synthetic insects. Highly erratic, aggressive, and impossible to predict.",
        20, YAKU_TANE, "Animals",
        "THE SWARM CONSUMES.",
        "THE HIVE MULTIPLIES. KOI-KOI!"
    },
    {
        6, "07: JULY BOAR", "Heavy-Armor Unit // Sector 07",
        "A heavy-armor unit designed to trample optimal strategies with sheer, unapologetic force.",
        24, YAKU_INO_SHIKA_CHO, "Ino-Shika-Cho",
        "BRUTE FORCE PROTOCOL ENGAGED.",
        "TRAMPLING ALL DEFENSES. KOI-KOI!"
    },
    {
        7, "08: AUGUST MOON", "Orbital Observer // Sector 08",
        "An orbital observer. It manipulates gravity and logic to pull the highest value targets into its orbit.",
        28, YAKU_GOKO, "Brights",
        "ECLIPSE IMMINENT.",
        "ORBITAL ALIGNMENT COMPLETE. KOI-KOI!"
    },
    {
        8, "09: SEPT. SAKE", "Chemical Processing Unit // Sector 09",
        "A chemical processing unit. It thrives on erratic multipliers and fluid dynamics.",
        32, YAKU_TANE, "Animals",
        "INTOXICATING ALGORITHMS.",
        "FERMENTING SYNAPSE. KOI-KOI!"
    },
    {
        9, "10: OCT. DEER", "Sensory-Stealth Unit // Sector 10",
        "A sensory-stealth unit. It isolates specific targets within the brush with terrifying accuracy.",
        36, YAKU_INO_SHIKA_CHO, "Ino-Shika-Cho",
        "PRECISION STRIKE.",
        "TARGET LOCK MAINTAINED. KOI-KOI!"
    },
    {
        10, "11: NOV. RAIN", "Atmospheric Anomaly // Sector 11",
        "An unpredictable atmospheric anomaly. It disrupts normal logic flows and thrives in chaos.",
        40, YAKU_TANZAKU, "Ribbons",
        "THE STORM WASHES ALL AWAY.",
        "DELUGE IMMINENT. KOI-KOI!"
    },
    {
        11, "12: DEC. PHOENIX", "The Apex Biomech // Sector 12",
        "The Apex Biomech. An immortal processor that endlessly resurrects its strategies from the ashes.",
        44, YAKU_GOKO, "Brights",
        "ASH TO ASH. DUST TO DUST.",
        "REBORN FROM CRUCIBLE. KOI-KOI!"
    }
};

static inline const Opponent* campaign_get_opponent(int index) {
    if (index < 0 || index >= CAMPAIGN_OPPONENT_COUNT) return &s_opponents[0];
    return &s_opponents[index];
}

static inline const Opponent* campaign_get_all(void) {
    return s_opponents;
}

#if defined(__has_include)
#if __has_include("raylib.h")
#include "raylib.h"
#include "storage.h"
#include "ui_board.h"
void campaign_draw_overlay(const BoardAssets *ba, const GameSettings *settings, int selected_opponent, Vector2 vm);
int campaign_handle_click(GameSettings *settings, int *out_selected, Vector2 vm);
#endif
#endif

#endif /* CAMPAIGN_H */
