#include "ui_screens.h"
#include "ui_replay.h"
#include "ui_speed.h"
#include "storage.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

extern Font font_title;
extern Font font_body;

static void draw_text_ex_snapped(Font font, const char *text, Vector2 pos, float fsz, float sp, Color col) {
    DrawTextEx(font, text, (Vector2){ floorf(pos.x), floorf(pos.y) }, fsz, sp, col);
}

static void draw_centered_text(Font font, const char *text, float cx, float y, float fsz, float sp, Color col) {
    Vector2 ts = MeasureTextEx(font, text, fsz, sp);
    Vector2 pos = { floorf(cx - ts.x / 2.0f), floorf(y) };
    DrawTextEx(font, text, pos, fsz, sp, col);
    if (font.texture.id == font_title.texture.id && fsz >= 26.0f)
        ui_theme_draw_text_flares(font, text, pos, fsz, sp, col);
}

static void draw_text_in_rect(Font font, const char *text, Rectangle b, float fsz, float sp, Color col) {
    Vector2 ts = MeasureTextEx(font, text, fsz, sp);
    draw_text_ex_snapped(font, text, (Vector2){ b.x + (b.width - ts.x) / 2.0f, b.y + (b.height - fsz) / 2.0f }, fsz, sp, col);
}

static int draw_btn(Rectangle r, const char *txt, int fsz, Vector2 vm, Color txt_col) {
    int hov = CheckCollisionPointRec(vm, r);
    DrawRectangleRec(r, hov ? COLOR_BTN_HOVER_BG : COLOR_PANEL_BG);
    ui_draw_panel_border(r, hov ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    Vector2 ts = MeasureTextEx(font_title, txt, (float)fsz, 1.2f);
    Vector2 pos = { floorf(r.x + (r.width - ts.x) * 0.5f), floorf(r.y + (r.height - fsz) * 0.5f) };
    DrawTextEx(font_title, txt, pos, (float)fsz, 1.2f, txt_col);
    if (hov) ui_theme_draw_text_flares(font_title, txt, pos, (float)fsz, 1.2f, txt_col);
    return hov;
}

static void draw_option_value(Rectangle r, const char *text, int fsz, Vector2 vm, Color text_color) {
    int hov = CheckCollisionPointRec(vm, r);
    DrawRectangleRec(r, hov ? COLOR_BTN_HOVER_BG : COLOR_PANEL_BG);
    ui_draw_panel_border(r, hov ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    draw_text_in_rect(font_body, text, r, (float)fsz, 1.0f, text_color);
}

static void draw_section_header(const char *text, float y) {
    draw_text_ex_snapped(font_title, text, (Vector2){ 300.0f, y }, 17.0f, 1.4f, COLOR_PHOSPHOR);
    DrawLine(300, (int)(y + 22.0f), 940, (int)(y + 22.0f), COLOR_PHOSPHOR_DIM);
}

static void draw_selector_row(float y, const char *lbl, const char *val, int fsz, Color val_col, Vector2 vm) {
    draw_text_ex_snapped(font_body, lbl, (Vector2){ 300.0f, y + 4.0f }, 18.0f, 1.2f, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 650, y, 40, 26 }, "<", 18, vm, COLOR_PHOSPHOR);
    draw_option_value((Rectangle){ 694, y, 172, 26 }, val, fsz, vm, val_col);
    draw_btn((Rectangle){ 870, y, 40, 26 }, ">", 18, vm, COLOR_PHOSPHOR);
}

static void draw_locked_row(float y, const char *lbl, const char *val) {
    Vector2 off = { -1.0f, -1.0f };
    draw_text_ex_snapped(font_body, lbl, (Vector2){ 300.0f, y + 4.0f }, 18.0f, 1.2f, COLOR_PHOSPHOR_DIM);
    draw_btn((Rectangle){ 650, y, 40, 26 }, "<", 18, off, COLOR_PHOSPHOR_DIM);
    draw_option_value((Rectangle){ 694, y, 172, 26 }, val, 14, off, COLOR_PHOSPHOR_DIM);
    draw_btn((Rectangle){ 870, y, 40, 26 }, ">", 18, off, COLOR_PHOSPHOR_DIM);
}

static int check_selector_click(Vector2 vm, float y) {
    if (CheckCollisionPointRec(vm, (Rectangle){ 650, y, 40, 26 })) return -1;
    if (CheckCollisionPointRec(vm, (Rectangle){ 694, y, 172, 26 }) || CheckCollisionPointRec(vm, (Rectangle){ 870, y, 40, 26 })) return 1;
    return 0;
}

static void draw_bg_if_loaded(const BoardAssets *ba, unsigned char alpha) {
    int bg = ba->current_bg;
    if (bg >= 0 && bg < 8 && ba->bgs[bg].id > 0) {
        if (ba->has_fog_shader) {
            float cur_time = (float)GetTime();
            float light_enabled = (ba->bg_normals[bg].id > 0 &&
                                   ba->light_normal_loc >= 0 && ba->light_enabled_loc >= 0) ? 1.0f : 0.0f;
            float flash_zero = 0.0f;
            SetShaderValue(ba->fog_shader, ba->fog_time_loc, &cur_time, SHADER_UNIFORM_FLOAT);
            SetShaderValue(ba->fog_shader, ba->light_enabled_loc, &light_enabled, SHADER_UNIFORM_FLOAT);
            if (ba->light_flash_loc >= 0)
                SetShaderValue(ba->fog_shader, ba->light_flash_loc, &flash_zero, SHADER_UNIFORM_FLOAT);
            BeginShaderMode(ba->fog_shader);
            if (light_enabled > 0.5f)
                SetShaderValueTexture(ba->fog_shader, ba->light_normal_loc, ba->bg_normals[bg]);
        }
        DrawTexturePro(ba->bgs[bg], (Rectangle){0,0,1280,720}, (Rectangle){0,0,1280,720}, (Vector2){0}, 0, WHITE);
        if (ba->has_fog_shader) EndShaderMode();
    }
    DrawRectangle(0, 0, 1280, 720, (Color){ 0, 0, 0, alpha });
}

static void get_yaku_cards(const GameState *g, int winner, const ScoredYaku *y, int *out, int *out_cnt) {
    if (y->card_count > 0) {
        *out_cnt = y->card_count;
        for (int i = 0; i < y->card_count; i++) out[i] = y->card_ids[i];
        return;
    }
    int cnt = 0;
    if (winner >= 0 && winner <= 1) {
        CardType t = (y->id == YAKU_TANE) ? CARD_TYPE_ANIMAL : (y->id == YAKU_TANZAKU) ? CARD_TYPE_RIBBON : CARD_TYPE_PLAIN;
        for (int i = 0; i < g->captured_count[winner] && cnt < 10; i++) {
            int cid = g->captured[winner][i];
            /* The sake cup only counts as a plain in the KK-ADD-10 ruleset (see score_captures);
               in every other ruleset it belongs to Tane/viewings and must not be shown as Kasu */
            int counts_as_plain = (CARDS[cid].type == t) ||
                                  (y->id == YAKU_KASU && cid == 32 && g->ruleset == RULESET_KK_ADD_10);
            if (counts_as_plain) out[cnt++] = cid;
        }
    }
    *out_cnt = cnt;
}

static void draw_yaku_cards(const BoardAssets *ba, const int *cards, int count, float x, float y) {
    float step = (count > 1) ? fminf(MINI_CARD_W + 3.0f, (260.0f - MINI_CARD_W) / (count - 1)) : 0.0f;
    for (int c = 0; c < count; c++) board_draw_card(ba, cards[c], (Rectangle){ x + c * step, y, MINI_CARD_W, MINI_CARD_H });
}

void screens_draw_start(const BoardAssets *ba, const GameStats *st, Vector2 vm, float saved_timer) {
    (void)st; draw_bg_if_loaded(ba, 95);
    draw_centered_text(font_title, "[XENO / KOI]", 640.0f, 52.0f, 136.0f, 3.0f, COLOR_PHOSPHOR);
    draw_centered_text(font_body, "HANAFUDA: ACOUSTICA APEXUS", 640.0f, 220.0f, 26.0f, 3.0f, COLOR_PHOSPHOR_DIM);
    const char *lbls[7] = { "START MATCH", "ATRIUM PLEX", "OPTION", "STATS", "SAVE", "CREDITS", "QUIT" };
    for (int i = 0; i < 7; i++) draw_btn((Rectangle){ 490, 320 + i * 50, 300, 40 }, lbls[i], 18, vm, COLOR_PHOSPHOR);
    if (saved_timer > 0.0f) draw_centered_text(font_body, "ALL FILES SAVED", 640.0f, 675.0f, 20.0f, 1.5f, COLOR_PHOSPHOR);
}

ScreenAction screens_handle_start_click(Vector2 vm) {
    ScreenAction acts[7] = { ACT_START_GAME, ACT_GOTO_ATRIUM, ACT_GOTO_OPTIONS, ACT_GOTO_STATS, ACT_SAVE, ACT_GOTO_CREDITS, ACT_QUIT };
    for (int i = 0; i < 7; i++) if (CheckCollisionPointRec(vm, (Rectangle){ 490, 320 + i * 50, 300, 40 })) return acts[i];
    return ACT_NONE;
}

void screens_draw_credits(const BoardAssets *ba, Vector2 vm) {
    draw_bg_if_loaded(ba, 180);
    Rectangle panel = { 320, 80, 640, 540 };
    DrawRectangleRec(panel, COLOR_PANEL_BG); ui_draw_panel_border(panel, COLOR_PHOSPHOR_DIM);
    draw_centered_text(font_title, "CREDITS", 640.0f, 100.0f, 32.0f, 1.6f, COLOR_PHOSPHOR);
    draw_centered_text(font_body, "[]", 640.0f, 145.0f, 24.0f, 1.5f, COLOR_PHOSPHOR_DIM);
    draw_centered_text(font_title, "XENO / KOI", 640.0f, 190.0f, 42.0f, 1.8f, COLOR_PHOSPHOR);
    draw_centered_text(font_body, "Vibe Coder: [Aquaslopper]", 640.0f, 275.0f, 22.0f, 1.5f, COLOR_PHOSPHOR);
    draw_centered_text(font_body, "Music: [Graf Punk]", 640.0f, 330.0f, 22.0f, 1.5f, COLOR_PHOSPHOR);
    draw_centered_text(font_body, "Development Platform: Arena.ai - Agent mode.", 640.0f, 385.0f, 20.0f, 1.4f, COLOR_PHOSPHOR);
    draw_centered_text(font_body, "[]", 640.0f, 445.0f, 24.0f, 1.5f, COLOR_PHOSPHOR_DIM);
    draw_btn((Rectangle){ 520, 530, 240, 46 }, "BACK", 22, vm, COLOR_PHOSPHOR);
}

ScreenAction screens_handle_credits_click(Vector2 vm) {
    return CheckCollisionPointRec(vm, (Rectangle){ 520, 530, 240, 46 }) ? ACT_GOTO_MENU : ACT_NONE;
}

static void draw_slider(int y, float val, const char *lbl, const char *val_txt, Vector2 vm) {
    draw_text_ex_snapped(font_body, lbl, (Vector2){ 300.0f, (float)y + 4.0f }, 18.0f, 1.2f, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 650, (float)y, 34, 26 }, "-", 18, vm, COLOR_PHOSPHOR);
    Rectangle track = { 688, (float)y + 5, 184, 16 };
    DrawRectangleRec(track, COLOR_PANEL_BG);
    DrawRectangleLinesEx(track, 1.0f, COLOR_PHOSPHOR_DIM);
    DrawRectangle(688, y + 5, (int)(184.0f * val), 16, COLOR_PHOSPHOR);
    DrawRectangle((int)(688 + 184.0f * val - 3), y + 1, 6, 24, COLOR_PHOSPHOR);
    ui_theme_draw_panel_flares(track);
    draw_btn((Rectangle){ 876, (float)y, 34, 26 }, "+", 18, vm, COLOR_PHOSPHOR);
    draw_text_ex_snapped(font_body, val_txt, (Vector2){ 920.0f, (float)y + 4.0f }, 17.0f, 1.2f, COLOR_TEXT_INFO);
}

