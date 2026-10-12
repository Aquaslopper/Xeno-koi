#include "ui_theme.h"
#include <stdlib.h>
#include <math.h>

TerminalTheme g_theme = {
    2, "TEAL",
    { 0x00, 0xE5, 0xFF, 0xFF },
    { 0x00, 0x88, 0x99, 0xFF },
    { 242, 253, 255, 255 },
    { 200, 232, 240, 255 },
    { 0x00, 0xE5, 0xFF, 10 },
    { 0, 34, 38, 230 }
};

TerminalTheme theme_get(int id) {
    TerminalTheme t;
    if (id == 1) {
        /* Theme 1 (Amber): Base #FFB000, Dim #996600 */
        t.id = 1;
        t.name = "AMBER";
        t.base = GetColor(0xFFB000FF);
        t.dim  = GetColor(0x996600FF);
        t.text_info = (Color){ 255, 250, 240, 255 };
        t.text_info_dim = (Color){ 238, 220, 195, 255 };
    } else if (id == 2) {
        /* Theme 2 (Teal): Base #00E5FF, Dim #008899 */
        t.id = 2;
        t.name = "TEAL";
        t.base = GetColor(0x00E5FFFF);
        t.dim  = GetColor(0x008899FF);
        t.text_info = (Color){ 242, 253, 255, 255 };
        t.text_info_dim = (Color){ 200, 232, 240, 255 };
    } else if (id == 3) {
        /* Theme 3 (Crimson): Base #FF3333, Dim #991111 */
        t.id = 3;
        t.name = "CRIMSON";
        t.base = (Color){ 0xFF, 0x33, 0x33, 0xFF };
        t.dim  = (Color){ 0x99, 0x11, 0x11, 0xFF };
        t.text_info = (Color){ 255, 242, 242, 255 };
        t.text_info_dim = (Color){ 240, 210, 210, 255 };
    } else if (id == 4) {
        /* Theme 4 (Violet): Base #D45CFF, Dim #7A2E9A */
        t.id = 4;
        t.name = "VIOLET";
        t.base = (Color){ 0xD4, 0x5C, 0xFF, 0xFF };
        t.dim  = (Color){ 0x7A, 0x2E, 0x9A, 0xFF };
        t.text_info = (Color){ 250, 242, 255, 255 };
        t.text_info_dim = (Color){ 230, 210, 245, 255 };
    } else {
        /* Theme 0 (Green): Base #5CFF8A, Dim #2E9E55 */
        t.id = 0;
        t.name = "GREEN";
        t.base = GetColor(0x5CFF8AFF);
        t.dim  = GetColor(0x2E9E55FF);
        t.text_info = (Color){ 244, 255, 246, 255 };
        t.text_info_dim = (Color){ 205, 235, 212, 255 };
    }
    t.scanline = (Color){ t.base.r, t.base.g, t.base.b, 10 };
    t.btn_hover_bg = (Color){ (unsigned char)(t.dim.r / 4), (unsigned char)(t.dim.g / 4), (unsigned char)(t.dim.b / 4), 230 };
    return t;
}

void theme_set(int id) {
    g_theme = theme_get(id);
}

#define MAX_PANEL_FLARES 3

typedef struct {
    float x;
    float y;
    float vx;
    float len;
    float min_x;
    float max_x;
    float life;
    int active;
} PanelFlare;

static PanelFlare s_panel_flares[MAX_PANEL_FLARES];

