#include "ui_replay.h"
#include "replay.h"
#include "ui_theme.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

extern Font font_title;
extern Font font_body;

#define REPLAY_DIR       "replays"
#define REPLAY_LIST_MAX  12                 /* rows shown in the load window */
#define REPLAY_NAME_MAX  24

/* Log lines: the ten rows under "LAST 10 MATCHES". */
#define ROW_X 650.0f
#define ROW_Y 155.0f
#define ROW_H 24.0f
#define ROW_W 400.0f
#define ROW_STEP 28.0f

/* Buttons under the log lines. */
#define BTN_Y 452.0f
#define BTN_W 145.0f
#define BTN_H 34.0f

static int   sel = -1;                      /* selected log line, -1 = none */
static int   save_open, load_open;
static char  save_name[REPLAY_NAME_MAX + 1];
static int   save_len;
static char  file_names[REPLAY_LIST_MAX][64];
static int   file_count, load_sel;
static int   start_req;                     /* [START REPLAY] or [PLAY] was pressed */
static char  start_path[160];
static char  status[64];
static float status_timer;

static void set_status(const char *msg) {
    snprintf(status, sizeof(status), "%s", msg);
    status_timer = 2.5f;
}

void ui_replay_set_status(const char *msg) { set_status(msg); }

static void draw_text_snapped(Font font, const char *text, float x, float y, float size, float spacing, Color col) {
    DrawTextEx(font, text, (Vector2){ floorf(x), floorf(y) }, size, spacing, col);
}

