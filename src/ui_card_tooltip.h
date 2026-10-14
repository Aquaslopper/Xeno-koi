#ifndef UI_CARD_TOOLTIP_H
#define UI_CARD_TOOLTIP_H

#include "raylib.h"

/* Hit test rebuilt in draw order each frame; no input handling.
   Show after 500 ms on the same card, hide immediately on hover loss. */
void card_tooltip_begin(int enabled, Vector2 mouse);
void card_tooltip_clear(void);
void card_tooltip_record(int card_id, Rectangle bounds);
void card_tooltip_draw(void);

#endif
