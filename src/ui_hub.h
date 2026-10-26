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
    HUB_ACT_BACK_TO_MAIN_HUB,
    HUB_ACT_TITLE_START_GAME,
    HUB_ACT_TITLE_OPTIONS,
    HUB_ACT_TITLE_SAVE,
    HUB_ACT_TITLE_CREDITS,
    HUB_ACT_TITLE_QUIT,
    HUB_ACT_DEUS_ACTIVATE,
    HUB_ACT_DEUS_DEACTIVATE
} HubAction;

typedef struct {
    Rectangle   rect;
    const char *label;
    int         font_size;
} HubButtonHit;

/* Pure readout helpers over GameSettings (sectors 1..12, coins over bits 0..35) */
int ui_hub_unlocked_sectors(const GameSettings *s);
int ui_hub_count_coins(const GameSettings *s);

/* Stage 4.1: Deus ON remains derived from settings; snapshot pre-activation fields. */
void ui_hub_deus_snapshot_capture(const GameSettings *s);
void ui_hub_deus_snapshot_invalidate(void);
int  ui_hub_deus_snapshot_is_valid(void);
const StorageDeusSessionSnapshot *ui_hub_deus_snapshot(void);

/* Title Screen (SCREEN_START -- 5-button cutover; saved_timer > 0 shows save notice) */
void      ui_hub_draw_title(const BoardAssets *ba, Vector2 vm, float saved_timer);
HubAction ui_hub_handle_title_click(Vector2 vm, HubButtonHit *out_hit);

/* Main Game Hub (SCREEN_HUB_MAIN):
   spine_count in 0..48 displays live Spine progress (+ NEW PLAYERS hint at 0/48);
   pass < 0 when Spine count is omitted. */
void      ui_hub_draw_main(const BoardAssets *ba, const GameSettings *s, int spine_count, Vector2 vm);
HubAction ui_hub_handle_main_click(Vector2 vm, HubButtonHit *out_hit);

/* Campaign Hub (SCREEN_HUB_CAMPAIGN):
   Deus row is read-only and derived strictly from s->difficulty == DIFFICULTY_DEUS. */
void      ui_hub_draw_campaign(const BoardAssets *ba, const GameSettings *s, Vector2 vm);
HubAction ui_hub_handle_campaign_click(const GameSettings *s, Vector2 vm, HubButtonHit *out_hit);

#endif /* UI_HUB_H */
