#ifndef UI_BOARD_H
#define UI_BOARD_H

#include "raylib.h"
#include "engine.h"
#include "ui_theme.h"
#include "ui_anim.h"

typedef struct {
    Texture2D atlas;
    Texture2D atlas_deus;
    Texture2D card_back;
    Texture2D coin_gold;
    Texture2D coin_silver;
    Texture2D coin_bronze;
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

/* Geometry helpers */
Rectangle board_get_hand_card_rect(int index, int total_cards, int hovered);
Rectangle board_get_field_card_rect(int index, int total_cards);
Rectangle board_get_field_card_rect_of(const GameState *g, int card_id);
Rectangle board_get_field_rect(void);
Rectangle board_get_stock_rect(void);
Vector2 board_get_card_field_pos(const GameState *g, int card_id);

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

/* Action-card audio effect telemetry getters and persistence hook for Atrium Plex Logfile */
int   board_get_audio_action_triggers(void);
float board_get_audio_peak_synergy(void);
float board_get_audio_full_hold_seconds(void);
void  board_set_audio_telemetry(int triggers, float peak, float full_hold_sec);
void  board_set_action_card_mode(int active, float music_level);
void  board_reset_action_card_match(void);
int   board_get_action_ceiling_count(void);

/* Global clean font for high-readability UI details */
extern Font small_font;

#endif /* UI_BOARD_H */
