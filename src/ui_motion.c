#include "ui_motion.h"
#include "ui_theme.h"
#include "ui_board.h"
#include "raymath.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static CardMotion g_cards[48];

static float s_cinematic_timer = 0.0f;
static float s_cinematic_duration = 1.5f;
static int s_cinematic_round = 1;

void motion_init(void) {
    memset(g_cards, 0, sizeof(g_cards));
    for (int i = 0; i < 48; i++) g_cards[i].tint_val = 255.0f;
    s_cinematic_timer = 0.0f;
}

void motion_set_instant(int card_id, Vector2 pos, Vector2 size) {
    if (card_id < 0 || card_id >= 48) return;
    CardMotion *m = &g_cards[card_id];
    m->current_pos = pos;
    m->target_pos = pos;
    m->start_pos = pos;
    m->prev_pos = pos;
    m->vel = (Vector2){ 0, 0 };
    m->current_size = size;
    m->target_size = size;
    m->start_size = size;
    m->elapsed = 0.0f;
    m->duration = 0.0f;
    if (!m->initialized) m->tint_val = 255.0f;
    m->moving = 0;
    m->initialized = 1;
}

void motion_set_target(int card_id, Vector2 pos, Vector2 size, float duration) {
    if (card_id < 0 || card_id >= 48) return;
    CardMotion *m = &g_cards[card_id];
    if (!m->initialized) {
        motion_set_instant(card_id, pos, size);
        return;
    }

    float dx = pos.x - m->target_pos.x;
    float dy = pos.y - m->target_pos.y;
    float dsx = size.x - m->target_size.x;
    float dsy = size.y - m->target_size.y;

    if (dx * dx + dy * dy > 0.5f || dsx * dsx + dsy * dsy > 0.5f) {
        m->start_pos = m->current_pos;
        m->prev_pos = m->current_pos;
        m->target_pos = pos;
        m->start_size = m->current_size;
        m->target_size = size;
        m->elapsed = 0.0f;
        m->duration = (duration > 0.05f) ? duration : 0.30f;
        m->moving = 1;
    }
}

Rectangle motion_get_rect(int card_id) {
    if (card_id < 0 || card_id >= 48) return (Rectangle){ 0, 0, 0, 0 };
    CardMotion *m = &g_cards[card_id];
    if (!m->initialized) return (Rectangle){ 0, 0, 0, 0 };
    return (Rectangle){ m->current_pos.x, m->current_pos.y, m->current_size.x, m->current_size.y };
}

Vector2 motion_get_pos(int card_id) {
    if (card_id < 0 || card_id >= 48) return (Vector2){ 0, 0 };
    return g_cards[card_id].current_pos;
}

int motion_is_card_moving(int card_id) {
    if (card_id < 0 || card_id >= 48) return 0;
    return g_cards[card_id].moving;
}

Vector2 motion_get_velocity(int card_id) {
    if (card_id < 0 || card_id >= 48) return (Vector2){ 0, 0 };
    return g_cards[card_id].vel;
}

Color motion_get_card_tint(int card_id) {
    (void)card_id;
    return WHITE;
}

void motion_set_drag_velocity(int card_id, Vector2 mouse_delta) {
    if (card_id < 0 || card_id >= 48) return;
    CardMotion *m = &g_cards[card_id];
    m->is_dragged = 1;
    m->vel = mouse_delta;
    float speed = sqrtf(mouse_delta.x * mouse_delta.x + mouse_delta.y * mouse_delta.y);
    float target_tint = 255.0f;
    if (speed > 0.3f) {
        float tilt = mouse_delta.x - mouse_delta.y;
        if (tilt < 0.0f) {
            float factor = fminf(1.0f, -tilt / 16.0f);
            target_tint = 255.0f - 55.0f * factor;
        }
    }
    float dt = GetFrameTime();
    if (dt > 0.05f) dt = 0.05f;
    m->tint_val += (target_tint - m->tint_val) * fminf(1.0f, dt * 14.0f);
}

