#include "ui_board.h"
#include "ui_motion.h"
#include "ui_board_status.h"
#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

extern Font font_title;
extern Font font_body;
extern GameSettings settings;

static void draw_text_ex_snapped(Font font, const char *text, Vector2 pos, float fsz, float sp, Color col) {
    DrawTextEx(font, text, (Vector2){ floorf(pos.x), floorf(pos.y) }, fsz, sp, col);
}

static void draw_centered_text(Font font, const char *text, float cx, float y, float fsz, float sp, Color col) {
    Vector2 ts = MeasureTextEx(font, text, fsz, sp);
    draw_text_ex_snapped(font, text, (Vector2){ cx - ts.x / 2.0f, y }, fsz, sp, col);
}

static void draw_text_in_rect(Font font, const char *text, Rectangle b, float fsz, float sp, Color col) {
    Vector2 ts = MeasureTextEx(font, text, fsz, sp);
    draw_text_ex_snapped(font, text, (Vector2){ b.x + (b.width - ts.x) / 2.0f, b.y + (b.height - fsz) / 2.0f }, fsz, sp, col);
}

#define ASH_COUNT 50

typedef struct { Vector2 pos; float speed_y, sway_speed, sway_amp, size, alpha, phase; } AshParticle;
static AshParticle g_ash[ASH_COUNT];
static int g_ash_initialized = 0;

static void ash_init(void) {
    for (int i = 0; i < ASH_COUNT; i++) {
        g_ash[i].pos = (Vector2){ (float)(rand() % 1280), (float)(rand() % 720) };
        g_ash[i].speed_y = 12.0f + (float)(rand() % 24); g_ash[i].sway_speed = 0.8f + (float)(rand() % 100) / 70.0f;
        g_ash[i].sway_amp = 8.0f + (float)(rand() % 12); g_ash[i].size = 1.5f + (float)(rand() % 16) / 10.0f;
        g_ash[i].alpha = 0.18f + (float)(rand() % 25) / 100.0f; g_ash[i].phase = (float)(rand() % 628) / 100.0f;
    }
    g_ash_initialized = 1;
}

static void ash_draw_and_update(float dt) {
    if (!g_ash_initialized) ash_init();
    float cur_time = (float)GetTime();
    for (int i = 0; i < ASH_COUNT; i++) {
        AshParticle *p = &g_ash[i];
        p->pos.y -= p->speed_y * dt;
        if (p->pos.y < -10.0f) { p->pos.y = 730.0f + (float)(rand() % 30); p->pos.x = (float)(rand() % 1280); }
        if (p->alpha <= 0.0f) continue;
        float sway_x = p->pos.x + sinf(p->phase + cur_time * p->sway_speed) * p->sway_amp;
        DrawRectangle((int)sway_x, (int)p->pos.y, (int)p->size, (int)p->size, (Color){ 5, 5, 5, (unsigned char)(255 * p->alpha) });
    }
}

static char path_buf[512];
static const char* get_path(const char* sub) {
    const char *base = GetApplicationDirectory();
    if (base && base[0] != '\0') {
        snprintf(path_buf, sizeof(path_buf), "%s%s", base, sub);
        if (FileExists(path_buf)) return path_buf;
    }
    snprintf(path_buf, sizeof(path_buf), "./%s", sub);
    return path_buf;
}