static int options_page; /* 0: original settings, 1: audio */
static int audio_test_state = -1; /* -1: idle, 0: same track dry, 1: effect on */
static int audio_test_unavailable;

int screens_audio_test_state(void) {
    return options_page == 1 ? audio_test_state : -1;
}

int screens_get_options_page(void) { return options_page; }
void screens_set_options_page(int page) { options_page = (page == 1) ? 1 : 0; }

#ifndef __EMSCRIPTEN__
static int music_folder_open;
static char music_folder_edit[MUSIC_FOLDER_CAP];
static const char *music_folder_message;

static void draw_music_folder_dialog(Vector2 vm) {
    DrawRectangle(0, 0, 1280, 720, (Color){0, 0, 0, 220});
    Rectangle panel = { 280, 230, 720, 250 };
    DrawRectangleRec(panel, COLOR_PANEL_BG);
    ui_draw_panel_border(panel, COLOR_PHOSPHOR);
    draw_centered_text(font_title, "CUSTOM MATCH MUSIC", 640, 250, 26, 1.2f, COLOR_PHOSPHOR);
    draw_centered_text(font_body, "Type a folder path or Ctrl+V to paste (replaces the field).", 640, 292, 17, 1, COLOR_TEXT_INFO);
    Rectangle field = { 300, 325, 680, 36 };
    DrawRectangleLinesEx(field, 1, COLOR_PHOSPHOR);
    float width = MeasureTextEx(font_body, music_folder_edit, 18, 1).x;
    BeginScissorMode(305, 330, 670, 26);
    draw_text_ex_snapped(font_body, music_folder_edit, (Vector2){305 - fmaxf(0, width - 665), 333}, 18, 1, COLOR_TEXT_INFO);
    EndScissorMode();
    draw_centered_text(font_body, music_folder_message, 640, 378, 16, 1, COLOR_TEXT_INFO);
    draw_btn((Rectangle){ 310, 424, 200, 34 }, "APPLY / ENTER", 17, vm, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 540, 424, 200, 34 }, "USE DEFAULT", 17, vm, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 770, 424, 200, 34 }, "CANCEL", 17, vm, COLOR_PHOSPHOR);
}