float motion_get_card_lift(int card_id) {
    if (card_id < 0 || card_id >= 48) return 0.0f;
    CardMotion *m = &g_cards[card_id];
    if (!m->moving || m->duration <= 0.001f) return 0.0f;
    float t = m->elapsed / m->duration;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return sinf(t * 3.14159265f);
}

int motion_is_reflowing(const GameState *g) {
    if (!g) return 0;

    /* Check field cards */
    for (int i = 0; i < g->field_count; i++) {
        int cid = g->field[i];
        if (cid >= 0 && cid < 48 && g_cards[cid].moving) return 1;
    }

    /* Check captured cards */
    for (int p = 0; p < 2; p++) {
        for (int i = 0; i < g->captured_count[p]; i++) {
            int cid = g->captured[p][i];
            if (cid >= 0 && cid < 48 && g_cards[cid].moving) return 1;
        }
    }

    /* Check player hand cards reflowing horizontally or far returns */
    for (int i = 0; i < g->hand_count[0]; i++) {
        int cid = g->hand[0][i];
        if (cid >= 0 && cid < 48 && g_cards[cid].moving) {
            float dx = fabsf(g_cards[cid].target_pos.x - g_cards[cid].current_pos.x);
            float dy = fabsf(g_cards[cid].target_pos.y - g_cards[cid].current_pos.y);
            if (dx > 4.0f || dy > 18.0f) return 1;
        }
    }

    /* Check CPU hand cards reflowing */
    for (int i = 0; i < g->hand_count[1]; i++) {
        int cid = g->hand[1][i];
        if (cid >= 0 && cid < 48 && g_cards[cid].moving) {
            float dx = fabsf(g_cards[cid].target_pos.x - g_cards[cid].current_pos.x);
            float dy = fabsf(g_cards[cid].target_pos.y - g_cards[cid].current_pos.y);
            if (dx > 4.0f || dy > 18.0f) return 1;
        }
    }

    return 0;
}

void motion_update(float dt) {
    for (int i = 0; i < 48; i++) {
        CardMotion *m = &g_cards[i];
        if (m->is_dragged) {
            m->is_dragged = 0;
            continue;
        }
        float target_tint = 255.0f;
        if (m->moving) {
            m->elapsed += dt;
            m->prev_pos = m->current_pos;
            if (m->elapsed >= m->duration) {
                m->elapsed = m->duration;
                m->current_pos = m->target_pos;
                m->current_size = m->target_size;
                m->moving = 0;
                m->vel = (Vector2){ 0, 0 };
            } else {
                float t = m->elapsed / m->duration;
                if (t < 0.0f) t = 0.0f;
                if (t > 1.0f) t = 1.0f;
                float ease = t * t * (3.0f - 2.0f * t); /* smooth cubic ease-in-out */
                m->current_pos = Vector2Lerp(m->start_pos, m->target_pos, ease);
                m->current_size = Vector2Lerp(m->start_size, m->target_size, ease);
                m->vel = (Vector2){ m->current_pos.x - m->prev_pos.x, m->current_pos.y - m->prev_pos.y };
            }
            float speed = sqrtf(m->vel.x * m->vel.x + m->vel.y * m->vel.y);
            if (speed > 0.15f) {
                float tilt = m->vel.x - m->vel.y;
                if (tilt < 0.0f) {
                    float factor = fminf(1.0f, -tilt / 18.0f);
                    target_tint = 255.0f - 55.0f * factor;
                }
            }
        } else {
            m->vel = (Vector2){ 0, 0 };
        }
        float rate = fminf(1.0f, dt * 14.0f);
        m->tint_val += (target_tint - m->tint_val) * rate;
    }
}

