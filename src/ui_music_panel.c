#include "ui_music_panel.h"
#include "ui_light_preset.h"
#include "ui_theme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* The match-music setup panel. No DSP lives here: it only drives the existing
   playlist (audio_set_music_folder / audio_start_match_music / audio_stop_music)
   and reuses the Options effect test (audio_preview_effect) through the frame
   loop. It behaves like a player: one pinned track starts a match, RANDOM plays
   a random track, LOOP 1 repeats one track, LOOP repeats the playlist, and with
   none of them the tracks play in order. State is global because music is global
   to both hubs; it is not persisted, and only the effect test is menu-only. */

extern Font font_title;
extern Font font_body;

enum { MP_REPEAT_OFF = 0, MP_REPEAT_ALL, MP_REPEAT_ONE };

#define MP_MAX_NAMES   AUDIO_MAX_TRACKS
#define MP_NAME_LEN    64
#define MP_MAX_FOLDERS 8
#define MP_FOLDER_LEN  MUSIC_FOLDER_CAP

static GameSettings *s_settings;
static AudioAssets  *s_audio;
static int s_open;
static int s_effect_test = -1;       /* -1 idle, 0 dry A/B, 1 reverb on */
static int s_stopped;                /* STOP / playlist end: no auto playback */
static int s_random;                 /* RANDOM chip */
static int s_repeat = MP_REPEAT_OFF; /* LOOP / LOOP 1 chips */
static int s_pinned = 1;             /* 1-based track that starts the next match */
static int s_folder_view;
static const char *s_message;
static char s_cached_folder[MUSIC_FOLDER_CAP];
static char s_names[MP_MAX_NAMES][MP_NAME_LEN];
static int  s_name_count;
static char s_folders[MP_MAX_FOLDERS][MP_FOLDER_LEN];
static char s_folder_labels[MP_MAX_FOLDERS][64];
static int  s_folder_count;

void music_panel_bind(GameSettings *settings, AudioAssets *audio) {
    s_settings = settings;
    s_audio = audio;
}

int  music_panel_is_open(void) { return s_open; }
int  music_panel_effect_test(void) { return s_open ? s_effect_test : -1; }
int  music_panel_music_stopped(void) { return s_stopped; }

void music_panel_close(void) {
    s_open = 0;
    s_folder_view = 0;
    s_effect_test = -1;
}

void music_panel_sync(int hub_visible) {
    if (!hub_visible && s_open) music_panel_close();
}

/* A folder switch can shrink the playlist, so clamp the pin before use. */
static int pinned_track(const AudioAssets *a) {
    int n = a ? a->game_track_count : 0;
    if (n <= 0) return 1;
    if (s_pinned < 1) s_pinned = 1;
    if (s_pinned > n) s_pinned = n;
    return s_pinned;
}

int music_panel_match_track(const AudioAssets *a) {
    s_stopped = 0; /* a new match always starts music, even after STOP */
    return s_random ? 0 : pinned_track(a); /* 0: RANDOM, let ui_main.c pick */
}

int music_panel_next_track(const AudioAssets *a, int random_track) {
    int n = a ? a->game_track_count : 0;
    if (n <= 0) return 1;
    int cur = (a->current_game_track >= 0 && a->current_game_track < n) ? a->current_game_track : 0;
    /* 1-based track numbers, wrapped by audio_start_match_music(). */
    if (s_repeat == MP_REPEAT_ONE) return cur + 1;
    if (s_random) return random_track;
    if (cur + 1 < n) return cur + 2;
    if (s_repeat == MP_REPEAT_ALL) return 1;
    s_stopped = 1; /* repeat off and the last track ended: stay stopped */
    return 0;
}

/* ---------------------------------------------------------------- file names */

static const char *base_name(const char *path) {
    const char *p = strrchr(path, '/');
    const char *q = strrchr(path, '\\');
    if (q && (!p || q > p)) p = q;
    return p ? p + 1 : path;
}

