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

extern const Opponent g_opponents[CAMPAIGN_OPPONENT_COUNT + 1];

static inline const Opponent* campaign_get_opponent(int index) {
    if (index == 12) return &g_opponents[12];
    if ((unsigned)index >= (unsigned)CAMPAIGN_OPPONENT_COUNT) return &g_opponents[0];
    return &g_opponents[index];
}

static inline const Opponent* campaign_get_all(void) {
    return g_opponents;
}

#if defined(__has_include)
#if __has_include("raylib.h")
#include "raylib.h"
#include "storage.h"
#include "ui_board.h"
void campaign_draw_overlay(const BoardAssets *ba, const GameSettings *settings, int selected_opponent, Vector2 vm);
int campaign_handle_click(GameSettings *settings, int *out_selected, Vector2 vm);
int campaign_update_unlocks(GameSettings *settings);
#endif
#endif

#endif /* CAMPAIGN_H */
