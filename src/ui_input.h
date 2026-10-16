#ifndef UI_INPUT_H
#define UI_INPUT_H

#include "raylib.h"
#include "engine.h"
#include "ui_board.h"
#include "ui_anim.h"

typedef struct {
    float scale;
    float ox;
    float oy;
} Letterbox;

typedef struct {
    int on;
    int idx;
    Vector2 grab;
    Vector2 pos;
    Vector2 press;
    int moved;
    int selected; /* Hand index + 1; zero means no click selection. */
} DragState;

/* Letterbox and virtual mouse conversion */
Letterbox input_get_letterbox(void);
Vector2 input_vmouse(Letterbox l);

/* Draw the persistent click selection and its matching field cards. */
void input_draw_selection(const GameState *g, const DragState *drag);

/* Input processing */
void input_handle_mouse(GameState *g, DragState *drag, AnimState *anim,
                        Letterbox l, Vector2 vm, int *hovered_idx, int input_unlocked);

#endif /* UI_INPUT_H */