static void handle_music_folder_input(GameSettings *s, AudioAssets *audio, Vector2 vm) {
    int codepoint;
    while ((codepoint = GetCharPressed()) > 0) {
        int bytes;
        const char *utf8 = CodepointToUTF8(codepoint, &bytes);
        size_t len = strlen(music_folder_edit);
        if (codepoint >= 32 && len + bytes < sizeof(music_folder_edit)) {
            memcpy(music_folder_edit + len, utf8, bytes);
            music_folder_edit[len + bytes] = '\0';
        }
    }
    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
        size_t len = strlen(music_folder_edit);
        if (len) {
            do { len--; } while (len && (music_folder_edit[len] & 0xc0) == 0x80);
            music_folder_edit[len] = '\0';
        }
    }
    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_V)) {
        const char *text = GetClipboardText();
        if (text && strlen(text) < sizeof(music_folder_edit) && !strpbrk(text, "\r\n"))
            strcpy(music_folder_edit, text);
        else music_folder_message = "Cannot paste: path too long or contains line breaks.";
    }
    int clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (clicked && CheckCollisionPointRec(vm, (Rectangle){ 770, 424, 200, 34 })) {
        music_folder_open = 0;
        return;
    }
    int defaults = clicked && CheckCollisionPointRec(vm, (Rectangle){ 540, 424, 200, 34 });
    if (defaults || IsKeyPressed(KEY_ENTER) ||
        (clicked && CheckCollisionPointRec(vm, (Rectangle){ 310, 424, 200, 34 }))) {
        if (defaults) music_folder_edit[0] = '\0';
        size_t len = strlen(music_folder_edit);
        /* Windows Explorer's Copy as path adds surrounding quotes. */
        if (len >= 2 && music_folder_edit[0] == '"' && music_folder_edit[len - 1] == '"') {
            memmove(music_folder_edit, music_folder_edit + 1, len - 2);
            music_folder_edit[len - 2] = '\0';
        }
        if (!audio || audio_set_music_folder(audio, music_folder_edit) < 0) {
            music_folder_message = "Folder unavailable or no playable audio files. Music unchanged.";
            return;
        }
        strcpy(s->music_folder, music_folder_edit);
        storage_save_settings(s);
        music_folder_open = 0;
    }
}
#endif

static void draw_options_footer(Vector2 vm) {
    draw_btn((Rectangle){ 300, 656, 260, 38 }, "BACK (SAVE)", 20, vm, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 842, 656, 38, 38 }, "<", 20, vm, COLOR_PHOSPHOR);
    draw_centered_text(font_body, TextFormat("%d / 2", options_page + 1), 920, 665, 18, 1, COLOR_TEXT_INFO);
    draw_btn((Rectangle){ 960, 656, 38, 38 }, ">", 20, vm, COLOR_PHOSPHOR);
}

static void handle_audio_options(GameSettings *s, AudioAssets *audio, Vector2 vm) {
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(vm, (Rectangle){490, 580, 300, 36})) {
        audio_test_unavailable = !audio || audio->game_track_count == 0;
        if (!audio_test_unavailable) audio_test_state = audio_test_state == 1 ? 0 : 1;
    }
    if (audio_test_state >= 0 && (!audio || audio->game_track_count == 0)) {
        audio_test_state = -1;
        audio_test_unavailable = 1;
    }
    float *values[] = { &s->effect_chorus, &s->effect_bass, &s->effect_reverb };
    for (int i = 0; i < 3; i++) {
        float y = 320.0f + i * 64.0f;
        float value = *values[i];
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(vm, (Rectangle){688, y, 184, 26}))
            value = (vm.x - 688.0f) / 184.0f;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (CheckCollisionPointRec(vm, (Rectangle){650, y, 34, 26})) value -= 0.05f;
            if (CheckCollisionPointRec(vm, (Rectangle){876, y, 34, 26})) value += 0.05f;
        }
        *values[i] = fmaxf(0, fminf(1, value));
    }
    if (audio) audio_set_effects(s->effect_chorus, s->effect_bass, s->effect_reverb);
}

