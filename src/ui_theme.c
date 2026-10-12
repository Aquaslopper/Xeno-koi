#include "ui_theme.h"

TerminalTheme g_theme = {
    0, "GREEN",
    { 0x5C, 0xFF, 0x8A, 0xFF },
    { 0x2E, 0x9E, 0x55, 0xFF },
    { 0x5C, 0xFF, 0x8A, 25 },
    { 14, 48, 26, 230 }
};

TerminalTheme theme_get(int id) {
    TerminalTheme t;
    if (id == 1) {
        /* Theme 1 (Amber): Base #FFB000, Dim #996600 */
        t.id = 1;
        t.name = "AMBER";
        t.base = GetColor(0xFFB000FF);
        t.dim  = GetColor(0x996600FF);
    } else if (id == 2) {
        /* Theme 2 (Teal): Base #00E5FF, Dim #008899 */
        t.id = 2;
        t.name = "TEAL";
        t.base = GetColor(0x00E5FFFF);
        t.dim  = GetColor(0x008899FF);
    } else {
        /* Theme 0 (Green): Base #5CFF8A, Dim #2E9E55 */
        t.id = 0;
        t.name = "GREEN";
        t.base = GetColor(0x5CFF8AFF);
        t.dim  = GetColor(0x2E9E55FF);
    }
    t.scanline = (Color){ t.base.r, t.base.g, t.base.b, 25 };
    t.btn_hover_bg = (Color){ (unsigned char)(t.dim.r / 4), (unsigned char)(t.dim.g / 4), (unsigned char)(t.dim.b / 4), 230 };
    return t;
}

void theme_set(int id) {
    g_theme = theme_get(id);
}
