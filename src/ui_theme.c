#include "ui_theme.h"
#include <stdlib.h>
#include <math.h>

TerminalTheme g_theme = {
    2, "TEAL",
    { 100, 240, 255, 255 },
    { 45, 175, 195, 255 },
    { 242, 253, 255, 255 },
    { 200, 232, 240, 255 },
    { 100, 240, 255, 10 },
    { 11, 43, 48, 230 }
};

TerminalTheme theme_get(int id) {
    TerminalTheme t;
    if (id == 1) {
        t.id = 1;
        t.name = "AMBER";
        t.base = (Color){ 255, 200, 80, 255 };
        t.dim  = (Color){ 195, 140, 45, 255 };
        t.text_info = (Color){ 255, 250, 240, 255 };
        t.text_info_dim = (Color){ 238, 220, 195, 255 };
    } else if (id == 2) {
        t.id = 2;
        t.name = "TEAL";
        t.base = (Color){ 100, 240, 255, 255 };
        t.dim  = (Color){ 45, 175, 195, 255 };
        t.text_info = (Color){ 242, 253, 255, 255 };
        t.text_info_dim = (Color){ 200, 232, 240, 255 };
    } else if (id == 3) {
        t.id = 3;
        t.name = "CRIMSON";
        t.base = (Color){ 255, 95, 95, 255 };
        t.dim  = (Color){ 195, 55, 55, 255 };
        t.text_info = (Color){ 255, 242, 242, 255 };
        t.text_info_dim = (Color){ 240, 210, 210, 255 };
    } else if (id == 4) {
        t.id = 4;
        t.name = "VIOLET";
        t.base = (Color){ 225, 130, 255, 255 };
        t.dim  = (Color){ 165, 85, 200, 255 };
        t.text_info = (Color){ 250, 242, 255, 255 };
        t.text_info_dim = (Color){ 230, 210, 245, 255 };
    } else if (id == 5) {
        t.id = 5;
        t.name = "DARK";
        t.base = (Color){ 145, 150, 160, 255 };
        t.dim  = (Color){ 75, 80, 90, 255 };
        t.text_info = (Color){ 225, 230, 235, 255 };
        t.text_info_dim = (Color){ 135, 140, 150, 255 };
    } else {
        t.id = 0;
        t.name = "GREEN";
        t.base = (Color){ 120, 255, 160, 255 };
        t.dim  = (Color){ 75, 195, 115, 255 };
        t.text_info = (Color){ 244, 255, 246, 255 };
        t.text_info_dim = (Color){ 205, 235, 212, 255 };
    }
    t.scanline = (Color){ t.base.r, t.base.g, t.base.b, 10 };
    t.btn_hover_bg = (id == 5) ? (Color){ 28, 30, 36, 230 } :
        (Color){ (unsigned char)(t.dim.r / 4), (unsigned char)(t.dim.g / 4), (unsigned char)(t.dim.b / 4), 230 };
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

    /* 2. Subtle center highlight on top & bottom lines (drawn in default alpha blend
          to avoid BeginBlendMode/EndBlendMode render-batch flushes per panel). */
    int half_w = rw >> 1;
    if (half_w > 0) {
        Color c_mid = (Color){ 255, 255, 255, 18 };
        Color c_edge = (Color){ 255, 255, 255, 0 };
        DrawRectangleGradientH(rx, ry, half_w, 1, c_edge, c_mid);
        DrawRectangleGradientH(rx + half_w, ry, rw - half_w, 1, c_mid, c_edge);
        DrawRectangleGradientH(rx, ry + rh - 1, half_w, 1, c_edge, c_mid);
        DrawRectangleGradientH(rx + half_w, ry + rh - 1, rw - half_w, 1, c_mid, c_edge);
    }

    /* 3. Horizontal slow crawling faint streaks on top border (max 3 globally) */
    if (rw >= 40) {
        static double s_last_time = -1.0;
        double now = GetTime();
        if (now != s_last_time) {
            float dt = GetFrameTime();
            if (dt > 0.05f) dt = 0.05f;
            s_last_time = now;
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

        int global_active = 0;
        for (int i = 0; i < MAX_PANEL_FLARES; i++) {
            if (s_panel_flares[i].active) global_active++;
        }
        if (global_active < MAX_PANEL_FLARES && (rand() % 100) == 0) {
            for (int i = 0; i < MAX_PANEL_FLARES; i++) {
                if (!s_panel_flares[i].active) {
                    PanelFlare *f = &s_panel_flares[i];
                    f->active = 1;
                    f->y = (float)ry;
                    f->min_x = (float)rx;
                    f->max_x = (float)(rx + rw - 1);
                    int go_right = rand() & 1;
                    float spd = 10.0f + (float)(rand() % 9);
                    f->vx = go_right ? spd : -spd;
                    f->len = 20.0f + (float)(rand() % 9);
                    f->x = go_right ? f->min_x : f->max_x;
                    f->life = (f->max_x - f->min_x) / spd + 2.0f;
                    break;
                }
            }
        }

        for (int i = 0; i < MAX_PANEL_FLARES; i++) {
            const PanelFlare *f = &s_panel_flares[i];
            if (!f->active) continue;
            if (fabsf(f->y - (float)ry) < 1.0f && f->min_x == (float)rx && f->max_x == (float)(rx + rw - 1)) {
                float x_tail = f->x - (f->vx > 0 ? f->len : -f->len);
                float x0 = fminf(f->x, x_tail), x1 = fmaxf(f->x, x_tail);
                if (x0 < f->min_x) x0 = f->min_x;
                if (x1 > f->max_x) x1 = f->max_x;
                if (x1 > x0) {
                    DrawLine((int)x0, ry, (int)x1, ry, (Color){ 255, 255, 255, 22 });
                }
            }
        }
    }
}
