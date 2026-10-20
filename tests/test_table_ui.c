/* Headless table tests using real engine, layout, input, animations and combo UI.
   Run ./tests/test_table_ui.sh (only raylib headers are needed). */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "ui_input.h"
#include "ui_combos.h"
#include "ui_motion.h"

Font font_body;
TerminalTheme g_theme;
static int pressed, released, key, right_click, records, last_target, glows;
static int sidebar, drawn_cards, saw_cup, saw_full_name;
static GameState g;
static DragState drag;
static AnimState anim;

bool IsMouseButtonPressed(int button) { return button == MOUSE_BUTTON_LEFT ? pressed : right_click; }
bool IsMouseButtonReleased(int button) { return button == MOUSE_BUTTON_LEFT && released; }
bool IsKeyPressed(int k) { return key == k; }
bool CheckCollisionPointRec(Vector2 p, Rectangle r) {
    return p.x >= r.x && p.x <= r.x + r.width && p.y >= r.y && p.y <= r.y + r.height;
}
Vector2 motion_get_pos(int id) { (void)id; return (Vector2){ 0, 0 }; }
Rectangle motion_get_rect(int id) { (void)id; return (Rectangle){ 500, 200, 76, 124 }; }
void replay_rec_play(int player, int card, int target) {
    assert(player == 0 && card >= 0 && card < 48);
    records++; last_target = target;
}
void DrawRectangleLinesEx(Rectangle r, float thick, Color color) { (void)r; (void)thick; (void)color; glows++; }
Vector2 MeasureTextEx(Font f, const char *text, float size, float spacing) {
    (void)f;
    return (Vector2){ strlen(text) * (size * 0.6f + spacing), size };
}
const char *TextFormat(const char *fmt, ...) {
    static char text[512];
    va_list args; va_start(args, fmt); vsnprintf(text, sizeof(text), fmt, args); va_end(args);
    return text;
}
void DrawTextEx(Font f, const char *text, Vector2 pos, float size, float spacing, Color c) {
    (void)c;
    Vector2 bounds = MeasureTextEx(f, text, size, spacing);
    assert(pos.x >= 0 && pos.y >= 0 && pos.x + bounds.x <= 1280 && pos.y + size <= 720);
    if (sidebar) {
        assert(pos.x >= 1168 && pos.x + bounds.x <= 1264);
        /* Every row is inside mode / CPU / human bounds, above pause and restart. */
        int bottom = pos.y < 88 ? 78 : pos.y < 316 ? 306 : 534;
        assert(pos.y + size <= bottom);
    }
    if (strstr(text, "Ino-shika-cho (Boar-Deer-Butterfly)")) saw_full_name = 1;
}
void DrawRectangleRec(Rectangle r, Color c) { (void)r; (void)c; }
void DrawRectangle(int x, int y, int w, int h, Color c) { (void)x; (void)y; (void)w; (void)h; (void)c; }
void ui_theme_draw_panel_flares(Rectangle r) { (void)r; }
void ui_draw_panel_border(Rectangle r, Color c) { (void)r; (void)c; }
void __wrap_board_draw_card(const BoardAssets *a, int id, Rectangle r) {
    (void)a;
    assert(id >= 0 && id < 48);
    assert(r.x >= 340 && r.x + r.width <= 1100 && r.y >= 220 && r.y + r.height < 610);
    drawn_cards++; if (id == 32) saw_cup = 1;
}

