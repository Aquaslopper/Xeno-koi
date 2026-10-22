#ifndef UI_SPEED_H
#define UI_SPEED_H

#include "engine.h"

/* KK-ADD-10 only: the bottom-right speed minigame. A short timer bar arms at
   the start of each human hand turn and drains over 300 ms; it is gone the
   moment the hand card is dropped (the stock flip never counts). Dropping the
   hand card onto a matching card while the bar still runs scores minipoints
   from speed (how much of the bar is left) times accuracy (how much of the
   target card the drop covers). Minipoints show in gold over the bar and stay
   after the bar is gone; every 10 of them become 1 extra score point at the
   end of the match. Human only - the CPU is never graded. */

void speed_bar_reset(void);                       /* new match: clear everything */
void speed_bar_update(const GameState *g, float dt, int input_unlocked);
void speed_bar_consume(void);                     /* hand play resolved with no grade */
void speed_bar_grade(float cover);                /* capture graded: cover is 0..1 */
int  speed_bar_apply_match_bonus(GameState *g);   /* +total/10 to the human; returns the bonus */
void speed_bar_draw(void);                        /* bar behind, gold numbers over it */

#endif /* UI_SPEED_H */
