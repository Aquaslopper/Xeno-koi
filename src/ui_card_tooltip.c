#include "ui_card_tooltip.h"
#include "cards.h"
#include "ui_theme.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>

/* DOM tooltips cannot appear above a fullscreen canvas. Use the same
   non-interactive Raylib panel there (and on desktop) instead. */
EM_JS(int, tooltip_web_update, (const char *month, const char *type, const char *name,
                               int red, int green, int blue), {
    var tip = document.getElementById('card-tooltip');
    var canvas = Module.canvas;
    if (!tip || !canvas) return 0;
    if (!month || document.fullscreenElement === canvas) {
        tip.hidden = true;
        return document.fullscreenElement === canvas ? 0 : 1;
    }
    if (!canvas.matches(':hover') || !document.hasFocus()) {
        tip.hidden = true;
        return 1;
    }
    tip.children[0].textContent = UTF8ToString(month);
    tip.children[1].textContent = UTF8ToString(type);
    tip.children[2].textContent = UTF8ToString(name);
    tip.style.color = '#fff';
    tip.style.borderColor = 'rgb(' + red + ',' + green + ',' + blue + ')';
    tip.hidden = false;
    var p = window.XenoKoiTooltipPointer;
    if (!p) { tip.hidden = true; return 1; }
    var x = p.x + 16, y = p.y + 18;
    if (x + tip.offsetWidth > window.innerWidth - 8) x = p.x - tip.offsetWidth - 16;
    if (y + tip.offsetHeight > window.innerHeight - 8) y = p.y - tip.offsetHeight - 18;
    tip.style.left = Math.max(8, x) + 'px';
    tip.style.top = Math.max(8, y) + 'px';
    return 1;
});
#endif

extern Font font_body;
static int tooltip_enabled;
static int hovered_card = -1;
static Vector2 tooltip_mouse;
/* Wait half a second on the same card, but never delay hiding. */
static const double TOOLTIP_HOVER_DELAY = 0.5;
static int waiting_card = -1;
static double hover_started;

void card_tooltip_begin(int enabled, Vector2 mouse) {
    tooltip_enabled = enabled;
    tooltip_mouse = mouse;
    card_tooltip_clear();
}

void card_tooltip_clear(void) {
    hovered_card = -1;
}

void card_tooltip_record(int card_id, Rectangle bounds) {
    if (!tooltip_enabled || bounds.width <= 0 || bounds.height <= 0) return;
    if (CheckCollisionPointRec(tooltip_mouse, bounds)) {
        /* Last drawn card wins. A face-down card (-1) occludes earlier cards
           without leaking the opponent's hand or the stock. */
        hovered_card = (unsigned)card_id < 48u ? card_id : -1;
    }
}

void card_tooltip_draw(void) {
    static const char *const months[12] = {
        "January / Pine", "February / Plum", "March / Cherry",
        "April / Wisteria", "May / Iris", "June / Peony",
        "July / Clover", "August / Pampas", "September / Chrysanthemum",
        "October / Maple", "November / Willow", "December / Paulownia"
    };
    static const char *const types[4] = {
        "Bright / Hikari", "Animal / Tane", "Ribbon / Tanzaku", "Plain / Kasu"
    };
    int candidate = tooltip_enabled ? hovered_card : -1;
    double now = GetTime();
    /* Resolve only after all cards/overlays are drawn. Clearing the per-frame
       hit test must not restart the delay while hovering the same card. */
    if (candidate != waiting_card) {
        waiting_card = candidate;
        hover_started = now;
    }
    if (candidate < 0 || now - hover_started < TOOLTIP_HOVER_DELAY) {
#ifdef __EMSCRIPTEN__
        tooltip_web_update(NULL, NULL, NULL, 0, 0, 0);
#endif
        return;
    }
    const Card *card = &CARDS[candidate];
    const char *month = months[card->month - 1];
    const char *type = types[card->type];
    /* CARDS is authoritative; strip only its internal ID prefix. */
    const char *name = strchr(card->name, ' ');
    name = name ? name + 1 : card->name;
#ifdef __EMSCRIPTEN__
    if (tooltip_web_update(month, type, name,
                           COLOR_PHOSPHOR.r, COLOR_PHOSPHOR.g, COLOR_PHOSPHOR.b)) return;
#endif
    float width = fmaxf(MeasureTextEx(font_body, month, 16, 1).x,
                       fmaxf(MeasureTextEx(font_body, type, 16, 1).x,
                             MeasureTextEx(font_body, name, 16, 1).x)) + 24;
    float height = 76;
    float x = tooltip_mouse.x + 16, y = tooltip_mouse.y + 18;
    if (x + width > CANVAS_W - 8) x = tooltip_mouse.x - width - 16;
    if (y + height > CANVAS_H - 8) y = tooltip_mouse.y - height - 18;
    Rectangle panel = { floorf(fmaxf(8, x)), floorf(fmaxf(8, y)), ceilf(width), height };
    DrawRectangleRec(panel, (Color){ 5, 8, 10, 245 });
    ui_draw_panel_border(panel, COLOR_PHOSPHOR_DIM);
    DrawTextEx(font_body, month, (Vector2){ panel.x + 12, panel.y + 10 }, 16, 1, WHITE);
    DrawTextEx(font_body, type, (Vector2){ panel.x + 12, panel.y + 30 }, 16, 1, WHITE);
    DrawTextEx(font_body, name, (Vector2){ panel.x + 12, panel.y + 50 }, 16, 1, WHITE);
}
