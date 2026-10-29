#include "storage.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif

static const StorageDeusSessionSnapshot *s_deus_session_snapshot;

#define PROFILE_SLOT_COUNT 3
#define PROFILE_FILE_COUNT 4
#define PROFILE_PATH_CAP 640

static const char *const s_profile_files[PROFILE_FILE_COUNT] = {
    "settings.txt", "stats.txt", "history.txt", "plexsave.txt"
};
static StorageProfileMode s_profile_mode = STORAGE_PROFILES_LEGACY;
static int s_profile_active_slot = 0;
static unsigned int s_profile_slot_mask = 0;
static int s_profiles_initialized = 0;

void storage_set_deus_session_snapshot(const StorageDeusSessionSnapshot *snapshot) {
    s_deus_session_snapshot = snapshot;
}

static int profile_is_slot_file(const char *filename) {
    if (!filename) return 0;
    for (int i = 0; i < PROFILE_FILE_COUNT; i++)
        if (strcmp(filename, s_profile_files[i]) == 0) return 1;
    return 0;
}

static void get_app_path_raw(const char *filename, char *out_path, size_t max_len) {
    const char *base = GetApplicationDirectory();
    if (base && base[0] != '\0') {
        snprintf(out_path, max_len, "%s%s", base, filename);
        if (FileExists(out_path)) return;
    }
    snprintf(out_path, max_len, "./%s", filename);
}

static void get_app_path(const char *filename, char *out_path, size_t max_len) {
    if (s_profiles_initialized && profile_is_slot_file(filename)) {
        char rel[160];
        snprintf(rel, sizeof(rel), "profiles/%s", filename);
        get_app_path_raw(rel, out_path, max_len);
        return;
    }
    get_app_path_raw(filename, out_path, max_len);
}

typedef struct {
    char path[PROFILE_PATH_CAP];
    char stage[PROFILE_PATH_CAP];
    char backup[PROFILE_PATH_CAP];
    int present;
    int had_original;
    int removed_original;
    int installed;
} ProfileTxnFile;

static int profile_path_for_slot(int slot, const char *filename, char *out, size_t cap) {
    if (slot < 1 || slot > PROFILE_SLOT_COUNT || !filename || !out || cap == 0) return 0;
    char rel[160];
    int n = snprintf(rel, sizeof(rel), "profiles/slot%02d/%s", slot, filename);
    if (n < 0 || (size_t)n >= sizeof(rel)) return 0;
    get_app_path(rel, out, cap);
    return 1;
}

static int profile_file_in_slot(int slot, const char *filename, char *out, size_t cap);

static int profile_sidecar_path(const char *path, const char *suffix, char *out, size_t cap) {
    size_t a = strlen(path), b = strlen(suffix);
    if (a + b + 1 > cap) return 0;
    memcpy(out, path, a);
    memcpy(out + a, suffix, b + 1);
    return 1;
}

static int profile_file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static void profile_remove_legacy_root_files(void) {
    for (int i = 0; i < PROFILE_FILE_COUNT; i++) {
        char path[512];
        get_app_path_raw(s_profile_files[i], path, sizeof(path));
        remove(path);
    }
}

static int profile_make_directory(const char *path) {
#ifdef _WIN32
    if (_mkdir(path) != 0 && errno != EEXIST) return 0;
#else
    if (mkdir(path, 0777) != 0 && errno != EEXIST) return 0;
#endif
    struct stat st;
    if (stat(path, &st) != 0) return 0;
#ifdef _WIN32
    return (st.st_mode & _S_IFMT) == _S_IFDIR;
#else
    return S_ISDIR(st.st_mode);
#endif
}

static int profile_ensure_directory(int slot) {
    if (!profile_make_directory("profiles")) return 0;
    if (slot >= 1 && slot <= PROFILE_SLOT_COUNT) {
        char rel[32];
        snprintf(rel, sizeof(rel), "profiles/slot%02d", slot);
        if (!profile_make_directory(rel)) return 0;
    }
    return 1;
}

static int profile_stage_copy(const char *src, ProfileTxnFile *dst) {
    if (!profile_sidecar_path(dst->path, ".profile-tmp", dst->stage, sizeof(dst->stage)) ||
        !profile_sidecar_path(dst->path, ".profile-bak", dst->backup, sizeof(dst->backup))) return 0;
    dst->present = profile_file_exists(src);
    dst->had_original = 0;
    dst->installed = 0;
    remove(dst->stage);
    if (!dst->present) return 1;

    FILE *in = fopen(src, "rb");
    if (!in) return 0;
    FILE *out = fopen(dst->stage, "wb");
    if (!out) { fclose(in); return 0; }
    char buf[4096];
    size_t n;
    int ok = 1;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) { ok = 0; break; }
    }
    if (ferror(in)) ok = 0;
    if (fclose(in) != 0) ok = 0;
    if (fclose(out) != 0) ok = 0;
    if (!ok) remove(dst->stage);
    return ok;
}