static Vector2 center(Rectangle r) { return (Vector2){ r.x + r.width / 2, r.y + r.height / 2 }; }
static void input(Vector2 p, int down, int up) {
    int hover;
    pressed = down; released = up;
    input_handle_mouse(&g, &drag, &anim, (Letterbox){ 1, 0, 0 }, p, &hover, 1);
    pressed = released = key = right_click = 0;
}
static void click(Vector2 p) { input(p, 1, 0); input(p, 0, 1); }
static Vector2 hand(int index) { return center(board_get_hand_card_rect(index, g.hand_count[0], 0)); }
static void setup(int matches) {
    engine_init(&g, RULESET_KK_ADD_10, 2, 42);
    memset(&drag, 0, sizeof(drag)); anim_init(&anim);
    g.current_player = 0; g.phase = PHASE_TURN_HAND;
    g.hand_count[0] = 2; g.hand[0][0] = 0; g.hand[0][1] = 4;
    g.hand_count[1] = 1; g.hand[1][0] = 8;
    /* Unsorted field verifies click hit testing uses actual painted slots. */
    g.field_count = matches + 1; g.field[0] = 12;
    for (int i = 0; i < matches; i++) g.field[i + 1] = i + 1;
    g.stock_count = 1; g.stock[0] = 16;
    records = glows = 0; last_target = -2;
}
static void test_clicks_and_drag(void) {
    for (int n = 1; n <= 3; n++) {
        setup(n);
        click(hand(0));
        assert(drag.selected == 1 && !drag.on && records == 0);
        input_draw_selection(&g, &drag); assert(glows == n + 1);
        click(center(board_get_field_card_rect_of(&g, 12))); /* wrong month */
        assert(records == 0 && drag.selected == 1);
        click((Vector2){ 450, 490 }); /* empty field cannot discard a matching card */
        assert(records == 0 && drag.selected == 1);
        click(center(board_get_field_card_rect_of(&g, n)));
        assert(records == 1 && !drag.selected && g.hand_count[0] == 1);
        assert(g.captured_count[0] == (n == 3 ? 4 : 2));
        assert(anim.type == ANIM_CAPTURE_FLY);
        if (n == 2) assert(last_target == 2);
    }
    setup(0); click(hand(0)); click((Vector2){ 450, 490 });
    assert(records == 1 && last_target == -1 && g.field_count == 2);

    setup(1); click(hand(0)); click(hand(1));
    assert(drag.selected == 2); click(hand(1)); assert(!drag.selected);
    click(hand(0)); key = KEY_ESCAPE; input(hand(0), 0, 0); assert(!drag.selected && !records);
    click(hand(0)); right_click = 1; input(hand(0), 0, 0); assert(!drag.selected);
    /* Small pointer jitter is a click, not a dragged play. */
    Vector2 p = hand(0); input(p, 1, 0); p.x += 8; input(p, 0, 1);
    assert(drag.selected == 1 && !records);

    setup(2); input(hand(0), 1, 0);
    input(center(board_get_field_card_rect_of(&g, 2)), 0, 1);
    assert(records == 1 && last_target == 2 && !drag.selected);
    setup(1); input(hand(0), 1, 0); input((Vector2){ 20, 20 }, 0, 1);
    assert(!records && anim.type == ANIM_GLIDE_RETURN);
    setup(1); input(hand(0), 1, 0); key = KEY_ESCAPE; input((Vector2){ 600, 300 }, 0, 1);
    assert(!records && !drag.on && !drag.selected);

    setup(1); click(hand(0)); g.current_player = 1; input(hand(0), 1, 0);
    assert(!drag.selected && !records);
    setup(1); click(hand(0)); g.phase = PHASE_ROUND_END; input(hand(0), 1, 0);
    assert(!drag.selected && !records);
    setup(1); click(hand(0));
    int hover; pressed = 1;
    input_handle_mouse(&g, &drag, &anim, (Letterbox){ 1, 0, 0 },
                       center(board_get_field_card_rect_of(&g, 1)), &hover, 0);
    pressed = 0; assert(!records && drag.selected == 1);
}
static void test_combos(void) {
    assert(combos_panel_hit((Vector2){ 1200, 100 }) == 1);
    assert(combos_panel_hit((Vector2){ 1200, 400 }) == 0);
    assert(combos_panel_hit((Vector2){ 1200, 580 }) == -1);
    assert(combos_panel_hit((Vector2){ 900, 400 }) == -1);
    BoardAssets assets = {0};
    for (int rule = 0; rule < 5; rule++) {
        setup(0); g.ruleset = rule;
        for (int n = 0; n <= 48; n++) {
            for (int p = 0; p < 2; p++) {
                g.captured_count[p] = n; g.koi_called[p] = 9;
                for (int i = 0; i < n; i++) g.captured[p][i] = i;
            }
            GameState before = g;
            sidebar = 1; combos_draw_sidebar(&g); sidebar = 0;
            ScoreResult s = score_captures(g.ruleset, g.captured[0], n);
            int page = 0, pages = (s.yaku_count + 2) / 3;
            if (!pages) pages = 1;
            for (int i = 0; i < pages; i++) {
                assert(page == i);
                combos_draw_details(&assets, &g, 0, page);
                combos_draw_details(&assets, &g, 1, page);
                pressed = 1;
                assert(!combos_handle_details(&g, 0, &page, center((Rectangle){980, 610, 120, 40})));
                pressed = 0;
            }
            assert(page == pages - 1); /* Last page cannot advance. */
            pressed = 1;
            assert(!combos_handle_details(&g, 0, &page, (Vector2){ 400, 400 }));
            assert(combos_handle_details(&g, 0, &page, (Vector2){ 1050, 88 }));
            while (page > 0) combos_handle_details(&g, 0, &page, (Vector2){ 240, 630 });
            combos_handle_details(&g, 0, &page, (Vector2){ 240, 630 }); assert(page == 0);
            pressed = 0; key = KEY_ESCAPE;
            assert(combos_handle_details(&g, 0, &page, (Vector2){0,0})); key = KEY_P;
            assert(combos_handle_details(&g, 0, &page, (Vector2){0,0})); key = 0;
            assert(memcmp(&g, &before, sizeof(g)) == 0); /* Viewing never mutates play/timer. */
        }
    }
    assert(drawn_cards && saw_cup && saw_full_name);
}
int main(void) {
    test_clicks_and_drag(); test_combos();
    puts("All table input and combo panel tests passed.");
    return 0;
}
