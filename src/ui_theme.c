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
    Rectangle bounds;
    float distance;
    float direction;
    float speed;
    float len;
    float life;
    float age;
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
static double s_last_flare_time = -1.0;

void ui_theme_set_mouse(Vector2 virtual_mouse) {
    s_previous_panel_mouse = s_panel_mouse;
    s_previous_panel_mouse_valid = s_panel_mouse_valid;
    s_panel_mouse = virtual_mouse;
    s_panel_mouse_valid = 1;
}

static Rectangle panel_flare_bounds(Rectangle r) {
    return (Rectangle){ (float)(int)r.x, (float)(int)r.y,
                        (float)(int)r.width, (float)(int)r.height };
}

static float panel_flare_perimeter(Rectangle r) {
    if (r.width < 2.0f || r.height < 2.0f) return 0.0f;
    return 2.0f * ((r.width - 1.0f) + (r.height - 1.0f));
}

static float panel_wrap_distance(float distance, float perimeter) {
    if (perimeter <= 0.0f) return 0.0f;
    distance = fmodf(distance, perimeter);
    if (distance < 0.0f) distance += perimeter;
    return distance;
}

static int panel_flare_matches(const PanelFlare *f, Rectangle bounds) {
    return f->active && f->bounds.x == bounds.x && f->bounds.y == bounds.y &&
           f->bounds.width == bounds.width && f->bounds.height == bounds.height;
}

static void panel_flares_update(double now) {
    if (now == s_last_flare_time) return;
    float dt = GetFrameTime();
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.05f) dt = 0.05f;
    s_last_flare_time = now;
    for (int i = 0; i < MAX_PANEL_FLARES; i++) {
        PanelFlare *f = &s_panel_flares[i];
        if (!f->active) continue;
        float perimeter = panel_flare_perimeter(f->bounds);
        f->distance = panel_wrap_distance(f->distance + f->direction * f->speed * dt, perimeter);
        f->life -= dt;
        f->age += dt;
        if (f->life <= 0.0f) f->active = 0;
    }
}

static void panel_flare_assign(PanelFlare *f, Rectangle bounds, double now, int flash) {
    float perimeter = panel_flare_perimeter(bounds);
    int perimeter_steps = (int)perimeter;
    if (!f || perimeter_steps <= 0) return;
    f->bounds = bounds;
    f->direction = (rand() & 1) ? 1.0f : -1.0f;
    f->speed = 12.0f + (float)(rand() % 29);
    f->len = 20.0f + (float)(rand() % 9);
    f->distance = (float)(rand() % perimeter_steps);
    f->life = perimeter / f->speed + 2.0f;
    f->age = 0.0f;
    f->shine_started = now;
    f->flash_active = flash;
    f->active = 1;
}

static PanelFlare *panel_flare_reusable_slot(void) {
    PanelFlare *free_slot = NULL;
    PanelFlare *oldest = NULL;
    for (int i = 0; i < MAX_PANEL_FLARES; i++) {
        PanelFlare *f = &s_panel_flares[i];
        if (!f->active && !free_slot) free_slot = f;
        if (f->active && (!oldest || f->age > oldest->age)) oldest = f;
    }
    return free_slot ? free_slot : oldest;
}

static void panel_flare_shine(Rectangle bounds, double now) {
    /* Flash every live dot together, then ensure the entered panel has one too. */
    int found_on_panel = 0;
    for (int i = 0; i < MAX_PANEL_FLARES; i++) {
        PanelFlare *f = &s_panel_flares[i];
        if (!f->active) continue;
        f->shine_started = now;
        f->flash_active = 1;
        if (panel_flare_matches(f, bounds)) found_on_panel = 1;
    }
    if (found_on_panel) return;
    panel_flare_assign(panel_flare_reusable_slot(), bounds, now, 1);
}

static void panel_flare_ensure(Rectangle bounds, double now) {
    for (int i = 0; i < MAX_PANEL_FLARES; i++) {
        if (panel_flare_matches(&s_panel_flares[i], bounds)) return;
    }
    panel_flare_assign(panel_flare_reusable_slot(), bounds, now, 0);
}

static Vector2 panel_perimeter_point(Rectangle r, float distance) {
    float width = r.width - 1.0f;
    float height = r.height - 1.0f;
    float perimeter = panel_flare_perimeter(r);
    distance = panel_wrap_distance(distance, perimeter);
    if (distance <= width) return (Vector2){ r.x + distance, r.y };
    distance -= width;
    if (distance <= height) return (Vector2){ r.x + width, r.y + distance };
    distance -= height;
    if (distance <= width) return (Vector2){ r.x + width - distance, r.y + height };
    distance -= width;
    return (Vector2){ r.x, r.y + height - distance };
}