void motion_deal_round(const GameState *g, float duration) {
    if (!g) return;
    float deal_dur = (duration > 0.1f) ? duration : 0.45f;
    Vector2 stock_pos = { 330.0f, 262.0f };
    Vector2 stock_size = { FIELD_CARD_W, FIELD_CARD_H };

    /* All 48 cards initialize at stock */
    for (int i = 0; i < 48; i++) {
        motion_set_instant(i, stock_pos, stock_size);
    }

    /* 1. Human hand cards deal out smoothly */
    for (int i = 0; i < g->hand_count[0]; i++) {
        int cid = g->hand[0][i];
        Rectangle r = board_get_hand_card_rect(i, g->hand_count[0], 0);
        CardMotion *m = &g_cards[cid];
        m->start_pos = stock_pos;
        m->current_pos = stock_pos;
        m->start_size = stock_size;
        m->target_pos = (Vector2){ r.x, r.y };
        m->target_size = (Vector2){ r.width, r.height };
        m->elapsed = 0.0f;
        m->duration = deal_dur;
        m->moving = 1;
        m->initialized = 1;
    }

    /* 2. CPU hand cards deal out smoothly */
    float opp_x = 710.0f - (g->hand_count[1] * (CPU_CARD_W + 6.0f)) / 2.0f;
    float opp_step = (g->hand_count[1] > 1) ? fminf(CPU_CARD_W + 6.0f, 360.0f / (g->hand_count[1] - 1)) : 0.0f;
    for (int i = 0; i < g->hand_count[1]; i++) {
        int cid = g->hand[1][i];
        Rectangle r = { opp_x + i * opp_step, 18.0f, CPU_CARD_W, CPU_CARD_H };
        CardMotion *m = &g_cards[cid];
        m->start_pos = stock_pos;
        m->current_pos = stock_pos;
        m->start_size = stock_size;
        m->target_pos = (Vector2){ r.x, r.y };
        m->target_size = (Vector2){ r.width, r.height };
        m->elapsed = 0.0f;
        m->duration = deal_dur;
        m->moving = 1;
        m->initialized = 1;
    }

    /* 3. Field cards deal out smoothly */
    int sorted_field[48];
    for (int i = 0; i < g->field_count; i++) sorted_field[i] = i;
    for (int i = 0; i < g->field_count - 1; i++) {
        for (int j = i + 1; j < g->field_count; j++) {
            int c1 = g->field[sorted_field[i]], c2 = g->field[sorted_field[j]];
            if (CARDS[c1].month > CARDS[c2].month || (CARDS[c1].month == CARDS[c2].month && c1 > c2)) {
                int tmp = sorted_field[i]; sorted_field[i] = sorted_field[j]; sorted_field[j] = tmp;
            }
        }
    }

    for (int i = 0; i < g->field_count; i++) {
        int cid = g->field[sorted_field[i]];
        Rectangle r = board_get_field_card_rect(i, g->field_count);
        CardMotion *m = &g_cards[cid];
        m->start_pos = stock_pos;
        m->current_pos = stock_pos;
        m->start_size = stock_size;
        m->target_pos = (Vector2){ r.x, r.y };
        m->target_size = (Vector2){ r.width, r.height };
        m->elapsed = 0.0f;
        m->duration = deal_dur;
        m->moving = 1;
        m->initialized = 1;
    }
}

void cinematic_start(int round_num) {
    s_cinematic_timer = 1.5f;
    s_cinematic_duration = 1.5f;
    s_cinematic_round = round_num;
}

void cinematic_update(float dt) {
    if (s_cinematic_timer > 0.0f) {
        s_cinematic_timer -= dt;
        if (s_cinematic_timer < 0.0f) s_cinematic_timer = 0.0f;
    }
}

int cinematic_is_active(void) {
    return (s_cinematic_timer > 0.0f);
}

