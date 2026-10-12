#ifndef UI_TAUNT_H
#define UI_TAUNT_H

#include "raylib.h"

void taunt_init(void);
void taunt_trigger(const char *text, float duration);
void taunt_update(float dt);
void taunt_draw(void);

#endif /* UI_TAUNT_H */