static float panel_distance_to_corner(Rectangle r, float distance, float direction) {
    float width = r.width - 1.0f;
    float height = r.height - 1.0f;
    float perimeter = panel_flare_perimeter(r);
    float bottom_left = 2.0f * width + height;
    distance = panel_wrap_distance(distance, perimeter);
    const float epsilon = 0.0001f;
    if (direction > 0.0f) {
        if (distance < width - epsilon) return width - distance;
        if (distance < width + height - epsilon) return width + height - distance;
        if (distance < bottom_left - epsilon) return bottom_left - distance;
        return perimeter - distance;
    }
    if (distance > bottom_left + epsilon) return distance - bottom_left;
    if (distance > width + height + epsilon) return distance - (width + height);
    if (distance > width + epsilon) return distance - width;
    if (distance > epsilon) return distance;
    return distance + (perimeter - bottom_left);
}

static void panel_draw_streak(Rectangle bounds, float head, float direction,
                              float length, float thickness, Color color) {
    float perimeter = panel_flare_perimeter(bounds);
    if (perimeter <= 0.0f) return;
    float remaining = fminf(length, perimeter);
    float distance = panel_wrap_distance(head - direction * remaining, perimeter);
    int segments = 0;
    while (remaining > 0.01f && segments++ < 8) {
        float to_corner = panel_distance_to_corner(bounds, distance, direction);
        if (to_corner <= 0.001f) break;
        float step = fminf(remaining, to_corner);
        float next_distance = panel_wrap_distance(distance + direction * step, perimeter);
        Vector2 start = panel_perimeter_point(bounds, distance);
        Vector2 end = panel_perimeter_point(bounds, next_distance);
        if (fabsf(end.x - start.x) > 0.001f || fabsf(end.y - start.y) > 0.001f)
            DrawLineEx(start, end, thickness, color);
        distance = next_distance;
        remaining -= step;
    }
}

static void panel_flares_draw(Rectangle bounds, int ensure) {
    bounds = panel_flare_bounds(bounds);
    if (bounds.width < 40.0f || bounds.height < 3.0f) return;
    double now = GetTime();
    int hovered = s_panel_mouse_valid && CheckCollisionPointRec(s_panel_mouse, bounds);
    int entered = hovered && (!s_previous_panel_mouse_valid ||
                              !CheckCollisionPointRec(s_previous_panel_mouse, bounds));
    panel_flares_update(now);

    if (entered) panel_flare_shine(bounds, now);
    else if (ensure) panel_flare_ensure(bounds, now);

    int active = 0;
    for (int i = 0; i < MAX_PANEL_FLARES; i++)
        if (s_panel_flares[i].active) active++;
    if (!entered && !ensure && active < MAX_PANEL_FLARES && (rand() % 100) == 0) {
        for (int i = 0; i < MAX_PANEL_FLARES; i++) {
            if (!s_panel_flares[i].active) {
                panel_flare_assign(&s_panel_flares[i], bounds, now, 0);
                break;
            }
        }
    }

    for (int i = 0; i < MAX_PANEL_FLARES; i++) {
        PanelFlare *f = &s_panel_flares[i];
        if (!panel_flare_matches(f, bounds)) continue;
        float shine = 0.0f;
        if (f->flash_active) {
            double elapsed = now - f->shine_started;
            if (elapsed >= 0.0 && elapsed < PANEL_SHINE_DURATION)
                shine = 1.0f - (float)(elapsed / PANEL_SHINE_DURATION);
            else f->flash_active = 0;
        }
        unsigned char alpha = (unsigned char)(22.0f + 233.0f * shine);
        Color flare = (Color){ 255, 255, 255, alpha };
        panel_draw_streak(bounds, f->distance, f->direction, f->len,
                          1.0f + 3.0f * shine, flare);
    }
}

void ui_theme_draw_panel_flares(Rectangle r) {
    panel_flares_draw(r, 1);
}

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

    /* 3. Existing moving flares crawl around all four sides (max five globally). */
    if (rw >= 40) panel_flares_draw((Rectangle){ (float)rx, (float)ry, (float)rw, (float)rh }, 0);
}