static int profile_stage_text(ProfileTxnFile *dst, const char *content) {
    if (!profile_sidecar_path(dst->path, ".profile-tmp", dst->stage, sizeof(dst->stage)) ||
        !profile_sidecar_path(dst->path, ".profile-bak", dst->backup, sizeof(dst->backup))) return 0;
    dst->present = 1;
    dst->had_original = 0;
    dst->installed = 0;
    remove(dst->stage);
    FILE *f = fopen(dst->stage, "wb");
    if (!f) return 0;
    size_t len = strlen(content);
    int ok = fwrite(content, 1, len, f) == len;
    if (fclose(f) != 0) ok = 0;
    if (!ok) remove(dst->stage);
    return ok;
}

static int profile_copy_bytes(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return 0;
    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return 0; }
    char buf[4096];
    size_t n;
    int ok = 1;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) { ok = 0; break; }
    }
    if (ferror(in)) ok = 0;
    if (fclose(in) != 0) ok = 0;
    if (fclose(out) != 0) ok = 0;
    if (!ok) remove(dst);
    return ok;
}

static int profile_commit_txn(ProfileTxnFile *files, int count) {
    int i;
    for (i = 0; i < count; i++) {
        remove(files[i].backup);
        files[i].had_original = profile_file_exists(files[i].path);
        files[i].removed_original = 0;
        if (files[i].had_original && !profile_copy_bytes(files[i].path, files[i].backup)) goto rollback;
        if (!files[i].present && files[i].had_original) {
            if (remove(files[i].path) != 0) goto rollback;
            files[i].removed_original = 1;
        }
    }
    for (i = 0; i < count; i++) {
        if (!files[i].present) continue;
        if (rename(files[i].stage, files[i].path) != 0) {
#ifdef _WIN32
            if (files[i].had_original && !files[i].removed_original && remove(files[i].path) == 0) {
                files[i].removed_original = 1;
                if (rename(files[i].stage, files[i].path) == 0) {
                    files[i].installed = 1;
                    continue;
                }
            }
#endif
            goto rollback;
        }
        files[i].installed = 1;
    }
    for (i = 0; i < count; i++) {
        remove(files[i].backup);
        remove(files[i].stage);
    }
    return 1;

rollback:
    for (int j = count - 1; j >= 0; j--) {
        if (files[j].installed) remove(files[j].path);
        if (files[j].had_original && (files[j].removed_original || files[j].installed) &&
            profile_file_exists(files[j].backup))
            rename(files[j].backup, files[j].path);
        else
            remove(files[j].backup);
        remove(files[j].stage);
    }
    return 0;
}

static int profile_copy_set(const char *const *src, const char *const *dst, int count) {
    ProfileTxnFile files[PROFILE_FILE_COUNT];
    if (count < 1 || count > PROFILE_FILE_COUNT) return 0;
    memset(files, 0, sizeof(files));
    for (int i = 0; i < count; i++) {
        snprintf(files[i].path, sizeof(files[i].path), "%s", dst[i]);
        if (!profile_stage_copy(src[i], &files[i])) {
            for (int j = 0; j <= i; j++) remove(files[j].stage);
            return 0;
        }
    }
    return profile_commit_txn(files, count);
}

static int profile_delete_paths(const char *const *paths, int count) {
    ProfileTxnFile files[PROFILE_FILE_COUNT];
    if (count < 1 || count > PROFILE_FILE_COUNT) return 0;
    memset(files, 0, sizeof(files));
    for (int i = 0; i < count; i++) {
        snprintf(files[i].path, sizeof(files[i].path), "%s", paths[i]);
        if (!profile_sidecar_path(files[i].path, ".profile-tmp", files[i].stage,
                                  sizeof(files[i].stage)) ||
            !profile_sidecar_path(files[i].path, ".profile-bak", files[i].backup,
                                  sizeof(files[i].backup))) return 0;
        files[i].present = 0;
    }
    return profile_commit_txn(files, count);
}

static int profile_clear_unregistered_slot(int slot) {
    char paths_buf[PROFILE_FILE_COUNT][PROFILE_PATH_CAP];
    const char *paths[PROFILE_FILE_COUNT];
    for (int i = 0; i < PROFILE_FILE_COUNT; i++) {
        if (!profile_file_in_slot(slot, s_profile_files[i], paths_buf[i], sizeof(paths_buf[i]))) return 0;
        paths[i] = paths_buf[i];
    }
    return profile_delete_paths(paths, PROFILE_FILE_COUNT);
}

static int profile_write_text_paths(const char *const *paths, int count, const char *content) {
    ProfileTxnFile files[2];
    if (!content || count < 1 || count > 2) return 0;
    memset(files, 0, sizeof(files));
    for (int i = 0; i < count; i++) {
        snprintf(files[i].path, sizeof(files[i].path), "%s", paths[i]);
        if (!profile_stage_text(&files[i], content)) {
            for (int j = 0; j <= i; j++) remove(files[j].stage);
            return 0;
        }
    }
    return profile_commit_txn(files, count);
}

static int profile_write_manifest(int active, unsigned int mask) {
    char path[512], text[96];
    get_app_path("profiles.txt", path, sizeof(path));
    snprintf(text, sizeof(text), "version=1\nactive_slot=%d\nslot_mask=%u\n", active, mask);
    const char *paths[1] = { path };
    return profile_write_text_paths(paths, 1, text);
}

