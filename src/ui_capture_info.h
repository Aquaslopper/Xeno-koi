#ifndef UI_CAPTURE_INFO_H
#define UI_CAPTURE_INFO_H

#include "raylib.h"
#include "ui_board.h"

#define CAPTURE_INFO_PANEL_X 12.0f
#define CAPTURE_INFO_PANEL_Y 304.0f
#define CAPTURE_INFO_PANEL_W CAPTURE_PANEL_W
#define CAPTURE_INFO_PANEL_H 200.0f

Rectangle capture_info_panel_rect(void);
void capture_info_begin(int enabled, Vector2 mouse);
void capture_info_clear(void);
void capture_info_record(int card_id, int player, int lane_index, int lane_total, Rectangle bounds);
int  capture_info_hovered_card(void);
void capture_info_draw(const BoardAssets *ba);

#endif /* UI_CAPTURE_INFO_H */
