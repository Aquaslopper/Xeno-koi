#include "ui_input.h"
#include "ui_motion.h"
#include "ui_theme.h"
#include "ui_speed.h"
#include "replay.h"
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

static float rect_overlap_area(Rectangle a, Rectangle b) {
    float x0 = fmaxf(a.x, b.x), y0 = fmaxf(a.y, b.y);
    float x1 = fminf(a.x + a.width, b.x + b.width);
    float y1 = fminf(a.y + a.height, b.y + b.height);
    return (x1 > x0 && y1 > y0) ? (x1 - x0) * (y1 - y0) : 0.0f;
}

static float rect_edge_dist_sq(Rectangle a, Rectangle b) {
    float dx = 0.0f, dy = 0.0f;
    if (a.x + a.width < b.x)       dx = b.x - (a.x + a.width);
    else if (b.x + b.width < a.x)  dx = a.x - (b.x + b.width);
    if (a.y + a.height < b.y)      dy = b.y - (a.y + a.height);
    else if (b.y + b.height < a.y) dy = a.y - (b.y + b.height);
    return dx * dx + dy * dy;
}

/* KK-ADD-10 speed minigame: how much of the target card the dropped card
   covers, 0..1. The drop rect is the hand card centred on the drop point. */
static float drop_cover(const GameState *g, Vector2 c, int target) {
    Rectangle tr = board_get_field_card_rect_of(g, target);
    if (tr.width <= 0.0f) return 0.0f;
    Rectangle dr = { c.x - HAND_CARD_W * 0.5f, c.y - HAND_CARD_H * 0.5f,
                     (float)HAND_CARD_W, (float)HAND_CARD_H };
    float inter = rect_overlap_area(dr, tr);
    float min_area = fminf((float)(HAND_CARD_W * HAND_CARD_H), tr.width * tr.height);
    return (min_area > 0.0f) ? inter / min_area : 0.0f;
}

/* Hit-test the dragged card rectangle `dr` (not the mouse cursor) against the
   painted slot of each matching field card. Any overlap or <= 10 px edge-to-edge
   gap counts as near (unless `dr` is mostly sitting on a neighbouring non-matching
   card), and multiple hits resolve to the match with greatest card overlap. */
static int find_nearest_match_hit(const GameState *g, int card_month,
                                  const int *matches, int mc, Vector2 c) {
    Rectangle dr = { c.x - HAND_CARD_W * 0.5f, c.y - HAND_CARD_H * 0.5f,
                     (float)HAND_CARD_W, (float)HAND_CARD_H };
    float max_other_ov = 0.0f;
    for (int i = 0; i < g->field_count; i++) {
        int fid = g->field[i];
        if (CARDS[fid].month == card_month) continue;
        Rectangle fr = board_get_field_card_rect_of(g, fid);
        if (fr.width <= 0.0f) continue;
        float ov = rect_overlap_area(dr, fr);
        if (ov > max_other_ov) max_other_ov = ov;
    }

    const float max_edge_d2 = 10.0f * 10.0f;
    const float min_card_area = (float)(FIELD_CARD_W * FIELD_CARD_H);
    int best_idx = -1;
    float best_ov = -1.0f;
    float best_d2 = 0.0f;
    for (int i = 0; i < mc; i++) {
        Rectangle r = board_get_field_card_rect_of(g, matches[i]);
        if (r.width <= 0.0f) continue;
        if (rect_edge_dist_sq(dr, r) > max_edge_d2) continue;
        float ov = rect_overlap_area(dr, r);
        if (ov < min_card_area * 0.15f && max_other_ov > ov) continue;
        float dx = c.x - (r.x + r.width * 0.5f);
        float dy = c.y - (r.y + r.height * 0.5f);
        float cd2 = dx * dx + dy * dy;
        if (best_idx < 0 || ov > best_ov + 0.5f ||
            (fabsf(ov - best_ov) <= 0.5f && cd2 < best_d2)) {
            best_idx = i;
            best_ov = ov;
            best_d2 = cd2;
        }
    }
    return best_idx;
}

