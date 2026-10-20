#include "ui_board.h"
#include "ui_card_tooltip.h"
#include "board_layout.h"
#include "ui_motion.h"
#include "ui_board_status.h"
#include "ui_combos.h"
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

typedef struct { Vector2 pos; float speed_y, sway_speed, sway_amp; int size; unsigned char alpha; float phase; } AshParticle;
static AshParticle g_ash[ASH_COUNT];
static int g_ash_initialized = 0;

static void ash_init(void) {
    for (int i = 0; i < ASH_COUNT; i++) {
        g_ash[i].pos = (Vector2){ (float)(rand() % 1280), (float)(rand() % 720) };
        g_ash[i].speed_y = 12.0f + (float)(rand() % 24);
        g_ash[i].sway_speed = 0.8f + (float)(rand() % 100) / 70.0f;
        g_ash[i].sway_amp = 8.0f + (float)(rand() % 12);
        g_ash[i].size = (int)(1.5f + (float)(rand() % 16) / 10.0f);
        float a = 0.18f + (float)(rand() % 25) / 100.0f;
        g_ash[i].alpha = (unsigned char)(255.0f * a);
        g_ash[i].phase = (float)(rand() % 628) / 100.0f;
    }
    g_ash_initialized = 1;
}

static void ash_draw_and_update(float dt) {
    if (!g_ash_initialized) ash_init();
    float cur_time = (float)GetTime();
    for (int i = 0; i < ASH_COUNT; i++) {
        AshParticle *p = &g_ash[i];
        p->pos.y -= p->speed_y * dt;
        if (p->pos.y < -10.0f) {
            p->pos.y = 730.0f + (float)(rand() % 30);
            p->pos.x = (float)(rand() % 1280);
        }
        if (p->alpha == 0) continue;
        float sway_x = p->pos.x + sinf(p->phase + cur_time * p->sway_speed) * p->sway_amp;
        DrawRectangle((int)sway_x, (int)p->pos.y, p->size, p->size, (Color){ 5, 5, 5, p->alpha });
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

    ba->coin_gold = LoadTexture(get_path("assets/coin_gold.png"));
    if (ba->coin_gold.id > 0) { GenTextureMipmaps(&ba->coin_gold); SetTextureFilter(ba->coin_gold, TEXTURE_FILTER_TRILINEAR); }
    ba->coin_silver = LoadTexture(get_path("assets/coin_silver.png"));
    if (ba->coin_silver.id > 0) { GenTextureMipmaps(&ba->coin_silver); SetTextureFilter(ba->coin_silver, TEXTURE_FILTER_TRILINEAR); }
    ba->coin_bronze = LoadTexture(get_path("assets/coin_bronze.png"));
    if (ba->coin_bronze.id > 0) { GenTextureMipmaps(&ba->coin_bronze); SetTextureFilter(ba->coin_bronze, TEXTURE_FILTER_TRILINEAR); }

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

    const char *bg_names[8] = {
        "assets/harvest.jpg", "assets/hole.jpg", "assets/arctic.jpg",
        "assets/castle.jpg",  "assets/city.jpg", "assets/deus.png",
        "assets/face.jpg", "assets/realm.jpg"
    };
    /* Bump maps made by tools/make_bg_normal_maps.py. Deus (index 5) is
       deliberately blank: it keeps its own look without moving lights. */
    const char *bg_normal_names[8] = {
        "assets/harvest_normal.png", "assets/hole_normal.png", "assets/arctic_normal.png",
        "assets/castle_normal.png",  "assets/city_normal.png", "",
        "assets/face_normal.png",    "assets/realm_normal.png"
    };
    for (int i = 0; i < 8; i++) ba->bgs[i] = LoadTexture(get_path(bg_names[i]));
    for (int i = 0; i < 8; i++) {
        ba->bg_normals[i].id = 0;
        if (bg_normal_names[i][0] == '\0') continue;
        ba->bg_normals[i] = LoadTexture(get_path(bg_normal_names[i]));
        if (ba->bg_normals[i].id > 0) {
            SetTextureFilter(ba->bg_normals[i], TEXTURE_FILTER_BILINEAR);
            SetTextureWrap(ba->bg_normals[i], TEXTURE_WRAP_CLAMP);
        }
    }

#ifdef __EMSCRIPTEN__
    ba->fog_shader = LoadShader(0, get_path("assets/depth_fog_web.fs"));
#else
    ba->fog_shader = LoadShader(0, get_path("assets/depth_fog.fs"));
#endif
    ba->has_fog_shader = (ba->fog_shader.id > 0);
    ba->light_normal_loc = ba->light_enabled_loc = -1;
    if (ba->has_fog_shader) {
        ba->fog_time_loc = GetShaderLocation(ba->fog_shader, "time");
        ba->light_normal_loc = GetShaderLocation(ba->fog_shader, "normalMap");
        ba->light_enabled_loc = GetShaderLocation(ba->fog_shader, "lightEnabled");
    }

    ba->current_bg = 0; ba->assets_loaded = 1;
}

void board_unload_assets(BoardAssets *ba) {
    if (!ba->assets_loaded) return;
    UnloadTexture(ba->atlas); UnloadTexture(ba->card_back);
    if (ba->atlas_deus.id > 0) UnloadTexture(ba->atlas_deus);
    if (ba->coin_gold.id > 0) UnloadTexture(ba->coin_gold);
    if (ba->coin_silver.id > 0) UnloadTexture(ba->coin_silver);
    if (ba->coin_bronze.id > 0) UnloadTexture(ba->coin_bronze);
    for (int i = 0; i < 8; i++) {
        UnloadTexture(ba->bgs[i]);
        if (ba->bg_normals[i].id > 0) UnloadTexture(ba->bg_normals[i]);
    }
    if (ba->has_fog_shader) { UnloadShader(ba->fog_shader); ba->has_fog_shader = 0; }
    ba->assets_loaded = 0;
}

Rectangle board_get_hand_card_rect(int index, int total_cards, int hovered) {
    float step = 96.0f, total_w = (total_cards > 0) ? ((total_cards - 1) * step + HAND_CARD_W) : 0;
    return (Rectangle){ 710.0f - total_w / 2.0f + index * step, hovered ? 552.0f : 566.0f, HAND_CARD_W, HAND_CARD_H };
}

Rectangle board_get_field_card_rect(int index, int total_cards) {
    float x, y;
    board_field_slot_xy(index, total_cards, &x, &y);
    return (Rectangle){ x, y, FIELD_CARD_W, FIELD_CARD_H };
}

/* The table is painted sorted by month (then by position in g->field[]), so a card's position in
   the field array is NOT the slot it is drawn in.  This returns the rectangle of the slot the card
   is painted in, using the same slot order as the painter (board_layout.c).  Pass a card that is not
   on the field yet (the drawn stock card) and it returns the slot that card will occupy once
   engine_reveal_stock() appends it. */
Rectangle board_get_field_card_rect_of(const GameState *g, int card_id) {
    int total = 0;
    int slot = board_field_slot_of(card_id, g->field, g->field_count, &total);
    if (slot < 0) return (Rectangle){ 0, 0, 0, 0 };
    return board_get_field_card_rect(slot, total);
}

Vector2 board_get_card_field_pos(const GameState *g, int card_id) {
    if (card_id >= 0 && card_id < 48) {
        Vector2 p = motion_get_pos(card_id);
        if (p.x > 0.0f || p.y > 0.0f) return p;
    }
    Rectangle r = board_get_field_card_rect_of(g, card_id);
    if (r.width > 0.0f) return (Vector2){ r.x, r.y };
    return (Vector2){ 710.0f, 240.0f };
}

Rectangle board_get_field_rect(void) { return (Rectangle){ FIELD_X, FIELD_Y, FIELD_W, FIELD_H }; }
Rectangle board_get_stock_rect(void) { return (Rectangle){ 330.0f, 262.0f, FIELD_CARD_W, FIELD_CARD_H }; }

void board_draw_card_shadow(Rectangle dest, float lift_amount) {
    if (lift_amount <= 0.01f) return;
    if (lift_amount > 1.0f) lift_amount = 1.0f;
    float off_x = 4.0f + lift_amount * 8.0f, off_y = 5.0f + lift_amount * 10.0f;
    DrawRectangleRec((Rectangle){ dest.x + off_x, dest.y + off_y, dest.width, dest.height }, (Color){ 0, 0, 0, (unsigned char)(102.0f * lift_amount) });
}

void board_draw_card_shimmer(Rectangle dest, float shimmer_val) {
    if (shimmer_val <= 0.5f || dest.width <= 0.0f || dest.height <= 0.0f) return;
    unsigned char top_a = (unsigned char)shimmer_val;
    if (top_a > 23) top_a = 23;
    unsigned char bot_a = (unsigned char)(top_a >> 2);
    DrawRectangleGradientV((int)dest.x, (int)dest.y, (int)dest.width, (int)dest.height,
                           (Color){ 255, 255, 255, top_a },
                           (Color){ 255, 255, 255, bot_a });
}

void board_draw_card(const BoardAssets *ba, int card_id, Rectangle dest) {
    if ((unsigned)card_id >= 48u) return;
    card_tooltip_record(card_id, dest);
    Rectangle src = { (float)((card_id >> 2) * ATLAS_CELL_W), (float)((card_id & 3) * ATLAS_CELL_H), (float)ATLAS_CELL_W, (float)ATLAS_CELL_H };
    Texture2D tex = (settings.difficulty == DIFFICULTY_DEUS && ba->atlas_deus.id > 0) ? ba->atlas_deus : ba->atlas;
    DrawTexturePro(tex, src, dest, (Vector2){ 0, 0 }, 0.0f, WHITE);
    DrawRectangleLinesEx(dest, 1.0f, (Color){ 20, 40, 25, 200 });
}

void board_draw_card_back(const BoardAssets *ba, Rectangle dest) {
    card_tooltip_record(-1, dest);
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
    static const char *const labels[4] = { "B", "A", "R", "P" };
    float card_w = MINI_CARD_W - 6.0f, card_h = MINI_CARD_H - 6.0f;
    int lanes[4][48], counts[4] = { 0, 0, 0, 0 }, cup = 0, cap_n = g->captured_count[player];
    for (int i = 0; i < cap_n; i++) {
        int cid = g->captured[player][i], t = (int)CARDS[cid].type;
        if (cid == 32) cup = 1;
        if ((unsigned)t < 4u) lanes[t][counts[t]++] = cid;
    }
    for (int t = 0; t < 4; t++) {
        float y = start_y + t * (CAPTURE_PANEL_H + 4.0f), card_y = y + (CAPTURE_PANEL_H - card_h) / 2.0f;
        Rectangle lr = { 12.0f, y, CAPTURE_PANEL_W, CAPTURE_PANEL_H };
        DrawRectangleRec(lr, COLOR_PANEL_BG); ui_draw_panel_border(lr, COLOR_PHOSPHOR_DIM);
        int count = counts[t];
        const char *cnt_str = TextFormat((t == 1 && cup) ? "%s %d*" : "%s %d", labels[t], count);
        draw_text_ex_snapped(font_body, cnt_str, (Vector2){ 16.0f, y + 8.5f }, 20.0f, 1.4f, COLOR_TEXT_INFO);
        float step = (count > 1) ? fminf(24.0f, (CAPTURE_PANEL_W - 54.0f - card_w) / (count - 1)) : 0.0f;
        for (int c = 0; c < count; c++) {
            int cid = lanes[t][c];
            motion_set_target(cid, (Vector2){ 54.0f + c * step, card_y }, (Vector2){ card_w, card_h }, 0.35f);
            Rectangle dr = motion_get_rect(cid);
            if (motion_is_card_moving(cid)) {
                board_draw_card_shadow(dr, motion_get_card_lift(cid));
                board_draw_card(ba, cid, dr); board_draw_card_glow(dr, 0.8f);
            } else board_draw_card(ba, cid, dr);
        }
    }
}

void board_draw_table(const BoardAssets *ba, const GameState *g, const AnimState *anim,
                      int hovered_hand_idx, int dragging_idx, Vector2 drag_pos,
                      int easy_mode, int replay_mode, const char *status_msg) {
    float cur_time = (float)GetTime();
    float frame_dt = GetFrameTime();

    /* 1. Background with hybrid 3D depth-fog shader */
    if (ba->has_fog_shader) {
        float light_enabled = (ba->current_bg >= 0 && ba->current_bg < 8 &&
                               ba->bg_normals[ba->current_bg].id > 0 &&
                               ba->light_normal_loc >= 0 && ba->light_enabled_loc >= 0) ? 1.0f : 0.0f;
        SetShaderValue(ba->fog_shader, ba->fog_time_loc, &cur_time, SHADER_UNIFORM_FLOAT);
        SetShaderValue(ba->fog_shader, ba->light_enabled_loc, &light_enabled, SHADER_UNIFORM_FLOAT);
        BeginShaderMode(ba->fog_shader);
        /* Register the bump map after BeginShaderMode: rlSetUniformSampler
           only survives until the next batch flush, and BeginShaderMode
           flushes. It then rides along with this background draw. */
        if (light_enabled > 0.0f)
            SetShaderValueTexture(ba->fog_shader, ba->light_normal_loc, ba->bg_normals[ba->current_bg]);
    }
    if (ba->current_bg >= 0 && ba->current_bg < 8 && ba->bgs[ba->current_bg].id > 0) {
        DrawTexturePro(ba->bgs[ba->current_bg], (Rectangle){ 0, 0, 1280, 720 }, (Rectangle){ 0, 0, 1280, 720 }, (Vector2){ 0, 0 }, 0.0f, WHITE);
    } else ClearBackground(BLACK);
    if (ba->has_fog_shader) EndShaderMode();

    DrawRectangle(0, 0, 1280, 720, COLOR_TABLE_DIM);
    DrawRectangleGradientV(0, 380, 1280, 340, (Color){ 5, 5, 5, 0 }, (Color){ 5, 5, 5, 110 });
    ash_draw_and_update(frame_dt);

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
        turn_flash_alpha -= frame_dt * 2.0f;
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
    int st_off = g->stock_count >> 2;
    for (int i = 0; i <= st_off; i++) board_draw_card_back(ba, (Rectangle){ 330.0f - i, 262.0f - i, FIELD_CARD_W, FIELD_CARD_H });
    draw_text_ex_snapped(font_body, TextFormat("%d", g->stock_count), (Vector2){ 356.0f, 392.0f }, 18.0f, 1.2f, COLOR_PHOSPHOR);

    /* 5. Field Cards (Insertion sort via board_field_sort_slots + month bitmask for O(1) Easy Mode glow) */
    int sf[48];
    board_field_sort_slots(g->field, g->field_count, sf);
    uint16_t field_month_mask = 0;
    for (int i = 0; i < g->field_count; i++) field_month_mask |= (uint16_t)(1u << CARDS[g->field[i]].month);
    int drag_m = (dragging_idx >= 0 && dragging_idx < g->hand_count[0]) ? CARDS[g->hand[0][dragging_idx]].month : -1;
    float pulse = 0.12f + 0.28f * (0.5f + 0.5f * sinf(cur_time * 4.0f));

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

    /* 5b. A hand card waiting on a choice (it matched two field cards): it has
           left the hand and is not on the field yet, so nothing else draws it. */
    if (g->phase == PHASE_HAND_CHOICE && g->played_hand_card >= 0) {
        Rectangle hr = motion_get_rect(g->played_hand_card);
        if (hr.width > 0.0f) board_draw_card(ba, g->played_hand_card, hr);
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
        /* A replay flies both players' cards, so the human hand has to let go of a
           card that is in the air as well. */
        if (anim && anim->active && anim->type == ANIM_CPU_FLY && anim->cpu_card == cid) continue;
        Rectangle r = board_get_hand_card_rect(i, g->hand_count[0], i == hovered_hand_idx);
        motion_set_target(cid, (Vector2){ r.x, r.y }, (Vector2){ r.width, r.height }, (i == hovered_hand_idx ? 0.15f : 0.30f));
        Rectangle dr = motion_get_rect(cid);
        if (motion_is_card_moving(cid)) {
            board_draw_card_shadow(dr, motion_get_card_lift(cid));
            board_draw_card(ba, cid, dr);
        } else board_draw_card(ba, cid, dr);
        if (easy_mode && g->current_player == 0 && dragging_idx < 0) {
            if ((field_month_mask >> CARDS[cid].month) & 1u) board_draw_card_glow(dr, pulse);
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
        ui_theme_draw_panel_flares_keyed(dr, cid + 1);
    }

    /* 9. Animated Cards */
    if (anim->active && anim->type == ANIM_GLIDE_RETURN) {
        Rectangle ar = { anim->current_pos.x, anim->current_pos.y, HAND_CARD_W, HAND_CARD_H };
        float lift = sinf((anim->elapsed / anim->duration) * 3.14159265f);
        board_draw_card_shadow(ar, lift);
        board_draw_card(ba, anim->card_id, ar);
    } else if (anim->active && anim->type == ANIM_CPU_FLY) {
        /* A card leaves a hand at hand size and settles at field size, so it
           grows or shrinks over the flight instead of popping on take-off. */
        float t = (anim->duration > 0.0f) ? (anim->elapsed / anim->duration) : 1.0f;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        float from_bottom = (anim->start_pos.y > 300.0f) ? 1.0f : 0.0f;   /* human hand sits low */
        float sw = from_bottom ? HAND_CARD_W : CPU_CARD_W;
        float sh = from_bottom ? HAND_CARD_H : CPU_CARD_H;
        Rectangle ar = { anim->current_pos.x, anim->current_pos.y,
                         sw + (FIELD_CARD_W - sw) * t, sh + (FIELD_CARD_H - sh) * t };
        float lift = sinf((anim->elapsed / anim->duration) * 3.14159265f);
        board_draw_card_shadow(ar, lift);
        board_draw_card(ba, anim->cpu_card, ar);
        board_draw_card_glow(ar, 1.0f);
        ui_theme_draw_panel_flares_keyed(ar, anim->cpu_card + 1);
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
        DrawRectangleRec(sr, (Color){ 0, 0, 0, 102 });
        if (cos_val > 0.0f) board_draw_card_back(ba, cr);
        else board_draw_card(ba, anim->card_id, cr);
        board_draw_card_glow(cr, 1.0f);
        ui_theme_draw_panel_flares_keyed(cr, anim->card_id + 1);
    }

    /* 10. Status Line & Master Timer */
    if (g->current_player == 0 && g->phase == PHASE_TURN_HAND && g->player_timer > 0.0f) {
        char t_str[32];
        if (g->player_timer <= 5.0f) {
            int flash = ((int)(g->player_timer * 6.0f)) & 1;
            Color tcol = flash ? (Color){ 255, 30, 30, 255 } : (Color){ 255, 230, 230, 255 };
            snprintf(t_str, sizeof(t_str), "! TIME: %.1fS !", g->player_timer);
            draw_centered_text(font_body, t_str, 640.0f, 502.0f, 24.0f, 1.2f, tcol);
        } else {
            snprintf(t_str, sizeof(t_str), "TIME: %.1fS", g->player_timer);
            draw_centered_text(font_body, t_str, 640.0f, 505.0f, 22.0f, 1.2f, COLOR_PHOSPHOR);
        }
    }
    board_draw_status_message(status_msg);

    /* 11. Fixed-size, clickable combo summaries. */
    combos_draw_sidebar(g);

    Rectangle pause_btn = { 1160, 560, 112, 40 }, restart_btn = { 1160, 610, 112, 40 };
    DrawRectangleRec(pause_btn, COLOR_PANEL_BG); ui_draw_panel_border(pause_btn, COLOR_PHOSPHOR);
    draw_text_in_rect(font_title, "PAUSE", pause_btn, 16.0f, 1.0f, COLOR_PHOSPHOR);
    DrawRectangleRec(restart_btn, COLOR_PANEL_BG); ui_draw_panel_border(restart_btn, COLOR_PHOSPHOR);
    /* A replay cannot be restarted: the second button ends it instead. */
    draw_text_in_rect(font_title, replay_mode ? "STOP" : "RESTART", restart_btn, 16.0f, 1.0f, COLOR_PHOSPHOR);
}

void board_draw_dealer_select(const BoardAssets *ba, const GameState *g, Vector2 vm) {
    DrawRectangle(0, 0, 1280, 720, (Color){ 0, 0, 0, 245 });
    draw_centered_text(font_title, "PICK A CARD TO DETERMINE DEALER (EARLIEST MONTH WINS)", 640.0f, 85.0f, 26.0f, 1.2f, COLOR_PHOSPHOR);
    Rectangle r[2] = { { 400, 180, 176, 286 }, { 704, 180, 176, 286 } };
    if (g->oya_player_choice < 0) {
        for (int i = 0; i < 2; i++) {
            board_draw_card_back(ba, r[i]);
            int hov = CheckCollisionPointRec(vm, r[i]);
            if (hov) {
                board_draw_card_glow(r[i], 1.0f);
                ui_theme_draw_panel_flares(r[i]);
            }
            draw_centered_text(font_body, "CLICK TO PICK", r[i].x + r[i].width / 2.0f, 485.0f, 18.0f, 1.2f, hov ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
        }
    } else {
        int pc = g->oya_player_choice, cc = 1 - pc, p_cid = g->oya_cards[pc], c_cid = g->oya_cards[cc];

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
            draw_centered_text(font_body, TextFormat("Month %d", CARDS[p_cid].month), r[pc].x + r[pc].width / 2.0f, 510.0f, 18.0f, 1.2f, COLOR_PHOSPHOR);
            draw_centered_text(font_body, TextFormat("Month %d", CARDS[c_cid].month), r[cc].x + r[cc].width / 2.0f, 510.0f, 18.0f, 1.2f, COLOR_PHOSPHOR_DIM);

            const char *res = (g->oya_winner == 0) ? "YOU ARE DEALER" : "CPU IS DEALER";
            Color res_col = (g->oya_winner == 0) ? COLOR_PHOSPHOR : COLOR_RED;
            draw_centered_text(font_title, res, 640.0f, 565.0f, 32.0f, 1.5f, res_col);
            Rectangle winner_card = r[(g->oya_winner == 0) ? pc : cc];
            board_draw_card_glow(winner_card, 1.0f);
            ui_theme_draw_panel_flares(winner_card);
        }
    }
}

int board_check_dealer_select_click(const GameState *g, Vector2 vm) {
    if (g->oya_player_choice >= 0) return -1;
    if (CheckCollisionPointRec(vm, (Rectangle){ 400, 180, 176, 286 })) return 0;
    if (CheckCollisionPointRec(vm, (Rectangle){ 704, 180, 176, 286 })) return 1;
    return -1;
}
