/* Headless table tests using real engine, layout, input, animations, Koi-Koi overlay and combo UI.
   Run ./tests/test_table_ui.sh (only raylib headers are needed). */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "ui_input.h"
#include "ui_combos.h"
#include "ui_screens.h"
#include "ui_motion.h"
#include "ui_speed.h"
#include "campaign.h"

Font font_title, font_body;
TerminalTheme g_theme;
static int pressed, released, key, right_click, records, last_target, glows;
static int sidebar, decide_overlay, drawn_cards, saw_cup, saw_full_name;
static char decide_log[4096];
static size_t decide_len;
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
    if (decide_overlay) {
        /* Every element stays inside the {200,45,880,630} panel; yaku rows stay above divider y=430.
           small_font.ttf averages ~0.405 * size advance (verified via stb_truetype: 296.4 px at 15 px). */
        float est_w = (float)strlen(text) * (size * 0.45f + spacing);
        assert(pos.x >= 200 && pos.x + est_w <= 1080 && pos.y >= 45 && pos.y + size <= 675);
        if (pos.y >= 120 && pos.y < 430) assert(pos.y + size <= 430);
        int wrote = snprintf(decide_log + decide_len, sizeof(decide_log) - decide_len, "%s\n", text);
        if (wrote > 0 && decide_len + (size_t)wrote < sizeof(decide_log)) decide_len += (size_t)wrote;
    }
    if (strstr(text, "Ino-shika-cho (Boar-Deer-Butterfly)")) saw_full_name = 1;
}
void DrawRectangleRec(Rectangle r, Color c) { (void)r; (void)c; }
void DrawRectangle(int x, int y, int w, int h, Color c) { (void)x; (void)y; (void)w; (void)h; (void)c; }
void DrawLine(int x1, int y1, int x2, int y2, Color c) { (void)x1; (void)y1; (void)x2; (void)y2; (void)c; }
void ui_theme_draw_panel_flares(Rectangle r) { (void)r; }
void ui_theme_draw_panel_flares_keyed(Rectangle r, int key) { (void)r; (void)key; }
void ui_theme_draw_text_flares(Font f, const char *t, Vector2 p, float s, float sp, Color c) {
    (void)f; (void)t; (void)p; (void)s; (void)sp; (void)c;
}
void ui_draw_panel_border(Rectangle r, Color c) { (void)r; (void)c; }
void __wrap_board_draw_card(const BoardAssets *a, int id, Rectangle r) {
    (void)a;
    assert(id >= 0 && id < 48);
    if (decide_overlay) {
        /* Koi-Koi overlay mini cards sit in column 690..950 and strictly above divider y=430. */
        assert(r.x >= 690 && r.x + r.width <= 951 && r.y >= 122 && r.y + r.height <= 430.5f);
    } else {
        assert(r.x >= 340 && r.x + r.width <= 1100 && r.y >= 220 && r.y + r.height < 610);
    }
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
static void draw_decide(const BoardAssets *ba, const GameState *gs, Vector2 vm) {
    decide_log[0] = '\0';
    decide_len = 0;
    saw_cup = 0;
    decide_overlay = 1;
    screens_draw_decide_overlay(ba, gs, vm);
    decide_overlay = 0;
}
/* Real Koi-Koi decision screen UI (screens_draw_decide_overlay + screens_check_decide_click):
   every wiki yaku, 8-combo KK-ADD-10 stacking layout, Sake Cup Kasu rule, multipliers, and clicks. */
static void test_decide_overlay(void) {
    static const int p_goko[]     = { 0, 8, 28, 40, 44 };
    static const int p_shiko[]    = { 0, 8, 28, 44 };
    static const int p_ame[]      = { 0, 8, 28, 40 };
    static const int p_sanko[]    = { 0, 8, 28 };
    static const int p_hanami[]   = { 8, 32 };
    static const int p_tsukimi[]  = { 28, 32 };
    static const int p_ino[]      = { 20, 24, 36 };
    static const int p_akatan[]   = { 1, 5, 9 };
    static const int p_aotan[]    = { 21, 33, 37 };
    static const int p_both[]     = { 1, 5, 9, 21, 33, 37 };
    static const int p_tane[]     = { 4, 12, 16, 29, 41 };
    static const int p_tanzaku4[] = { 1, 13, 17, 25 };
    static const int p_kasu_cup[] = { 2, 3, 6, 7, 10, 11, 14, 15, 18, 32 };
    /* 25 distinct cards yielding all 8 simultaneous yakus possible in KK-ADD-10. */
    static const int p_all8[]     = {
        0, 8, 28, 40, 44,           /* Goko (10) + Curtain/Moon */
        32, 20, 24, 36, 4,          /* Viewings (5+5) + Ino-shika-cho (5) + Tane (1) + Cup */
        1, 5, 9, 21, 33, 37,        /* Akatan + Aotan (10) + Tanzaku (3) */
        2, 3, 6, 7, 10, 11, 14, 15, 18 /* + 9 Plains + Sake Cup = Kasu (1) -> Base 40 */
    };
    const struct { const int *pile; int n; const char *line; } cases[] = {
        { p_goko,     5,  "- Goko (Five Brights)" },
        { p_shiko,    4,  "- Shiko (Four Brights)" },
        { p_ame,      4,  "- Ame-shiko (Rainy Four Brights)" },
        { p_sanko,    3,  "- Sanko (Three Brights)" },
        { p_hanami,   2,  "- Hanami-zake (Cherry Viewing)" },
        { p_tsukimi,  2,  "- Tsukimi-zake (Moon Viewing)" },
        { p_ino,      3,  "- Ino-shika-cho (Boar-Deer-Butterfly)" },
        { p_akatan,   3,  "- Akatan (Red Poetry Ribbons)" },
        { p_aotan,    3,  "- Aotan (Blue Ribbons)" },
        { p_both,     6,  "- Akatan + Aotan (All Ribbons)" },
        { p_tane,     5,  "- Tane (Animals)" },
        { p_tanzaku4, 4,  "- Tanzaku (Ribbons)" },
        { p_kasu_cup, 10, "- Kasu (Plains)" },
    };
    BoardAssets assets = {0};

    /* 1. Every wiki yaku renders on the Koi-Koi overlay without mutating GameState. */
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        setup(0);
        g.ruleset = RULESET_KK_ADD_10;
        g.phase = PHASE_DECIDE;
        memcpy(g.captured[0], cases[i].pile, sizeof(int) * (size_t)cases[i].n);
        g.captured_count[0] = cases[i].n;
        GameState before = g;
        draw_decide(&assets, &g, (Vector2){ 440, 550 });
        assert(memcmp(&g, &before, sizeof(g)) == 0);
        assert(strstr(decide_log, "YAKU DETECTED") && strstr(decide_log, "KOI-KOI / SHOBU"));
        assert(strstr(decide_log, cases[i].line));
        int base = score_captures_base(RULESET_KK_ADD_10, cases[i].pile, cases[i].n);
        int award = calculate_award(base, 0, 0);
        char want[64];
        snprintf(want, sizeof(want), "IF YOU SHOBU NOW: %d POINTS", award);
        assert(strstr(decide_log, want));
    }

    /* 2. Sake Cup (id 32) appears in Kasu's drawn cards in KK-ADD-10, never in Nintendo. */
    setup(0);
    g.ruleset = RULESET_KK_ADD_10;
    g.phase = PHASE_DECIDE;
    memcpy(g.captured[0], p_kasu_cup, sizeof(p_kasu_cup));
    g.captured_count[0] = 10;
    draw_decide(&assets, &g, (Vector2){ 0, 0 });
    assert(saw_cup == 1);

    static const int p_nintendo_kasu_plus_cup[] = { 32, 2, 3, 6, 7, 10, 11, 14, 15, 18, 19 };
    g.ruleset = RULESET_NINTENDO;
    memcpy(g.captured[0], p_nintendo_kasu_plus_cup, sizeof(p_nintendo_kasu_plus_cup));
    g.captured_count[0] = 11;
    draw_decide(&assets, &g, (Vector2){ 0, 0 });
    assert(saw_cup == 0);

    /* 3. All 8 simultaneous KK-ADD-10 yakus fit above y=430 with all multipliers & cap text. */
    g.ruleset = RULESET_KK_ADD_10;
    memcpy(g.captured[0], p_all8, sizeof(p_all8));
    g.captured_count[0] = 25;
    g.koi_called[0] = 5; /* capped at x6 */
    g.koi_called[1] = 1; /* opponent Koi x2 */
    draw_decide(&assets, &g, (Vector2){ 840, 550 });
    assert(strstr(decide_log, "Base: 40 pts"));
    assert(strstr(decide_log, "x2  7+ Multiplier"));
    assert(strstr(decide_log, "x2  Opponent Koi"));
    assert(strstr(decide_log, "x6  Koi Streak (5 calls)"));
    assert(strstr(decide_log, "IF YOU SHOBU NOW: 960 POINTS"));
    assert(strstr(decide_log, "Streak is maxed at x6."));

    /* 4. Clicking KOI-KOI vs SHOBU vs outside on the real overlay drives engine_decide(). */
    DecisionType dec;
    assert(!screens_check_decide_click((Vector2){ 640, 300 }, &dec));
    assert(screens_check_decide_click((Vector2){ 440, 550 }, &dec) && dec == DECISION_KOI_KOI);
    assert(screens_check_decide_click((Vector2){ 840, 550 }, &dec) && dec == DECISION_SHOBU);
    assert(engine_decide(&g, dec) && g.phase == PHASE_ROUND_END && g.total_score[0] == 960);
}
static unsigned long long medal_mask(int bronze, int silver, int gold) {
    unsigned long long mask = 0;
    for (int i = 0; i < bronze; i++) mask |= 1ULL << (i * 3);
    for (int i = 0; i < silver; i++) mask |= 1ULL << (i * 3 + 1);
    for (int i = 0; i < gold; i++) mask |= 1ULL << (i * 3 + 2);
    return mask;
}
static void test_campaign_unlocks(void) {
    GameSettings settings = {0};
    settings.unlocked_opponents = 12; /* stale saves must not bypass medal gates */
    assert(campaign_update_unlocks(&settings) && settings.unlocked_opponents == 1);
    settings.cleared_mask = medal_mask(11, 0, 0);
    assert(campaign_update_unlocks(&settings) && settings.unlocked_opponents == 4);
    settings.cleared_mask = medal_mask(7, 6, 0);
    assert(campaign_update_unlocks(&settings) && settings.unlocked_opponents == 7);
    settings.cleared_mask = medal_mask(7, 6, 1);
    assert(campaign_update_unlocks(&settings) && settings.unlocked_opponents == 8);
}
/* KK-ADD-10 speed minigame: the minipoint math and the match-end conversion.
   A bonus-flipped win must reach total_score before any winner is decided. */