static void resolve_drop(GameState *g, DragState *drag, AnimState *anim, Vector2 c,
                         int allow_speed_grade) {
    int hand_idx = drag->idx;
    int card_id = g->hand[0][hand_idx];
    float grade_cover = -1.0f;

    int matches[4];
    int mc = 0;
    int card_month = CARDS[card_id].month;
    for (int i = 0; i < g->field_count; i++) {
        if (CARDS[g->field[i]].month == card_month) {
            matches[mc++] = g->field[i];
        }
    }

    Vector2 cap_starts[4];
    cap_starts[0] = drag->pos;
    Vector2 player_cap_dest = { 80.0f, 570.0f };
    Vector2 hit_pos = { -1.0f, -1.0f };

    if (mc == 0) {
        Rectangle f = board_get_field_rect();
        Rectangle f_grown = { f.x - 24.0f, f.y - 24.0f, f.width + 48.0f, f.height + 48.0f };
        if (!CheckCollisionPointRec(c, f_grown)) {
            Rectangle slot = board_get_hand_card_rect(hand_idx, g->hand_count[0], 0);
            anim_start_glide(anim, card_id, drag->pos, (Vector2){ slot.x, slot.y }, 0.40f);
            printf("DROP centre=(%d,%d) -> FAR DROP, returning hand[%d]=%s\n",
                   (int)c.x, (int)c.y, hand_idx, CARDS[card_id].name);
            fflush(stdout);
            return;
        }
        printf("DROP centre=(%d,%d) -> PLACE hand[%d]=%s\n",
               (int)c.x, (int)c.y, hand_idx, CARDS[card_id].name);
        fflush(stdout);
        board_hint_field_drop(card_id, c);
        engine_play_hand(g, card_id, -1);
        replay_rec_play(0, card_id, -1);
    } else {
        int near_idx = find_nearest_match_hit(g, card_month, matches, mc, c);
        if (near_idx < 0) {
            Rectangle slot = board_get_hand_card_rect(hand_idx, g->hand_count[0], 0);
            anim_start_glide(anim, card_id, drag->pos, (Vector2){ slot.x, slot.y }, 0.40f);
            printf("DROP centre=(%d,%d) -> FAR DROP, returning hand[%d]=%s\n",
                   (int)c.x, (int)c.y, hand_idx, CARDS[card_id].name);
            fflush(stdout);
            return;
        }
        if (mc == 1) {
            printf("DROP centre=(%d,%d) -> CAPTURE %s+%s\n",
                   (int)c.x, (int)c.y, CARDS[card_id].name, CARDS[matches[0]].name);
            fflush(stdout);
            cap_starts[1] = board_get_card_field_pos(g, matches[0]);
            hit_pos = cap_starts[1];
            grade_cover = drop_cover(g, c, matches[0]);
            engine_play_hand(g, card_id, -1);
            replay_rec_play(0, card_id, -1);
            if (g->last_captured_count > 0) {
                anim_start_capture(anim, 0, g->last_captured, g->last_captured_count,
                                   cap_starts, player_cap_dest, 0.50f);
            }
        } else if (mc == 2) {
            int target = matches[near_idx];
            printf("DROP centre=(%d,%d) -> DIRECT CAPTURE %s+%s\n",
                   (int)c.x, (int)c.y, CARDS[card_id].name, CARDS[target].name);
            fflush(stdout);
            cap_starts[1] = board_get_card_field_pos(g, target);
            hit_pos = cap_starts[1];
            grade_cover = drop_cover(g, c, target);
            engine_play_hand(g, card_id, target);
            replay_rec_play(0, card_id, target);
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
            hit_pos = cap_starts[near_idx + 1];
            grade_cover = drop_cover(g, c, matches[near_idx]);
            engine_play_hand(g, card_id, -1);
            replay_rec_play(0, card_id, -1);
            if (g->last_captured_count > 0) {
                anim_start_capture(anim, 0, g->last_captured, g->last_captured_count,
                                   cap_starts, player_cap_dest, 0.50f);
            }
        }
    }
    if (g->ruleset == RULESET_KK_ADD_10) {
        if (allow_speed_grade && grade_cover >= 0.0f) speed_bar_grade(grade_cover);
        else speed_bar_consume();
    } else {
        if (allow_speed_grade && grade_cover >= 0.0f) speed_bar_grade_visual(grade_cover);
        else speed_bar_consume_visual();
    }
    if (allow_speed_grade && hit_pos.x >= 0.0f && g->last_captured_count > 0) {
        speed_hit_popup_commit(hit_pos.x + 38.0f, hit_pos.y);
    }
}

