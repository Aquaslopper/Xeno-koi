#ifndef UI_BOARD_H
#define UI_BOARD_H

#include "raylib.h"
#include "engine.h"
#include "ui_theme.h"
#include "ui_anim.h"

/* One forged card-table skin = one folder in assets/maps/<name>/ holding the static
   casing (overlay.png, 1280x720, drawn over the dimmed backdrop and under every live
   element) and the one card frame (bezel.png, transparent inside, drawn under and over
   every card). Slot = the TABLE_DESIGN_* id in storage.h; slot 0 is the CRT table, which
   has no files. Every slot loads its OWN pair, so editing one map's overlay.png changes
   that map only. */
#define BOARD_TABLE_SLOTS 8

typedef struct {
    Texture2D overlay;
    Texture2D bezel;
    int ready;          /* both files loaded */
} TableSkin;

typedef struct {
    Texture2D atlas;
    Texture2D atlas_deus;
    Texture2D card_back;
    Texture2D coin_gold;
    Texture2D coin_silver;
    Texture2D coin_bronze;
    TableSkin table_skins[BOARD_TABLE_SLOTS];   /* assets/maps/<name>/, indexed by TABLE_DESIGN_* */
    Texture2D bgs[8];
    Texture2D bg_normals[8];
    int current_bg;
    int assets_loaded;
    Shader fog_shader;
    int fog_time_loc;
    int light_normal_loc;
    int light_enabled_loc;
    int light_flash_loc;
    int has_fog_shader;
} BoardAssets;

/* Load / unload graphical assets */
void board_load_assets(BoardAssets *ba);
void board_unload_assets(BoardAssets *ba);

/* True when a forged table skin (any TABLE_DESIGN_* except CRT) is selected AND its
   textures loaded. Gates the CRT panel fills/hairlines so the casing shows through
   (tables/table_<name>/). */
int board_table_skin_active(void);

/* Mute the card bezel for presentation draws that are not table cards (the
   capture-info hover preview): board_set_table_bezel_muted(1), draw, then (0). */
void board_set_table_bezel_muted(int muted);

/* Geometry helpers */
Rectangle board_get_hand_card_rect(int index, int total_cards, int hovered);
Rectangle board_get_field_card_rect(int index, int total_cards);
Rectangle board_get_field_card_rect_of(const GameState *g, int card_id);
Rectangle board_get_field_rect(void);
Rectangle board_get_stock_rect(void);
Vector2 board_get_card_field_pos(const GameState *g, int card_id);
void board_reset_field_slots(const GameState *g);
void board_hint_field_drop(int card_id, Vector2 drop_center);

/* Drawing primitives */
void board_draw_card(const BoardAssets *ba, int card_id, Rectangle dest);
void board_draw_card_back(const BoardAssets *ba, Rectangle dest);
void board_draw_card_shimmer(Rectangle dest, float speed);
void board_draw_card_glow(Rectangle dest, float pulse_alpha);
void board_draw_card_shadow(Rectangle dest, float lift_amount);

/* Full table drawing */
void board_draw_table(const BoardAssets *ba, const GameState *g, const AnimState *anim,
                      int hovered_hand_idx, int dragging_idx, Vector2 drag_pos,
                      int easy_mode, int replay_mode, const char *status_msg);

/* Pre-match Oya-gime dealer selection */
void board_draw_dealer_select(const BoardAssets *ba, const GameState *g, Vector2 vm);
int board_check_dealer_select_click(const GameState *g, Vector2 vm);

/* Audio FX telemetry getters and thin Action Card input/blindness mirror */
int   board_get_audio_action_triggers(void);
float board_get_audio_peak_synergy(void);
float board_get_audio_full_hold_seconds(void);
void  board_set_audio_telemetry(int triggers, float peak, float full_hold_sec);
void  board_sync_action_card_mask(uint64_t blind_mask, int distraction_high);
int   board_is_blind_field_card(int card_id);
int   board_is_action_distraction_high(void);

/* Global clean font for high-readability UI details */
extern Font small_font;

#endif /* UI_BOARD_H */