void screens_draw_options(const BoardAssets *ba, GameSettings *s, Vector2 vm) {
    int is_deus = (s->difficulty == DIFFICULTY_DEUS);
    draw_bg_if_loaded(ba, 180);
    Rectangle panel = { 260, 14, 760, 692 };
    DrawRectangleRec(panel, COLOR_PANEL_BG);
    ui_draw_panel_border(panel, COLOR_PHOSPHOR_DIM);

    draw_centered_text(font_title, "SYSTEM CONFIGURATION", 640.0f, 24.0f, 28.0f, 1.6f, COLOR_PHOSPHOR);

    if (options_page == 1) {
        draw_section_header("GAME MUSIC", 80);
#ifndef __EMSCRIPTEN__
        draw_text_ex_snapped(font_body, "Folder for music:", (Vector2){300, 135}, 18, 1.2f, COLOR_PHOSPHOR);
        draw_btn((Rectangle){650, 128, 300, 38}, s->music_folder[0] ? "CUSTOM FOLDER" : "BUILT-IN MUSIC", 18, vm, COLOR_PHOSPHOR);
#else
        draw_centered_text(font_body, "Music folder: desktop only (browser uses bundled music).", 640, 135, 17, 1, COLOR_TEXT_INFO);
#endif
        draw_section_header("Audio effect options", 230);
        draw_centered_text(font_body, "These settings change the Action card audio effect.", 640, 268, 17, 1, COLOR_TEXT_INFO);
        draw_slider(320, s->effect_chorus, "Chorus:", TextFormat("%d%%", (int)(100 * s->effect_chorus + 0.5f)), vm);
        draw_slider(384, s->effect_bass, "Bass boost:", TextFormat("+%.1f dB", 20 * s->effect_bass), vm);
        draw_slider(448, s->effect_reverb, "Space reverb:", TextFormat("%d%%", (int)(100 * s->effect_reverb + 0.5f)), vm);
        draw_centered_text(font_body, "Triggered by captures and Koi-Koi calls; fades afterwards.", 640, 510, 17, 1, COLOR_TEXT_INFO);
        draw_centered_text(font_body, "More reverb means a larger space. Bass enters near maximum.", 640, 542, 16, 1, COLOR_TEXT_INFO);
        draw_btn((Rectangle){490, 580, 300, 36}, audio_test_state == 1 ? "TEST EFFECT: ON" : "TEST EFFECT: OFF", 18, vm, COLOR_PHOSPHOR);
        draw_centered_text(font_body, audio_test_unavailable ? "No playable match music. Choose a folder or restore music assets."
                           : "ON tests full strength; OFF compares the same track dry.", 640, 626, 15, 1, COLOR_TEXT_INFO);
        draw_options_footer(vm);
#ifndef __EMSCRIPTEN__
        if (music_folder_open) draw_music_folder_dialog(vm);
#endif
        return;
    }

    draw_section_header("DISPLAY & SOUND", 56.0f);
    const char *cnames[6] = { "GREEN", "AMBER", "TEAL", "CRIMSON", "VIOLET", "DARK" };
    if (is_deus) draw_locked_row(96.0f, "Terminal Color:", "[ DARK (LOCKED) ]");
    else draw_selector_row(96.0f, "Terminal Color:", cnames[s->terminal_color % 6], 15, COLOR_TEXT_INFO, vm);

    const char *bg_names[8] = { "0: Harvest", "1: Hole", "2: Arctic", "3: Castle", "4: City", "5: Deus", "6: Face", "7: Realm" };
    Vector2 bg_vm = is_deus ? (Vector2){ -1.0f, -1.0f } : vm;
    Color bg_c = is_deus ? COLOR_PHOSPHOR_DIM : COLOR_PHOSPHOR;
    draw_text_ex_snapped(font_body, "Background:", (Vector2){ 300.0f, 158.0f }, 18.0f, 1.2f, bg_c);
    draw_btn((Rectangle){ 650, 154, 36, 26 }, "<", 18, bg_vm, bg_c);
    draw_option_value((Rectangle){ 690, 154, 130, 26 }, is_deus ? "[ DEUS (LOCKED) ]" : bg_names[s->background % 8], is_deus ? 13 : 15, bg_vm, is_deus ? COLOR_PHOSPHOR_DIM : COLOR_TEXT_INFO);
    draw_btn((Rectangle){ 824, 154, 36, 26 }, ">", 18, bg_vm, bg_c);
    int bg_idx = is_deus ? 5 : s->background;
    if (bg_idx >= 0 && bg_idx < 8 && ba->bgs[bg_idx].id > 0) {
        Rectangle prev = { 864, 154, 46, 26 };
        DrawTexturePro(ba->bgs[bg_idx], (Rectangle){0,0,1280,720}, prev, (Vector2){0}, 0, WHITE);
        DrawRectangleLinesEx(prev, 1.0f, COLOR_PHOSPHOR_DIM);
        ui_theme_draw_panel_flares(prev);
    }

    draw_text_ex_snapped(font_body, "Anim Speed:", (Vector2){ 300.0f, 216.0f }, 18.0f, 1.2f, COLOR_PHOSPHOR);
    draw_option_value((Rectangle){ 650, 212, 84, 26 }, "SLOW", 15, vm, (s->anim_speed == 0) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    draw_option_value((Rectangle){ 738, 212, 84, 26 }, "NORMAL", 15, vm, (s->anim_speed == 1) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    draw_option_value((Rectangle){ 826, 212, 84, 26 }, "FAST", 15, vm, (s->anim_speed == 2) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);

    draw_slider(270, s->bgm_volume, "BGM Volume:", TextFormat("%d%%", (int)(s->bgm_volume * 100.0f + 0.5f)), vm);
    draw_slider(328, s->sfx_volume, "SFX Volume:", TextFormat("%d%%", (int)(s->sfx_volume * 100.0f + 0.5f)), vm);

#ifdef __EMSCRIPTEN__
    /* The web shell owns fullscreen in the browser build, so show the browser's
       real state here rather than the saved desktop preference (which is
       ignored there, see ui_main.c). */
    int fs_active = EM_ASM_INT({ return (window.XenoKoiFullscreen && window.XenoKoiFullscreen.active()) ? 1 : 0; });
    const char *fs_names[2] = { "WINDOWED", "FULLSCREEN" };
    draw_selector_row(386.0f, "Fullscreen:", fs_names[fs_active ? 1 : 0], 15, COLOR_TEXT_INFO, vm);
#else
    const char *fs_names[2] = { "WINDOWED", "BORDERLESS" };
    draw_selector_row(386.0f, "Fullscreen:", fs_names[s->fullscreen ? 1 : 0], 15, COLOR_TEXT_INFO, vm);
#endif
    const char *bl_names[3] = { "OFF", "MID", "FULL" };
    draw_selector_row(444.0f, "Bloom Effect:", bl_names[s->bloom % 3], 15, (s->bloom > 0) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM, vm);
    const char *crt_names[4] = { "0: OFF", "1: LOW", "2: MID", "3: HIGH" };
    draw_selector_row(502.0f, "CRT Filter:", crt_names[s->crt_filter % 4], 15, (s->crt_filter > 0) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM, vm);

    float g_frac = fmaxf(0.0f, fminf(1.0f, (s->gamma - 0.5f) / 1.5f));
    draw_slider(560, g_frac, "Display Gamma:", TextFormat("%.2f", s->gamma), vm);
    draw_options_footer(vm);
}

ScreenAction screens_handle_options_click(GameSettings *s, BoardAssets *ba, AudioAssets *audio, Vector2 vm) {
#ifndef __EMSCRIPTEN__
    if (music_folder_open) {
        handle_music_folder_input(s, audio, vm);
        return ACT_NONE;
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && options_page == 1 && CheckCollisionPointRec(vm, (Rectangle){ 650, 128, 300, 38 })) {
        strcpy(music_folder_edit, s->music_folder);
        music_folder_message = "Up to 100 tracks (.mp3/.ogg/.wav), no subfolders. Blank restores default.";
        music_folder_open = 1;
        return ACT_NONE;
    }
#endif
    int pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT), down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    if (pressed && (CheckCollisionPointRec(vm, (Rectangle){842, 656, 38, 38}) ||
                    CheckCollisionPointRec(vm, (Rectangle){960, 656, 38, 38}))) {
        audio_test_state = -1; audio_test_unavailable = 0;
        options_page = 1 - options_page;
        return ACT_NONE;
    }
    if (pressed && CheckCollisionPointRec(vm, (Rectangle){300, 656, 260, 38})) {
        storage_save_settings(s);
        if (audio) audio_play_click(audio);
        audio_test_state = -1; audio_test_unavailable = 0;
        options_page = 0;
        return ACT_GOTO_MENU;
    }
    if (options_page == 1) {
        handle_audio_options(s, audio, vm);
        return ACT_NONE;
    }
    if (down) {
        if (CheckCollisionPointRec(vm, (Rectangle){ 688, 270, 184, 26 })) {
            s->bgm_volume = fmaxf(0.0f, fminf(1.0f, (vm.x - 688.0f) / 184.0f));
            if (audio) audio_set_bgm_volume(audio, s->bgm_volume);
        } else if (CheckCollisionPointRec(vm, (Rectangle){ 688, 328, 184, 26 })) {
            s->sfx_volume = fmaxf(0.0f, fminf(1.0f, (vm.x - 688.0f) / 184.0f));
            if (audio) { audio_set_sfx_volume(audio, s->sfx_volume); if (pressed) audio_play_click(audio); }
        } else if (CheckCollisionPointRec(vm, (Rectangle){ 688, 560, 184, 26 })) {
            s->gamma = 0.5f + fmaxf(0.0f, fminf(1.0f, (vm.x - 688.0f) / 184.0f)) * 1.5f;
            if (pressed && audio) audio_play_click(audio);
        }
    }
    if (!pressed) return ACT_NONE;

    int dir, is_deus = (s->difficulty == DIFFICULTY_DEUS);
    if (!is_deus && (dir = check_selector_click(vm, 96.0f)) != 0) { s->terminal_color = (s->terminal_color + 5 + dir) % 5; theme_set(s->terminal_color); if (audio) audio_play_click(audio); }
    else if (!is_deus && CheckCollisionPointRec(vm, (Rectangle){ 650, 154, 36, 26 })) { s->background = (s->background == 0) ? 7 : (s->background == 6) ? 4 : s->background - 1; ba->current_bg = s->background; if (audio) audio_play_click(audio); }
    else if (!is_deus && (CheckCollisionPointRec(vm, (Rectangle){ 690, 154, 130, 26 }) || CheckCollisionPointRec(vm, (Rectangle){ 824, 154, 36, 26 }))) { s->background = (s->background == 4) ? 6 : (s->background + 1) % 8; ba->current_bg = s->background; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 650, 212, 84, 26 })) { s->anim_speed = 0; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 738, 212, 84, 26 })) { s->anim_speed = 1; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 826, 212, 84, 26 })) { s->anim_speed = 2; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 650, 270, 34, 26 })) { s->bgm_volume = fmaxf(0.0f, s->bgm_volume - 0.10f); if (audio) { audio_set_bgm_volume(audio, s->bgm_volume); audio_play_click(audio); } }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 876, 270, 34, 26 })) { s->bgm_volume = fminf(1.0f, s->bgm_volume + 0.10f); if (audio) { audio_set_bgm_volume(audio, s->bgm_volume); audio_play_click(audio); } }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 650, 328, 34, 26 })) { s->sfx_volume = fmaxf(0.0f, s->sfx_volume - 0.10f); if (audio) { audio_set_sfx_volume(audio, s->sfx_volume); audio_play_click(audio); } }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 876, 328, 34, 26 })) { s->sfx_volume = fminf(1.0f, s->sfx_volume + 0.10f); if (audio) { audio_set_sfx_volume(audio, s->sfx_volume); audio_play_click(audio); } }
    else if (check_selector_click(vm, 386.0f) != 0) {
#ifdef __EMSCRIPTEN__
        /* Same switch as the page's [ FULLSCREEN ] button. Works while the game
           is fullscreen too, which is the only in-game way back out. */
        EM_ASM({ if (window.XenoKoiFullscreen) window.XenoKoiFullscreen.toggle(); });
#else
        s->fullscreen = 1 - s->fullscreen; ToggleBorderlessWindowed();
#endif
        if (audio) audio_play_click(audio);
    }
    else if ((dir = check_selector_click(vm, 444.0f)) != 0) { s->bloom = (s->bloom + 3 + dir) % 3; if (audio) audio_play_click(audio); }
    else if ((dir = check_selector_click(vm, 502.0f)) != 0) { s->crt_filter = (s->crt_filter + 4 + dir) % 4; if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 650, 560, 34, 26 })) { s->gamma = fmaxf(0.5f, s->gamma - 0.05f); if (audio) audio_play_click(audio); }
    else if (CheckCollisionPointRec(vm, (Rectangle){ 876, 560, 34, 26 })) { s->gamma = fminf(2.0f, s->gamma + 0.05f); if (audio) audio_play_click(audio); }

    return ACT_NONE;
}

