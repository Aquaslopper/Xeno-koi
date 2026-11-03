#include "ui_light_preset.h"
#include "raylib.h"
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* One place owns the numbers: k_default is the shipped look, every .txt file
   starts as a copy of it and overrides keys, presets/README.txt explains them.
   Nothing here allocates on the audio thread or opens a file after the UI has
   asked for a reload. */

#define LP_MAX 16

static const LightPreset k_default = {
    "DEFAULT", 12,
    1.12f, 0.008f, 0.006f, 0.09f, 0.034f, 2.50f, 1.65f,
    0.015f, 0.50f, 0.33f, -0.25f, 0.44f, 0.50f,
    0.21f, 0.59f, 1.0f, 0.021f, 0.64f, 1.08f, 1.3f,
    0.60f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f,
    2.55f, 1.1f
};

/* The browser build has no presets/ folder and only a couple of bundled
   tracks, so it keeps these two compiled-in alternatives. They are short
   override lists: the parser starts from k_default. Keep them in step with
   presets/techno.txt and presets/jazz.txt. */
static const char *const k_builtin_overrides[2] = {
    "name = TECHNO\n"
    "strength = 1.35\ncore_gain = 3.10\nexposure = 1.90\n"
    "size_a = 0.006\nsize_b = 0.004\nhalo_a = 0.070\nhalo_b = 0.025\n"
    "relief_min = 0.010\nrelief_full = 0.42\n"
    "speed_a = 0.75\nspeed_b = -0.60\n"
    "idle = 0.10\nmusic_gain = 0.95\nlevel_gain = 1.25\n"
    "attack = 0.006\nrelease = 0.14\nadapt_speed = 1.80\nbeat_contrast = 1.90\n"
    "theme_mix = 0.35\ncolor_g = 0.90\n"
    "peak_r = 0.60\npeak_g = 0.90\ncolor_shift = 0.35\n"
    "bloom_gain = 3.20\nbloom_radius = 1.00\n",

    "name = JAZZ\n"
    "lights = 8\nstrength = 0.95\ncore_gain = 1.90\nexposure = 1.25\n"
    "size_a = 0.011\nsize_b = 0.009\nhalo_a = 0.130\nhalo_b = 0.060\n"
    "relief_min = 0.020\nrelief_full = 0.60\n"
    "speed_a = 0.12\nspeed_b = -0.09\ntravel_x = 0.40\ntravel_y = 0.44\n"
    "idle = 0.28\nmusic_gain = 0.38\nlevel_gain = 0.85\n"
    "attack = 0.16\nrelease = 1.40\nadapt_speed = 0.60\nbeat_contrast = 0.85\n"
    "theme_mix = 0.45\ncolor_g = 0.82\ncolor_b = 0.62\n"
    "peak_r = 0.55\npeak_g = 0.75\ncolor_shift = 0.75\n"
    "bloom_gain = 1.70\nbloom_radius = 1.80\n"
};

static LightPreset s_presets[LP_MAX];
static int s_count, s_selected, s_ready;
static unsigned int s_adapt_raw = 1080000u, s_contrast_raw = 1300000u;

/* Whitelist: unknown keys are ignored, out-of-range values are clamped, so a
   hand-edited file can never produce NaN, a negative radius or a dead shader. */
