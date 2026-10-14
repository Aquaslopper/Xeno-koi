/* Headless tooltip regression tests. Build with raylib headers, not its library:
   gcc -std=c99 -Wall -Wextra -Isrc -I/path/to/raylib/src \
       tests/test_card_tooltip.c src/ui_card_tooltip.c src/cards.c -lm -o /tmp/test_card_tooltip
*/
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ui_card_tooltip.h"
#include "ui_theme.h"

Font font_body;
TerminalTheme g_theme;
static int text_count;
static char lines[3][128];
static Rectangle last_panel;
static double now;
double GetTime(void) { return now; }

bool CheckCollisionPointRec(Vector2 p, Rectangle r) {
    return p.x >= r.x && p.x <= r.x + r.width && p.y >= r.y && p.y <= r.y + r.height;
}
Vector2 MeasureTextEx(Font font, const char *text, float size, float spacing) {
    (void)font; (void)spacing;
    return (Vector2){ strlen(text) * size / 2, size };
}
void DrawRectangleRec(Rectangle rect, Color color) { (void)color; last_panel = rect; }
void ui_draw_panel_border(Rectangle rect, Color color) { (void)rect; (void)color; }
void DrawTextEx(Font font, const char *text, Vector2 pos, float size, float spacing, Color tint) {
    (void)font; (void)pos; (void)size; (void)spacing;
    assert(tint.r == 255 && tint.g == 255 && tint.b == 255 && tint.a == 255);
    assert(text_count < 3);
    snprintf(lines[text_count++], sizeof(lines[0]), "%s", text);
}
static void draw(int expected) {
    text_count = 0;
    card_tooltip_draw();
    assert(text_count == expected);
}
int main(void) {
    Rectangle card = { 20, 20, 88, 143 };
    Vector2 mouse = { 30, 30 };
    card_tooltip_begin(1, mouse);
    card_tooltip_record(0, card);
    draw(0); /* No immediate popup. */
    now = 0.499;
    draw(0);
    now = 0.5;
    draw(3); /* Exactly 500 ms. */
    card_tooltip_begin(1, (Vector2){ 31, 31 });
    card_tooltip_record(0, card);
    draw(3); /* Movement within the same card / a new frame keeps it visible. */
    assert(strcmp(lines[0], "January / Pine") == 0);
    assert(strcmp(lines[1], "Bright / Hikari") == 0);
    assert(strcmp(lines[2], "Crane") == 0);

    /* Last drawn face-up card wins, including overlaps/animated rectangles. */
    card_tooltip_record(32, card);
    draw(0); /* Switching cards hides the old tooltip and restarts the delay. */
    now += 0.5;
    draw(3);
    assert(strcmp(lines[0], "September / Chrysanthemum") == 0);
    assert(strcmp(lines[1], "Animal / Tane") == 0);
    assert(strcmp(lines[2], "Sake Cup") == 0);
    card_tooltip_record(-1, card); /* Face-down cards hide metadata. */
    draw(0);

    card_tooltip_begin(0, mouse); /* Switching Easy off immediately hides. */
    card_tooltip_record(0, card);
    draw(0);
    card_tooltip_begin(1, (Vector2){ 200, 200 }); /* Leaving a card. */
    card_tooltip_record(0, card);
    draw(0);
    card_tooltip_begin(1, mouse);
    card_tooltip_record(0, card);
    draw(0); /* Re-entering starts a fresh delay. */
    now += 0.5;
    draw(3);
    card_tooltip_begin(0, mouse);
    draw(0);
    card_tooltip_begin(1, mouse);
    card_tooltip_record(0, card);
    draw(0); /* Toggling Easy off/on also restarts the delay. */
    now += 0.5;
    draw(3);
    card_tooltip_clear(); /* Modal foreground hides the underlying table. */
    draw(0);

    /* Every card has all three metadata lines; edge placement stays on canvas. */
    for (int id = 0; id < 48; ++id) {
        card_tooltip_begin(1, (Vector2){ 1270, 710 });
        card_tooltip_record(id, (Rectangle){ 1200, 650, 80, 70 });
        draw(0);
        now += 0.5;
        draw(3);
        assert(strstr(lines[0], " / "));
        assert(strstr(lines[1], " / "));
        assert(lines[2][0]);
        assert(last_panel.x >= 8 && last_panel.y >= 8);
        assert(last_panel.x + last_panel.width <= CANVAS_W - 8);
        assert(last_panel.y + last_panel.height <= CANVAS_H - 8);
    }
    card_tooltip_begin(1, mouse); /* A new frame never reuses stale hover. */
    draw(0);
    puts("All card tooltip tests passed.");
    return 0;
}