static int builtin_exists(const char *file) {
    char buf[512];
    const char *base = GetApplicationDirectory();
    if (base && base[0]) {
        snprintf(buf, sizeof(buf), "%sassets/music/%s", base, file);
        if (FileExists(buf)) return 1;
    }
    snprintf(buf, sizeof(buf), "./assets/music/%s", file);
    return FileExists(buf);
}

#ifndef __EMSCRIPTEN__
static int compare_paths(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
#endif

static void add_name(const char *name) {
    if (s_name_count >= MP_MAX_NAMES) return;
    snprintf(s_names[s_name_count++], MP_NAME_LEN, "%s", name);
}

/* Mirrors audio_set_music_folder()'s order and extension filter so the label
   lines up with the loaded stream. A file that fails to decode shifts later
   labels by one; the number beside the name is always the real track index. */
static void refresh_names(void) {
    s_name_count = 0;
    if (!s_settings) return;
    if (!s_settings->music_folder[0]) {
        if (builtin_exists("menu.mp3")) add_name("menu.mp3");
        for (int i = 1; i <= 10; i++) {
            char file[32];
            snprintf(file, sizeof(file), "game%d.mp3", i);
            if (builtin_exists(file)) add_name(file);
        }
        return;
    }
#ifndef __EMSCRIPTEN__
    FilePathList files = LoadDirectoryFilesEx(s_settings->music_folder, ".mp3;.ogg;.wav", false);
    if (files.count > 1) qsort(files.paths, files.count, sizeof(*files.paths), compare_paths);
    for (unsigned int i = 0; i < files.count && s_name_count < MP_MAX_NAMES; i++)
        add_name(base_name(files.paths[i]));
    UnloadDirectoryFiles(files);
#else
    add_name("BUNDLED TRACKS");
#endif
}

static void add_folder(const char *path, const char *label) {
    if (s_folder_count >= MP_MAX_FOLDERS || !path || strlen(path) >= MP_FOLDER_LEN) return;
    snprintf(s_folders[s_folder_count], MP_FOLDER_LEN, "%s", path);
    snprintf(s_folder_labels[s_folder_count], sizeof(s_folder_labels[0]), "%s", label);
    s_folder_count++;
}

#ifndef __EMSCRIPTEN__
static void music_root(char *buf, size_t cap) {
    const char *base = GetApplicationDirectory();
    if (base && base[0]) {
        snprintf(buf, cap, "%sassets/music", base);
        if (DirectoryExists(buf)) return;
    }
    snprintf(buf, cap, "./assets/music");
}

/* One level of subfolders under each folder: a subfolder can be picked as the
   playlist folder, which is how subfolders are supported without touching the
   hash-guarded audio DSP file. */
static void scan_subfolders(const char *dir, const char *prefix, int depth) {
    FilePathList list = LoadDirectoryFiles(dir);
    for (unsigned int i = 0; i < list.count && s_folder_count < MP_MAX_FOLDERS; i++) {
        const char *p = list.paths[i];
        const char *name = base_name(p);
        if (!name[0] || !strcmp(name, ".") || !strcmp(name, "..")) continue;
        if (!DirectoryExists(p)) continue;
        char label[64], next[128];
        snprintf(label, sizeof(label), "%s%s", prefix, name);
        add_folder(p, label);
        if (depth < 2) {
            snprintf(next, sizeof(next), "%s%s/", prefix, name);
            scan_subfolders(p, next, depth + 1);
        }
    }
    UnloadDirectoryFiles(list);
}

#endif /* __EMSCRIPTEN__ */

static void refresh_folders(void) {
    s_folder_count = 0;
    add_folder("", "BUILT-IN MUSIC");
#ifndef __EMSCRIPTEN__
    char root[512];
    music_root(root, sizeof(root));
    if (DirectoryExists(root)) scan_subfolders(root, "", 1);
    if (s_settings && s_settings->music_folder[0]) {
        int found = 0;
        for (int i = 0; i < s_folder_count; i++)
            if (!strcmp(s_folders[i], s_settings->music_folder)) found = 1;
        if (!found) {
            char label[64];
            snprintf(label, sizeof(label), "CURRENT: %s", base_name(s_settings->music_folder));
            add_folder(s_settings->music_folder, label);
        }
    }
#endif
}

void music_panel_open(void) {
    if (s_open) return;
    s_open = 1;
    s_folder_view = 0;
    s_message = NULL;
    if (s_settings) snprintf(s_cached_folder, sizeof(s_cached_folder), "%s", s_settings->music_folder);
    light_preset_init();
    refresh_names();
    refresh_folders();
}

void music_panel_toggle(void) {
    if (s_open) music_panel_close();
    else music_panel_open();
}

/* ------------------------------------------------------------------- actions */

static void apply_folder(const char *path) {
    if (!s_audio || !s_settings) return;
    if (audio_set_music_folder(s_audio, path) < 0) {
        s_message = "FOLDER UNAVAILABLE OR NO PLAYABLE AUDIO. MUSIC UNCHANGED.";
        return;
    }
    snprintf(s_settings->music_folder, sizeof(s_settings->music_folder), "%s", path);
    storage_save_settings(s_settings);
    snprintf(s_cached_folder, sizeof(s_cached_folder), "%s", path);
    s_stopped = 0;
    s_pinned = 1;
    s_effect_test = -1;
    s_folder_view = 0;
    refresh_names();
    refresh_folders();
    /* audio_set_music_folder() restarts on a random track; keep the pin audible. */
    audio_start_match_music(s_audio, s_pinned);
    s_message = "PLAYLIST REPLACED.";
}

static void step_track(int dir) {
    if (!s_audio || s_audio->game_track_count <= 0) return;
    int n = s_audio->game_track_count;
    s_pinned = ((pinned_track(s_audio) - 1 + dir) % n + n) % n + 1;
    audio_start_match_music(s_audio, s_pinned); /* preview the match-start track */
    s_stopped = 0;
}

static void step_preset(int dir) {
    int n = light_preset_count();
    if (n <= 0) return;
    int i = ((light_preset_selected() + dir) % n + n) % n;
    light_preset_select(i);
    s_message = "LIGHT PRESET SET FOR YOUR MATCHES.";
}

static const char *preset_label(void) {
    static char buf[128];
    int n = light_preset_count(), i = light_preset_selected();
    snprintf(buf, sizeof(buf), "%s   %d / %d", light_preset_name(i), i + 1, n);
    return buf;
}

static const char *track_label(const AudioAssets *a) {
    static char buf[128];
    int n = a ? a->game_track_count : 0;
    if (n <= 0) return "NO TRACKS";
    int idx = pinned_track(a) - 1;
    if (idx < s_name_count)
        snprintf(buf, sizeof(buf), "%02d / %02d   %s", idx + 1, n, s_names[idx]);
    else
        snprintf(buf, sizeof(buf), "TRACK %02d / %02d", idx + 1, n);
    return buf;
}

/* --------------------------------------------------------------------- layout */

typedef struct {
    Rectangle test, minus, name, plus, play, stop;
    Rectangle mode[3];
    Rectangle preset_minus, preset_name, preset_plus;
    Rectangle folder[MP_MAX_FOLDERS];
    Rectangle back;
} MusicLayout;

/* The second row underneath the player is the light preset stepper. Rows were
   tightened by 6-14 px each so both hosts fit: Pancratium's info frame is
   300 px tall, the Plex stage frame is 266 px. */
static MusicLayout music_layout(Rectangle r) {
    MusicLayout l;
    memset(&l, 0, sizeof(l));
    float x = r.x + 20.0f, w = r.width - 40.0f, y = r.y;
    l.test  = (Rectangle){ x,          y + 46.0f,  w,          34.0f };
    l.minus = (Rectangle){ x + 86.0f,  y + 86.0f,  34.0f,      30.0f };
    l.name  = (Rectangle){ x + 126.0f, y + 86.0f,  w - 168.0f, 30.0f };
    l.plus  = (Rectangle){ x + w - 34.0f, y + 86.0f, 34.0f,    30.0f };
    l.play  = (Rectangle){ x,          y + 122.0f, 110.0f,     30.0f };
    l.stop  = (Rectangle){ x + 118.0f, y + 122.0f, 110.0f,     30.0f };
    for (int i = 0; i < 3; i++)
        l.mode[i] = (Rectangle){ x + (float)i * 118.0f, y + 158.0f, 110.0f, 30.0f };
    l.preset_minus = (Rectangle){ x + 86.0f,  y + 198.0f, 34.0f,      30.0f };
    l.preset_name  = (Rectangle){ x + 126.0f, y + 198.0f, w - 168.0f, 30.0f };
    l.preset_plus  = (Rectangle){ x + w - 34.0f, y + 198.0f, 34.0f,    30.0f };
    for (int i = 0; i < MP_MAX_FOLDERS; i++)
        l.folder[i] = (Rectangle){ x + (float)(i % 2) * (w * 0.5f + 4.0f),
                                   y + 52.0f + (float)(i / 2) * 38.0f,
                                   w * 0.5f - 4.0f, 32.0f };
    l.back = (Rectangle){ x, y + 212.0f, 110.0f, 30.0f };
    return l;
}

static void draw_chip(Rectangle r, const char *txt, Font font, float fsz, Vector2 vm, int active) {
    int hov = CheckCollisionPointRec(vm, r);
    DrawRectangleRec(r, (hov || active) ? COLOR_BTN_HOVER_BG : COLOR_PANEL_BG);
    ui_draw_panel_border(r, (hov || active) ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    Vector2 ts = MeasureTextEx(font, txt, fsz, 1.0f);
    if (ts.x > r.width - 10.0f && ts.x > 0.0f) {
        fsz *= (r.width - 10.0f) / ts.x;
        ts = MeasureTextEx(font, txt, fsz, 1.0f);
    }
    DrawTextEx(font, txt,
               (Vector2){ floorf(r.x + (r.width - ts.x) * 0.5f),
                          floorf(r.y + (r.height - fsz) * 0.5f) },
               fsz, 1.0f, active ? COLOR_PHOSPHOR : (hov ? COLOR_TEXT_INFO : COLOR_TEXT_INFO_DIM));
}

static void draw_value(Rectangle r, const char *txt, Vector2 vm) {
    int hov = CheckCollisionPointRec(vm, r);
    DrawRectangleRec(r, hov ? COLOR_BTN_HOVER_BG : COLOR_PANEL_BG);
    ui_draw_panel_border(r, hov ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    char buf[128];
    snprintf(buf, sizeof(buf), "%s", txt);
    float fsz = 14.0f;
    Vector2 ts = MeasureTextEx(font_body, buf, fsz, 1.0f);
    while (ts.x > r.width - 10.0f && strlen(buf) > 1) {
        size_t n = strlen(buf);
        memmove(buf, buf + 1, n);
        ts = MeasureTextEx(font_body, buf, fsz, 1.0f);
    }
    DrawTextEx(font_body, buf,
               (Vector2){ floorf(r.x + 4.0f), floorf(r.y + (r.height - fsz) * 0.5f) },
               fsz, 1.0f, COLOR_TEXT_INFO);
}

static void draw_label(const char *txt, float x, float y, float fsz, Color c) {
    DrawTextEx(font_body, txt, (Vector2){ floorf(x), floorf(y) }, fsz, 1.0f, c);
}

static void draw_heading(const char *txt, float x, float y, float fsz, Color c) {
    DrawTextEx(font_title, txt, (Vector2){ floorf(x), floorf(y) }, fsz, 1.0f, c);
}

void music_panel_draw(Rectangle r, Vector2 vm) {
    if (!s_open) return;
    /* Same frame and border as the Pancratium STAGE // MAP SELECT panel. */
    DrawRectangleRec(r, COLOR_PANEL_BG);
    ui_draw_panel_border(r, COLOR_PHOSPHOR_DIM);
    if (s_settings && strcmp(s_cached_folder, s_settings->music_folder)) {
        snprintf(s_cached_folder, sizeof(s_cached_folder), "%s", s_settings->music_folder);
        refresh_names();
        refresh_folders();
    }
    MusicLayout l = music_layout(r);
    float x = r.x + 20.0f, y = r.y;
    draw_heading(s_folder_view ? "MUSIC // PICK FOLDER" : "MUSIC // MATCH START TRACK",
                 x, y + 12.0f, 17.0f, COLOR_PHOSPHOR);
    DrawLine((int)x, (int)(y + 42.0f), (int)(r.x + r.width - 20.0f), (int)(y + 42.0f), COLOR_PHOSPHOR_DIM);

    if (s_folder_view) {
        for (int i = 0; i < s_folder_count; i++) {
            int active = s_settings && !strcmp(s_folders[i], s_settings->music_folder);
            draw_chip(l.folder[i], s_folder_labels[i], font_body, 14.0f, vm, active);
        }
        if (s_folder_count <= 1)
            draw_label("NO SUBFOLDERS FOUND. ADD FOLDERS UNDER assets/music.", x, y + 92.0f, 13.0f, COLOR_TEXT_INFO_DIM);
        draw_chip(l.back, "< BACK", font_title, 14.0f, vm, 0);
        draw_label("PICK A FOLDER TO REPLACE THE MATCH PLAYLIST.", x + 126.0f, y + 219.0f, 13.0f, COLOR_TEXT_INFO_DIM);
        if (s_message) draw_label(s_message, x, y + 248.0f, 12.0f, COLOR_TEXT_INFO);
        return;
    }

    char mode_label[3][16];
    snprintf(mode_label[0], sizeof(mode_label[0]), "RANDOM %s", s_random ? "ON" : "OFF");
    snprintf(mode_label[1], sizeof(mode_label[1]), "LOOP %s", s_repeat == MP_REPEAT_ALL ? "ON" : "OFF");
    snprintf(mode_label[2], sizeof(mode_label[2]), "LOOP 1 %s", s_repeat == MP_REPEAT_ONE ? "ON" : "OFF");
    int mode_on[3] = { s_random, s_repeat == MP_REPEAT_ALL, s_repeat == MP_REPEAT_ONE };
    draw_chip(l.test, "TEST WITH REVERB EFFECTS ON", font_title, 15.0f, vm, s_effect_test == 1);
    draw_label("START", x, y + 92.0f, 16.0f, COLOR_PHOSPHOR);
    draw_chip(l.minus, "-", font_title, 18.0f, vm, 0);
    draw_value(l.name, track_label(s_audio), vm);
    draw_chip(l.plus, "+", font_title, 18.0f, vm, 0);
    draw_chip(l.play, "PLAY", font_title, 15.0f, vm, !s_stopped && s_audio && s_audio->current_music == 2);
    draw_chip(l.stop, "STOP", font_title, 15.0f, vm, s_stopped);
    draw_label(s_stopped ? "STOPPED" : (s_audio && s_audio->current_music == 2 ? "PLAYING" : "READY"),
               x + 240.0f, y + 129.0f, 13.0f, COLOR_TEXT_INFO);
    for (int i = 0; i < 3; i++)
        draw_chip(l.mode[i], mode_label[i], font_title, 14.0f, vm, mode_on[i]);
    const char *next = s_repeat == MP_REPEAT_ONE ? "REPEAT ONE" :
                       s_random ? "RANDOM TRACK" :
                       s_repeat == MP_REPEAT_ALL ? "LOOP PLAYLIST" : "NEXT IN ORDER";
    draw_label(next, x + 366.0f, y + 166.0f, 12.0f, COLOR_TEXT_INFO_DIM);

    /* Second row underneath the player: pick how the 12 table dots react. */
    DrawLine((int)x, (int)(y + 192.0f), (int)(r.x + r.width - 20.0f), (int)(y + 192.0f), COLOR_PHOSPHOR_DIM);
    draw_label("PRESETS", x, y + 204.0f, 16.0f, COLOR_PHOSPHOR);
    draw_chip(l.preset_minus, "<", font_title, 18.0f, vm, 0);
    draw_value(l.preset_name, preset_label(), vm);
    draw_chip(l.preset_plus, ">", font_title, 18.0f, vm, 0);

    char folder_line[160];
    const char *folder = (s_settings && s_settings->music_folder[0]) ?
                         base_name(s_settings->music_folder) : "BUILT-IN";
    snprintf(folder_line, sizeof(folder_line), "FOLDER: %s  //  %d TRACKS",
             folder, s_audio ? s_audio->game_track_count : 0);
    draw_label(folder_line, x, y + 234.0f, 13.0f, COLOR_TEXT_INFO);
    if (s_message) draw_label(s_message, x, y + 250.0f, 12.0f, COLOR_TEXT_INFO);
    else draw_label("START TRACK PLAYS IN MATCHES. CLICK NAMES FOR FOLDERS / RELOAD.", x, y + 250.0f, 12.0f, COLOR_TEXT_INFO_DIM);
}

int music_panel_handle_click(Rectangle r, Vector2 vm) {
    if (!s_open || !CheckCollisionPointRec(vm, r)) return 0;
    MusicLayout l = music_layout(r);
    if (s_folder_view) {
        for (int i = 0; i < s_folder_count; i++) {
            if (CheckCollisionPointRec(vm, l.folder[i])) {
                apply_folder(s_folders[i]);
                return 1;
            }
        }
        if (CheckCollisionPointRec(vm, l.back)) s_folder_view = 0;
        return 1;
    }
    if (CheckCollisionPointRec(vm, l.test)) {
        /* The Options A/B test, three states: wet, dry, back to normal playback. */
        s_effect_test = (s_effect_test == 1) ? 0 : (s_effect_test == 0) ? -1 : 1;
        s_stopped = 0;
        s_message = (s_effect_test == 1) ? "REVERB EFFECT: ON  (FULL STRENGTH)." :
                    (s_effect_test == 0) ? "REVERB EFFECT: DRY A/B COMPARISON." : "EFFECT TEST OFF.";
    } else if (CheckCollisionPointRec(vm, l.minus)) {
        step_track(-1);
    } else if (CheckCollisionPointRec(vm, l.plus)) {
        step_track(1);
    } else if (CheckCollisionPointRec(vm, l.name)) {
        refresh_folders();
        s_folder_view = 1;
    } else if (CheckCollisionPointRec(vm, l.preset_minus)) {
        step_preset(-1);
    } else if (CheckCollisionPointRec(vm, l.preset_plus)) {
        step_preset(1);
    } else if (CheckCollisionPointRec(vm, l.preset_name)) {
        light_preset_reload();
        s_message = "PRESETS RELOADED FROM THE presets/ FOLDER.";
    } else if (CheckCollisionPointRec(vm, l.play)) {
        if (s_audio && s_audio->game_track_count > 0)
            audio_start_match_music(s_audio, pinned_track(s_audio));
        s_stopped = 0;
        s_effect_test = -1;
        s_message = "PLAYING SELECTED TRACK.";
    } else if (CheckCollisionPointRec(vm, l.stop)) {
        if (s_audio) audio_stop_music(s_audio);
        s_stopped = 1;
        s_effect_test = -1;
        s_message = "PLAYBACK STOPPED.";
    } else {
        for (int i = 0; i < 3; i++) {
            if (CheckCollisionPointRec(vm, l.mode[i])) {
                if (i == 0) s_random = !s_random;
                else {
                    int mode = (i == 1) ? MP_REPEAT_ALL : MP_REPEAT_ONE;
                    s_repeat = (s_repeat == mode) ? MP_REPEAT_OFF : mode;
                }
                s_message = "TRACK ADVANCE MODE SET.";
                break;
            }
        }
    }
    return 1;
}
