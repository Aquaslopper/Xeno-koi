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

#define MAX_PANEL_FLARES 5

typedef struct {
    float x;
    float y;
    float vx;
    float len;
    float min_x;
    float max_x;
    float life;
    double shine_started;
    int flash_active;
    int active;
} PanelFlare;

static PanelFlare s_panel_flares[MAX_PANEL_FLARES];
#define PANEL_SHINE_DURATION 0.4

static Vector2 s_panel_mouse;
static Vector2 s_previous_panel_mouse;
static int s_panel_mouse_valid;
static int s_previous_panel_mouse_valid;

void ui_theme_set_mouse(Vector2 virtual_mouse) {
    s_previous_panel_mouse = s_panel_mouse;
    s_previous_panel_mouse_valid = s_panel_mouse_valid;
    s_panel_mouse = virtual_mouse;
    s_panel_mouse_valid = 1;
}

static int panel_flare_matches(const PanelFlare *f, int rx, int ry, int rw) {
    return f->active && fabsf(f->y - (float)ry) < 1.0f &&
           f->min_x == (float)rx && f->max_x == (float)(rx + rw - 1);
}

static void panel_flare_shine(int rx, int ry, int rw, double now) {
    PanelFlare *slot = NULL;
    PanelFlare *oldest = NULL;
    int found = 0;
    for (int i = 0; i < MAX_PANEL_FLARES; i++) {
        PanelFlare *f = &s_panel_flares[i];
        if (panel_flare_matches(f, rx, ry, rw)) {
            f->shine_started = now;
            f->flash_active = 1;
            found = 1;
        } else {
            if (!f->active && !slot) slot = f;
            if (f->active && (!oldest || f->life < oldest->life)) oldest = f;
        }
    }
    if (found) return;
    if (!slot) slot = oldest;
    if (!slot) return;

    if (!panel_flare_matches(slot, rx, ry, rw)) {
        int go_right = rand() & 1;
        float speed = 10.0f + (float)(rand() % 9);
        slot->active = 1;
        slot->y = (float)ry;
        slot->min_x = (float)rx;
        slot->max_x = (float)(rx + rw - 1);
        slot->vx = go_right ? speed : -speed;
        slot->len = 20.0f + (float)(rand() % 9);
        slot->x = go_right ? slot->min_x + slot->len : slot->max_x - slot->len;
        slot->life = (slot->max_x - slot->min_x) / speed + 2.0f;
    }
    slot->shine_started = now;
    slot->flash_active = 1;
}

void ui_draw_panel_border(Rectangle r, Color border_color) {
    if (r.width <= 2.0f || r.height <= 2.0f) return;
    int rx = (int)r.x, ry = (int)r.y, rw = (int)r.width, rh = (int)r.height;
    double now = GetTime();
    int hovered = s_panel_mouse_valid && CheckCollisionPointRec(s_panel_mouse, r);
    int entered = hovered && (!s_previous_panel_mouse_valid ||
                              !CheckCollisionPointRec(s_previous_panel_mouse, r));

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

    /* 3. Horizontal slow crawling faint streaks on top border (max 5 globally) */
    if (rw >= 40) {
        static double s_last_time = -1.0;
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

        if (entered) panel_flare_shine(rx, ry, rw, now);

        int global_active = 0;
        for (int i = 0; i < MAX_PANEL_FLARES; i++) {
            if (s_panel_flares[i].active) global_active++;
        }
        if (!entered && global_active < MAX_PANEL_FLARES && (rand() % 100) == 0) {
            for (int i = 0; i < MAX_PANEL_FLARES; i++) {
                if (!s_panel_flares[i].active) {
                    PanelFlare *f = &s_panel_flares[i];
                    f->active = 1;
                    f->flash_active = 0;
                    f->shine_started = 0.0;
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
            PanelFlare *f = &s_panel_flares[i];
            if (!f->active) continue;
            if (panel_flare_matches(f, rx, ry, rw)) {
                float shine = 0.0f;
                if (f->flash_active) {
                    double elapsed = now - f->shine_started;
                    if (elapsed >= 0.0 && elapsed < PANEL_SHINE_DURATION)
                        shine = 1.0f - (float)(elapsed / PANEL_SHINE_DURATION);
                    else f->flash_active = 0;
                }
                float x_tail = f->x - (f->vx > 0 ? f->len : -f->len);
                float x0 = fminf(f->x, x_tail), x1 = fmaxf(f->x, x_tail);
                if (x0 < f->min_x) x0 = f->min_x;
                if (x1 > f->max_x) x1 = f->max_x;
                if (x1 > x0) {
                    unsigned char alpha = (unsigned char)(22.0f + 233.0f * shine);
                    Color flare = (Color){ 255, 255, 255, alpha };
                    if (shine > 0.0f) {
                        DrawLineEx((Vector2){ x0, (float)ry }, (Vector2){ x1, (float)ry },
                                   1.0f + 5.0f * shine, flare);
                    } else {
                        DrawLine((int)x0, ry, (int)x1, ry, flare);
                    }
                }
            }
        }
    }
}