void board_load_assets(BoardAssets *ba) {
    ba->atlas = LoadTexture(get_path("assets/atlas.png"));
    GenTextureMipmaps(&ba->atlas);
    SetTextureFilter(ba->atlas, TEXTURE_FILTER_TRILINEAR);
    ba->atlas_deus = LoadTexture(get_path("assets/atlas_deus.png"));
    if (ba->atlas_deus.id > 0) {
        GenTextureMipmaps(&ba->atlas_deus);
        SetTextureFilter(ba->atlas_deus, TEXTURE_FILTER_TRILINEAR);
    }

    Image img = LoadImage(get_path("assets/back.png"));
    if (img.data) {
        float crop_x = (img.width - 945) / 2.0f;
        ImageCrop(&img, (Rectangle){ crop_x > 0 ? crop_x : 0, 0, 945, (float)img.height });
        ImageResize(&img, ATLAS_CELL_W, ATLAS_CELL_H);
        ba->card_back = LoadTextureFromImage(img);
        UnloadImage(img);
        GenTextureMipmaps(&ba->card_back);
        SetTextureFilter(ba->card_back, TEXTURE_FILTER_TRILINEAR);
    }

    const char *bg_names[6] = {
        "assets/harvest.jpg", "assets/hole.jpg", "assets/arctic.jpg",
        "assets/castle.jpg",  "assets/city.jpg", "assets/deus.png"
    };
    for (int i = 0; i < 6; i++) ba->bgs[i] = LoadTexture(get_path(bg_names[i]));

    ba->fog_shader = LoadShader(0, get_path("assets/depth_fog.fs"));
    ba->has_fog_shader = (ba->fog_shader.id > 0);
    if (ba->has_fog_shader) ba->fog_time_loc = GetShaderLocation(ba->fog_shader, "time");

    ba->current_bg = 0;
    ba->assets_loaded = 1;
}

void board_unload_assets(BoardAssets *ba) {
    if (!ba->assets_loaded) return;
    UnloadTexture(ba->atlas); UnloadTexture(ba->card_back);
    if (ba->atlas_deus.id > 0) UnloadTexture(ba->atlas_deus);
    for (int i = 0; i < 6; i++) UnloadTexture(ba->bgs[i]);
    if (ba->has_fog_shader) { UnloadShader(ba->fog_shader); ba->has_fog_shader = 0; }
    ba->assets_loaded = 0;
}

Rectangle board_get_hand_card_rect(int index, int total_cards, int hovered) {
    float step = 96.0f, total_w = (total_cards > 0) ? ((total_cards - 1) * step + HAND_CARD_W) : 0;
    return (Rectangle){ 710.0f - total_w / 2.0f + index * step, hovered ? 552.0f : 566.0f, HAND_CARD_W, HAND_CARD_H };
}

Rectangle board_get_field_card_rect(int index, int total_cards) {
    if (total_cards <= 0) return (Rectangle){ FIELD_X, 170.0f, FIELD_CARD_W, FIELD_CARD_H };
    int top_count = (total_cards + 1) / 2, bot_count = total_cards - top_count;
    int row = (index < top_count) ? 0 : 1, col = (row == 0) ? index : (index - top_count);
    int row_count = (row == 0) ? top_count : bot_count;
    float step = 96.0f, total_w = (row_count > 0) ? ((row_count - 1) * step + FIELD_CARD_W) : FIELD_CARD_W;
    if (total_w > (FIELD_W - 20.0f) && row_count > 1) {
        step = (FIELD_W - 20.0f - FIELD_CARD_W) / (float)(row_count - 1);
        total_w = (row_count - 1) * step + FIELD_CARD_W;
    }
    return (Rectangle){ FIELD_X + (FIELD_W - total_w) / 2.0f + col * step, (row == 0) ? 170.0f : 310.0f, FIELD_CARD_W, FIELD_CARD_H };
}

Vector2 board_get_card_field_pos(const GameState *g, int card_id) {
    if (card_id >= 0 && card_id < 48) {
        Vector2 p = motion_get_pos(card_id);
        if (p.x > 0.0f || p.y > 0.0f) return p;
    }
    for (int i = 0; i < g->field_count; i++) {
        if (g->field[i] == card_id) { Rectangle r = board_get_field_card_rect(i, g->field_count); return (Vector2){ r.x, r.y }; }
    }
    return (Vector2){ 710.0f, 240.0f };
}

Rectangle board_get_field_rect(void) { return (Rectangle){ FIELD_X, FIELD_Y, FIELD_W, FIELD_H }; }
Rectangle board_get_stock_rect(void) { return (Rectangle){ 330.0f, 262.0f, FIELD_CARD_W, FIELD_CARD_H }; }

void board_draw_card_shadow(Rectangle dest, float lift_amount) {
    if (lift_amount <= 0.01f) return;
    if (lift_amount > 1.0f) lift_amount = 1.0f;
    float off_x = 4.0f + lift_amount * 8.0f, off_y = 5.0f + lift_amount * 10.0f;
    DrawRectangleRec((Rectangle){ dest.x + off_x, dest.y + off_y, dest.width, dest.height }, (Color){ 0, 0, 0, (unsigned char)(255 * 0.40f * lift_amount) });
}

