#include "ui_input.h"
#include <stdio.h>
#include <math.h>

Letterbox input_get_letterbox(void) {
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    float s = fminf(sw / 1280.0f, sh / 720.0f);
    return (Letterbox){ s, (sw - 1280.0f * s) / 2.0f, (sh - 720.0f * s) / 2.0f };
}

Vector2 input_vmouse(Letterbox l) {
    Vector2 m = GetMousePosition();
    return (Vector2){ (m.x - l.ox) / l.scale, (m.y - l.oy) / l.scale };
}

static void resolve_drop(GameState *g, DragState *drag, AnimState *anim, Vector2 c) {
    int hand_idx = drag->idx;
    int card_id = g->hand[0][hand_idx];

    Rectangle f = board_get_field_rect();
    Rectangle f_grown = { f.x - 24.0f, f.y - 24.0f, f.width + 48.0f, f.height + 48.0f };

    /* 1. Outside grown field: glide back */
    if (!CheckCollisionPointRec(c, f_grown)) {
        Rectangle slot = board_get_hand_card_rect(hand_idx, g->hand_count[0], 0);
        anim_start_glide(anim, card_id, drag->pos, (Vector2){ slot.x, slot.y }, 0.40f);
        printf("DROP centre=(%d,%d) -> FAR DROP, returning hand[%d]=%s\n",
               (int)c.x, (int)c.y, hand_idx, CARDS[card_id].name);
        fflush(stdout);
        return;
    }

    /* 2. Inside field */
    int matches[4];
    int mc = 0;
    for (int i = 0; i < g->field_count; i++) {
        if (CARDS[g->field[i]].month == CARDS[card_id].month) {
            matches[mc++] = g->field[i];
        }
    }

    Vector2 cap_starts[4];
    cap_starts[0] = drag->pos;
    Vector2 player_cap_dest = { 80.0f, 570.0f };

    if (mc == 0) {
        printf("DROP centre=(%d,%d) -> PLACE hand[%d]=%s\n",
               (int)c.x, (int)c.y, hand_idx, CARDS[card_id].name);
        fflush(stdout);
        engine_play_hand(g, card_id, -1);
    } else if (mc == 1) {
        printf("DROP centre=(%d,%d) -> CAPTURE %s+%s\n",
               (int)c.x, (int)c.y, CARDS[card_id].name, CARDS[matches[0]].name);
        fflush(stdout);
        cap_starts[1] = board_get_card_field_pos(g, matches[0]);
        engine_play_hand(g, card_id, -1);
        if (g->last_captured_count > 0) {
            anim_start_capture(anim, 0, g->last_captured, g->last_captured_count,
                               cap_starts, player_cap_dest, 0.50f);
        }
    } else if (mc == 3) {
        printf("DROP centre=(%d,%d) -> CAPTURE ALL FOUR for %s\n",
               (int)c.x, (int)c.y, CARDS[card_id].name);
        fflush(stdout);
        for (int k = 0; k < 3; k++) {
            cap_starts[k + 1] = board_get_card_field_pos(g, matches[k]);
        }
        engine_play_hand(g, card_id, -1);
        if (g->last_captured_count > 0) {
            anim_start_capture(anim, 0, g->last_captured, g->last_captured_count,
                               cap_starts, player_cap_dest, 0.50f);
        }
    } else if (mc == 2) {
        /* Check if dropped directly onto one matching card (hit-test against the slot each card is
           actually painted in, not its position in the field array) */
        int hit0 = 0, hit1 = 0;
        Rectangle r0 = board_get_field_card_rect_of(g, matches[0]);
        Rectangle r1 = board_get_field_card_rect_of(g, matches[1]);
        if (r0.width > 0.0f) {
            Rectangle rg = { r0.x - 10, r0.y - 10, r0.width + 20, r0.height + 20 };
            if (CheckCollisionPointRec(c, rg)) hit0 = 1;
        }
        if (r1.width > 0.0f) {
            Rectangle rg = { r1.x - 10, r1.y - 10, r1.width + 20, r1.height + 20 };
            if (CheckCollisionPointRec(c, rg)) hit1 = 1;
        }

        if (hit0 && !hit1) {
            printf("DROP centre=(%d,%d) -> DIRECT CAPTURE %s+%s\n",
                   (int)c.x, (int)c.y, CARDS[card_id].name, CARDS[matches[0]].name);
            fflush(stdout);
            cap_starts[1] = board_get_card_field_pos(g, matches[0]);
            engine_play_hand(g, card_id, matches[0]);
            if (g->last_captured_count > 0) {
                anim_start_capture(anim, 0, g->last_captured, g->last_captured_count,
                                   cap_starts, player_cap_dest, 0.50f);
            }
        } else if (hit1 && !hit0) {
            printf("DROP centre=(%d,%d) -> DIRECT CAPTURE %s+%s\n",
                   (int)c.x, (int)c.y, CARDS[card_id].name, CARDS[matches[1]].name);
            fflush(stdout);
            cap_starts[1] = board_get_card_field_pos(g, matches[1]);
            engine_play_hand(g, card_id, matches[1]);
            if (g->last_captured_count > 0) {
                anim_start_capture(anim, 0, g->last_captured, g->last_captured_count,
                                   cap_starts, player_cap_dest, 0.50f);
            }
        } else {
            printf("DROP centre=(%d,%d) -> 2-MATCH CHOICE for %s\n",
                   (int)c.x, (int)c.y, CARDS[card_id].name);
            fflush(stdout);
            engine_play_hand(g, card_id, -1);
        }
    }
}