static int profile_read_manifest(int *active, unsigned int *mask) {
    char path[512], line[64];
    get_app_path("profiles.txt", path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int version = 0, slot = 0, nmask = 0, lines = 0, valid = 1;
    while (fgets(line, sizeof(line), f)) {
        if (!strchr(line, '\n') && !feof(f)) { valid = 0; break; }
        line[strcspn(line, "\r\n")] = '\0';
        char *eq = strchr(line, '=');
        if (!eq || strchr(eq + 1, '=') || lines >= 3) { valid = 0; break; }
        *eq = '\0';
        const char *key = line;
        const char *value = eq + 1;
        if (!value[0]) { valid = 0; break; }
        for (const char *digit = value; *digit; digit++) {
            if (*digit < '0' || *digit > '9') { valid = 0; break; }
        }
        if (!valid) break;
        char *end = NULL;
        long v = strtol(value, &end, 10);
        if (!end || *end != '\0' || v > 7) { valid = 0; break; }
        if (strcmp(key, "version") == 0 && lines == 0) version = (int)v;
        else if (strcmp(key, "active_slot") == 0 && lines == 1) slot = (int)v;
        else if (strcmp(key, "slot_mask") == 0 && lines == 2) nmask = (int)v;
        else { valid = 0; break; }
        lines++;
    }
    if (ferror(f)) valid = 0;
    fclose(f);
    if (lines != 3 || version != 1 || slot < 1 || slot > PROFILE_SLOT_COUNT ||
        nmask < 1 || nmask > 7 || !(nmask & (1 << (slot - 1)))) valid = 0;
    if (!valid) return 0;
    *active = slot;
    *mask = (unsigned int)nmask;
    return 1;
}

static int profile_build_paths(int slot, char src[][PROFILE_PATH_CAP], char dst[][PROFILE_PATH_CAP], int to_live) {
    for (int i = 0; i < PROFILE_FILE_COUNT; i++) {
        char live[512], prof[PROFILE_PATH_CAP];
        get_app_path(s_profile_files[i], live, sizeof(live));
        if (!profile_path_for_slot(slot, s_profile_files[i], prof, sizeof(prof))) return 0;
        snprintf(src[i], PROFILE_PATH_CAP, "%s", to_live ? prof : live);
        snprintf(dst[i], PROFILE_PATH_CAP, "%s", to_live ? live : prof);
    }
    return 1;
}

static int profile_copy_slot(int slot, int to_live) {
    char src[PROFILE_FILE_COUNT][PROFILE_PATH_CAP], dst[PROFILE_FILE_COUNT][PROFILE_PATH_CAP];
    const char *srcp[PROFILE_FILE_COUNT], *dstp[PROFILE_FILE_COUNT];
    if (!profile_ensure_directory(slot)) return 0;
    if (!profile_build_paths(slot, src, dst, to_live)) return 0;
    for (int i = 0; i < PROFILE_FILE_COUNT; i++) { srcp[i] = src[i]; dstp[i] = dst[i]; }
    return profile_copy_set(srcp, dstp, PROFILE_FILE_COUNT);
}

static int profile_valid_slot(int slot) { return slot >= 1 && slot <= PROFILE_SLOT_COUNT; }

static int profile_file_in_slot(int slot, const char *filename, char *out, size_t cap) {
    return profile_path_for_slot(slot, filename, out, cap);
}

static int safe_write_text(const char *filename, const char *content) {
    char live[512];
    get_app_path(filename, live, sizeof(live));
    if (s_profile_mode == STORAGE_PROFILES_READY && profile_is_slot_file(filename)) {
        char slotpath[PROFILE_PATH_CAP];
        if (!profile_file_in_slot(s_profile_active_slot, filename, slotpath, sizeof(slotpath))) return 0;
        if (!profile_ensure_directory(s_profile_active_slot)) return 0;
        /* Commit slot first: if the process stops between renames, startup can
           reload the new active snapshot into the live working files. */
        const char *paths[2] = { slotpath, live };
        return profile_write_text_paths(paths, 2, content);
    }
    const char *paths[1] = { live };
    return profile_write_text_paths(paths, 1, content);
}

static int profile_remove_slot_transaction(int slot, unsigned int new_mask) {
    ProfileTxnFile files[PROFILE_FILE_COUNT];
    int count = 0;
    memset(files, 0, sizeof(files));
    for (int i = 0; i < PROFILE_FILE_COUNT; i++) {
        if (!profile_file_in_slot(slot, s_profile_files[i], files[count].path, sizeof(files[count].path))) return 0;
        if (!profile_sidecar_path(files[count].path, ".profile-bak", files[count].backup,
                                  sizeof(files[count].backup))) return 0;
        files[count].present = 0;
        count++;
    }
    for (int i = 0; i < count; i++) {
        remove(files[i].backup);
        files[i].had_original = profile_file_exists(files[i].path);
        if (files[i].had_original) {
            if (!profile_copy_bytes(files[i].path, files[i].backup) || remove(files[i].path) != 0) {
                for (int j = i; j >= 0; j--) {
                    if (files[j].had_original && !profile_file_exists(files[j].path))
                        rename(files[j].backup, files[j].path);
                    else remove(files[j].backup);
                }
                return 0;
            }
            files[i].removed_original = 1;
        }
    }
    if (!profile_write_manifest(s_profile_active_slot, new_mask)) {
        for (int i = count - 1; i >= 0; i--) {
            if (files[i].removed_original) rename(files[i].backup, files[i].path);
            else remove(files[i].backup);
        }
        return 0;
    }
    for (int i = 0; i < count; i++) remove(files[i].backup);
    s_profile_slot_mask = new_mask;
    return 1;
}

StorageProfileMode storage_profiles_init(void) {
    char manifest[512];
    int active = 0;
    unsigned int mask = 0;
    s_profile_mode = STORAGE_PROFILES_LEGACY;
    s_profile_active_slot = 0;
    s_profile_slot_mask = 0;
    s_profiles_initialized = 1;
    if (!profile_ensure_directory(0)) return s_profile_mode;
    get_app_path("profiles.txt", manifest, sizeof(manifest));

    if (profile_file_exists(manifest)) {
        if (!profile_read_manifest(&active, &mask)) {
            profile_remove_legacy_root_files();
            return s_profile_mode;
        }
        s_profile_active_slot = active;
        s_profile_slot_mask = mask;
        s_profile_mode = STORAGE_PROFILES_READY;
        if (!profile_copy_slot(active, 1)) {
            s_profile_mode = STORAGE_PROFILES_LEGACY;
            s_profile_active_slot = 0;
            s_profile_slot_mask = 0;
            return s_profile_mode;
        }
        profile_remove_legacy_root_files();
        return s_profile_mode;
    }

    /* Root-level saves belong to the retired single-profile layout. Do not import them. */
    profile_remove_legacy_root_files();
    s_profile_mode = STORAGE_PROFILES_NEW;
    return s_profile_mode;
}

StorageProfileMode storage_profiles_mode(void) { return s_profile_mode; }

int storage_profiles_active_slot(void) {
    return (s_profile_mode == STORAGE_PROFILES_READY) ? s_profile_active_slot : 0;
}

int storage_profiles_slot_exists(int slot) {
    return profile_valid_slot(slot) && (s_profile_slot_mask & (1u << (slot - 1))) != 0;
}

int storage_profiles_flush_active(void) {
    if (s_profile_mode != STORAGE_PROFILES_READY || !profile_valid_slot(s_profile_active_slot)) return 0;
    return profile_copy_slot(s_profile_active_slot, 0);
}

int storage_profiles_switch(int slot) {
    if (s_profile_mode != STORAGE_PROFILES_READY || !profile_valid_slot(slot) ||
        !storage_profiles_slot_exists(slot)) return 0;
    if (slot == s_profile_active_slot) return 1;
    int old_slot = s_profile_active_slot;
    if (!storage_profiles_flush_active()) return 0;
    if (!profile_copy_slot(slot, 1)) return 0;
    if (!profile_write_manifest(slot, s_profile_slot_mask)) {
        profile_copy_slot(old_slot, 1);
        return 0;
    }
    s_profile_active_slot = slot;
    return 1;
}

int storage_profiles_create(int slot) {
    if (!profile_valid_slot(slot)) return 0;
    if (s_profile_mode == STORAGE_PROFILES_NEW) {
        if (!profile_ensure_directory(slot) || !profile_clear_unregistered_slot(slot) ||
            !profile_copy_slot(slot, 0)) return 0;
        unsigned int mask = 1u << (slot - 1);
        if (!profile_write_manifest(slot, mask)) return 0;
        s_profile_active_slot = slot;
        s_profile_slot_mask = mask;
        s_profile_mode = STORAGE_PROFILES_READY;
        return 1;
    }
    if (s_profile_mode != STORAGE_PROFILES_READY || storage_profiles_slot_exists(slot)) return 0;
    if (!storage_profiles_flush_active()) return 0;
    if (!profile_ensure_directory(slot) || !profile_clear_unregistered_slot(slot)) return 0;
    unsigned int old_mask = s_profile_slot_mask;
    unsigned int new_mask = old_mask | (1u << (slot - 1));
    if (!profile_write_manifest(s_profile_active_slot, new_mask)) return 0;
    s_profile_slot_mask = new_mask;
    if (!storage_profiles_switch(slot)) {
        profile_write_manifest(s_profile_active_slot, old_mask);
        s_profile_slot_mask = old_mask;
        return 0;
    }
    return 1;
}

int storage_profiles_erase(int slot) {
    if (s_profile_mode != STORAGE_PROFILES_READY || !profile_valid_slot(slot) ||
        slot == s_profile_active_slot || !storage_profiles_slot_exists(slot)) return 0;
    unsigned int new_mask = s_profile_slot_mask & ~(1u << (slot - 1));
    return profile_remove_slot_transaction(slot, new_mask);
}

int storage_profiles_commit_live_temp(const char *filename, const char *temp_path) {
    if (!filename || !temp_path || !profile_is_slot_file(filename)) return 0;
    char live[512], slotpath[PROFILE_PATH_CAP];
    get_app_path(filename, live, sizeof(live));
    ProfileTxnFile files[2];
    memset(files, 0, sizeof(files));
    if (s_profile_mode == STORAGE_PROFILES_READY) {
        if (!profile_file_in_slot(s_profile_active_slot, filename, slotpath, sizeof(slotpath))) return 0;
        if (!profile_ensure_directory(s_profile_active_slot)) return 0;
        snprintf(files[1].path, sizeof(files[1].path), "%s", live);
        snprintf(files[0].path, sizeof(files[0].path), "%s", slotpath);
        if (!profile_stage_copy(temp_path, &files[0]) || !profile_stage_copy(temp_path, &files[1])) {
            remove(files[0].stage); remove(files[1].stage); return 0;
        }
        if (!profile_commit_txn(files, 2)) return 0;
    } else {
        snprintf(files[0].path, sizeof(files[0].path), "%s", live);
        if (!profile_stage_copy(temp_path, &files[0])) return 0;
        if (!profile_commit_txn(files, 1)) return 0;
    }
    remove(temp_path);
    return 1;
}

void storage_load_settings(GameSettings *s) {
    s->music_folder[0] = '\0';
    s->rounds = 2;
    s->difficulty = DIFFICULTY_EASY;
    s->master_timer = 15;
    s->ruleset = RULESET_KK_ADD_10;
    s->background = 0;
    s->bgm_volume = 1.00f;
    s->sfx_volume = 0.50f;
    s->effect_chorus = 0.0f;
    s->effect_bass = 0.0f;
    s->effect_reverb = 0.50f;
    s->anim_speed = 0;
    s->terminal_color = 1;
    s->fullscreen = 0;
    s->bloom = 1;
    s->crt_filter = 0;
    s->gamma = 1.00f;
    s->unlocked_opponents = 1;
    s->current_opponent = 0;
    s->cleared_mask = 0;

    char path[512];
    get_app_path("settings.txt", path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[MUSIC_FOLDER_CAP + 64];
    while (fgets(line, sizeof(line), f)) {
        if (!strchr(line, '\n') && !feof(f)) {
            int ch;
            while ((ch = fgetc(f)) != '\n' && ch != EOF) {}
            continue; /* Ignore overlong settings rather than truncating a path. */
        }
        if (strncmp(line, "music_folder=", 13) == 0) {
            line[strcspn(line, "\r\n")] = '\0';
            if (strlen(line + 13) < sizeof(s->music_folder))
                strcpy(s->music_folder, line + 13);
            continue;
        }
        char k[64], v[64];
        if (sscanf(line, "%63[^=]=%63s", k, v) == 2) {
            if (strcmp(k, "rounds") == 0) {
                int r = atoi(v);
                if (r >= 1 && r <= 12) s->rounds = r;
            } else if (strcmp(k, "difficulty") == 0) {
                if (strcmp(v, "deus") == 0) s->difficulty = DIFFICULTY_DEUS;
                else if (strcmp(v, "master") == 0) s->difficulty = DIFFICULTY_MASTER;
                else if (strcmp(v, "hard") == 0) s->difficulty = DIFFICULTY_HARD;
                else s->difficulty = DIFFICULTY_EASY;
            } else if (strcmp(k, "master_timer") == 0) {
                int mt = atoi(v);
                s->master_timer = (mt == 30) ? 30 : 15;
            } else if (strcmp(k, "ruleset") == 0) {
                if (strcmp(v, "fudawiki") == 0) s->ruleset = RULESET_FUDAWIKI;
                else if (strcmp(v, "kk_add_10") == 0 || strcmp(v, "kk-add-10") == 0) s->ruleset = RULESET_KK_ADD_10;
                else if (strcmp(v, "tournament") == 0) s->ruleset = RULESET_TOURNAMENT;
                else if (strcmp(v, "yakuza") == 0) s->ruleset = RULESET_YAKUZA;
                else s->ruleset = RULESET_NINTENDO;
            } else if (strcmp(k, "background") == 0) {
                int bg = atoi(v);
                if (bg >= 0 && bg < 8) s->background = bg;
            } else if (strcmp(k, "bgm_volume") == 0) {
                float vol = (float)atof(v);
                s->bgm_volume = (vol < 0.0f) ? 0.0f : (vol > 1.0f) ? 1.0f : vol;
            } else if (strcmp(k, "effect_chorus") == 0 || strcmp(k, "effect_bass") == 0 || strcmp(k, "effect_reverb") == 0) {
                float value = strtof(v, NULL);
                if (!(value >= 0.0f)) value = 0.0f; /* Includes NaN. */
                if (value > 1.0f) value = 1.0f;
                if (strcmp(k, "effect_chorus") == 0) s->effect_chorus = value;
                else if (strcmp(k, "effect_bass") == 0) s->effect_bass = value;
                else s->effect_reverb = value;
            } else if (strcmp(k, "sfx_volume") == 0) {
                float vol = (float)atof(v);
                s->sfx_volume = (vol < 0.0f) ? 0.0f : (vol > 1.0f) ? 1.0f : vol;
            } else if (strcmp(k, "anim_speed") == 0) {
                int sp = atoi(v);
                if (sp >= 0 && sp <= 2) s->anim_speed = sp;
            } else if (strcmp(k, "terminal_color") == 0 || strcmp(k, "theme") == 0) {
                if (strcmp(v, "amber") == 0 || strcmp(v, "1") == 0) s->terminal_color = 1;
                else if (strcmp(v, "teal") == 0 || strcmp(v, "2") == 0) s->terminal_color = 2;
                else if (strcmp(v, "crimson") == 0 || strcmp(v, "3") == 0) s->terminal_color = 3;
                else if (strcmp(v, "violet") == 0 || strcmp(v, "4") == 0) s->terminal_color = 4;
                else if (strcmp(v, "dark") == 0 || strcmp(v, "5") == 0) s->terminal_color = 5;
                else s->terminal_color = 0;
            } else if (strcmp(k, "fullscreen") == 0) {
                s->fullscreen = (atoi(v) == 1) ? 1 : 0;
            } else if (strcmp(k, "bloom") == 0) {
                int bl = atoi(v);
                if (bl >= 0 && bl <= 2) s->bloom = bl;
            } else if (strcmp(k, "crt_filter") == 0 || strcmp(k, "crt") == 0) {
                int crt = atoi(v);
                if (crt >= 0 && crt <= 3) s->crt_filter = crt;
            } else if (strcmp(k, "gamma") == 0) {
                float gm = (float)atof(v);
                if (gm >= 0.5f && gm <= 2.0f) s->gamma = gm;
            } else if (strcmp(k, "unlocked_opponents") == 0) {
                int uo = atoi(v);
                if (uo >= 1 && uo <= 12) s->unlocked_opponents = uo;
            } else if (strcmp(k, "current_opponent") == 0) {
                int co = atoi(v);
                if (co >= 0 && co <= 12) s->current_opponent = co;
            } else if (strcmp(k, "cleared_mask") == 0) {
                unsigned long long cm = strtoull(v, NULL, 10);
                s->cleared_mask = cm;
            }
        }
    }
    fclose(f);
    if (s->difficulty == DIFFICULTY_DEUS) {
        s->ruleset = RULESET_YAKUZA;
        s->master_timer = 15;
        s->terminal_color = 5;
        s->background = 5;
    }
}

void storage_save_settings(const GameSettings *s) {
    char buf[MUSIC_FOLDER_CAP + 1024];
    GameSettings save_settings = *s;
    if (s_deus_session_snapshot) {
        save_settings.difficulty = s_deus_session_snapshot->difficulty;
        save_settings.ruleset = s_deus_session_snapshot->ruleset;
        save_settings.master_timer = s_deus_session_snapshot->master_timer;
        save_settings.terminal_color = s_deus_session_snapshot->terminal_color;
        save_settings.background = s_deus_session_snapshot->background;
        save_settings.current_opponent = s_deus_session_snapshot->current_opponent;
        s = &save_settings;
    }
    const char *diff_str = (s->difficulty == DIFFICULTY_DEUS) ? "deus" :
                           (s->difficulty == DIFFICULTY_MASTER) ? "master" :
                           (s->difficulty == DIFFICULTY_HARD) ? "hard" : "easy";
    const char *rule_str = (s->ruleset == RULESET_FUDAWIKI) ? "fudawiki" :
                           (s->ruleset == RULESET_KK_ADD_10) ? "kk_add_10" :
                           (s->ruleset == RULESET_TOURNAMENT) ? "tournament" :
                           (s->ruleset == RULESET_YAKUZA) ? "yakuza" : "nintendo";
    const char *col_str = (s->terminal_color == 1) ? "amber" :
                          (s->terminal_color == 2) ? "teal" :
                          (s->terminal_color == 3) ? "crimson" :
                          (s->terminal_color == 4) ? "violet" :
                          (s->terminal_color == 5) ? "dark" : "green";

    snprintf(buf, sizeof(buf),
             "music_folder=%s\n"
             "rounds=%d\n"
             "difficulty=%s\n"
             "master_timer=%d\n"
             "ruleset=%s\n"
             "background=%d\n"
             "bgm_volume=%.2f\n"
             "sfx_volume=%.2f\n"
             "effect_chorus=%.2f\n"
             "effect_bass=%.2f\n"
             "effect_reverb=%.2f\n"
             "anim_speed=%d\n"
             "terminal_color=%s\n"
             "fullscreen=%d\n"
             "bloom=%d\n"
             "crt_filter=%d\n"
             "gamma=%.2f\n"
             "unlocked_opponents=%d\n"
             "current_opponent=%d\n"
             "cleared_mask=%llu\n",
             s->music_folder, s->rounds, diff_str, s->master_timer, rule_str, s->background,
             s->bgm_volume, s->sfx_volume, s->effect_chorus, s->effect_bass, s->effect_reverb, s->anim_speed, col_str,
             s->fullscreen, s->bloom, s->crt_filter, s->gamma,
             s->unlocked_opponents, s->current_opponent, (unsigned long long)s->cleared_mask);
    safe_write_text("settings.txt", buf);
}

void storage_load_stats(GameStats *st) {
    memset(st, 0, sizeof(*st));
    char path[512];
    get_app_path("stats.txt", path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char k[64], v[64];
        if (sscanf(line, "%63[^=]=%63s", k, v) == 2) {
            int val = atoi(v);
            if (strcmp(k, "matches_played") == 0) st->matches_played = val;
            else if (strcmp(k, "matches_won") == 0) st->matches_won = val;
            else if (strcmp(k, "matches_lost") == 0) st->matches_lost = val;
            else if (strcmp(k, "matches_drawn") == 0) st->matches_drawn = val;
            else if (strcmp(k, "rounds_won_you") == 0) st->rounds_won_you = val;
            else if (strcmp(k, "rounds_won_cpu") == 0) st->rounds_won_cpu = val;
            else if (strcmp(k, "points_for") == 0) st->points_for = val;
            else if (strcmp(k, "points_against") == 0) st->points_against = val;
            else if (strcmp(k, "highest_round_score") == 0) st->highest_round_score = val;
            else if (strcmp(k, "best_award") == 0) st->best_award = val;
            else if (strcmp(k, "koi_calls_you") == 0) st->koi_calls_you = val;
            else if (strcmp(k, "koi_calls_cpu") == 0) st->koi_calls_cpu = val;
            else if (strcmp(k, "easy_matches") == 0) st->easy_matches = val;
            else if (strcmp(k, "hard_matches") == 0) st->hard_matches = val;
            else if (strcmp(k, "master_matches") == 0) st->master_matches = val;
            else if (strcmp(k, "current_streak") == 0) st->current_streak = val;
            else if (strcmp(k, "best_streak") == 0 || strcmp(k, "max_win_streak") == 0) st->best_streak = val;
            else if (strncmp(k, "yaku_", 5) == 0) {
                int yidx = atoi(k + 5);
                if (yidx >= 0 && yidx < YAKU_COUNT) st->yaku_counts[yidx] = val;
            }
        }
    }
    fclose(f);
}

const char *storage_get_favorite_combo(const GameStats *st) {
    static const char *const names[YAKU_COUNT] = {
        "GOKO", "SHIKO", "AME-SHIKO", "SANKO",
        "HANAMI-ZAKE", "TSUKIMI-ZAKE", "INO-SHIKA-CHO",
        "AKATAN", "AOTAN", "AKATAN + AOTAN",
        "TANE", "TANZAKU", "KASU"
    };
    if (!st) return "NONE";
    int best = -1, max_val = 0;
    for (int i = 0; i < YAKU_COUNT; i++) {
        if (st->yaku_counts[i] > max_val) {
            max_val = st->yaku_counts[i];
            best = i;
        }
    }
    return (best >= 0 && max_val > 0) ? names[best] : "NONE";
}

void storage_save_stats(const GameStats *st) {
    char buf[2048];
    int n = snprintf(buf, sizeof(buf),
             "matches_played=%d\n"
             "matches_won=%d\n"
             "matches_lost=%d\n"
             "matches_drawn=%d\n"
             "rounds_won_you=%d\n"
             "rounds_won_cpu=%d\n"
             "points_for=%d\n"
             "points_against=%d\n"
             "highest_round_score=%d\n"
             "best_award=%d\n"
             "koi_calls_you=%d\n"
             "koi_calls_cpu=%d\n"
             "easy_matches=%d\n"
             "hard_matches=%d\n"
             "current_streak=%d\n"
             "max_win_streak=%d\n"
             "best_streak=%d\n"
             "favorite_combo=%s\n",
             st->matches_played, st->matches_won, st->matches_lost, st->matches_drawn,
             st->rounds_won_you, st->rounds_won_cpu, st->points_for, st->points_against,
             st->highest_round_score, st->best_award,
             st->koi_calls_you, st->koi_calls_cpu,
             st->easy_matches, st->hard_matches,
             st->current_streak, st->best_streak, st->best_streak,
             storage_get_favorite_combo(st));
    size_t pos = (n > 0 && (size_t)n < sizeof(buf)) ? (size_t)n : 0;

    if (st->master_matches > 0 && pos < sizeof(buf)) {
        int w = snprintf(buf + pos, sizeof(buf) - pos, "master_matches=%d\n", st->master_matches);
        if (w > 0 && pos + (size_t)w < sizeof(buf)) pos += (size_t)w;
    }

    for (int i = 0; i < YAKU_COUNT && pos < sizeof(buf); i++) {
        int w = snprintf(buf + pos, sizeof(buf) - pos, "yaku_%d=%d\n", i, st->yaku_counts[i]);
        if (w > 0 && pos + (size_t)w < sizeof(buf)) pos += (size_t)w;
    }
    safe_write_text("stats.txt", buf);
}

static int profile_remove_live_file(const char *filename) {
    char live[512];
    get_app_path(filename, live, sizeof(live));
    if (s_profile_mode == STORAGE_PROFILES_READY && profile_is_slot_file(filename)) {
        char slotpath[PROFILE_PATH_CAP];
        if (!profile_file_in_slot(s_profile_active_slot, filename, slotpath, sizeof(slotpath))) return 0;
        const char *paths[2] = { live, slotpath };
        return profile_delete_paths(paths, 2);
    }
    const char *paths[1] = { live };
    return profile_delete_paths(paths, 1);
}

void storage_reset_stats(GameStats *st) {
    memset(st, 0, sizeof(*st));
    storage_save_stats(st);

    char rep_path[512];
    get_app_path("stats_report.txt", rep_path, sizeof(rep_path));
    profile_remove_live_file("history.txt");
    remove(rep_path);
}

void storage_load_history(MatchHistory *h) {
    h->count = 0;
    char path[512];
    get_app_path("history.txt", path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[128];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';
        if (len > 0 && h->count < 10) {
            snprintf(h->lines[h->count++], sizeof(h->lines[0]), "%s", line);
        }
    }
    fclose(f);
}

void storage_add_history(MatchHistory *h, const char *record_line) {
    if (h->count >= 10) {
        memmove(&h->lines[0], &h->lines[1], 9 * sizeof(h->lines[0]));
        h->count = 9;
    }
    snprintf(h->lines[h->count++], sizeof(h->lines[0]), "%s", record_line);

    char buf[2048];
    size_t pos = 0;
    buf[0] = '\0';
    for (int i = 0; i < h->count && pos < sizeof(buf); i++) {
        int w = snprintf(buf + pos, sizeof(buf) - pos, "%s\n", h->lines[i]);
        if (w > 0 && pos + (size_t)w < sizeof(buf)) pos += (size_t)w;
    }
    safe_write_text("history.txt", buf);
}

void storage_write_report(const GameStats *st, const MatchHistory *h) {
    char buf[4096];
    float win_rate = (st->matches_played > 0) ? ((float)st->matches_won / st->matches_played * 100.0f) : 0.0f;

    int n = snprintf(buf, sizeof(buf),
             "====================================================\n"
             "        HANAFUDA // KOI-KOI - STATS REPORT          \n"
             "====================================================\n\n"
             "MATCHES\n"
             "  Played:              %d\n"
             "  Won / Lost:          %d / %d (Drawn: %d)\n"
             "  Win Rate:            %.1f%%\n\n"
             "ROUNDS & SCORING\n"
             "  Rounds Won:          YOU %d | CPU %d\n"
             "  Total Points:        FOR %d | AGAINST %d\n"
             "  Highest Round Score: %d pts\n"
             "  Best Award (Best Win): %d pts\n"
             "  Koi-Koi Calls:       YOU %d | CPU %d\n\n"
             "DIFFICULTY & STREAKS\n"
             "  Easy / Hard:         %d / %d\n"
             "  Current Streak:      %d\n"
             "  Max Win Streak:      %d\n"
             "  Favorite Combo:      %s\n\n"
             "LAST 10 MATCHES\n",
             st->matches_played, st->matches_won, st->matches_lost, st->matches_drawn,
             win_rate, st->rounds_won_you, st->rounds_won_cpu,
             st->points_for, st->points_against, st->highest_round_score, st->best_award,
             st->koi_calls_you, st->koi_calls_cpu,
             st->easy_matches, st->hard_matches,
             st->current_streak, st->best_streak,
             storage_get_favorite_combo(st));
    size_t pos = (n > 0 && (size_t)n < sizeof(buf)) ? (size_t)n : 0;

    for (int i = 0; i < h->count && pos < sizeof(buf); i++) {
        int w = snprintf(buf + pos, sizeof(buf) - pos, "  [%d] %s\n", i + 1, h->lines[i]);
        if (w > 0 && pos + (size_t)w < sizeof(buf)) pos += (size_t)w;
    }
    if (pos < sizeof(buf)) {
        snprintf(buf + pos, sizeof(buf) - pos, "\n====================================================\n");
    }
    safe_write_text("stats_report.txt", buf);
}

void storage_save_all(const GameSettings *s, const GameStats *st, const MatchHistory *h) {
    storage_save_settings(s);
    storage_save_stats(st);
    storage_write_report(st, h);
}

void storage_record_match(const GameState *g, const GameSettings *s, GameStats *st, MatchHistory *h) {
    st->matches_played++;
    if (s->difficulty == DIFFICULTY_MASTER || s->difficulty == DIFFICULTY_DEUS) st->master_matches++;
    else if (s->difficulty == DIFFICULTY_HARD) st->hard_matches++;
    else st->easy_matches++;
    int you = g->total_score[0], cpu = g->total_score[1];
    st->points_for += you; st->points_against += cpu;
    const char *outcome = (you > cpu) ? (st->matches_won++, st->current_streak++, "WIN") :
                          (you < cpu) ? (st->matches_lost++, st->current_streak = 0, "LOSS") : (st->matches_drawn++, "DRAW");
    if (st->current_streak > st->best_streak) st->best_streak = st->current_streak;
    if (you > cpu && you > st->best_award) st->best_award = you;
    for (int i = 0; i < g->ledger_count; i++) {
        if (g->ledger[i].winner == 0) st->rounds_won_you++;
        else if (g->ledger[i].winner == 1) st->rounds_won_cpu++;
        if (g->ledger[i].award > st->highest_round_score) st->highest_round_score = g->ledger[i].award;
    }
    for (int y = 0; y < YAKU_COUNT; y++) st->yaku_counts[y] += g->human_yaku_counts[y];
    st->koi_calls_you += g->match_koi_calls[0]; st->koi_calls_cpu += g->match_koi_calls[1];
    time_t now = time(NULL); char time_str[32], record_line[160];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", localtime(&now));
    const char *r_str = (s->ruleset == RULESET_FUDAWIKI) ? "fudawiki" : (s->ruleset == RULESET_KK_ADD_10) ? "kk-add-10" :
                        (s->ruleset == RULESET_TOURNAMENT) ? "tournament" : (s->ruleset == RULESET_YAKUZA) ? "yakuza" : "nintendo";
    const char *d_str = (s->difficulty == DIFFICULTY_DEUS) ? "deus" : (s->difficulty == DIFFICULTY_MASTER) ? "master" :
                        (s->difficulty == DIFFICULTY_HARD) ? "hard" : "easy";
    snprintf(record_line, sizeof(record_line), "%s;rounds=%d;%s;%s;you=%d;cpu=%d;%s", time_str, s->rounds, r_str, d_str, you, cpu, outcome);
    storage_add_history(h, record_line); storage_save_stats(st); storage_write_report(st, h);
}
