#ifndef UI_MOTION_H
#define UI_MOTION_H

#include "raylib.h"
#include "engine.h"

typedef struct {
    Vector2 current_pos;
    Vector2 target_pos;
    Vector2 start_pos;
    Vector2 prev_pos;
    Vector2 vel;
    Vector2 current_size;
    Vector2 target_size;
    Vector2 start_size;
    float elapsed;
    float duration;
    float tint_val;
    float shimmer_alpha;
    int moving;
    int is_dragged;
    int initialized;
} CardMotion;

/* Initialize or reset card motions */
void motion_init(void);

/* Set instantaneous position/size without animating */
void motion_set_instant(int card_id, Vector2 pos, Vector2 size);

/* Set target position/size; if position or size changes, smoothly lerps over duration (~300ms) */
void motion_set_target(int card_id, Vector2 pos, Vector2 size, float duration);

/* Access current interpolated bounds */
Rectangle motion_get_rect(int card_id);
Vector2 motion_get_pos(int card_id);

/* Check if a card is currently moving */
int motion_is_card_moving(int card_id);
Vector2 motion_get_velocity(int card_id);
Color motion_get_card_tint(int card_id);
float motion_get_card_shimmer(int card_id);
void motion_set_drag_velocity(int card_id, Vector2 mouse_delta);

/* Get physical lift factor (0.0 .. 1.0) for flying card drop shadows */
float motion_get_card_lift(int card_id);

/* Check if any field, hand, or captured card is currently reflowing/animating */
int motion_is_reflowing(const GameState *g);

/* Advance all card lerps */
void motion_update(float dt, int dragged_card_id);

/* Deal cards smoothly from stock pile to hands and field at round start */
void motion_deal_round(const GameState *g, float duration);

/* --- MATCH START / ROUND START Cinematic --- */
void cinematic_start(int round_num);
void cinematic_start_countdown(int wave_num);
float cinematic_countdown_elapsed(void);
void cinematic_update(float dt);
int cinematic_is_active(void);
float cinematic_get_alpha(void);
void cinematic_draw(void);

#endif /* UI_MOTION_H */