/* Left-column statistics table: labels and values use the readable body face. */
static void draw_stat_row(int row, const char *label, const char *value, int divider) {
    float y = 120.0f + row * 30.0f;
    draw_text_ex_snapped(font_body, label, (Vector2){ 135.0f, y }, 18.0f, 1.2f, COLOR_PHOSPHOR_DIM);
    draw_text_ex_snapped(font_body, value, (Vector2){ 350.0f, y }, 18.0f, 1.2f, COLOR_TEXT_INFO);
    if (divider) DrawLine(135, (int)(y + 24), 585, (int)(y + 24), (Color){ COLOR_PHOSPHOR_DIM.r, COLOR_PHOSPHOR_DIM.g, COLOR_PHOSPHOR_DIM.b, 60 });
}

void screens_draw_stats(const BoardAssets *ba, const GameStats *st, const GameSettings *settings, const MatchHistory *h, Vector2 vm, int confirm_reset, float log_timer) {
    draw_bg_if_loaded(ba, 180);
    draw_centered_text(font_title, "COMBAT TELEMETRY // STATS", 640.0f, 34.0f, 30.0f, 1.4f, COLOR_PHOSPHOR);
    Rectangle p = { 100, 90, 1080, 500 };
    DrawRectangleRec(p, COLOR_PANEL_BG);
    ui_draw_panel_border(p, COLOR_PHOSPHOR_DIM);

    float wr = (st->matches_played > 0) ? ((float)st->matches_won / st->matches_played * 100.0f) : 0.0f;
    draw_stat_row(0, "Matches Played", TextFormat("%d", st->matches_played), 0);
    draw_stat_row(1, "Won / Lost / Draw", TextFormat("%d / %d / %d", st->matches_won, st->matches_lost, st->matches_drawn), 0);
    draw_stat_row(2, "Win Rate", TextFormat("%.1f%%", wr), 1);
    draw_stat_row(3, "Rounds Won", TextFormat("YOU %d | CPU %d", st->rounds_won_you, st->rounds_won_cpu), 0);
    draw_stat_row(4, "Total Points", TextFormat("FOR %d | AGAINST %d", st->points_for, st->points_against), 0);
    draw_stat_row(5, "Highest Round Score", TextFormat("%d pts", st->highest_round_score), 1);
    draw_stat_row(11, "Best Award", TextFormat("%d pts", st->best_award), 0);
    draw_stat_row(6, "Koi Calls", TextFormat("YOU %d | CPU %d", st->koi_calls_you, st->koi_calls_cpu), 0);
    draw_stat_row(7, "Difficulty", TextFormat("Easy %d | Hard %d | Master %d", st->easy_matches, st->hard_matches, st->master_matches), 1);
    draw_stat_row(8, "Current Streak", TextFormat("%d", st->current_streak), 0);
    draw_stat_row(9, "Max Win Streak", TextFormat("%d", st->best_streak), 0);
    draw_stat_row(10, "Favorite Combo", storage_get_favorite_combo(st), 0);

    DrawLine(620, 110, 620, 560, COLOR_PHOSPHOR_DIM);

    /* Clearance coin rows (step 2): one row per tier, 12 slots (one per opponent) */
    if (settings) {
        int slot_size = 18;
        int slot_step = 22;
        int slot_start = 270;
        int row_y0 = 485;
        const char *labels[3] = { "GOLD", "SILVER", "BRONZE" };
        Texture2D coins[3] = { ba->coin_gold, ba->coin_silver, ba->coin_bronze };
        for (int r = 0; r < 3; r++) {
            int ry = row_y0 + r * 30;
            if (coins[r].id > 0) {
                Rectangle src = { 0.0f, 0.0f, (float)coins[r].width, (float)coins[r].height };
                Rectangle dst = { 135.0f, (float)ry, 20.0f, 20.0f };
                DrawTexturePro(coins[r], src, dst, (Vector2){0,0}, 0.0f, WHITE);
            }
            draw_text_ex_snapped(font_body, labels[r], (Vector2){ 160.0f, (float)ry + 2.0f }, 16.0f, 1.2f, COLOR_PHOSPHOR_DIM);
            for (int i = 0; i < 12; i++) {
                int bit = i * 3 + (2 - r);
                int earned = (settings->cleared_mask & (1ULL << bit)) != 0;
                float sx = (float)(slot_start + i * slot_step);
                float sy = (float)(ry + 1);
                if (earned && coins[r].id > 0) {
                    Rectangle src = { 0.0f, 0.0f, (float)coins[r].width, (float)coins[r].height };
                    Rectangle dst = { sx, sy, (float)slot_size, (float)slot_size };
                    DrawTexturePro(coins[r], src, dst, (Vector2){0,0}, 0.0f, WHITE);
                } else {
                    DrawCircleLines((int)(sx + slot_size / 2), (int)(sy + slot_size / 2), slot_size / 2 - 1, (Color){ 60, 60, 60, 255 });
                }
            }
        }
    }
    draw_text_ex_snapped(font_title, "LAST 10 MATCHES", (Vector2){ 650.0f, 120.0f }, 18.0f, 1.2f, COLOR_PHOSPHOR);
#ifndef __EMSCRIPTEN__
    ui_replay_draw_rows(h, vm);
#else
    for (int i = 0, hy = 155; i < h->count; i++, hy += 28)
        draw_text_ex_snapped(font_body, h->lines[i], (Vector2){ 650.0f, (float)hy }, 16.0f, 1.2f, COLOR_TEXT_INFO_DIM);
#endif

    draw_btn((Rectangle){ 220, 610, 200, 44 }, "SAVE LOG", 20, vm, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 540, 610, 200, 44 }, "RESET STATS", 20, vm, COLOR_RED);
    draw_btn((Rectangle){ 860, 610, 200, 44 }, "BACK", 20, vm, COLOR_PHOSPHOR);
#ifndef __EMSCRIPTEN__
    /* The browser build has no replays/ folder, so it gets no replay controls. */
    ui_replay_draw_controls(vm);
#endif
    if (log_timer > 0.0f) draw_text_ex_snapped(font_body, "stats_report.txt written!", (Vector2){ 230.0f, 664.0f }, 16.0f, 1.2f, COLOR_TEXT_INFO);
    if (confirm_reset) {
        Rectangle modal = { 440, 240, 400, 200 };
        DrawRectangleRec(modal, (Color){ 0, 0, 0, 240 });
        ui_draw_panel_border(modal, COLOR_RED);
        draw_centered_text(font_title, "RESET ALL STATS & LOGS?", 640.0f, 270.0f, 20.0f, 1.1f, COLOR_RED);
        draw_text_ex_snapped(font_body, "This action cannot be undone.", (Vector2){ 510.0f, 305.0f }, 16.0f, 1.2f, COLOR_TEXT_INFO_DIM);
        draw_btn((Rectangle){ 480, 360, 140, 40 }, "CONFIRM", 18, vm, COLOR_RED);
        draw_btn((Rectangle){ 660, 360, 140, 40 }, "CANCEL", 18, vm, COLOR_PHOSPHOR);
    }
}

