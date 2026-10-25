#include "ui_theme.h"
#include <stdlib.h>
#include <string.h>
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

#define MIN_FLARES_PER_BOX 3
#define MAX_FLARES_PER_BOX 5
#define MAX_FLARE_BOXES 128

typedef struct {
    float distance;
    float direction;
    float speed;
    float len;
    float life;
    double shine_started;
    int flash_active;
    int active;
} PanelFlare;

typedef struct {
    Rectangle bounds;
    PanelFlare flares[MAX_FLARES_PER_BOX];
    double last_seen;
    float glow_floor;
    int key;
    int active;
} PanelFlareBox;

static PanelFlareBox s_panel_flare_boxes[MAX_FLARE_BOXES];
#define PANEL_SHINE_DURATION 0.4

static Vector2 s_panel_mouse;
static Vector2 s_previous_panel_mouse;
static int s_panel_mouse_valid;
static int s_previous_panel_mouse_valid;
static double s_last_flare_time = -1.0;
static float s_global_flare_glow = 0.0f;
static float s_global_flare_speed = 1.0f;

void ui_theme_set_global_flare_boost(float glow_floor, float speed_mult) {
    s_global_flare_glow = (glow_floor < 0.0f) ? 0.0f : ((glow_floor > 1.0f) ? 1.0f : glow_floor);
    s_global_flare_speed = (speed_mult < 0.1f) ? 1.0f : ((speed_mult > 5.0f) ? 5.0f : speed_mult);
}

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

static int panel_bounds_match(Rectangle a, Rectangle b) {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
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

static void panel_flare_box_rebind(PanelFlareBox *box, Rectangle bounds) {
    float old_perimeter = panel_flare_perimeter(box->bounds);
    float new_perimeter = panel_flare_perimeter(bounds);
    if (old_perimeter > 0.0f && new_perimeter > 0.0f && old_perimeter != new_perimeter) {
        float scale = new_perimeter / old_perimeter;
        for (int i = 0; i < MAX_FLARES_PER_BOX; i++) {
            PanelFlare *f = &box->flares[i];
            if (!f->active) continue;
            f->distance = panel_wrap_distance(f->distance * scale, new_perimeter);
            f->len = fminf(f->len * scale, new_perimeter * 0.25f);
        }
    }
    box->bounds = bounds;
}

static PanelFlareBox *panel_flare_box_get(Rectangle bounds, double now, int key) {
    PanelFlareBox *free_box = NULL;
    PanelFlareBox *oldest = NULL;
    for (int i = 0; i < MAX_FLARE_BOXES; i++) {
        PanelFlareBox *box = &s_panel_flare_boxes[i];
        int matches = box->active && (key ? box->key == key :
                                      box->key == 0 && panel_bounds_match(box->bounds, bounds));
        if (matches) {
            if (key) panel_flare_box_rebind(box, bounds);
            box->last_seen = now;
            return box;
        }
        if (!box->active && !free_box) free_box = box;
        if (box->active && (!oldest || box->last_seen < oldest->last_seen)) oldest = box;
    }
    if (!free_box) free_box = oldest;
    if (!free_box) return NULL;
    free_box->bounds = bounds;
    free_box->last_seen = now;
    free_box->glow_floor = 0.0f;
    free_box->key = key;
    free_box->active = 1;
    for (int i = 0; i < MAX_FLARES_PER_BOX; i++) free_box->flares[i].active = 0;
    return free_box;
}

static void panel_flares_update(double now) {
    if (now == s_last_flare_time) return;
    float dt = GetFrameTime();
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.05f) dt = 0.05f;
    s_last_flare_time = now;
    for (int b = 0; b < MAX_FLARE_BOXES; b++) {
        PanelFlareBox *box = &s_panel_flare_boxes[b];
        if (!box->active) continue;
        float perimeter = panel_flare_perimeter(box->bounds);
        for (int i = 0; i < MAX_FLARES_PER_BOX; i++) {
            PanelFlare *f = &box->flares[i];
            if (!f->active) continue;
            f->distance = panel_wrap_distance(f->distance + f->direction * f->speed * s_global_flare_speed * dt, perimeter);
            f->life -= dt;
            if (f->life <= 0.0f) f->active = 0;
        }
    }
}