void input_handle_mouse(GameState *g, DragState *drag, AnimState *anim,
                        Letterbox l, Vector2 vm, int *hovered_idx, int input_unlocked) {
    (void)l;
    *hovered_idx = -1;

    /* Update hover when not dragging */
    if (!drag->on && input_unlocked && g->phase == PHASE_TURN_HAND && g->current_player == 0) {
        for (int i = g->hand_count[0] - 1; i >= 0; i--) {
            Rectangle r = board_get_hand_card_rect(i, g->hand_count[0], 0);
            if (CheckCollisionPointRec(vm, r)) {
                *hovered_idx = i;
                break;
            }
        }
    }

    /* Press: start drag in reverse draw order */
    if (!drag->on && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && input_unlocked &&
        g->phase == PHASE_TURN_HAND && g->current_player == 0) {
        for (int i = g->hand_count[0] - 1; i >= 0; i--) {
            Rectangle r = board_get_hand_card_rect(i, g->hand_count[0], 0);
            if (CheckCollisionPointRec(vm, r)) {
                drag->on = 1;
                drag->idx = i;
                drag->grab = (Vector2){ vm.x - r.x, vm.y - r.y };
                drag->pos = (Vector2){ r.x, r.y };
                printf("PRESS hand[%d]=%s at (%d,%d)\n",
                       i, CARDS[g->hand[0][i]].name, (int)vm.x, (int)vm.y);
                fflush(stdout);
                break;
            }
        }
    }

    /* Drag move: keep grab offset exactly */
    if (drag->on) {
        drag->pos = (Vector2){ vm.x - drag->grab.x, vm.y - drag->grab.y };
    }

    /* Release: resolve drop */
    if (drag->on && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        drag->on = 0;
        Vector2 c = { drag->pos.x + HAND_CARD_W / 2.0f, drag->pos.y + HAND_CARD_H / 2.0f };
        resolve_drop(g, drag, anim, c);
    }

    /* Cancel drag on ESC */
    if (drag->on && IsKeyPressed(KEY_ESCAPE)) {
        drag->on = 0;
        Rectangle slot = board_get_hand_card_rect(drag->idx, g->hand_count[0], 0);
        anim_start_glide(anim, g->hand[0][drag->idx], drag->pos, (Vector2){ slot.x, slot.y }, 0.40f);
    }
}