ScreenAction screens_handle_stats_click(Vector2 vm, int *confirm_reset) {
    if (*confirm_reset) {
        if (CheckCollisionPointRec(vm, (Rectangle){ 480, 360, 140, 40 })) { *confirm_reset = 0; return ACT_RESET_STATS; }
        if (CheckCollisionPointRec(vm, (Rectangle){ 660, 360, 140, 40 })) *confirm_reset = 0;
        return ACT_NONE;
    }
    if (CheckCollisionPointRec(vm, (Rectangle){ 220, 610, 200, 44 })) return ACT_SAVE_LOG;
    if (CheckCollisionPointRec(vm, (Rectangle){ 540, 610, 200, 44 })) { *confirm_reset = 1; return ACT_NONE; }
    return CheckCollisionPointRec(vm, (Rectangle){ 860, 610, 200, 44 }) ? ACT_GOTO_MENU : ACT_NONE;
}

void screens_draw_pause(Vector2 vm, int confirm_restart, int replay) {
    DrawRectangle(0, 0, 1280, 720, COLOR_OVERLAY_BG);
    Rectangle p = { 440, 180, 400, 360 };
    DrawRectangleRec(p, COLOR_PANEL_BG); ui_draw_panel_border(p, COLOR_PHOSPHOR);
    draw_centered_text(font_title, replay ? "REPLAY PAUSED" : "GAME PAUSED", 640.0f, 210.0f, 28.0f, 1.4f, COLOR_PHOSPHOR);
    if (replay) {
        draw_centered_text(font_body, "Nothing here counts towards stats.", 640.0f, 262.0f, 16.0f, 1.2f, COLOR_PHOSPHOR_DIM);
        draw_btn((Rectangle){ 490, 300, 300, 50 }, "RESUME", 20, vm, COLOR_PHOSPHOR);
        draw_btn((Rectangle){ 490, 380, 300, 50 }, "STOP REPLAY", 20, vm, COLOR_RED);
        return;
    }
    if (confirm_restart) {
        draw_centered_text(font_body, "Restart this match?", 640.0f, 280.0f, 20.0f, 1.2f, COLOR_PHOSPHOR);
        draw_btn((Rectangle){ 480, 350, 140, 40 }, "YES", 18, vm, COLOR_RED);
        draw_btn((Rectangle){ 660, 350, 140, 40 }, "NO", 18, vm, COLOR_PHOSPHOR);
    } else {
        draw_btn((Rectangle){ 490, 280, 300, 50 }, "RESUME", 20, vm, COLOR_PHOSPHOR);
        draw_btn((Rectangle){ 490, 350, 300, 50 }, "RESTART MATCH", 20, vm, COLOR_PHOSPHOR);
        draw_btn((Rectangle){ 490, 420, 300, 50 }, "ABANDON MATCH", 20, vm, COLOR_RED);
    }
}

ScreenAction screens_handle_pause_click(Vector2 vm, int *confirm_restart, int replay) {
    if (replay) {
        if (CheckCollisionPointRec(vm, (Rectangle){ 490, 300, 300, 50 })) return ACT_RESUME;
        return CheckCollisionPointRec(vm, (Rectangle){ 490, 380, 300, 50 }) ? ACT_STOP_REPLAY : ACT_NONE;
    }
    if (*confirm_restart) {
        if (CheckCollisionPointRec(vm, (Rectangle){ 480, 350, 140, 40 })) { *confirm_restart = 0; return ACT_RESTART_MATCH; }
        if (CheckCollisionPointRec(vm, (Rectangle){ 660, 350, 140, 40 })) { *confirm_restart = 0; return ACT_RESUME; }
        return ACT_NONE;
    }
    if (CheckCollisionPointRec(vm, (Rectangle){ 490, 280, 300, 50 })) return ACT_RESUME;
    if (CheckCollisionPointRec(vm, (Rectangle){ 490, 350, 300, 50 })) { *confirm_restart = 1; return ACT_NONE; }
    return CheckCollisionPointRec(vm, (Rectangle){ 490, 420, 300, 50 }) ? ACT_ABANDON_MATCH : ACT_NONE;
}

void screens_draw_match_result(const BoardAssets *ba, const GameState *g, const GameSettings *s, Vector2 vm) {
    draw_bg_if_loaded(ba, 170);
    int p0 = g->total_score[0], p1 = g->total_score[1];
    const char *title = (p0 > p1) ? "VICTORY" : (p0 < p1) ? "DEFEAT" : "DRAW";
    Color tcol = (p0 > p1) ? COLOR_PHOSPHOR : (p0 < p1) ? COLOR_RED : COLOR_PHOSPHOR_DIM;
    draw_centered_text(font_title, "MATCH OVER", 640.0f, 30.0f, 27.0f, 1.4f, COLOR_PHOSPHOR_DIM);
    draw_centered_text(font_title, title, 640.0f, 72.0f, 42.0f, 1.7f, tcol);
    if (p0 > p1 && s && s->difficulty != DIFFICULTY_DEUS) {
        int meets_clearance = (s->difficulty == DIFFICULTY_MASTER && s->rounds >= 3) ||
                              (s->difficulty == DIFFICULTY_HARD   && s->rounds >= 6) ||
                              (s->difficulty == DIFFICULTY_EASY   && s->rounds >= 9);
        if (meets_clearance) {
            int unlocked_next = (s->unlocked_opponents == s->current_opponent + 2);
            if (unlocked_next)
                draw_centered_text(font_body, "/// CLEARANCE VERIFIED: NEXT SECTOR UNLOCKED! ///", 640.0f, 120.0f, 17.0f, 1.2f, COLOR_PHOSPHOR);
            else
                draw_centered_text(font_body, "/// CLEARANCE VERIFIED: COIN EARNED ///", 640.0f, 120.0f, 17.0f, 1.2f, COLOR_PHOSPHOR);
        }
        else if (s->current_opponent == s->unlocked_opponents - 1)
            draw_centered_text(font_body, "/// MATCH WON (Clearance Unfulfilled: Requires Master 3+, Hard 6+, or Easy 9+ Rounds) ///", 640.0f, 120.0f, 17.0f, 1.2f, COLOR_AMBER);
    }
    Rectangle p = { 340, 150, 600, 440 };
    DrawRectangleRec(p, COLOR_PANEL_BG); ui_draw_panel_border(p, COLOR_PHOSPHOR_DIM);
    draw_text_ex_snapped(font_body, TextFormat("FINAL SCORE:   YOU %d  -  CPU %d", p0, p1), (Vector2){ 380.0f, 190.0f }, 24.0f, 1.3f, COLOR_PHOSPHOR);
    DrawLine(380, 230, 900, 230, COLOR_PHOSPHOR_DIM);
    draw_text_ex_snapped(font_title, "ROUND LEDGER:", (Vector2){ 380.0f, 245.0f }, 18.0f, 1.2f, COLOR_PHOSPHOR);
    /* All rounds are listed (a full match is 12). Short matches keep the single column; longer ones
       split into two columns so the rows never reach the PLAY AGAIN / MAIN MENU buttons below */
    int lc = (g->ledger_count > 12) ? 12 : g->ledger_count;
    int two_col = (lc > 6);
    int col_size = two_col ? (lc + 1) / 2 : lc;
    for (int i = 0; i < lc; i++) {
        const RoundRecord *r = &g->ledger[i];
        const char *w = (r->winner == 0) ? "YOU" : (r->winner == 1) ? "CPU" : "DRAW";
        float lx = two_col ? ((i < col_size) ? 370.0f : 660.0f) : 400.0f;
        float ly = 275.0f + (float)(two_col ? ((i < col_size) ? i : i - col_size) : i) * 26.0f;
        draw_text_ex_snapped(font_body, TextFormat("Round %d: %s (%d pts)", r->round_number, w, r->award), (Vector2){ lx, ly }, 18.0f, 1.2f, COLOR_TEXT_INFO);
    }
    draw_btn((Rectangle){ 490, 470, 300, 46 }, "PLAY AGAIN", 20, vm, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 490, 526, 300, 46 }, "MAIN MENU", 20, vm, COLOR_PHOSPHOR);

    /* KK-ADD-10 speed minigame: the conversion, in gold, in its own box below the
       board panel - the ledger window itself is full on a 12-round match. */
    if (g->ruleset == RULESET_KK_ADD_10 && speed_bar_total() > 0) {
        int mp = speed_bar_total(), bonus = mp / 10;
        Rectangle mp_box = { 400.0f, 604.0f, 480.0f, 48.0f };
        DrawRectangleRec(mp_box, COLOR_PANEL_BG);
        ui_draw_panel_border(mp_box, COLOR_AMBER);
        float glint = 0.82f + 0.18f * sinf((float)GetTime() * 2.2f);
        Color gold = COLOR_AMBER;
        gold.a = (unsigned char)(255.0f * glint);
        draw_centered_text(font_body, TextFormat("SPEED MINIPOINTS:  %d  =  +%d PTS", mp, bonus),
                           640.0f, mp_box.y + (mp_box.height - 20.0f) / 2.0f, 20.0f, 1.2f, gold);
    }
}

