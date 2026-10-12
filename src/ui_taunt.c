#include "ui_taunt.h"
#include "ui_theme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern Font small_font;

typedef struct {
    Vector2 pos;
    Vector2 vel;
    float life;
    float max_life;
    float size;
    Color color;
} AshParticle;

static char s_active_taunt[256];
static float s_taunt_timer = 0.0f;
static int s_ash_spawned = 0;

#define MAX_ASH_PARTICLES 64
static AshParticle s_particles[MAX_ASH_PARTICLES];
static int s_particle_count = 0;

void taunt_init(void) {
    s_active_taunt[0] = '\0';
    s_taunt_timer = 0.0f;
    s_ash_spawned = 0;
    s_particle_count = 0;
}

void taunt_trigger(const char *text, float duration) {
    if (!text || text[0] == '\0') return;
    strncpy(s_active_taunt, text, sizeof(s_active_taunt) - 1);
    s_active_taunt[sizeof(s_active_taunt) - 1] = '\0';
    s_taunt_timer = (duration > 0.0f) ? duration : 3.5f;
    s_ash_spawned = 0;
}

void taunt_update(float dt) {
    Rectangle box = { 12.0f, 260.0f, 255.0f, 95.0f };

    if (s_taunt_timer > 0.0f) {
        s_taunt_timer -= dt;
        if (s_taunt_timer < 0.0f) s_taunt_timer = 0.0f;

        /* When taunt_timer enters last 0.5s, spawn 15-20 dark ash particles */
        if (s_taunt_timer <= 0.5f && !s_ash_spawned) {
            s_ash_spawned = 1;
            int to_spawn = 18;
            for (int i = 0; i < to_spawn && s_particle_count < MAX_ASH_PARTICLES; i++) {
                AshParticle *p = &s_particles[s_particle_count++];
                p->pos = (Vector2){ box.x + 8.0f + (float)(rand() % (int)(box.width - 16.0f)),
                                    box.y + 10.0f + (float)(rand() % (int)(box.height - 20.0f)) };
                p->vel = (Vector2){ ((float)(rand() % 60) - 30.0f), -((float)(rand() % 50) + 30.0f) };
                p->life = 0.6f + (float)(rand() % 40) / 100.0f;
                p->max_life = p->life;
                p->size = 3.0f + (float)(rand() % 4);
                p->color = (rand() % 2 == 0) ? (Color){ 20, 20, 20, 240 } : (Color){ 55, 55, 55, 220 };
            }
        }
    }

    /* Update ash particles */
    for (int i = 0; i < s_particle_count; i++) {
        AshParticle *p = &s_particles[i];
        p->pos.x += p->vel.x * dt;
        p->pos.y += p->vel.y * dt;
        p->life -= dt;
        if (p->life <= 0.0f) {
            s_particles[i] = s_particles[--s_particle_count];
            i--;
        }
    }
}

static void draw_text_wrap_box(Font font, const char *text, Rectangle r, float font_size, Color col) {
    char buf[256];
    strncpy(buf, text, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *word = strtok(buf, " ");
    char line[256] = "";
    float cur_y = r.y;
    float max_w = r.width;
    float lh = font_size + 4.0f;

    while (word != NULL) {
        char test[256];
        if (line[0] == '\0') snprintf(test, sizeof(test), "%s", word);
        else snprintf(test, sizeof(test), "%s %s", line, word);

        if (MeasureTextEx(font, test, font_size, 2.0f).x > max_w && line[0] != '\0') {
            DrawTextEx(font, line, (Vector2){ r.x, cur_y }, font_size, 2.0f, col);
            cur_y += lh;
            snprintf(line, sizeof(line), "%s", word);
        } else {
            strncpy(line, test, sizeof(line));
        }
        word = strtok(NULL, " ");
    }
    if (line[0] != '\0') {
        DrawTextEx(font, line, (Vector2){ r.x, cur_y }, font_size, 2.0f, col);
    }
}

void taunt_draw(void) {
    Rectangle box = { 12.0f, 260.0f, 255.0f, 95.0f };

    if (s_taunt_timer > 0.0f && s_active_taunt[0] != '\0') {
        float alpha = 1.0f;
        if (s_taunt_timer <= 0.5f) {
            alpha = s_taunt_timer / 0.5f;
        }

        Color bg = COLOR_PANEL_BG; bg.a = (unsigned char)(235 * alpha);
        Color border = g_theme.base; border.a = (unsigned char)(255 * alpha);
        Color title_col = COLOR_RED; title_col.a = (unsigned char)(255 * alpha);
        Color txt_col = COLOR_TEXT_INFO; txt_col.a = (unsigned char)(250 * alpha);
        Color dim = COLOR_PHOSPHOR_DIM; dim.a = (unsigned char)(180 * alpha);

        DrawRectangleRec(box, bg);
        ui_draw_panel_border(box, border);
        DrawText("[ CPU TRANSMISSION ]", (int)box.x + 8, (int)box.y + 7, 12, title_col);
        DrawLine((int)box.x + 6, (int)box.y + 24, (int)(box.x + box.width - 6), (int)box.y + 24, dim);

        Rectangle text_r = { box.x + 8.0f, box.y + 30.0f, box.width - 16.0f, box.height - 34.0f };
        draw_text_wrap_box(small_font, s_active_taunt, text_r, 14.0f, txt_col);
    }

    /* Draw dark ash particles */
    for (int i = 0; i < s_particle_count; i++) {
        const AshParticle *p = &s_particles[i];
        float p_alpha = p->life / p->max_life;
        Color c = p->color;
        c.a = (unsigned char)(c.a * p_alpha);
        DrawRectangleV(p->pos, (Vector2){ p->size, p->size }, c);
    }
}
