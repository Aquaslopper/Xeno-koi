#include "ui_anim.h"
#include "ui_motion.h"
#include <string.h>

void anim_init(AnimState *a) {
    memset(a, 0, sizeof(*a));
}

void anim_start_glide(AnimState *a, int card_id, Vector2 from, Vector2 to, float duration) {
    a->type = ANIM_GLIDE_RETURN;
    a->active = 1;
    a->elapsed = 0.0f;
    a->duration = duration > 0.0f ? duration : 0.40f;
    a->card_id = card_id;
    a->start_pos = from;
    a->current_pos = from;
    a->target_pos = to;
}

void anim_start_stock_flip(AnimState *a, int card_id, float duration) {
    a->type = ANIM_STOCK_FLIP;
    a->active = 1;
    a->elapsed = 0.0f;
    a->duration = duration > 0.0f ? duration : 0.50f;
    a->card_id = card_id;
}

void anim_start_cpu_think(AnimState *a, float duration) {
    a->type = ANIM_CPU_THINK;
    a->active = 1;
    a->elapsed = 0.0f;
    a->duration = duration > 0.0f ? duration : 0.25f;
}

void anim_start_cpu_fly(AnimState *a, int card, int target, Vector2 from, Vector2 to, float duration) {
    a->type = ANIM_CPU_FLY;
    a->active = 1;
    a->elapsed = 0.0f;
    a->duration = duration > 0.0f ? duration : 0.50f;
    a->card_id = card;
    a->cpu_card = card;
    a->cpu_target = target;
    a->start_pos = from;
    a->current_pos = from;
    a->target_pos = to;
}

void anim_start_stock_fly(AnimState *a, int card_id, Vector2 from, Vector2 to, float duration) {
    a->type = ANIM_STOCK_FLY;
    a->active = 1;
    a->elapsed = 0.0f;
    a->duration = duration > 0.0f ? duration : 0.50f;
    a->card_id = card_id;
    a->start_pos = from;
    a->current_pos = from;
    a->target_pos = to;
    motion_set_instant(card_id, from, (Vector2){ 76.0f, 116.0f });
}

void anim_start_capture(AnimState *a, int player, const int *cards, int count,
                        const Vector2 *starts, Vector2 target, float duration) {
    a->type = ANIM_CAPTURE_FLY;
    a->active = 1;
    a->elapsed = 0.0f;
    a->duration = duration > 0.0f ? duration : 0.50f;
    a->capture_player = player;
    a->capture_count = (count > 4) ? 4 : count;
    a->capture_target = target;
    for (int i = 0; i < a->capture_count; i++) {
        a->capture_cards[i] = cards[i];
        a->capture_starts[i] = starts[i];
        a->capture_currents[i] = starts[i];
    }
}

void anim_start_banner(AnimState *a, const char *text, float duration) {
    a->type = ANIM_BANNER;
    a->active = 1;
    a->elapsed = 0.0f;
    a->duration = duration > 0.0f ? duration : 1.5f;
    strncpy(a->banner_text, text, sizeof(a->banner_text) - 1);
    a->banner_text[sizeof(a->banner_text) - 1] = '\0';
}

void anim_update(AnimState *a, float dt) {
    if (!a->active) return;
    a->elapsed += dt;
    if (a->elapsed >= a->duration) {
        a->elapsed = a->duration;
        a->current_pos = a->target_pos;
        for (int i = 0; i < a->capture_count; i++) {
            a->capture_currents[i] = a->capture_target;
        }
        if (a->type == ANIM_STOCK_FLY) {
            motion_set_instant(a->card_id, a->target_pos, (Vector2){ 76.0f, 116.0f });
        } else if (a->type == ANIM_CPU_FLY) {
            motion_set_instant(a->cpu_card, a->target_pos, (Vector2){ 76.0f, 116.0f });
        }
        if (a->type != ANIM_CPU_THINK && a->type != ANIM_CPU_FLY && a->type != ANIM_STOCK_FLY) {
            a->active = 0;
        }
        return;
    }

    float t = a->elapsed / a->duration;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    /* Smooth cubic ease-in-out (smoothstep) */
    float ease = t * t * (3.0f - 2.0f * t);

    if (a->type == ANIM_GLIDE_RETURN || a->type == ANIM_CPU_FLY || a->type == ANIM_STOCK_FLY) {
        a->current_pos.x = a->start_pos.x + (a->target_pos.x - a->start_pos.x) * ease;
        a->current_pos.y = a->start_pos.y + (a->target_pos.y - a->start_pos.y) * ease;
        if (a->type == ANIM_STOCK_FLY) {
            motion_set_instant(a->card_id, a->current_pos, (Vector2){ 76.0f, 116.0f });
        } else if (a->type == ANIM_CPU_FLY) {
            motion_set_instant(a->cpu_card, a->current_pos, (Vector2){ 76.0f, 116.0f });
        }
    } else if (a->type == ANIM_CAPTURE_FLY) {
        for (int i = 0; i < a->capture_count; i++) {
            a->capture_currents[i].x = a->capture_starts[i].x + (a->capture_target.x - a->capture_starts[i].x) * ease;
            a->capture_currents[i].y = a->capture_starts[i].y + (a->capture_target.y - a->capture_starts[i].y) * ease;
        }
    }
}

int anim_is_blocking(const AnimState *a) {
    if (!a->active) return 0;
    return (a->type != ANIM_BANNER);
}
