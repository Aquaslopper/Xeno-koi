#include "ui_board_status.h"
#include "ui_theme.h"
#include <math.h>
#include <string.h>

extern Font font_title;
extern Font font_body;

static void draw_text_ex_snapped(Font font, const char *text, Vector2 position,
                                 float font_size, float spacing, Color color) {
    position.x = floorf(position.x);
    position.y = floorf(position.y);
    DrawTextEx(font, text, position, font_size, spacing, color);
}

static void draw_centered_text(Font font, const char *text, float center_x, float y,
                               float font_size, float spacing, Color color) {
    Vector2 text_size = MeasureTextEx(font, text, font_size, spacing);
    draw_text_ex_snapped(font, text, (Vector2){ center_x - text_size.x / 2.0f, y },
               font_size, spacing, color);
}

void board_draw_info_box(Rectangle box, const char *text, int is_active, float flash_alpha) {
    Color label = is_active ? COLOR_TEXT_INFO : COLOR_TEXT_INFO_DIM;
    float text_size = 16.0f;
    float max_width = box.width - 12.0f;
    float measured = MeasureTextEx(font_body, text, text_size, 1.0f).x;
    if (measured > max_width && measured > 0.0f) text_size *= max_width / measured;
    draw_text_ex_snapped(font_body, text,
        (Vector2){ box.x + 8.0f, box.y + (box.height - text_size) * 0.5f },
        text_size, 1.0f, label);

    /* Turn ownership is an editorial accent, not a glowing box around the score. */
    if (is_active) {
        Color accent = COLOR_PHOSPHOR;
        accent.a = (unsigned char)(150.0f + 105.0f * fmaxf(0.0f, fminf(1.0f, flash_alpha)));
        DrawLineEx((Vector2){ box.x + 1.0f, box.y + 5.0f },
                   (Vector2){ box.x + 1.0f, box.y + box.height - 5.0f }, 2.0f, accent);
        DrawLineEx((Vector2){ box.x + 4.0f, box.y + box.height - 2.0f },
                   (Vector2){ box.x + 44.0f + 30.0f * flash_alpha, box.y + box.height - 2.0f },
                   1.5f, accent);
    }
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
