#ifndef UI_THEME_H
#define UI_THEME_H

#include "raylib.h"

typedef struct {
    int id;
    const char *name;
    Color base;
    Color dim;
    Color scanline;
    Color btn_hover_bg;
} TerminalTheme;

extern TerminalTheme g_theme;

TerminalTheme theme_get(int id);
void theme_set(int id);

#define COLOR_PHOSPHOR       (g_theme.base)
#define COLOR_PHOSPHOR_DIM   (g_theme.dim)
#define COLOR_SCANLINE       (g_theme.scanline)
#define COLOR_BTN_HOVER_BG   (g_theme.btn_hover_bg)

#define COLOR_AMBER          (Color){ 0xFF, 0xB0, 0x00, 0xFF }
#define COLOR_RED            (Color){ 0xFF, 0x40, 0x40, 0xFF }
#define COLOR_PANEL_BG       (Color){ 0, 0, 0, 178 }           /* 70% alpha */
#define COLOR_OVERLAY_BG     (Color){ 0, 0, 0, 220 }           /* heavy dimming */
#define COLOR_TABLE_DIM      (Color){ 0, 0, 0, 38 }            /* 15% alpha table wash */

/* Virtual canvas dimensions */
#define CANVAS_W 1280
#define CANVAS_H 720

/* Card dimensions */
#define ATLAS_CELL_W 176
#define ATLAS_CELL_H 286

#define HAND_CARD_W  88
#define HAND_CARD_H  143

#define FIELD_CARD_W 76
#define FIELD_CARD_H 124

/* Mini cards inside capture lanes: shrunk by 15%, then by an additional 2% */
#define MINI_CARD_W  23.324f
#define MINI_CARD_H  36.652f

/* Left-hand capture panels: shrunk by exactly 15% */
#define CAPTURE_PANEL_W 255.0f
#define CAPTURE_PANEL_H 37.4f

#define CPU_CARD_W   56
#define CPU_CARD_H   91

#define CHOICE_CARD_W 176
#define CHOICE_CARD_H 286

/* Field area */
#define FIELD_X 430
#define FIELD_Y 132
#define FIELD_W 650
#define FIELD_H 400

#endif /* UI_THEME_H */
