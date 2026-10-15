#include "ui_replay.h"
#include "replay.h"
#include "ui_theme.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern Font font_title;
extern Font font_body;

#define REPLAY_PAGE_SIZE 10                 /* ten files per load-list page */
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
static char (*file_names)[256];
static int   file_count, file_page, load_sel;
static int   start_req;                     /* [START REPLAY] or [PLAY] was pressed */
static char  start_path[512];
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

static void replay_dir_path(char *out, size_t max) {
    const char *base = GetApplicationDirectory();
    if (base && base[0]) {
        size_t n = strlen(base);
        const char *sep = (base[n - 1] == '/' || base[n - 1] == '\\') ? "" : "/";
        snprintf(out, max, "%s%sreplays", base, sep);
    } else snprintf(out, max, "replays");
}

static void replay_file_path(const char *name, char *out, size_t max) {
    char dir[512];
    replay_dir_path(dir, sizeof(dir));
    snprintf(out, max, "%s/%s", dir, name);
}

static void ensure_dir(void) {
    char dir[512];
    replay_dir_path(dir, sizeof(dir));
    if (!DirectoryExists(dir)) MakeDirectory(dir);
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

/* Names of every .txt replay in the folder, shown ten at a time. */
static void refresh_files(void) {
    free(file_names);
    file_names = NULL;
    file_count = 0; file_page = 0; load_sel = -1;
    ensure_dir();
    char dir[512];
    replay_dir_path(dir, sizeof(dir));
    FilePathList list = LoadDirectoryFiles(dir);
    for (unsigned i = 0; i < list.count; i++)
        if (is_txt(list.paths[i]) && !DirectoryExists(list.paths[i])) file_count++;
    if (file_count > 0) {
        file_names = (char (*)[256])calloc((size_t)file_count, sizeof(*file_names));
        if (!file_names) file_count = 0;
    }
    int out = 0;
    for (unsigned i = 0; i < list.count && out < file_count; i++) {
        if (!is_txt(list.paths[i]) || DirectoryExists(list.paths[i])) continue;
        snprintf(file_names[out++], sizeof(file_names[0]), "%s", GetFileName(list.paths[i]));
    }
    UnloadDirectoryFiles(list);
    for (int i = 1; i < file_count; i++) {
        char key[256];
        int j = i - 1;
        snprintf(key, sizeof(key), "%s", file_names[i]);
        while (j >= 0 && strcmp(file_names[j], key) > 0) {
            memcpy(file_names[j + 1], file_names[j], strlen(file_names[j]) + 1);
            j--;
        }
        snprintf(file_names[j + 1], sizeof(file_names[0]), "%s", key);
    }
}

static void replay_base_from_line(const char *line, char *base, size_t max) {
    if (max) base[0] = '\0';
    if (strlen(line) < 17 || line[4] != '-' || line[7] != '-' || line[10] != ' ' ||
        line[13] != ':' || line[16] != ';') return;
    for (int i = 0; i < 16; i++) {
        if (i == 4 || i == 7 || i == 10 || i == 13) continue;
        if (line[i] < '0' || line[i] > '9') return;
    }
    snprintf(base, max, "%.10s_%.2s%.2s", line, line + 11, line + 14);
}

static int auto_name_matches_base(const char *name, const char *base) {
    if (strncmp(name, base, 15) != 0) return 0;
    size_t len = strlen(name);
    if (len == 19 && name[15] == '.' && strcmp(name + 15, ".txt") == 0) return 1;
    if (name[15] != '_' || name[16] < '0' || name[16] > '9') return 0;
    const char *dot = strrchr(name, '.');
    if (!dot || strcmp(dot, ".txt") != 0) return 0;
    for (const char *p = name + 16; p < dot; p++)
        if (*p < '0' || *p > '9') return 0;
    return atoi(name + 16) >= 2;
}

static int auto_name_suffix(const char *name) {
    return (strlen(name) > 16 && name[15] == '_') ? atoi(name + 16) : 1;
}

static int auto_name_compare(const char *a, const char *b) {
    size_t an = strlen(a), bn = strlen(b);
    if (an < 15 || bn < 15) return strcmp(a, b);
    int base_cmp = strncmp(a, b, 15);
    if (base_cmp != 0) return base_cmp;
    int as = auto_name_suffix(a), bs = auto_name_suffix(b);
    return (as > bs) - (as < bs);
}

static void sort_auto_names(char names[][64], int count) {
    for (int i = 1; i < count; i++) {
        char key[64];
        int j = i - 1;
        snprintf(key, sizeof(key), "%s", names[i]);
        while (j >= 0 && auto_name_compare(names[j], key) > 0) {
            memcpy(names[j + 1], names[j], strlen(names[j]) + 1);
            j--;
        }
        snprintf(names[j + 1], sizeof(names[0]), "%s", key);
    }
}

/* Find the automatic records matching one minute, in numeric suffix order. */
static int auto_names_for_base(const char *base, char names[][64], int max_names) {
    int n = 0;
    char dir[512];
    replay_dir_path(dir, sizeof(dir));
    FilePathList list = LoadDirectoryFiles(dir);
    for (unsigned i = 0; i < list.count && n < max_names; i++) {
        const char *name = GetFileName(list.paths[i]);
        if (!is_txt(name) || DirectoryExists(list.paths[i]) || !auto_name_matches_base(name, base)) continue;
        if (!is_auto_file(list.paths[i])) continue;
        snprintf(names[n++], sizeof(names[0]), "%s", name);
    }
    UnloadDirectoryFiles(list);
    sort_auto_names(names, n);
    return n;
}

/* The history has no record key, so same-minute collisions are matched by
   order. If auto records outlive a reset or some older records are missing,
   align the available files with the newest visible lines for that minute. */
static int auto_path_for_line(const MatchHistory *h, int idx, char *out, size_t max) {
    if (!h || idx < 0 || idx >= h->count) return 0;
    const char *line = h->lines[idx];
    char base[32], names[64][64];
    replay_base_from_line(line, base, sizeof(base));
    if (!base[0]) return 0;

    int ordinal = 0, rows = 0;
    for (int i = 0; i < h->count; i++) {
        if (strncmp(h->lines[i], line, 16) == 0) {
            if (i < idx) ordinal++;
            rows++;
        }
    }
    int files = auto_names_for_base(base, names, 64);
    int file_idx = files - rows + ordinal;
    if (file_idx < 0 || file_idx >= files) return 0;
    replay_file_path(names[file_idx], out, max);
    return FileExists(out);
}
/* Keep only the ten newest automatic records; player-saved files are never touched. */
static void rotate_auto_files(void) {
    char autos[64][64];
    int n = 0;
    char dir[512];
    replay_dir_path(dir, sizeof(dir));
    FilePathList list = LoadDirectoryFiles(dir);
    for (unsigned i = 0; i < list.count && n < 64; i++) {
        if (!is_txt(list.paths[i]) || DirectoryExists(list.paths[i])) continue;
        if (!is_auto_file(list.paths[i])) continue;
        snprintf(autos[n], sizeof(autos[0]), "%s", GetFileName(list.paths[i]));
        n++;
    }
    UnloadDirectoryFiles(list);
    sort_auto_names(autos, n);
    char path[512];
    for (int i = 0; i < n - 10; i++) {
        replay_file_path(autos[i], path, sizeof(path));
        remove(path);
    }
}

void ui_replay_commit_auto(const MatchHistory *h) {
    sel = -1;
    const char *blob = replay_rec_blob();
    if (!blob || !h || h->count < 1) { set_status("REPLAY RECORD UNAVAILABLE"); replay_rec_abort(); return; }
    ensure_dir();

    const char *line = h->lines[h->count - 1];
    if (strlen(line) < 17) { set_status("REPLAY LOG ENTRY INVALID"); replay_rec_abort(); return; }
    char base[32], name[64], path[512], autos[64][64];
    replay_base_from_line(line, base, sizeof(base));
    if (!base[0]) { set_status("REPLAY LOG ENTRY INVALID"); replay_rec_abort(); return; }
    int auto_count = auto_names_for_base(base, autos, 64);
    int n = auto_count > 0 ? auto_name_suffix(autos[auto_count - 1]) + 1 : 1;
    for (;;) {
        if (n == 1) snprintf(name, sizeof(name), "%s.txt", base);
        else snprintf(name, sizeof(name), "%s_%d.txt", base, n);
        replay_file_path(name, path, sizeof(path));
        if (!FileExists(path)) break; /* do not replace a player-saved file */
        n++;
    }
    char *text = replay_build_file(blob, 1);
    if (text && SaveFileText(path, text)) {
        rotate_auto_files();
        status_timer = 0.0f;
        status[0] = '\0';
    } else set_status("COULD NOT SAVE AUTO REPLAY");
    replay_rec_abort();
}

static void save_as_copy(const MatchHistory *h) {
    if (sel < 0) { set_status("SELECT A MATCH FIRST"); return; }
    if (save_len == 0) { set_status("ENTER A NAME FIRST"); return; }
    char src[512];
    if (!auto_path_for_line(h, sel, src, sizeof(src))) { set_status("NO REPLAY FOR THIS MATCH"); return; }
    char *text = LoadFileText(src);
    if (!text) { set_status("COULD NOT READ THE RECORD"); return; }
    char *flag = strstr(text, "auto=1");
    if (flag) flag[5] = '0';                        /* a saved copy is never rotated away */
    char name[64], dst[512];
    snprintf(name, sizeof(name), "%s.txt", save_name);
    replay_file_path(name, dst, sizeof(dst));
    if (FileExists(dst)) { set_status("NAME ALREADY EXISTS"); UnloadFileText(text); return; }
    int ok = SaveFileText(dst, text);
    UnloadFileText(text);
    if (ok) {
        char msg[64];
        snprintf(msg, sizeof(msg), "SAVED TO replays/%s.txt", save_name);
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
        int first = file_page * REPLAY_PAGE_SIZE;
        int shown = file_count - first;
        if (shown > REPLAY_PAGE_SIZE) shown = REPLAY_PAGE_SIZE;
        for (int i = 0; i < shown; i++) {
            if (CheckCollisionPointRec(vm, (Rectangle){ 420, 200 + i * 26, 440, 22 })) { load_sel = first + i; return 1; }
        }
        if (CheckCollisionPointRec(vm, (Rectangle){ 420, 470, 90, 32 })) {
            if (file_page > 0) { file_page--; load_sel = -1; }
            return 1;
        }
        if (CheckCollisionPointRec(vm, (Rectangle){ 790, 470, 90, 32 })) {
            if ((file_page + 1) * REPLAY_PAGE_SIZE < file_count) { file_page++; load_sel = -1; }
            return 1;
        }
        if (CheckCollisionPointRec(vm, (Rectangle){ 420, 520, 160, 38 })) {
            if (load_sel >= 0 && load_sel < file_count) {
                replay_file_path(file_names[load_sel], start_path, sizeof(start_path));
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
    button((Rectangle){ ROW_X + BTN_W + 10.0f, BTN_Y, BTN_W, BTN_H }, "SAVE", 13.0f, vm, COLOR_PHOSPHOR);
    button((Rectangle){ ROW_X + 2 * (BTN_W + 10.0f), BTN_Y, BTN_W, BTN_H }, "LOAD", 13.0f, vm, COLOR_PHOSPHOR);

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
        int first = file_page * REPLAY_PAGE_SIZE;
        int shown = file_count - first;
        if (shown > REPLAY_PAGE_SIZE) shown = REPLAY_PAGE_SIZE;
        for (int i = 0; i < shown; i++) {
            int idx = first + i;
            Rectangle r = { 420, 200 + i * 26, 440, 22 };
            if (idx == load_sel) DrawRectangleRec(r, COLOR_BTN_HOVER_BG);
            draw_text_snapped(font_body, file_names[idx], 424.0f, 202.0f + i * 26, 16.0f, 1.0f,
                              (idx == load_sel) ? COLOR_PHOSPHOR : COLOR_TEXT_INFO);
        }
        button((Rectangle){ 420, 470, 90, 32 }, "PREV", 14.0f, vm, COLOR_PHOSPHOR);
        char page_label[48];
        snprintf(page_label, sizeof(page_label), "%d / %d", file_count ? file_page + 1 : 0,
                 file_count ? (file_count + REPLAY_PAGE_SIZE - 1) / REPLAY_PAGE_SIZE : 0);
        draw_text_snapped(font_body, page_label, 620.0f - MeasureTextEx(font_body, page_label, 16.0f, 1.0f).x / 2.0f,
                          478.0f, 16.0f, 1.0f, COLOR_TEXT_INFO_DIM);
        button((Rectangle){ 790, 470, 90, 32 }, "NEXT", 14.0f, vm, COLOR_PHOSPHOR);
        button((Rectangle){ 420, 520, 160, 38 }, "PLAY", 16.0f, vm, COLOR_PHOSPHOR);
        button((Rectangle){ 600, 520, 160, 38 }, "CANCEL", 16.0f, vm, COLOR_PHOSPHOR);
    }
}
