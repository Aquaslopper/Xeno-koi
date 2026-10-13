#include "ui_board_status.h"
#include "ui_theme.h"
#include <math.h>
#include <string.h>

extern Font font_title;
extern Font font_body;

static void draw_centered_text(Font font, const char *text, float center_x, float y,
                               float font_size, float spacing, Color color) {
    Vector2 text_size = MeasureTextEx(font, text, font_size, spacing);
    DrawTextEx(font, text, (Vector2){ center_x - text_size.x / 2.0f, y },
               font_size, spacing, color);
}

void board_draw_info_box(Rectangle box, const char *text, int is_active, float flash_alpha) {
    Color panel = COLOR_PANEL_BG;
    panel.a = is_active ? 205 : 112;
    DrawRectangleRec(box, panel);

    Color label = is_active ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM;
    if (!is_active) {
        Color border = COLOR_PHOSPHOR_DIM;
        border.a = 145;
        label.a = 165;
        ui_draw_panel_border(box, border);
    } else {
        float breathe = 0.5f + 0.5f * sinf((float)GetTime() * 4.0f);
        float glow_alpha = 0.5f + 0.5f * breathe;
        const float outset[5] = { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f };
        const float thickness[5] = { 3.0f, 2.7f, 2.4f, 2.0f, 1.6f };
        const float strength[5] = { 1.0f, 0.88f, 0.74f, 0.60f, 0.48f };
        for (int i = 0; i < 5; i++) {
            Color ring = COLOR_PHOSPHOR;
            ring.a = (unsigned char)(255.0f * glow_alpha * strength[i]);
            Rectangle r = { box.x - outset[i], box.y - outset[i],
                             box.width + outset[i] * 2.0f, box.height + outset[i] * 2.0f };
            DrawRectangleLinesEx(r, thickness[i], ring);
        }
        if (flash_alpha > 0.001f) {
            Color flash = COLOR_PHOSPHOR;
            flash.a = (unsigned char)(255.0f * flash_alpha);
            DrawRectangleLinesEx((Rectangle){ box.x - 1, box.y - 1, box.width + 2, box.height + 2 }, 3.0f, flash);
        }

        Rectangle tag = { box.x + box.width - 31.0f, box.y + 5.0f, 25.0f, box.height - 10.0f };
        Color badge = COLOR_PHOSPHOR;
        badge.a = (unsigned char)(190.0f + 65.0f * breathe);
        DrawRectangleRec(tag, badge);
        DrawTextEx(font_title, ">>", (Vector2){ tag.x + 4.0f, tag.y + 2.0f }, 14.0f, 0.8f, (Color){ 0, 0, 0, 255 });
    }

    int font_size = 16;
    int text_x = (int)box.x + 6;
    int max_width = (int)box.width - 12;
    if (is_active) max_width = (int)box.width - 45;
    while (font_size > 12 && MeasureTextEx(font_body, text, (float)font_size, 1.0f).x > (float)max_width) font_size--;
    int text_y = (int)box.y + ((int)box.height - font_size) / 2;
    DrawTextEx(font_body, text, (Vector2){ (float)text_x, (float)text_y },
               (float)font_size, 1.0f, label);
}

void board_draw_status_message(const char *text) {
    if (!text || text[0] == '\0') return;
    int is_callout = strstr(text, "KOI-KOI") != NULL || strstr(text, "SHOBU") != NULL ||
                     strstr(text, "YAKU DETECTED") != NULL;
    Font font = is_callout ? font_title : font_body;
    const float font_size = is_callout ? 20.0f : 18.0f;
    const float spacing = is_callout ? 1.1f : 1.2f;
    draw_centered_text(font, text, 710.0f, 529.0f, font_size, spacing, COLOR_PHOSPHOR);
}