void board_draw_card_shimmer(Rectangle dest, float shimmer_val) {
    if (shimmer_val <= 0.5f || dest.width <= 0.0f || dest.height <= 0.0f) return;
    unsigned char top_a = (unsigned char)shimmer_val;
    if (top_a > 23) top_a = 23;
    unsigned char bot_a = (unsigned char)(top_a * 0.25f);
    BeginBlendMode(BLEND_ADDITIVE);
    DrawRectangleGradientV((int)dest.x, (int)dest.y, (int)dest.width, (int)dest.height,
                           (Color){ 255, 255, 255, top_a },
                           (Color){ 255, 255, 255, bot_a });
    EndBlendMode();
}

void board_draw_card(const BoardAssets *ba, int card_id, Rectangle dest) {
    if (card_id < 0 || card_id >= 48) return;
    Rectangle src = { (float)((card_id / 4) * ATLAS_CELL_W), (float)((card_id % 4) * ATLAS_CELL_H), (float)ATLAS_CELL_W, (float)ATLAS_CELL_H };
    Texture2D tex = (settings.difficulty == DIFFICULTY_DEUS && ba->atlas_deus.id > 0) ? ba->atlas_deus : ba->atlas;
    DrawTexturePro(tex, src, dest, (Vector2){ 0, 0 }, 0.0f, WHITE);
    DrawRectangleLinesEx(dest, 1.0f, (Color){ 20, 40, 25, 200 });
}

void board_draw_card_back(const BoardAssets *ba, Rectangle dest) {
    Rectangle src = { 0, 0, (float)ba->card_back.width, (float)ba->card_back.height };
    DrawTexturePro(ba->card_back, src, dest, (Vector2){ 0, 0 }, 0.0f, WHITE);
    Color bo = g_theme.dim; bo.a = 100; Color bi = g_theme.base; bi.a = 180;
    DrawRectangleLinesEx((Rectangle){ dest.x - 1, dest.y - 1, dest.width + 2, dest.height + 2 }, 1.0f, bo);
    DrawRectangleLinesEx(dest, 1.5f, bi);
}

void board_draw_card_glow(Rectangle dest, float pulse_alpha) {
    Color gc = g_theme.base; gc.a = (unsigned char)(pulse_alpha * 255.0f);
    DrawRectangleLinesEx(dest, 1.5f, gc);
}

static void draw_capture_lanes(const BoardAssets *ba, const GameState *g, int player, float start_y) {
    CardType types[4] = { CARD_TYPE_BRIGHT, CARD_TYPE_ANIMAL, CARD_TYPE_RIBBON, CARD_TYPE_PLAIN };
    const char *labels[4] = { "B", "A", "R", "P" };
    float card_w = MINI_CARD_W - 6.0f;
    float card_h = MINI_CARD_H - 6.0f;
    for (int t = 0; t < 4; t++) {
        float y = start_y + t * (CAPTURE_PANEL_H + 4.0f);
        Rectangle lr = { 12.0f, y, CAPTURE_PANEL_W, CAPTURE_PANEL_H };
        DrawRectangleRec(lr, COLOR_PANEL_BG);
        ui_draw_panel_border(lr, COLOR_PHOSPHOR_DIM);
        int lc[48], count = 0, cup = 0;
        for (int i = 0; i < g->captured_count[player]; i++) {
            int cid = g->captured[player][i];
            if (cid == 32) cup = 1;
            if (CARDS[cid].type == types[t]) lc[count++] = cid;
        }
        const char *cnt_str = TextFormat((t == 1 && cup) ? "%s %d*" : "%s %d", labels[t], count);
        draw_text_ex_snapped(font_body, cnt_str, (Vector2){ 16.0f, y + 8.5f }, 20.0f, 1.4f, COLOR_TEXT_INFO);
        float fa = CAPTURE_PANEL_W - 54.0f - card_w, step = (count > 1) ? fminf(24.0f, fa / (count - 1)) : 0.0f;
        float card_y = y + (CAPTURE_PANEL_H - card_h) / 2.0f;
        for (int c = 0; c < count; c++) {
            int cid = lc[c];
            Rectangle cr = { 54.0f + c * step, card_y, card_w, card_h };
            motion_set_target(cid, (Vector2){ cr.x, cr.y }, (Vector2){ cr.width, cr.height }, 0.35f);
            Rectangle dr = motion_get_rect(cid);
            if (motion_is_card_moving(cid)) {
                board_draw_card_shadow(dr, motion_get_card_lift(cid));
                board_draw_card(ba, cid, dr);
                board_draw_card_glow(dr, 0.8f);
            } else board_draw_card(ba, cid, dr);
        }
    }
}

