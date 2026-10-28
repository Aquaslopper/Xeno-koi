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
    HUB_ACT_DEUS_DEACTIVATE,
    HUB_ACT_OPTIONS_SYSTEM,
    HUB_ACT_OPTIONS_PROFILES,
    HUB_ACT_OPTIONS_CREDITS,
    HUB_ACT_OPTIONS_BACK,
    HUB_ACT_PROFILE_SLOT_SELECTED,
    HUB_ACT_PROFILE_CREATE,
    HUB_ACT_PROFILE_LOAD,
    HUB_ACT_PROFILE_ERASE,
    HUB_ACT_PROFILE_SAVE,
    HUB_ACT_PROFILE_BACK
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

/* Title Screen: active profiles see CONTINUE; new/legacy installs see START GAME. */
void      ui_hub_draw_title(const BoardAssets *ba, Vector2 vm, float saved_timer, int has_active_profile, float music_level);
HubAction ui_hub_handle_title_click(Vector2 vm, HubButtonHit *out_hit, int has_active_profile);

/* Options landing menu and 3-slot profile manager. Draw functions are side-effect free. */
void      ui_hub_draw_options_menu(const BoardAssets *ba, Vector2 vm, float saved_timer);
HubAction ui_hub_handle_options_menu_click(Vector2 vm, HubButtonHit *out_hit);
void      ui_hub_draw_profile_manager(const BoardAssets *ba, const int slots[3], int active_slot,
                                      int selected_slot, Vector2 vm, float saved_timer, int error);
HubAction ui_hub_handle_profile_manager_click(Vector2 vm, HubButtonHit *out_hit, int *out_slot,
                                                const int slots[3], int active_slot);
void      ui_hub_profile_manager_reset(int selected_slot);
int       ui_hub_profile_manager_selected_slot(void);

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