float cinematic_get_alpha(void) {
    if (s_cinematic_timer <= 0.0f) return 0.0f;
    float elapsed = s_cinematic_duration - s_cinematic_timer;
    if (elapsed < 0.25f) {
        return elapsed / 0.25f;
    } else if (elapsed <= 1.10f) {
        return 1.0f;
    } else {
        float f = (s_cinematic_duration - elapsed) / 0.40f;
        return (f < 0.0f) ? 0.0f : ((f > 1.0f) ? 1.0f : f);
    }
}

void cinematic_draw(void) {
    float alpha = cinematic_get_alpha();
    if (alpha <= 0.001f) return;

    const char *title;
    char buf[64];
    if (s_cinematic_round <= 1) {
        title = "MATCH START";
    } else {
        snprintf(buf, sizeof(buf), "ROUND %d START", s_cinematic_round);
        title = buf;
    }

    int font_size = 42;
    int tw = MeasureText(title, font_size);
    int pad_x = 54;
    int pad_y = 24;
    int box_w = tw + pad_x * 2;
    int box_h = font_size + pad_y * 2;
    int box_x = (CANVAS_W - box_w) / 2;
    int box_y = (CANVAS_H - box_h) / 2 - 20;

    Rectangle box = { (float)box_x, (float)box_y, (float)box_w, (float)box_h };

    /* Translucent dark background */
    DrawRectangleRec(box, (Color){ 0, 0, 0, (unsigned char)(225 * alpha) });

    /* Phosphor border with outer bloom */
    Color col_base = g_theme.base;
    Color col_dim  = g_theme.dim;

    Color border_outer = col_dim;
    border_outer.a = (unsigned char)(110 * alpha);
    DrawRectangleLinesEx((Rectangle){ box.x - 3, box.y - 3, box.width + 6, box.height + 6 }, 1.5f, border_outer);

    Color border_inner = col_base;
    border_inner.a = (unsigned char)(255 * alpha);
    DrawRectangleLinesEx(box, 2.0f, border_inner);

    /* Biomechanical corner accents */
    float c_len = 16.0f;
    DrawLineEx((Vector2){ box.x, box.y }, (Vector2){ box.x + c_len, box.y }, 3.0f, border_inner);
    DrawLineEx((Vector2){ box.x, box.y }, (Vector2){ box.x, box.y + c_len }, 3.0f, border_inner);

    DrawLineEx((Vector2){ box.x + box.width, box.y }, (Vector2){ box.x + box.width - c_len, box.y }, 3.0f, border_inner);
    DrawLineEx((Vector2){ box.x + box.width, box.y }, (Vector2){ box.x + box.width, box.y + c_len }, 3.0f, border_inner);

    DrawLineEx((Vector2){ box.x, box.y + box.height }, (Vector2){ box.x + c_len, box.y + box.height }, 3.0f, border_inner);
    DrawLineEx((Vector2){ box.x, box.y + box.height }, (Vector2){ box.x, box.y + box.height - c_len }, 3.0f, border_inner);

    DrawLineEx((Vector2){ box.x + box.width, box.y + box.height }, (Vector2){ box.x + box.width - c_len, box.y + box.height }, 3.0f, border_inner);
    DrawLineEx((Vector2){ box.x + box.width, box.y + box.height }, (Vector2){ box.x + box.width, box.y + box.height - c_len }, 3.0f, border_inner);

    /* Text */
    Color text_col = col_base;
    text_col.a = (unsigned char)(255 * alpha);
    DrawText(title, box_x + pad_x, box_y + pad_y, font_size, text_col);

    /* Subtitle prompt */
    const char *sub = (s_cinematic_round <= 1) ? "// NEURAL LINK ESTABLISHED //" : "// DECK SHUFFLED & DEALT //";
    int sub_w = MeasureText(sub, 14);
    Color sub_col = col_dim;
    sub_col.a = (unsigned char)(210 * alpha);
    DrawText(sub, (CANVAS_W - sub_w) / 2, box_y + box_h + 10, 14, sub_col);
}
