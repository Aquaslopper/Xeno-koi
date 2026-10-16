#ifndef UI_COMBOS_H
#define UI_COMBOS_H

#include "ui_board.h"

Rectangle combos_panel_rect(int player);
int combos_panel_hit(Vector2 mouse); /* player 0/1, or -1 */
void combos_draw_sidebar(const GameState *g);
void combos_draw_details(const BoardAssets *assets, const GameState *g, int player, int page);
int combos_handle_details(const GameState *g, int player, int *page, Vector2 mouse); /* 1 = close */

#endif
