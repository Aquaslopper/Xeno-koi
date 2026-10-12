#ifndef UI_BOARD_STATUS_H
#define UI_BOARD_STATUS_H

#include "raylib.h"

void board_draw_info_box(Rectangle box, const char *text, int is_active, float flash_alpha);
void board_draw_status_message(const char *text);

#endif /* UI_BOARD_STATUS_H */