static float draw_combo_panel(float y, const char *title, const ScoreResult *s, int koi_called) {
    float base_h = 48.0f, content_h = 0.0f;
    int max_yakus = (s->yaku_count > 5) ? 5 : s->yaku_count;
    for (int i = 0; i < max_yakus; i++) {
        char full[64]; snprintf(full, sizeof(full), "%s: %d", s->yakus[i].name, s->yakus[i].points);
        content_h += (MeasureTextEx(font_body, full, 16.0f, 1.2f).x > 96.0f) ? 34.0f : 18.0f;
    }
    float total_h = base_h + content_h + (max_yakus > 0 ? 6.0f : 0.0f);
    Rectangle r = { 1160, y, 112, total_h };
    DrawRectangleRec(r, COLOR_PANEL_BG);
    ui_draw_panel_border(r, (max_yakus > 0) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    draw_text_ex_snapped(font_title, title, (Vector2){ 1168.0f, y + 8.0f }, 11.0f, 0.8f, COLOR_PHOSPHOR);
    draw_text_ex_snapped(font_body, TextFormat("Base: %d%s", s->base_points, (koi_called > 0) ? " [KOI]" : ""),
               (Vector2){ 1168.0f, y + 25.0f }, 15.0f, 1.0f,
               (koi_called > 0) ? COLOR_RED : COLOR_TEXT_INFO_DIM);

    if (max_yakus > 0) {
        DrawLine(1164, (int)y + 44, 1268, (int)y + 44, COLOR_PHOSPHOR_DIM);
        float cy = y + 49.0f;
        for (int i = 0; i < max_yakus; i++) {
            char full[64]; snprintf(full, sizeof(full), "%s: %d", s->yakus[i].name, s->yakus[i].points);
            if (MeasureTextEx(font_body, full, 16.0f, 1.2f).x > 96.0f) {
                draw_text_ex_snapped(font_body, s->yakus[i].name, (Vector2){ 1168.0f, cy }, 16.0f, 1.1f, COLOR_TEXT_INFO);
                draw_text_ex_snapped(font_body, TextFormat("  -> %d pts", s->yakus[i].points), (Vector2){ 1168.0f, cy + 17.0f }, 14.0f, 1.0f, COLOR_TEXT_INFO_DIM); cy += 34.0f;
            } else {
                draw_text_ex_snapped(font_body, full, (Vector2){ 1168.0f, cy }, 16.0f, 1.1f, COLOR_TEXT_INFO); cy += 18.0f;
            }
        }
    }
    return total_h;
}

void board_draw_table(const BoardAssets *ba, const GameState *g, const AnimState *anim,
                      int hovered_hand_idx, int dragging_idx, Vector2 drag_pos,
                      int easy_mode, const char *status_msg) {
    /* 1. Background with hybrid 3D depth-fog shader */
    if (ba->has_fog_shader) {
        float t = (float)GetTime();
        SetShaderValue(ba->fog_shader, ba->fog_time_loc, &t, SHADER_UNIFORM_FLOAT);
        BeginShaderMode(ba->fog_shader);
    }
    if (ba->current_bg >= 0 && ba->current_bg < 6 && ba->bgs[ba->current_bg].id > 0) {
        DrawTexturePro(ba->bgs[ba->current_bg], (Rectangle){ 0, 0, 1280, 720 }, (Rectangle){ 0, 0, 1280, 720 }, (Vector2){ 0, 0 }, 0.0f, WHITE);
    } else ClearBackground(BLACK);
    if (ba->has_fog_shader) EndShaderMode();

    DrawRectangle(0, 0, 1280, 720, COLOR_TABLE_DIM);

    /* Deep pure black ground fog in C (#050505) */
    DrawRectangleGradientV(0, 380, 1280, 340, (Color){ 5, 5, 5, 0 }, (Color){ 5, 5, 5, 110 });

    /* 1b. Dark Ash Particles drifting upwards */
    ash_draw_and_update(GetFrameTime());

    /* 2. Left Column */
    static int s_last_active_turn = -1;
    static float turn_flash_alpha = 0.0f;
    int active_turn = (g->phase != PHASE_ROUND_END && g->phase != PHASE_MATCH_END) ?
                      ((anim && anim->active && anim->type == ANIM_CAPTURE_FLY) ? anim->capture_player : g->current_player) : -1;
    if (active_turn != s_last_active_turn && active_turn >= 0) {
        turn_flash_alpha = 1.0f;
        s_last_active_turn = active_turn;
    }
    if (turn_flash_alpha > 0.0f) {
        turn_flash_alpha -= GetFrameTime() * 2.0f;
        if (turn_flash_alpha < 0.0f) turn_flash_alpha = 0.0f;
    }

    board_draw_info_box((Rectangle){ 12, 12, CAPTURE_PANEL_W, 30 }, TextFormat("CPU   SCORE: %d%s", g->total_score[1], (g->dealer == 1) ? " [DEALER]" : ""), (active_turn == 1), turn_flash_alpha);
    draw_capture_lanes(ba, g, 1, 46.0f);
    draw_capture_lanes(ba, g, 0, 508.0f);
    board_draw_info_box((Rectangle){ 12, 678, CAPTURE_PANEL_W, 30 }, TextFormat("YOU   SCORE: %d%s", g->total_score[0], (g->dealer == 0) ? " [DEALER]" : ""), (active_turn == 0), turn_flash_alpha);

    /* 3. CPU Hand */
    int cpu_count = g->hand_count[1];
    float cpu_step = 62.0f, cpu_w = (cpu_count > 0) ? ((cpu_count - 1) * cpu_step + CPU_CARD_W) : 0, cpu_x = 710.0f - cpu_w / 2.0f;
    for (int i = 0; i < cpu_count; i++) {
        int cid = g->hand[1][i];
        if (anim && anim->active && anim->type == ANIM_CPU_FLY && anim->cpu_card == cid) continue;
        Rectangle r = { cpu_x + i * cpu_step, 12.0f, CPU_CARD_W, CPU_CARD_H };
        motion_set_target(cid, (Vector2){ r.x, r.y }, (Vector2){ r.width, r.height }, 0.30f);
        Rectangle dr = motion_get_rect(cid);
        if (motion_is_card_moving(cid)) {
            board_draw_card_shadow(dr, motion_get_card_lift(cid));
            board_draw_card_back(ba, dr);
        } else board_draw_card_back(ba, dr);
    }

    /* 4. Stock Pile */
    int st_off = g->stock_count / 4;
    for (int i = 0; i <= st_off; i++) board_draw_card_back(ba, (Rectangle){ 330.0f - i, 262.0f - i, FIELD_CARD_W, FIELD_CARD_H });
    draw_text_ex_snapped(font_body, TextFormat("%d", g->stock_count), (Vector2){ 356.0f, 392.0f }, 18.0f, 1.2f, COLOR_PHOSPHOR);

    /* 5. Field Cards */
    int sf[48];
    for (int i = 0; i < g->field_count; i++) sf[i] = i;
    for (int i = 0; i < g->field_count - 1; i++) {
        for (int j = i + 1; j < g->field_count; j++) {
            if (CARDS[g->field[sf[i]]].month > CARDS[g->field[sf[j]]].month || (CARDS[g->field[sf[i]]].month == CARDS[g->field[sf[j]]].month && sf[i] > sf[j])) {
                int tmp = sf[i]; sf[i] = sf[j]; sf[j] = tmp;
            }
        }
    }
    int drag_m = (dragging_idx >= 0 && dragging_idx < g->hand_count[0]) ? CARDS[g->hand[0][dragging_idx]].month : -1;
    float pulse = 0.12f + 0.28f * (0.5f + 0.5f * sinf((float)GetTime() * 4.0f));

    for (int i = 0; i < g->field_count; i++) {
        int cid = g->field[sf[i]];
        Rectangle r = board_get_field_card_rect(i, g->field_count);
        motion_set_target(cid, (Vector2){ r.x, r.y }, (Vector2){ r.width, r.height }, 0.30f);
        Rectangle dr = motion_get_rect(cid);
        if (motion_is_card_moving(cid)) {
            board_draw_card_shadow(dr, motion_get_card_lift(cid));
            board_draw_card(ba, cid, dr);
        } else board_draw_card(ba, cid, dr);
        if (easy_mode && drag_m > 0 && CARDS[cid].month == drag_m) board_draw_card_glow(dr, pulse);
    }

    /* 6. Pending Stock Card */
    if (g->has_pending && g->pending_card >= 0 && (!anim->active || anim->type != ANIM_STOCK_FLY)) {
        Rectangle pr = motion_get_rect(g->pending_card);
        if (pr.width > 0 && (pr.x != 330.0f || pr.y != 262.0f)) {
            board_draw_card(ba, g->pending_card, pr);
        } else board_draw_card(ba, g->pending_card, (Rectangle){ 330.0f, 262.0f, FIELD_CARD_W, FIELD_CARD_H });
    }

    /* 7. Player Hand */
    for (int i = 0; i < g->hand_count[0]; i++) {
        if (i == dragging_idx) continue;
        int cid = g->hand[0][i];
        if (anim && anim->active && anim->type == ANIM_GLIDE_RETURN && anim->card_id == cid) continue;
        Rectangle r = board_get_hand_card_rect(i, g->hand_count[0], i == hovered_hand_idx);
        motion_set_target(cid, (Vector2){ r.x, r.y }, (Vector2){ r.width, r.height }, (i == hovered_hand_idx ? 0.15f : 0.30f));
        Rectangle dr = motion_get_rect(cid);
        if (motion_is_card_moving(cid)) {
            board_draw_card_shadow(dr, motion_get_card_lift(cid));
            board_draw_card(ba, cid, dr);
        } else board_draw_card(ba, cid, dr);
        if (easy_mode && g->current_player == 0 && dragging_idx < 0) {
            int m = 0;
            for (int f = 0; f < g->field_count; f++) { if (CARDS[g->field[f]].month == CARDS[cid].month) { m = 1; break; } }
            if (m) board_draw_card_glow(dr, pulse);
        }
    }

    /* 8. Dragged Card */
    if (dragging_idx >= 0 && dragging_idx < g->hand_count[0]) {
        int cid = g->hand[0][dragging_idx];
        motion_set_drag_velocity(cid, GetMouseDelta());
        motion_set_instant(cid, drag_pos, (Vector2){ HAND_CARD_W, HAND_CARD_H });
        Rectangle dr = { drag_pos.x, drag_pos.y, HAND_CARD_W, HAND_CARD_H };
        board_draw_card_shadow(dr, 1.0f);
        board_draw_card(ba, cid, dr);
        board_draw_card_shimmer(dr, motion_get_card_shimmer(cid));
        board_draw_card_glow(dr, 1.0f);
    }

    /* 9. Animated Cards */
    if (anim->active && anim->type == ANIM_GLIDE_RETURN) {
        Rectangle ar = { anim->current_pos.x, anim->current_pos.y, HAND_CARD_W, HAND_CARD_H };
        float lift = sinf((anim->elapsed / anim->duration) * 3.14159265f);
        board_draw_card_shadow(ar, lift);
        board_draw_card(ba, anim->card_id, ar);
    } else if (anim->active && anim->type == ANIM_CPU_FLY) {
        Rectangle ar = { anim->current_pos.x, anim->current_pos.y, FIELD_CARD_W, FIELD_CARD_H };
        float lift = sinf((anim->elapsed / anim->duration) * 3.14159265f);
        board_draw_card_shadow(ar, lift);
        board_draw_card(ba, anim->cpu_card, ar);
        board_draw_card_glow(ar, 1.0f);
    } else if (anim->active && (anim->type == ANIM_STOCK_FLY || anim->type == ANIM_STOCK_FLIP)) {
        float t = anim->elapsed / anim->duration;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        float flip_t = (t < 0.70f) ? (t / 0.70f) : 1.0f;
        float hop = sinf(flip_t * 3.14159265f) * 18.0f;
        float cos_val = cosf(flip_t * 3.14159265f), cur_w = FIELD_CARD_W * fmaxf(0.06f, fabsf(cos_val));
        float cur_x = anim->current_pos.x + (FIELD_CARD_W - cur_w) / 2.0f, cur_y = anim->current_pos.y - hop;
        Rectangle cr = { cur_x, cur_y, cur_w, FIELD_CARD_H };
        float lift = hop / 18.0f;
        Rectangle sr = { cur_x + 4.0f + lift * 8.0f, cur_y + 5.0f + lift * 10.0f + hop, cur_w, FIELD_CARD_H };
        DrawRectangleRec(sr, (Color){ 0, 0, 0, (unsigned char)(255 * 0.40f) });
        if (cos_val > 0.0f) board_draw_card_back(ba, cr);
        else board_draw_card(ba, anim->card_id, cr);
        board_draw_card_glow(cr, 1.0f);
    }

    /* 10. Status Line & Master Timer */
    if (g->current_player == 0 && g->phase == PHASE_TURN_HAND && g->player_timer > 0.0f) {
        char t_str[32];
        if (g->player_timer <= 5.0f) {
            int flash = ((int)(g->player_timer * 6.0f)) % 2;
            Color tcol = flash ? (Color){ 255, 30, 30, 255 } : (Color){ 255, 230, 230, 255 };
            snprintf(t_str, sizeof(t_str), "! TIME: %.1fS !", g->player_timer);
            draw_centered_text(font_body, t_str, 640.0f, 502.0f, 24.0f, 1.2f, tcol);
        } else {
            snprintf(t_str, sizeof(t_str), "TIME: %.1fS", g->player_timer);
            draw_centered_text(font_body, t_str, 640.0f, 505.0f, 22.0f, 1.2f, COLOR_PHOSPHOR);
        }
    }
    board_draw_status_message(status_msg);

    /* 11. Right Column */
    Rectangle cp = { 1160, 12, 112, 54 };
    DrawRectangleRec(cp, COLOR_PANEL_BG); ui_draw_panel_border(cp, COLOR_PHOSPHOR_DIM);
    draw_text_ex_snapped(font_body, TextFormat("RND %d/%d", g->current_round, g->total_rounds), (Vector2){ 1168.0f, 20.0f }, 14.0f, 1.0f, COLOR_PHOSPHOR);
    const char *rnames[5] = { "NINTENDO", "FUDAWIKI", "KK-ADD-10", "TOURNAMENT", "YAKUZA" };
    draw_text_ex_snapped(font_body, rnames[g->ruleset], (Vector2){ 1168.0f, 38.0f }, 11.0f, 0.7f, COLOR_PHOSPHOR_DIM);

    int rmonth = ruleset_round_month(g->current_round, g->total_rounds);
    ScoreResult s0 = score_captures_ex(g->ruleset, g->captured[0], g->captured_count[0], rmonth), s1 = score_captures_ex(g->ruleset, g->captured[1], g->captured_count[1], rmonth);
    float cpu_h = draw_combo_panel(74.0f, "CPU COMBOS", &s1, g->koi_called[1]);
    draw_combo_panel(74.0f + cpu_h + 10.0f, "YOU COMBOS", &s0, g->koi_called[0]);

    Rectangle pause_button = { 1160, 560, 112, 40 };
    DrawRectangleRec(pause_button, COLOR_PANEL_BG);
    ui_draw_panel_border(pause_button, COLOR_PHOSPHOR);
    draw_text_in_rect(font_title, "PAUSE", pause_button, 16.0f, 1.0f, COLOR_PHOSPHOR);
    Rectangle restart_button = { 1160, 610, 112, 40 };
    DrawRectangleRec(restart_button, COLOR_PANEL_BG);
    ui_draw_panel_border(restart_button, COLOR_PHOSPHOR);
    draw_text_in_rect(font_title, "RESTART", restart_button, 16.0f, 1.0f, COLOR_PHOSPHOR);
}

void board_draw_dealer_select(const BoardAssets *ba, const GameState *g, Vector2 vm) {
    DrawRectangle(0, 0, 1280, 720, (Color){ 0, 0, 0, 245 });

    const char *title = "PICK A CARD TO DETERMINE DEALER (EARLIEST MONTH WINS)";
    draw_centered_text(font_title, title, 640.0f, 85.0f, 26.0f, 1.2f, COLOR_PHOSPHOR);

    Rectangle r[2] = { { 400, 180, 176, 286 }, { 704, 180, 176, 286 } };

    if (g->oya_player_choice < 0) {
        for (int i = 0; i < 2; i++) {
            board_draw_card_back(ba, r[i]);
            int hov = CheckCollisionPointRec(vm, r[i]);
            if (hov) board_draw_card_glow(r[i], 1.0f);
            const char *hint = "CLICK TO PICK";
            draw_centered_text(font_body, hint, r[i].x + r[i].width / 2.0f, 485.0f,
                               18.0f, 1.2f, hov ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
        }
    } else {
        int pc = g->oya_player_choice, cc = 1 - pc;
        int p_cid = g->oya_cards[pc], c_cid = g->oya_cards[cc];

        float pt = fminf(1.0f, g->oya_timer / 0.45f);
        float pw = r[pc].width * fmaxf(0.06f, fabsf(cosf(pt * 3.14159f)));
        Rectangle pr = { r[pc].x + (r[pc].width - pw) / 2.0f, r[pc].y, pw, r[pc].height };
        if (pt < 0.5f) board_draw_card_back(ba, pr); else board_draw_card(ba, p_cid, pr);

        float ct = (g->oya_timer > 0.45f) ? fminf(1.0f, (g->oya_timer - 0.45f) / 0.45f) : 0.0f;
        float cw = r[cc].width * fmaxf(0.06f, fabsf(cosf(ct * 3.14159f)));
        Rectangle cr = { r[cc].x + (r[cc].width - cw) / 2.0f, r[cc].y, cw, r[cc].height };
        if (ct < 0.5f) board_draw_card_back(ba, cr); else board_draw_card(ba, c_cid, cr);

        draw_centered_text(font_body, "YOUR CARD", r[pc].x + r[pc].width / 2.0f, 480.0f, 20.0f, 1.2f, COLOR_PHOSPHOR);
        draw_centered_text(font_body, "CPU'S CARD", r[cc].x + r[cc].width / 2.0f, 480.0f, 20.0f, 1.2f, COLOR_PHOSPHOR_DIM);

        if (g->oya_timer >= 0.9f) {
            const char *ptxt = TextFormat("Month %d", CARDS[p_cid].month);
            draw_centered_text(font_body, ptxt, r[pc].x + r[pc].width / 2.0f, 510.0f, 18.0f, 1.2f, COLOR_PHOSPHOR);
            const char *ctxt = TextFormat("Month %d", CARDS[c_cid].month);
            draw_centered_text(font_body, ctxt, r[cc].x + r[cc].width / 2.0f, 510.0f, 18.0f, 1.2f, COLOR_PHOSPHOR_DIM);

            const char *res = (g->oya_winner == 0) ? "YOU ARE DEALER" : "CPU IS DEALER";
            Color res_col = (g->oya_winner == 0) ? COLOR_PHOSPHOR : COLOR_RED;
            draw_centered_text(font_title, res, 640.0f, 565.0f, 32.0f, 1.5f, res_col);
            board_draw_card_glow(r[(g->oya_winner == 0) ? pc : cc], 1.0f);
        }
    }
}

int board_check_dealer_select_click(const GameState *g, Vector2 vm) {
    if (g->oya_player_choice >= 0) return -1;
    if (CheckCollisionPointRec(vm, (Rectangle){ 400, 180, 176, 286 })) return 0;
    if (CheckCollisionPointRec(vm, (Rectangle){ 704, 180, 176, 286 })) return 1;
    return -1;
}