static int button(Rectangle r, const char *label, float size, Vector2 vm, Color col) {
    int hov = CheckCollisionPointRec(vm, r);
    DrawRectangleRec(r, hov ? COLOR_BTN_HOVER_BG : COLOR_PANEL_BG);
    ui_draw_panel_border(r, hov ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    Vector2 ts = MeasureTextEx(font_title, label, size, 1.0f);
    draw_text_snapped(font_title, label, r.x + (r.width - ts.x) * 0.5f, r.y + (r.height - size) * 0.5f,
                      size, 1.0f, hov ? COLOR_PHOSPHOR : col);
    return hov;
}

/* ---- files ---- */

static void ensure_dir(void) {
    if (!DirectoryExists(REPLAY_DIR)) MakeDirectory(REPLAY_DIR);
}

static int is_txt(const char *path) {
    size_t n = strlen(path);
    return n > 4 && strcmp(path + n - 4, ".txt") == 0;
}

static int is_auto_file(const char *path) {
    char *text = LoadFileText(path);
    if (!text) return 0;
    int is_auto = strstr(text, "auto=1") != NULL;
    UnloadFileText(text);
    return is_auto;
}

/* Names of the records in replays/, oldest first (automatic names are timestamps). */
static void refresh_files(void) {
    file_count = 0; load_sel = -1;
    ensure_dir();
    FilePathList list = LoadDirectoryFiles(REPLAY_DIR);
    for (unsigned i = 0; i < list.count && file_count < REPLAY_LIST_MAX; i++) {
        if (!is_txt(list.paths[i]) || DirectoryExists(list.paths[i])) continue;
        snprintf(file_names[file_count], sizeof(file_names[0]), "%s", GetFileName(list.paths[i]));
        file_count++;
    }
    UnloadDirectoryFiles(list);
    for (int i = 1; i < file_count; i++) {
        char key[64];
        int j = i - 1;
        snprintf(key, sizeof(key), "%s", file_names[i]);
        while (j >= 0 && strcmp(file_names[j], key) > 0) {
            snprintf(file_names[j + 1], sizeof(file_names[0]), "%s", file_names[j]);
            j--;
        }
        snprintf(file_names[j + 1], sizeof(file_names[0]), "%s", key);
    }
}

/* The record of log line `idx`: automatic files are named after the line's
   timestamp, "YYYY-MM-DD_HHMM.txt", and a second match in the same minute gets
   "_2", "_3" and so on - so the nth line of a minute reads the nth file. */
static int auto_path_for_line(const MatchHistory *h, int idx, char *out, size_t max) {
    if (!h || idx < 0 || idx >= h->count) return 0;
    const char *line = h->lines[idx];
    if (strlen(line) < 16) return 0;
    char base[32];
    snprintf(base, sizeof(base), "%.10s_%.2s%.2s", line, line + 11, line + 14);
    int nth = 0;
    for (int i = 0; i < idx; i++) if (strncmp(h->lines[i], line, 16) == 0) nth++;
    for (int n = 1; n <= nth + 1; n++) {
        if (n == 1) snprintf(out, max, "%s/%s.txt", REPLAY_DIR, base);
        else snprintf(out, max, "%s/%s_%d.txt", REPLAY_DIR, base, n);
    }
    return FileExists(out);
}

/* Keep only the ten newest automatic records; player-saved files are never touched. */
static void rotate_auto_files(void) {
    char autos[64][64];
    int n = 0;
    FilePathList list = LoadDirectoryFiles(REPLAY_DIR);
    for (unsigned i = 0; i < list.count && n < 64; i++) {
        if (!is_txt(list.paths[i]) || DirectoryExists(list.paths[i])) continue;
        if (!is_auto_file(list.paths[i])) continue;
        snprintf(autos[n], sizeof(autos[0]), "%s", GetFileName(list.paths[i]));
        n++;
    }
    UnloadDirectoryFiles(list);
    for (int i = 1; i < n; i++) {
        char key[64];
        int j = i - 1;
        snprintf(key, sizeof(key), "%s", autos[i]);
        while (j >= 0 && strcmp(autos[j], key) > 0) {
            snprintf(autos[j + 1], sizeof(autos[0]), "%s", autos[j]);
            j--;
        }
        snprintf(autos[j + 1], sizeof(autos[0]), "%s", key);
    }
    char path[160];
    for (int i = 0; i < n - 10; i++) {
        snprintf(path, sizeof(path), "%s/%s", REPLAY_DIR, autos[i]);
        remove(path);
    }
}

void ui_replay_commit_auto(const MatchHistory *h) {
    const char *blob = replay_rec_blob();
    if (!blob || !h || h->count < 1) { replay_rec_abort(); return; }
    ensure_dir();

    const char *line = h->lines[h->count - 1];
    if (strlen(line) < 16) { replay_rec_abort(); return; }
    char base[32], path[160];
    snprintf(base, sizeof(base), "%.10s_%.2s%.2s", line, line + 11, line + 14);
    for (int n = 1;; n++) {
        if (n == 1) snprintf(path, sizeof(path), "%s/%s.txt", REPLAY_DIR, base);
        else snprintf(path, sizeof(path), "%s/%s_%d.txt", REPLAY_DIR, base, n);
        if (!FileExists(path)) break;
    }
    char *text = replay_build_file(blob, 1);
    if (text) SaveFileText(path, text);
    rotate_auto_files();
    replay_rec_abort();
}

static void save_as_copy(const MatchHistory *h) {
    if (sel < 0) { set_status("SELECT A MATCH FIRST"); return; }
    char src[160];
    if (!auto_path_for_line(h, sel, src, sizeof(src))) { set_status("NO REPLAY FOR THIS MATCH"); return; }
    char *text = LoadFileText(src);
    if (!text) { set_status("COULD NOT READ THE RECORD"); return; }
    char *flag = strstr(text, "auto=1");
    if (flag) flag[5] = '0';                        /* a saved copy is never rotated away */
    char dst[200];
    snprintf(dst, sizeof(dst), "%s/%s.txt", REPLAY_DIR, save_name);
    if (FileExists(dst)) { set_status("NAME ALREADY EXISTS"); UnloadFileText(text); return; }
    int ok = SaveFileText(dst, text);
    UnloadFileText(text);
    if (ok) {
        char msg[64];
        snprintf(msg, sizeof(msg), "SAVED TO %s/%s.txt", REPLAY_DIR, save_name);
        set_status(msg);
    } else set_status("SAVE FAILED");
}

/* ---- clicks ---- */

int ui_replay_click(const MatchHistory *h, Vector2 vm) {
    if (save_open) {
        if (CheckCollisionPointRec(vm, (Rectangle){ 480, 356, 140, 40 })) { save_as_copy(h); save_open = 0; }
        else if (CheckCollisionPointRec(vm, (Rectangle){ 660, 356, 140, 40 })) save_open = 0;
        return 1;
    }
    if (load_open) {
        for (int i = 0; i < file_count; i++) {
            if (CheckCollisionPointRec(vm, (Rectangle){ 420, 200 + i * 26, 440, 22 })) { load_sel = i; return 1; }
        }
        if (CheckCollisionPointRec(vm, (Rectangle){ 420, 520, 160, 38 })) {
            if (load_sel >= 0) {
                snprintf(start_path, sizeof(start_path), "%s/%s", REPLAY_DIR, file_names[load_sel]);
                start_req = 1; load_open = 0;
            } else set_status("SELECT A REPLAY FILE");
        } else if (CheckCollisionPointRec(vm, (Rectangle){ 600, 520, 160, 38 })) load_open = 0;
        return 1;
    }

    for (int i = 0; i < h->count; i++) {
        if (CheckCollisionPointRec(vm, (Rectangle){ ROW_X, ROW_Y + i * ROW_STEP - 3.0f, ROW_W, ROW_H })) {
            sel = (sel == i) ? -1 : i;
            return 1;
        }
    }
    if (CheckCollisionPointRec(vm, (Rectangle){ ROW_X, BTN_Y, BTN_W, BTN_H })) {
        if (!auto_path_for_line(h, sel, start_path, sizeof(start_path))) set_status("NO REPLAY FOR THIS MATCH");
        else start_req = 1;
        return 1;
    }
    if (CheckCollisionPointRec(vm, (Rectangle){ ROW_X + BTN_W + 10.0f, BTN_Y, BTN_W, BTN_H })) {
        if (sel < 0) set_status("SELECT A MATCH FIRST");
        else { save_open = 1; save_len = 0; save_name[0] = '\0'; }
        return 1;
    }
    if (CheckCollisionPointRec(vm, (Rectangle){ ROW_X + 2 * (BTN_W + 10.0f), BTN_Y, BTN_W, BTN_H })) {
        refresh_files();
        load_open = 1;
        return 1;
    }
    return 0;
}

void ui_replay_keys(const MatchHistory *h) {
    if (!save_open) return;
    int c;
    while ((c = GetCharPressed()) > 0) {
        int ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || c == '_';
        if (ok && save_len < REPLAY_NAME_MAX) { save_name[save_len++] = (char)c; save_name[save_len] = '\0'; }
    }
    if (IsKeyPressed(KEY_BACKSPACE) && save_len > 0) save_name[--save_len] = '\0';
    if (IsKeyPressed(KEY_ENTER)) {
        if (save_len == 0) set_status("ENTER A NAME FIRST");
        else { save_as_copy(h); save_open = 0; }
    }
    if (IsKeyPressed(KEY_ESCAPE)) save_open = 0;
}

int ui_replay_take_start(GameState *g) {
    if (!start_req) return 0;
    start_req = 0;
    char *text = LoadFileText(start_path);
    if (!text) { set_status("COULD NOT READ THE REPLAY FILE"); return 0; }
    int ok = replay_pb_load(text);
    UnloadFileText(text);
    if (!ok) { set_status(replay_pb_error()); return 0; }
    replay_pb_begin(g);
    return 1;
}

/* ---- drawing ---- */

void ui_replay_draw_rows(const MatchHistory *h, Vector2 vm) {
    if (sel >= h->count) sel = -1;
    for (int i = 0; i < h->count; i++) {
        Rectangle r = { ROW_X, ROW_Y + i * ROW_STEP - 3.0f, ROW_W, ROW_H };
        int hov = CheckCollisionPointRec(vm, r);
        if (i == sel) {
            DrawRectangleRec(r, COLOR_BTN_HOVER_BG);
            ui_draw_panel_border(r, COLOR_PHOSPHOR);
        } else if (hov) ui_draw_panel_border(r, COLOR_PHOSPHOR_DIM);
        draw_text_snapped(font_body, h->lines[i], ROW_X, ROW_Y + i * ROW_STEP, 16.0f, 1.2f,
                          (i == sel) ? COLOR_PHOSPHOR : COLOR_TEXT_INFO_DIM);
    }
}

void ui_replay_draw_controls(Vector2 vm) {
    if (status_timer > 0.0f) {
        status_timer -= GetFrameTime();
        if (status_timer > 0.0f) draw_text_snapped(font_body, status, ROW_X, 494.0f, 15.0f, 1.0f, COLOR_AMBER);
    }

    button((Rectangle){ ROW_X, BTN_Y, BTN_W, BTN_H }, "START REPLAY", 13.0f, vm, COLOR_PHOSPHOR);
    button((Rectangle){ ROW_X + BTN_W + 10.0f, BTN_Y, BTN_W, BTN_H }, "SAVE REPLAY", 13.0f, vm, COLOR_PHOSPHOR);
    button((Rectangle){ ROW_X + 2 * (BTN_W + 10.0f), BTN_Y, BTN_W, BTN_H }, "LOAD REPLAY", 13.0f, vm, COLOR_PHOSPHOR);

    if (save_open) {
        Rectangle p = { 440, 240, 400, 200 };
        DrawRectangleRec(p, (Color){ 0, 0, 0, 240 });
        ui_draw_panel_border(p, COLOR_PHOSPHOR);
        draw_text_snapped(font_title, "SAVE REPLAY AS", 640.0f - MeasureTextEx(font_title, "SAVE REPLAY AS", 20.0f, 1.1f).x / 2.0f,
                          262.0f, 20.0f, 1.1f, COLOR_PHOSPHOR);
        Rectangle field = { 470, 296, 340, 34 };
        DrawRectangleRec(field, COLOR_PANEL_BG);
        ui_draw_panel_border(field, COLOR_PHOSPHOR_DIM);
        draw_text_snapped(font_body, save_name, field.x + 8.0f, field.y + 7.0f, 18.0f, 1.0f, COLOR_TEXT_INFO);
        draw_text_snapped(font_body, "A-Z 0-9 - _   ENTER = SAVE   ESC = CANCEL", 470.0f, 338.0f, 14.0f, 1.0f, COLOR_TEXT_INFO_DIM);
        button((Rectangle){ 480, 356, 140, 40 }, "SAVE", 16.0f, vm, COLOR_PHOSPHOR);
        button((Rectangle){ 660, 356, 140, 40 }, "CANCEL", 16.0f, vm, COLOR_PHOSPHOR);
    } else if (load_open) {
        Rectangle p = { 380, 150, 520, 430 };
        DrawRectangleRec(p, (Color){ 0, 0, 0, 240 });
        ui_draw_panel_border(p, COLOR_PHOSPHOR);
        draw_text_snapped(font_title, "LOAD REPLAY", 640.0f - MeasureTextEx(font_title, "LOAD REPLAY", 20.0f, 1.1f).x / 2.0f,
                          166.0f, 20.0f, 1.1f, COLOR_PHOSPHOR);
        if (file_count == 0) draw_text_snapped(font_body, "replays/ IS EMPTY", 420.0f, 205.0f, 16.0f, 1.0f, COLOR_TEXT_INFO_DIM);
        for (int i = 0; i < file_count; i++) {
            Rectangle r = { 420, 200 + i * 26, 440, 22 };
            if (i == load_sel) DrawRectangleRec(r, COLOR_BTN_HOVER_BG);
            draw_text_snapped(font_body, file_names[i], 424.0f, 202.0f + i * 26, 16.0f, 1.0f,
                              (i == load_sel) ? COLOR_PHOSPHOR : COLOR_TEXT_INFO);
        }
        button((Rectangle){ 420, 520, 160, 38 }, "PLAY", 16.0f, vm, COLOR_PHOSPHOR);
        button((Rectangle){ 600, 520, 160, 38 }, "CANCEL", 16.0f, vm, COLOR_PHOSPHOR);
    }
}