void input_handle_mouse(GameState *g, DragState *drag, AnimState *anim,
                        Letterbox l, Vector2 vm, int *hovered_idx, int input_unlocked) {
    (void)l;
    *hovered_idx = -1;

    if (g->phase != PHASE_TURN_HAND || g->current_player != 0 ||
        drag->selected > g->hand_count[0]) drag->selected = 0;
    if (drag->on && (g->phase != PHASE_TURN_HAND || g->current_player != 0 ||
        drag->idx < 0 || drag->idx >= g->hand_count[0])) {
        drag->on = 0;
        drag->idx = -1;
        drag->moved = 0;
    }
    if (!input_unlocked && !drag->on) return;
    if (IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        drag->selected = 0;
        if (drag->on) {
            Rectangle slot = board_get_hand_card_rect(drag->idx, g->hand_count[0], 0);
            anim_start_glide(anim, g->hand[0][drag->idx], drag->pos,
                             (Vector2){ slot.x, slot.y }, 0.40f);
            drag->on = 0;
            drag->idx = -1;
            drag->moved = 0;
        }
        return;
    }

    /* A selected card only plays on a matching card, or empty field if unmatched.
       Feed the existing drop resolver so captures and replay recording stay identical. */
    if (!drag->on && input_unlocked && drag->selected && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        int cid = g->hand[0][drag->selected - 1], matches = 0, hit = -1;
        for (int i = 0; i < g->field_count; i++) {
            int f = g->field[i];
            if (!board_is_blind_field_card(f) && CARDS[f].month == CARDS[cid].month) matches++;
            if (CheckCollisionPointRec(vm, board_get_field_card_rect_of(g, f))) hit = f;
        }
        int hit_blind = (hit >= 0 && board_is_blind_field_card(hit));
        if ((hit >= 0 && !hit_blind && CARDS[hit].month == CARDS[cid].month) ||
            hit_blind ||
            (!matches && hit < 0 && CheckCollisionPointRec(vm, board_get_field_rect()))) {
            drag->idx = drag->selected - 1;
            Rectangle slot = board_get_hand_card_rect(drag->idx, g->hand_count[0], 0);
            drag->pos = (Vector2){ slot.x, slot.y };
            Vector2 target = vm;
            if (hit >= 0) {
                Rectangle r = board_get_field_card_rect_of(g, hit);
                target = (Vector2){ r.x + r.width / 2, r.y + r.height / 2 };
            }
            drag->selected = 0;
            /* Click-to-play resolves normally but is never precision-graded. */
            resolve_drop(g, drag, anim, target, 0);
            drag->idx = -1;
            return;
        }
    }

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
                drag->press = vm;
                drag->moved = 0;
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
        float dx = vm.x - drag->press.x, dy = vm.y - drag->press.y;
        if (dx * dx + dy * dy > 144.0f) { drag->moved = 1; drag->selected = 0; }
        if (drag->moved) drag->pos = (Vector2){ vm.x - drag->grab.x, vm.y - drag->grab.y };
    }

    /* Release: resolve drop */
    if (drag->on && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        int was_moved = drag->moved;
        drag->on = 0;
        drag->moved = 0;
        if (!was_moved) {
            drag->selected = (drag->selected == drag->idx + 1) ? 0 : drag->idx + 1;
            drag->idx = -1;
            return;
        }
        Vector2 c = { drag->pos.x + HAND_CARD_W / 2.0f, drag->pos.y + HAND_CARD_H / 2.0f };
        resolve_drop(g, drag, anim, c, 1);
        drag->idx = -1;
    }

}

void input_draw_selection(const GameState *g, const DragState *drag) {
    if (drag->on || !drag->selected || drag->selected > g->hand_count[0] ||
        g->current_player != 0 || g->phase != PHASE_TURN_HAND) return;
    int cid = g->hand[0][drag->selected - 1];
    Rectangle selected_card = motion_get_rect(cid);
    board_draw_card_glow(selected_card, 1.0f);
    ui_theme_draw_panel_flares_keyed(selected_card, cid + 1);
    if (board_is_action_distraction_high()) return;
    for (int i = 0; i < g->field_count; i++) {
        int f = g->field[i];
        if (!board_is_blind_field_card(f) && CARDS[f].month == CARDS[cid].month) {
            Rectangle matching_card = motion_get_rect(f);
            board_draw_card_glow(matching_card, 1.0f);
            ui_theme_draw_panel_flares_keyed(matching_card, f + 1);
        }
    }
}