static void panel_flare_assign(PanelFlare *f, Rectangle bounds, double now, int flash) {
    float perimeter = panel_flare_perimeter(bounds);
    int perimeter_steps = (int)perimeter;
    if (!f || perimeter_steps <= 0) return;
    f->direction = (rand() & 1) ? 1.0f : -1.0f;
    f->speed = (12.0f + (float)(rand() % 29)) * 0.80f;
    float requested_len = 19.0f + (float)(rand() % 9);
    f->len = fminf(requested_len, perimeter * 0.25f);
    f->distance = (float)(rand() % perimeter_steps);
    f->life = perimeter / f->speed + 2.0f;
    f->shine_started = now;
    f->flash_active = flash;
    f->active = 1;
}

static int panel_flare_count(const PanelFlareBox *box) {
    int count = 0;
    for (int i = 0; i < MAX_FLARES_PER_BOX; i++)
        if (box->flares[i].active) count++;
    return count;
}

static void panel_flare_ensure_minimum(PanelFlareBox *box, double now, int target_count, int flash) {
    int count = panel_flare_count(box);
    if (target_count > MAX_FLARES_PER_BOX) target_count = MAX_FLARES_PER_BOX;
    for (int i = 0; i < MAX_FLARES_PER_BOX && count < target_count; i++) {
        if (box->flares[i].active) continue;
        panel_flare_assign(&box->flares[i], box->bounds, now, flash);
        count++;
    }
}

static void panel_flare_shine(PanelFlareBox *box, double now) {
    for (int i = 0; i < MAX_FLARES_PER_BOX; i++) {
        PanelFlare *f = &box->flares[i];
        if (!f->active) continue;
        f->shine_started = now;
        f->flash_active = 1;
    }
}