ScreenAction screens_handle_match_result_click(Vector2 vm) {
    if (CheckCollisionPointRec(vm, (Rectangle){ 490, 470, 300, 46 })) return ACT_START_GAME;
    return CheckCollisionPointRec(vm, (Rectangle){ 490, 526, 300, 46 }) ? ACT_GOTO_MENU : ACT_NONE;
}

void screens_draw_choice_overlay(const BoardAssets *ba, const GameState *g, Vector2 vm, int is_hand) {
    DrawRectangle(0, 0, 1280, 720, COLOR_OVERLAY_BG);
    draw_centered_text(font_title, "CHOOSE ONE OF THE MATCHING CARDS", 640.0f, 140.0f, 28.0f, 1.2f, COLOR_PHOSPHOR);
    Rectangle r0 = { 400, 180, CHOICE_CARD_W, CHOICE_CARD_H }, r1 = { 704, 180, CHOICE_CARD_W, CHOICE_CARD_H };
    board_draw_card(ba, g->choice_targets[0], r0); board_draw_card(ba, g->choice_targets[1], r1);
    if (CheckCollisionPointRec(vm, r0)) {
        board_draw_card_glow(r0, 1.0f);
        ui_theme_draw_panel_flares(r0);
    }
    if (CheckCollisionPointRec(vm, r1)) {
        board_draw_card_glow(r1, 1.0f);
        ui_theme_draw_panel_flares(r1);
    }
    if (is_hand) draw_btn((Rectangle){ 560, 500, 160, 44 }, "CANCEL", 20, vm, COLOR_RED);
}

int screens_check_choice_click(const GameState *g, Vector2 vm, int is_hand, int *out_target_id) {
    if (CheckCollisionPointRec(vm, (Rectangle){ 400, 180, CHOICE_CARD_W, CHOICE_CARD_H })) { *out_target_id = g->choice_targets[0]; return 1; }
    if (CheckCollisionPointRec(vm, (Rectangle){ 704, 180, CHOICE_CARD_W, CHOICE_CARD_H })) { *out_target_id = g->choice_targets[1]; return 1; }
    if (is_hand && CheckCollisionPointRec(vm, (Rectangle){ 560, 500, 160, 44 })) { *out_target_id = -1; return 2; }
    return 0;
}

void screens_draw_decide_overlay(const BoardAssets *ba, const GameState *g, Vector2 vm) {
    DrawRectangle(0, 0, 1280, 720, COLOR_OVERLAY_BG);
    Rectangle panel = { 200, 45, 880, 630 };
    DrawRectangleRec(panel, COLOR_PANEL_BG); ui_draw_panel_border(panel, COLOR_PHOSPHOR);
    int p = g->current_player, opp = 1 - p;
    ScoreResult sr = score_captures(g->ruleset, g->captured[p], g->captured_count[p]);
    int self_koi = g->koi_called[p];
    int award = calculate_award(sr.base_points, self_koi, g->koi_called[opp]);
    draw_centered_text(font_title, "YAKU DETECTED", 640.0f, 56.0f, 25.0f, 1.4f, COLOR_PHOSPHOR);
    draw_centered_text(font_title, "KOI-KOI / SHOBU", 640.0f, 86.0f, 22.0f, 1.2f, COLOR_PHOSPHOR_DIM);

    /* Every detected combination must be listed: KK-ADD-10 can stack more than six, so tighten the
       row step when needed to keep the last row above the totals divider at y=430 */
    int max_show = sr.yaku_count;
    float row_step = 46.0f;
    if (max_show > 6) {
        row_step = (430.0f - 122.0f - 40.0f) / (float)(max_show - 1);
        if (row_step > 46.0f) row_step = 46.0f;
    }
    for (int i = 0; i < max_show; i++) {
        const ScoredYaku *yaku = &sr.yakus[i];
        float y = 122.0f + (float)i * row_step;
        draw_text_ex_snapped(font_body, TextFormat("- %s", yaku->name), (Vector2){ 240.0f, y + 9.0f }, 20.0f, 1.3f, COLOR_TEXT_INFO);
        draw_text_ex_snapped(font_body, TextFormat("%d pts", yaku->points), (Vector2){ 600.0f, y + 9.0f }, 20.0f, 1.3f, COLOR_TEXT_INFO);
        int ycards[16], ycount = 0; get_yaku_cards(g, p, yaku, ycards, &ycount);
        draw_yaku_cards(ba, ycards, ycount, 690.0f, y);
    }

    DrawLine(240, 430, 1040, 430, COLOR_PHOSPHOR_DIM);
    draw_text_ex_snapped(font_body, TextFormat("Base: %d pts", sr.base_points), (Vector2){ 240.0f, 442.0f }, 20.0f, 1.3f, COLOR_TEXT_INFO);
    if (sr.base_points >= 7) draw_text_ex_snapped(font_body, "x2  7+ Multiplier", (Vector2){ 420.0f, 442.0f }, 18.0f, 1.3f, COLOR_TEXT_INFO);
    if (g->koi_called[opp] > 0) draw_text_ex_snapped(font_body, "x2  Opponent Koi", (Vector2){ 610.0f, 442.0f }, 17.0f, 1.3f, COLOR_TEXT_INFO);
    /* Escalating streak: every call you made adds +1 to your own multiplier. */
    if (self_koi > 0) {
        int streak = koi_caller_multiplier(self_koi);
        draw_text_ex_snapped(font_body, TextFormat("x%d  Koi Streak (%d call%s)", streak, self_koi, (self_koi == 1) ? "" : "s"),
                             (Vector2){ 790.0f, 442.0f }, 17.0f, 1.3f, COLOR_TEXT_INFO);
    }
    draw_text_ex_snapped(font_body, TextFormat("IF YOU SHOBU NOW: %d POINTS", award), (Vector2){ 240.0f, 475.0f }, 22.0f, 1.3f, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 280, 520, 320, 60 }, "KOI-KOI (CONTINUE)", 20, vm, COLOR_PHOSPHOR);
    draw_btn((Rectangle){ 680, 520, 320, 60 }, "SHOBU (BANK & END)", 20, vm, COLOR_PHOSPHOR);
    int next_mult = koi_caller_multiplier(self_koi + 1);
    if (next_mult > koi_caller_multiplier(self_koi)) {
        draw_text_ex_snapped(font_body, TextFormat("Next call pays x%d on your win.", next_mult), (Vector2){ 280.0f, 590.0f }, 15.0f, 1.2f, COLOR_TEXT_INFO_DIM);
    } else {
        draw_text_ex_snapped(font_body, TextFormat("Streak is maxed at x%d.", next_mult), (Vector2){ 280.0f, 590.0f }, 15.0f, 1.2f, COLOR_TEXT_INFO_DIM);
    }
    draw_text_ex_snapped(font_body, "Bank your current score and win this round.", (Vector2){ 680.0f, 590.0f }, 15.0f, 1.2f, COLOR_TEXT_INFO_DIM);
}