static void test_speed_minigame(void) {
    GameState t;
    memset(&t, 0, sizeof(t));
    t.ruleset = RULESET_KK_ADD_10;
    t.phase = PHASE_TURN_HAND;
    t.current_player = 0;
    t.current_round = 1;
    speed_bar_reset();

    /* An instant, dead-on catch is a perfect 10. */
    speed_bar_update(&t, 0.0f, 1);
    speed_bar_grade(1.0f);
    assert(speed_bar_total() == 10);

    /* A second perfect catch opens the serie: +10 base, +10 serie, +10 retro. */
    speed_bar_update(&t, 0.0f, 1);
    speed_bar_grade(1.0f);
    assert(speed_bar_total() == 40);

    /* A sloppy catch scores its share and breaks the serie. */
    speed_bar_update(&t, 0.0f, 1);
    speed_bar_grade(0.4f);
    assert(speed_bar_total() == 45);

    /* The next perfect only starts a new serie: +10. */
    speed_bar_update(&t, 0.0f, 1);
    speed_bar_grade(1.0f);
    assert(speed_bar_total() == 55);

    /* Match end: every full 10 minipoints becomes 1 human score point. */
    assert(speed_bar_apply_match_bonus(&t) == 5);
    assert(t.total_score[0] == 5);
    assert(speed_bar_apply_match_bonus(&t) == 0); /* applied only once */

    /* Other rulesets never receive the bonus. */
    GameState t2;
    memset(&t2, 0, sizeof(t2));
    t2.ruleset = RULESET_NINTENDO;
    speed_bar_reset();
    speed_bar_update(&t2, 0.0f, 1);
    speed_bar_grade(1.0f);
    assert(speed_bar_apply_match_bonus(&t2) == 0);
    assert(t2.total_score[0] == 0);
}

int main(void) {
    test_clicks_and_drag();
    test_combos();
    test_decide_overlay();
    test_campaign_unlocks();
    test_speed_minigame();
    puts("All table input, campaign unlock, combo panel, Koi-Koi overlay, and speed minigame tests passed.");
    return 0;
}
