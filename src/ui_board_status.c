#include "ui_board_status.h"
#include "ui_theme.h"
#include <math.h>

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
        DrawText(">>", (int)tag.x + 4, (int)tag.y + 2, 14, (Color){ 0, 0, 0, 255 });
    }

    int font_size = 16;
    int text_x = (int)box.x + 6;
    int max_width = (int)box.width - 12;
    if (is_active) max_width = (int)box.width - 45;
    while (font_size > 12 && MeasureText(text, font_size) > max_width) font_size--;
    int text_y = (int)box.y + ((int)box.height - font_size) / 2;
    DrawText(text, text_x, text_y, font_size, label);
}

void board_draw_status_message(const char *text) {
    if (!text || text[0] == '\0') return;
    const int font_size = 20;
    int text_width = MeasureText(text, font_size);
    DrawText(text, (int)(710.0f - text_width * 0.5f), 529, font_size, COLOR_PHOSPHOR);
}