static int panel_flare_box_shining(const PanelFlareBox *box, double now) {
    for (int i = 0; i < MAX_FLARES_PER_BOX; i++) {
        const PanelFlare *f = &box->flares[i];
        if (!f->active || !f->flash_active) continue;
        double elapsed = now - f->shine_started;
        if (elapsed >= 0.0 && elapsed < PANEL_SHINE_DURATION) return 1;
    }
    return 0;
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

static void panel_flares_draw(Rectangle r, int key) {
    Rectangle bounds = panel_flare_bounds(r);
    if (bounds.width < 12.0f || bounds.height < 12.0f) return;
    double now = GetTime();
    PanelFlareBox *box = panel_flare_box_get(bounds, now, key);
    if (!box) return;
    float glow_floor = (s_global_flare_glow > box->glow_floor) ? s_global_flare_glow : box->glow_floor;
    box->glow_floor = 0.0f;
    int held_card = (key > 0);
    int hovered = s_panel_mouse_valid && CheckCollisionPointRec(s_panel_mouse, bounds);
    int entered = hovered && (!s_previous_panel_mouse_valid ||
                              !CheckCollisionPointRec(s_previous_panel_mouse, bounds));
    int target_dots = MIN_FLARES_PER_BOX;
    if (held_card || glow_floor >= 0.65f) target_dots = MAX_FLARES_PER_BOX;
    else if (glow_floor >= 0.25f) target_dots = 4;
    panel_flares_update(now);
    panel_flare_ensure_minimum(box, now, target_dots, held_card);
    if (entered || held_card) panel_flare_shine(box, now);

    int count = panel_flare_count(box);
    if (!entered && !held_card && count < MAX_FLARES_PER_BOX && (rand() % 100) == 0) {
        int flash = panel_flare_box_shining(box, now);
        for (int i = 0; i < MAX_FLARES_PER_BOX; i++) {
            if (!box->flares[i].active) {
                panel_flare_assign(&box->flares[i], bounds, now, flash);
                break;
            }
        }
    }

    for (int i = 0; i < MAX_FLARES_PER_BOX; i++) {
        PanelFlare *f = &box->flares[i];
        if (!f->active) continue;
        float shine = 0.0f;
        if (held_card) {
            shine = 1.0f;
        } else if (f->flash_active) {
            double elapsed = now - f->shine_started;
            if (elapsed >= 0.0 && elapsed < PANEL_SHINE_DURATION)
                shine = 1.0f - (float)(elapsed / PANEL_SHINE_DURATION);
            else f->flash_active = 0;
        }
        if (glow_floor > shine) shine = glow_floor;
        unsigned char alpha = (unsigned char)(22.0f + 233.0f * shine);
        Color flare = (Color){ 255, 255, 255, alpha };
        panel_draw_streak(bounds, f->distance, f->direction, f->len,
                          1.0f + 2.0f * shine, flare);
    }
}

void ui_theme_set_panel_flare_glow(Rectangle r, float glow) {
    Rectangle bounds = panel_flare_bounds(r);
    if (bounds.width < 12.0f || bounds.height < 12.0f) return;
    if (glow < 0.0f) glow = 0.0f;
    if (glow > 1.0f) glow = 1.0f;
    PanelFlareBox *box = panel_flare_box_get(bounds, GetTime(), 0);
    if (box && glow > box->glow_floor) box->glow_floor = glow;
}

void ui_theme_draw_panel_flares(Rectangle r) {
    panel_flares_draw(r, 0);
}

void ui_theme_draw_panel_flares_keyed(Rectangle r, int key) {
    if (key != 0) panel_flares_draw(r, key);
}

#define GLYPH_LOOP_PTS   64
#define MAX_GLYPH_LOOPS  3
#define MAX_TRACE_DIM    128

typedef struct {
    Vector2 pts[GLYPH_LOOP_PTS];
    float perim;
} GlyphContourLoop;

typedef struct {
    int built;
    int loop_count;
    GlyphContourLoop loops[MAX_GLYPH_LOOPS];
} GlyphContourEntry;

static GlyphContourEntry s_glyph_contours[96];
static unsigned int s_glyph_font_tex_id = 0;
static int s_glyph_font_base_size = 0;

static int glyph_pixel_inside(const Image *img, int x, int y) {
    if (!img || !img->data || x < 0 || y < 0 || x >= img->width || y >= img->height) return 0;
    const unsigned char *data = (const unsigned char *)img->data;
    if (img->format == PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA)
        return data[(y * img->width + x) * 2 + 1] >= 110;
    if (img->format == PIXELFORMAT_UNCOMPRESSED_GRAYSCALE)
        return data[y * img->width + x] >= 110;
    if (img->format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8)
        return data[(y * img->width + x) * 4 + 3] >= 110;
    return 0;
}

static const GlyphContourEntry *glyph_contours_get(const Font *font, int idx) {
    if (!font || !font->glyphs || idx < 0 || idx >= 96 || idx >= font->glyphCount) return NULL;
    if (font->texture.id != s_glyph_font_tex_id || font->baseSize != s_glyph_font_base_size) {
        memset(s_glyph_contours, 0, sizeof(s_glyph_contours));
        s_glyph_font_tex_id = font->texture.id;
        s_glyph_font_base_size = font->baseSize;
    }
    GlyphContourEntry *entry = &s_glyph_contours[idx];
    if (entry->built) return entry;
    entry->built = 1;
    entry->loop_count = 0;

    const Image *img = &font->glyphs[idx].image;
    int gw = img->width, gh = img->height;
    if (!img->data || gw <= 1 || gh <= 1 || gw > MAX_TRACE_DIM || gh > MAX_TRACE_DIM) return entry;

    int vw = gw + 1, vh = gh + 1;
    static unsigned char out_mask[(MAX_TRACE_DIM + 1) * (MAX_TRACE_DIM + 1)];
    memset(out_mask, 0, (size_t)(vw * vh));

    for (int y = 0; y < gh; y++) {
        for (int x = 0; x < gw; x++) {
            if (!glyph_pixel_inside(img, x, y)) continue;
            if (!glyph_pixel_inside(img, x, y - 1)) out_mask[y * vw + x] |= (1u << 0);
            if (!glyph_pixel_inside(img, x + 1, y)) out_mask[y * vw + (x + 1)] |= (1u << 1);
            if (!glyph_pixel_inside(img, x, y + 1)) out_mask[(y + 1) * vw + (x + 1)] |= (1u << 2);
            if (!glyph_pixel_inside(img, x - 1, y)) out_mask[(y + 1) * vw + x] |= (1u << 3);
        }
    }

    static const int dx[4] = { 1, 0, -1, 0 };
    static const int dy[4] = { 0, 1, 0, -1 };
    static Vector2 raw[1024];
    static Vector2 tmp[1024];
    static float cum[1025];

    for (int sy = 0; sy < vh && entry->loop_count < MAX_GLYPH_LOOPS; sy++) {
        for (int sx = 0; sx < vw && entry->loop_count < MAX_GLYPH_LOOPS; sx++) {
            if (!out_mask[sy * vw + sx]) continue;
            int cx = sx, cy = sy, n = 0;
            while (n < 1024) {
                unsigned char m = out_mask[cy * vw + cx];
                if (!m) break;
                int dir = (m & 1u) ? 0 : (m & 2u) ? 1 : (m & 4u) ? 2 : 3;
                out_mask[cy * vw + cx] &= (unsigned char)~(1u << dir);
                raw[n++] = (Vector2){ (float)cx, (float)cy };
                cx += dx[dir];
                cy += dy[dir];
                if (cx == sx && cy == sy) break;
            }
            if (n < 14) continue;
            for (int pass = 0; pass < 2; pass++) {
                for (int i = 0; i < n; i++) {
                    Vector2 p_prev = raw[(i + n - 1) % n];
                    Vector2 p_curr = raw[i];
                    Vector2 p_next = raw[(i + 1) % n];
                    tmp[i].x = 0.25f * p_prev.x + 0.50f * p_curr.x + 0.25f * p_next.x;
                    tmp[i].y = 0.25f * p_prev.y + 0.50f * p_curr.y + 0.25f * p_next.y;
                }
                memcpy(raw, tmp, (size_t)n * sizeof(Vector2));
            }
            cum[0] = 0.0f;
            for (int i = 0; i < n; i++) {
                Vector2 a = raw[i], b = raw[(i + 1) % n];
                cum[i + 1] = cum[i] + hypotf(b.x - a.x, b.y - a.y);
            }
            float total_len = cum[n];
            if (total_len < 10.0f) continue;

            GlyphContourLoop *loop = &entry->loops[entry->loop_count++];
            loop->perim = total_len;
            int seg = 0;
            for (int k = 0; k < GLYPH_LOOP_PTS; k++) {
                float target = (total_len * (float)k) / (float)GLYPH_LOOP_PTS;
                while (seg + 1 < n && cum[seg + 1] < target) seg++;
                float seg_len = cum[seg + 1] - cum[seg];
                float t = (seg_len > 0.0001f) ? ((target - cum[seg]) / seg_len) : 0.0f;
                Vector2 a = raw[seg], b = raw[(seg + 1) % n];
                loop->pts[k].x = a.x + (b.x - a.x) * t;
                loop->pts[k].y = a.y + (b.y - a.y) * t;
            }
        }
    }
    return entry;
}

static Vector2 glyph_loop_sample(const GlyphContourLoop *loop, float u, Vector2 origin, float scale) {
    u = u - floorf(u);
    float f = u * (float)GLYPH_LOOP_PTS;
    int i0 = ((int)f) & (GLYPH_LOOP_PTS - 1);
    int i1 = (i0 + 1) & (GLYPH_LOOP_PTS - 1);
    float frac = f - floorf(f);
    float gx = loop->pts[i0].x + (loop->pts[i1].x - loop->pts[i0].x) * frac;
    float gy = loop->pts[i0].y + (loop->pts[i1].y - loop->pts[i0].y) * frac;
    return (Vector2){ origin.x + gx * scale, origin.y + gy * scale };
}

void ui_theme_draw_text_flares(Font font, const char *text, Vector2 pos, float font_size, float spacing, Color col) {
    if (!text || !text[0] || font.texture.id == 0 || font.baseSize <= 0 || !font.glyphs || !font.recs) return;
    float scale = font_size / (float)font.baseSize;
    float now = (float)GetTime();
    int len = TextLength(text);
    float off_x = 0.0f, off_y = 0.0f;
    int char_seq = 0;

    for (int i = 0; i < len;) {
        int cp_bytes = 0;
        int cp = GetCodepointNext(&text[i], &cp_bytes);
        int idx = GetGlyphIndex(font, cp);
        if (cp == '\n') {
            off_y += font_size;
            off_x = 0.0f;
        } else {
            if (cp != ' ' && cp != '\t') {
                const GlyphContourEntry *entry = glyph_contours_get(&font, idx);
                if (entry && entry->loop_count > 0) {
                    Vector2 g_origin = {
                        pos.x + off_x + (float)font.glyphs[idx].offsetX * scale,
                        pos.y + off_y + (float)font.glyphs[idx].offsetY * scale
                    };
                    Rectangle g_rect = {
                        g_origin.x - 6.0f, g_origin.y - 6.0f,
                        font.recs[idx].width * scale + 12.0f,
                        font.recs[idx].height * scale + 12.0f
                    };
                    int hovered = s_panel_mouse_valid && CheckCollisionPointRec(s_panel_mouse, g_rect);

                    for (int l = 0; l < entry->loop_count; l++) {
                        const GlyphContourLoop *loop = &entry->loops[l];
                        float s_perim = loop->perim * scale;
                        if (s_perim < 16.0f) continue;
                        int n_flares = (s_perim >= 230.0f) ? 3 : ((s_perim >= 95.0f) ? 2 : 1);
                        for (int d = 0; d < n_flares; d++) {
                            unsigned int h = (unsigned int)(char_seq * 131 + l * 53 + d * 97 + idx * 17);
                            float dir = ((h & 1u) == 0u) ? 1.0f : -1.0f;
                            float spd = (12.0f + (float)(h % 21u)) * 0.80f;
                            float phase = (float)((h * 37u) % 1000u) * 0.001f;
                            float u_head = phase + dir * (spd * now) / s_perim;

                            /* Base state is 1px; ever so often (or on hover) grow by +1px to shine in the font's color. */
                            float period = 5.5f + (float)(h % 45u) * 0.10f;
                            float cycle_t = fmodf(now + (float)((h * 73u) % 1000u) * 0.01f, period);
                            float shine = 0.0f;
                            if (hovered) {
                                shine = 1.0f;
                            } else if (cycle_t < (float)PANEL_SHINE_DURATION) {
                                shine = sinf((cycle_t / (float)PANEL_SHINE_DURATION) * 3.14159265f);
                            }

                            float flen = fminf(12.0f + 0.05f * font_size, s_perim * 0.16f);
                            float thick = 1.0f + 1.0f * shine;
                            int segs = 4;
                            for (int s = 0; s < segs; s++) {
                                float t0 = (float)s / (float)segs;
                                float t1 = (float)(s + 1) / (float)segs;
                                float u0 = u_head - dir * (1.0f - t0) * (flen / s_perim);
                                float u1 = u_head - dir * (1.0f - t1) * (flen / s_perim);
                                Vector2 p0 = glyph_loop_sample(loop, u0, g_origin, scale);
                                Vector2 p1 = glyph_loop_sample(loop, u1, g_origin, scale);
                                unsigned char a = (unsigned char)((55.0f + 200.0f * shine) * (0.35f + 0.65f * t1));
                                Color c = { col.r, col.g, col.b, a };
                                DrawLineEx(p0, p1, thick, c);
                            }
                        }
                    }
                }
                char_seq++;
            }
            if (font.glyphs[idx].advanceX == 0)
                off_x += ((float)font.recs[idx].width * scale + spacing);
            else
                off_x += ((float)font.glyphs[idx].advanceX * scale + spacing);
        }
        i += cp_bytes;
    }
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

    /* 3. Each bordered box gets its own three-to-five perimeter dots. */
    panel_flares_draw((Rectangle){ (float)rx, (float)ry, (float)rw, (float)rh }, 0);
}