int screens_check_decide_click(Vector2 vm, DecisionType *out_decision) {
    if (CheckCollisionPointRec(vm, (Rectangle){ 280, 520, 320, 60 })) { *out_decision = DECISION_KOI_KOI; return 1; }
    if (CheckCollisionPointRec(vm, (Rectangle){ 680, 520, 320, 60 })) { *out_decision = DECISION_SHOBU; return 1; }
    return 0;
}

void screens_draw_round_result(const BoardAssets *ba, const GameState *g, Vector2 vm) {
    DrawRectangle(0, 0, 1280, 720, COLOR_OVERLAY_BG);
    Rectangle panel = { 200, 45, 880, 630 };
    DrawRectangleRec(panel, COLOR_PANEL_BG); ui_draw_panel_border(panel, COLOR_PHOSPHOR);
    const RoundRecord *rec = &g->current_round_record;
    const char *title = (rec->winner == 0) ? "YOU WIN THE ROUND!" : (rec->winner == 1) ? "CPU WINS THE ROUND!" : "ROUND DRAW!";
    Color tcol = (rec->winner == 0) ? COLOR_PHOSPHOR : (rec->winner == 1) ? COLOR_RED : COLOR_PHOSPHOR_DIM;
    draw_centered_text(font_title, title, 640.0f, 65.0f, 28.0f, 1.3f, tcol);

    if (rec->winner < 0) {
        if (rec->draw_reason == 1) draw_text_ex_snapped(font_body, "Reason: Exhaustion (No one scored)", (Vector2){ 460.0f, 150.0f }, 20.0f, 1.2f, COLOR_PHOSPHOR_DIM);
        else if (rec->draw_reason == 2) draw_text_ex_snapped(font_body, "Reason: Both players had Lucky Hands", (Vector2){ 460.0f, 150.0f }, 20.0f, 1.2f, COLOR_PHOSPHOR_DIM);
    } else {
        int y = 110;
        if (rec->lucky_hand) {
            draw_text_ex_snapped(font_body, "LUCKY HAND (Teshi / Kuttsuki): 6 pts", (Vector2){ 230.0f, (float)y + 10.0f }, 19.0f, 1.3f, COLOR_TEXT_INFO);
            /* The lucky hand is still sitting in the winner's hand - a lucky round ends at the deal,
               so the capture pile is empty and the row used to stay blank. */
            int lcards[8], lcnt = 0;
            if (rec->winner == 0 || rec->winner == 1) {
                for (int i = 0; i < g->hand_count[rec->winner] && lcnt < 8; i++) lcards[lcnt++] = g->hand[rec->winner][i];
            }
            draw_yaku_cards(ba, lcards, lcnt, 680.0f, (float)y); y += 48;
        } else if (rec->score_result.yaku_count == 0) {
            draw_text_ex_snapped(font_body, "OYA-KEN (Dealer's Privilege): 6 pts", (Vector2){ 230.0f, (float)y + 10.0f }, 19.0f, 1.3f, COLOR_TEXT_INFO); y += 48;
        } else {
            const ScoreResult *sr = &rec->score_result;
            for (int i = 0, max_show = (sr->yaku_count > 5) ? 5 : sr->yaku_count; i < max_show; i++, y += 44) {
                const ScoredYaku *yaku = &sr->yakus[i];
                draw_text_ex_snapped(font_body, TextFormat("- %s", yaku->name), (Vector2){ 230.0f, (float)y + 9.0f }, 18.0f, 1.2f, COLOR_TEXT_INFO);
                draw_text_ex_snapped(font_body, TextFormat("%d pts", yaku->points), (Vector2){ 600.0f, (float)y + 9.0f }, 18.0f, 1.2f, COLOR_TEXT_INFO);
                int ycards[16], ycount = 0; get_yaku_cards(g, rec->winner, yaku, ycards, &ycount);
                draw_yaku_cards(ba, ycards, ycount, 680.0f, (float)y);
            }
            if (sr->yaku_count > 5) { draw_text_ex_snapped(font_body, TextFormat("+ %d more combos...", sr->yaku_count - 5), (Vector2){ 230.0f, (float)y + 4.0f }, 16.0f, 1.2f, COLOR_TEXT_INFO_DIM); y += 24; }
            DrawLine(230, y + 6, 1050, y + 6, COLOR_PHOSPHOR_DIM); y += 16;
            draw_text_ex_snapped(font_body, TextFormat("Base: %d pts", rec->base_points), (Vector2){ 230.0f, (float)y }, 18.0f, 1.2f, COLOR_TEXT_INFO); y += 22;
            if (rec->mult_seven > 1) { draw_text_ex_snapped(font_body, "x2 7+ Points Multiplier", (Vector2){ 230.0f, (float)y }, 16.0f, 1.2f, COLOR_TEXT_INFO); y += 20; }
            if (rec->mult_opp_koi > 1) { draw_text_ex_snapped(font_body, "x2 Opponent Koi-Koi Multiplier", (Vector2){ 230.0f, (float)y }, 16.0f, 1.2f, COLOR_TEXT_INFO); y += 20; }
            if (rec->mult_self_koi > 1) { draw_text_ex_snapped(font_body, TextFormat("x%d Koi-Koi Streak Multiplier (%d calls)", rec->mult_self_koi, rec->self_koi_calls), (Vector2){ 230.0f, (float)y }, 16.0f, 1.2f, COLOR_TEXT_INFO); y += 20; }
        }
        draw_text_ex_snapped(font_body, TextFormat("ROUND AWARD: %d PTS", rec->award), (Vector2){ 230.0f, (float)y + 4.0f }, 22.0f, 1.2f, COLOR_PHOSPHOR);
    }

    draw_text_ex_snapped(font_body, TextFormat("MATCH RUNNING SCORE:  YOU %d  -  CPU %d", g->total_score[0], g->total_score[1]), (Vector2){ 230.0f, 565.0f }, 20.0f, 1.2f, COLOR_PHOSPHOR_DIM);
    draw_btn((Rectangle){ 520, 610, 240, 48 }, (g->current_round >= g->total_rounds) ? "MATCH RESULTS" : "NEXT ROUND", 20, vm, COLOR_PHOSPHOR);
}

int screens_check_round_result_click(Vector2 vm) { return CheckCollisionPointRec(vm, (Rectangle){ 520, 610, 240, 48 }); }

void screens_draw_banner(const char *text) {
    Vector2 ts = MeasureTextEx(font_title, text, 28.0f, 1.4f);
    Rectangle br = { (1280.0f - ts.x) / 2.0f - 30.0f, 320.0f, ts.x + 60.0f, 60.0f };
    DrawRectangleRec(br, (Color){ 0, 0, 0, 230 }); ui_draw_panel_border(br, COLOR_PHOSPHOR);
    draw_text_ex_snapped(font_title, text, (Vector2){ (1280.0f - ts.x) / 2.0f, 336.0f }, 28.0f, 1.4f, COLOR_PHOSPHOR);
}