void ui_draw_panel_border(Rectangle r, Color border_color) {
    if (r.width <= 2.0f || r.height <= 2.0f) return;
    int rx = (int)r.x, ry = (int)r.y, rw = (int)r.width, rh = (int)r.height;

    /* 1. Base border stroke */
    DrawRectangleLinesEx(r, 1.0f, border_color);

    /* Extremely subtle, almost imperceptible ribbed texture (highly transparent black) */
    Color scan_dim = (Color){ 0, 0, 0, 40 };
    for (int y = ry; y < ry + rh; y += 3) {
        DrawPixel(rx, y, scan_dim);
        DrawPixel(rx + rw - 1, y, scan_dim);
    }
    for (int x = rx; x < rx + rw; x += 3) {
        DrawPixel(x, ry, scan_dim);
        DrawPixel(x, ry + rh - 1, scan_dim);
    }

    /* 2. Barely visible ambient glow & top horizontal crawling streak */
    BeginBlendMode(BLEND_ADDITIVE);

    /* Very faint ambient glow gradient on top & bottom lines (max alpha 14/255) */
    int half_w = rw / 2;
    if (half_w > 0) {
        Color c_mid = (Color){ border_color.r, border_color.g, border_color.b, 14 };
        Color c_edge = (Color){ border_color.r, border_color.g, border_color.b, 0 };
        DrawRectangleGradientH(rx, ry, half_w, 1, c_edge, c_mid);
        DrawRectangleGradientH(rx + half_w, ry, rw - half_w, 1, c_mid, c_edge);
        DrawRectangleGradientH(rx, ry + rh - 1, half_w, 1, c_edge, c_mid);
        DrawRectangleGradientH(rx + half_w, ry + rh - 1, rw - half_w, 1, c_mid, c_edge);
    }

    /* 3. Horizontal slow crawling faint streaks on top border (max 2-3 globally) */
    if (rw >= 40) {
        float dt = GetFrameTime();
        if (dt > 0.05f) dt = 0.05f;

        /* Advance all active flares once per frame */
        static int s_last_frame = -1;
        int cur_frame = (int)(GetTime() * 120.0);
        if (s_last_frame != cur_frame) {
            s_last_frame = cur_frame;
            for (int i = 0; i < MAX_PANEL_FLARES; i++) {
                if (!s_panel_flares[i].active) continue;
                s_panel_flares[i].x += s_panel_flares[i].vx * dt;
                s_panel_flares[i].life -= dt;
                if (s_panel_flares[i].life <= 0.0f ||
                    (s_panel_flares[i].vx > 0 && s_panel_flares[i].x - s_panel_flares[i].len > s_panel_flares[i].max_x) ||
                    (s_panel_flares[i].vx < 0 && s_panel_flares[i].x + s_panel_flares[i].len < s_panel_flares[i].min_x)) {
                    s_panel_flares[i].active = 0;
                }
            }
        }

        /* Spawn chance on top horizontal border if below global cap of 3 */
        int global_active = 0;
        for (int i = 0; i < MAX_PANEL_FLARES; i++) {
            if (s_panel_flares[i].active) global_active++;
        }
        if (global_active < 3 && (rand() % 100) == 0) {
            for (int i = 0; i < MAX_PANEL_FLARES; i++) {
                if (!s_panel_flares[i].active) {
                    PanelFlare *f = &s_panel_flares[i];
                    f->active = 1;
                    f->y = (float)ry;
                    f->min_x = (float)rx;
                    f->max_x = (float)(rx + rw - 1);
                    int go_right = rand() % 2;
                    f->vx = go_right ? (10.0f + (float)(rand() % 9)) : -(10.0f + (float)(rand() % 9));
                    f->len = 20.0f + (float)(rand() % 9);
                    f->x = go_right ? f->min_x : f->max_x;
                    f->life = (f->max_x - f->min_x) / fabsf(f->vx) + 2.0f;
                    break;
                }
            }
        }

        /* Render faint 1px elongated streaks crawling along top border (no dots) */
        for (int i = 0; i < MAX_PANEL_FLARES; i++) {
            PanelFlare *f = &s_panel_flares[i];
            if (!f->active) continue;
            if (fabsf(f->y - (float)ry) < 1.0f && f->min_x == (float)rx && f->max_x == (float)(rx + rw - 1)) {
                float x_tail = f->x - (f->vx > 0 ? f->len : -f->len);
                float x0 = fminf(f->x, x_tail), x1 = fmaxf(f->x, x_tail);
                if (x0 < f->min_x) x0 = f->min_x;
                if (x1 > f->max_x) x1 = f->max_x;
                if (x1 > x0) {
                    Color streak_col = (Color){ border_color.r, border_color.g, border_color.b, 14 };
                    DrawLine((int)x0, ry, (int)x1, ry, streak_col);
                }
            }
        }
    }

    EndBlendMode();
}