typedef struct { const char *key; size_t off; float lo, hi; } KeyDef;
static const KeyDef k_keys[] = {
    { "strength",       offsetof(LightPreset, strength),       0.00f, 4.00f },
    { "size_a",         offsetof(LightPreset, size_a),         0.001f, 0.05f },
    { "size_b",         offsetof(LightPreset, size_b),         0.001f, 0.05f },
    { "halo_a",         offsetof(LightPreset, halo_a),         0.005f, 0.30f },
    { "halo_b",         offsetof(LightPreset, halo_b),         0.005f, 0.30f },
    { "core_gain",      offsetof(LightPreset, core_gain),      0.00f, 8.00f },
    { "exposure",       offsetof(LightPreset, exposure),       0.10f, 6.00f },
    { "relief_min",     offsetof(LightPreset, relief_min),     0.00f, 0.90f },
    { "relief_full",    offsetof(LightPreset, relief_full),    0.00f, 1.00f },
    { "speed_a",        offsetof(LightPreset, speed_a),       -3.00f, 3.00f },
    { "speed_b",        offsetof(LightPreset, speed_b),       -3.00f, 3.00f },
    { "travel_x",       offsetof(LightPreset, travel_x),       0.00f, 0.90f },
    { "travel_y",       offsetof(LightPreset, travel_y),       0.00f, 0.90f },
    { "idle",           offsetof(LightPreset, idle),           0.00f, 1.00f },
    { "music_gain",     offsetof(LightPreset, music_gain),     0.00f, 2.00f },
    { "level_gain",     offsetof(LightPreset, level_gain),     0.00f, 4.00f },
    { "attack",         offsetof(LightPreset, attack),         0.001f, 1.00f },
    { "release",        offsetof(LightPreset, release),        0.01f, 5.00f },
    { "adapt_speed",    offsetof(LightPreset, adapt_speed),    0.01f, 5.00f },
    { "beat_contrast",  offsetof(LightPreset, beat_contrast),  0.01f, 5.00f },
    { "theme_mix",      offsetof(LightPreset, theme_mix),      0.00f, 1.00f },
    { "color_r",        offsetof(LightPreset, color_r),        0.00f, 1.00f },
    { "color_g",        offsetof(LightPreset, color_g),        0.00f, 1.00f },
    { "color_b",        offsetof(LightPreset, color_b),        0.00f, 1.00f },
    { "peak_r",         offsetof(LightPreset, peak_r),         0.00f, 1.00f },
    { "peak_g",         offsetof(LightPreset, peak_g),         0.00f, 1.00f },
    { "peak_b",         offsetof(LightPreset, peak_b),         0.00f, 1.00f },
    { "color_shift",    offsetof(LightPreset, color_shift),    0.00f, 1.00f },
    { "bloom_gain",     offsetof(LightPreset, bloom_gain),     0.00f, 8.00f },
    { "bloom_radius",   offsetof(LightPreset, bloom_radius),   0.00f, 6.00f }
};

static float clampf(float v, float lo, float hi) {
    if (!(v >= lo)) return lo; /* also catches NaN */
    return v > hi ? hi : v;
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\r') s++;
    char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) *--end = 0;
    return s;
}

static void parse_line(LightPreset *p, char *line, int *named) {
    char *hash = strchr(line, '#');
    if (hash) *hash = 0;
    char *eq = strchr(line, '=');
    if (!eq) return;
    *eq = 0;
    char *key = trim(line), *val = trim(eq + 1);
    if (!*key || !*val) return;
    if (!strcmp(key, "name")) {
        snprintf(p->name, sizeof(p->name), "%s", val);
        *named = 1;
        return;
    }
    if (!strcmp(key, "lights")) {
        int n = atoi(val);
        p->lights = n < 1 ? 1 : (n > 12 ? 12 : n);
        return;
    }
    float v = strtof(val, NULL);
    for (size_t i = 0; i < sizeof(k_keys) / sizeof(k_keys[0]); i++) {
        if (!strcmp(key, k_keys[i].key)) {
            float c = clampf(v, k_keys[i].lo, k_keys[i].hi);
            memcpy((unsigned char *)p + k_keys[i].off, &c, sizeof(c));
            return;
        }
    }
}

static int parse_text(const char *text, LightPreset *out) {
    *out = k_default; /* every file starts as the shipped look */
    char buf[8192];
    size_t n = strlen(text);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, text, n);
    buf[n] = 0;
    int named = 0;
    char *line = buf;
    while (line && *line) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = 0;
        parse_line(out, line, &named);
        line = nl ? nl + 1 : NULL;
    }
    return named;
}

