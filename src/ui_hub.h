#ifndef UI_HUB_H
#define UI_HUB_H

#include "raylib.h"
#include "storage.h"
#include "ui_board.h"

typedef enum {
    HUB_ACT_NONE = 0,
    HUB_ACT_GOTO_CAMPAIGN_HUB,
    HUB_ACT_GOTO_ATRIUM,
    HUB_ACT_BACK_TO_TITLE,
    HUB_ACT_OPEN_CAMPAIGN_OVERLAY,
    HUB_ACT_GOTO_STATS,
    HUB_ACT_BACK_TO_MAIN_HUB
} HubAction;

typedef struct {
    Rectangle   rect;
    const char *label;
    int         font_size;
} HubButtonHit;

/* Pure readout helpers over GameSettings (sectors 1..12, coins over bits 0..35) */
int ui_hub_unlocked_sectors(const GameSettings *s);
int ui_hub_count_coins(const GameSettings *s);

/* Main Game Hub (SCREEN_HUB_MAIN):
   spine_count in 0..48 displays live Spine progress (+ NEW PLAYERS hint at 0/48);
   pass < 0 when Spine count is omitted. */
void      ui_hub_draw_main(const BoardAssets *ba, const GameSettings *s, int spine_count, Vector2 vm);
HubAction ui_hub_handle_main_click(Vector2 vm, HubButtonHit *out_hit);

/* Campaign Hub (SCREEN_HUB_CAMPAIGN):
   Deus row is read-only and derived strictly from s->difficulty == DIFFICULTY_DEUS. */
void      ui_hub_draw_campaign(const BoardAssets *ba, const GameSettings *s, Vector2 vm);
HubAction ui_hub_handle_campaign_click(Vector2 vm, HubButtonHit *out_hit);

#endif /* UI_HUB_H */
