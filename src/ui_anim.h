#ifndef UI_ANIM_H
#define UI_ANIM_H

#include "raylib.h"

typedef enum {
    ANIM_NONE = 0,
    ANIM_GLIDE_RETURN,    /* Far drop glide back to hand */
    ANIM_STOCK_FLIP,      /* Stock card reveal flip */
    ANIM_CPU_THINK,       /* CPU thinking delay */
    ANIM_CPU_FLY,         /* CPU card moving to table */
    ANIM_STOCK_FLY,       /* Stock card flying from deck to table */
    ANIM_CAPTURE_FLY,     /* Cards flying to capture pile */
    ANIM_BANNER           /* Temporary notice (e.g. CPU Koi-Koi) */
} AnimType;

typedef struct {
    AnimType type;
    int active;
    float elapsed;
    float duration;
    int card_id;
    Vector2 start_pos;
    Vector2 current_pos;
    Vector2 target_pos;
    char banner_text[64];

    /* CPU play info */
    int cpu_card;
    int cpu_target;

    /* Capture fly info */
    int capture_player;
    int capture_count;
    int capture_cards[4];
    Vector2 capture_starts[4];
    Vector2 capture_currents[4];
    Vector2 capture_target;
} AnimState;

void anim_init(AnimState *a);
void anim_start_glide(AnimState *a, int card_id, Vector2 from, Vector2 to, float duration);
void anim_start_stock_flip(AnimState *a, int card_id, float duration);
void anim_start_cpu_think(AnimState *a, float duration);
void anim_start_cpu_fly(AnimState *a, int card, int target, Vector2 from, Vector2 to, float duration);
void anim_start_stock_fly(AnimState *a, int card_id, Vector2 from, Vector2 to, float duration);
void anim_start_capture(AnimState *a, int player, const int *cards, int count,
                        const Vector2 *starts, Vector2 target, float duration);
void anim_start_banner(AnimState *a, const char *text, float duration);
void anim_update(AnimState *a, float dt);
int anim_is_blocking(const AnimState *a);

#endif /* UI_ANIM_H */