#ifndef __EMSCRIPTEN__
static int compare_paths(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* Same "folder next to the exe, else ./folder" rule as replays/ in ui_replay.c. */
static void preset_root(char *buf, size_t cap) {
    const char *base = GetApplicationDirectory();
    if (base && base[0]) {
        size_t n = strlen(base);
        const char *sep = (base[n - 1] == '/' || base[n - 1] == '\\') ? "" : "/";
        snprintf(buf, cap, "%s%spresets", base, sep);
        if (DirectoryExists(buf)) return;
    }
    snprintf(buf, cap, "./presets");
}

static int scan_folder(LightPreset *out, int cap) {
    char root[512];
    preset_root(root, sizeof(root));
    if (!DirectoryExists(root)) return 0;
    FilePathList list = LoadDirectoryFilesEx(root, ".txt", false);
    if (list.count > 1) qsort(list.paths, list.count, sizeof(*list.paths), compare_paths);
    int n = 0;
    for (unsigned int i = 0; i < list.count && n < cap; i++) {
        char *text = LoadFileText(list.paths[i]);
        if (!text) continue;
        LightPreset p;
        int named = parse_text(text, &p);
        UnloadFileText(text);
        if (named) out[n++] = p; /* README.txt has no name= line and is skipped */
    }
    UnloadDirectoryFiles(list);
    return n;
}
#endif

static void publish_meter(void) {
    const LightPreset *p = &s_presets[s_selected];
    __atomic_store_n(&s_adapt_raw,
        (unsigned int)(clampf(p->adapt_speed, 0.01f, 5.0f) * 1000000.0f), __ATOMIC_RELAXED);
    __atomic_store_n(&s_contrast_raw,
        (unsigned int)(clampf(p->beat_contrast, 0.01f, 5.0f) * 1000000.0f), __ATOMIC_RELAXED);
}

void light_preset_init(void) {
    if (s_ready) return;
    s_ready = 1;
    LightPreset tmp[LP_MAX];
    int n = 0;
    tmp[n++] = k_default;
    for (int i = 0; i < 2 && n < LP_MAX; i++) parse_text(k_builtin_overrides[i], &tmp[n++]);
#ifndef __EMSCRIPTEN__
    int found = scan_folder(tmp, LP_MAX);
    if (found > 0) n = found; /* the player's folder is the preset list */
#endif
    memcpy(s_presets, tmp, (size_t)n * sizeof(tmp[0]));
    s_count = n;
    /* Always launch on the untouched factory look, even if a player's own file
       sorts before default.txt. A reload restores the previous selection below. */
    s_selected = 0;
    for (int i = 0; i < n; i++)
        if (!strcmp(s_presets[i].name, "DEFAULT")) { s_selected = i; break; }
    publish_meter();
}

void light_preset_reload(void) {
    if (!s_ready) { light_preset_init(); return; }
    /* Keep the chosen preset by name, not by index: adding or deleting a file
       shifts the sorted list but should not silently change the light style. */
    char keep[32];
    snprintf(keep, sizeof(keep), "%s", light_preset_name(s_selected));
    s_ready = 0;
    light_preset_init();
    int found = -1, factory = 0;
    for (int i = 0; i < s_count; i++) {
        if (!strcmp(s_presets[i].name, keep)) found = i;
        if (!strcmp(s_presets[i].name, "DEFAULT")) factory = i;
    }
    s_selected = found >= 0 ? found : factory;
    publish_meter();
}

int light_preset_count(void) {
    if (!s_ready) light_preset_init();
    return s_count;
}

const char *light_preset_name(int index) {
    if (!s_ready) light_preset_init();
    return (index >= 0 && index < s_count) ? s_presets[index].name : "";
}

int light_preset_selected(void) {
    if (!s_ready) light_preset_init();
    return s_selected;
}

void light_preset_select(int index) {
    if (!s_ready) light_preset_init();
    if (index < 0 || index >= s_count) return;
    s_selected = index;
    publish_meter();
}

const LightPreset *light_preset_current(void) {
    if (!s_ready) light_preset_init();
    return &s_presets[s_selected];
}

float light_preset_adapt_speed(void) {
    return (float)__atomic_load_n(&s_adapt_raw, __ATOMIC_RELAXED) / 1000000.0f;
}

float light_preset_beat_contrast(void) {
    return (float)__atomic_load_n(&s_contrast_raw, __ATOMIC_RELAXED) / 1000000.0f;
}
