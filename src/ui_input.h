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
} DragState;

/* Letterbox and virtual mouse conversion */
Letterbox input_get_letterbox(void);
Vector2 input_vmouse(Letterbox l);

/* Input processing */
void input_handle_mouse(GameState *g, DragState *drag, AnimState *anim,
                        Letterbox l, Vector2 vm, int *hovered_idx, int input_unlocked);

#endif /* UI_INPUT_H */
