/* Real playlist replacement and settings persistence, synthetic Raylib files. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../src/ui_audio.c"
#include "../src/storage.c"

int main(void) {
    AudioAssets a;
    audio_init(&a, 0.37f, 0.5f);
    assert(a.current_game_track == -1);
    raylib_stub_files_exist = true;
    assert(audio_set_music_folder(&a, "") == 10);
    assert(raylib_stub_attached == 10);
    assert(fabsf(raylib_stub_music_volume - 0.37f) < 0.001f);

    char *paths[] = {"/custom music/z.MP3", "/custom music/broken.mp3", "/custom music/a.mp3"};
    raylib_stub_files = (FilePathList){3, 3, paths};
    a.current_music = 1; /* Replacing match music must not interrupt the menu. */
    assert(audio_set_music_folder(&a, "/custom music") == 2);
    assert(a.game_track_count == 2 && a.current_music == 1);
    assert(strcmp(paths[0], "/custom music/a.mp3") == 0);
    assert(a.music_game[0].frameCount < a.music_game[1].frameCount);
    assert(raylib_stub_music_unloaded == 10 && raylib_stub_detached == 10);
    assert(raylib_stub_lists_unloaded == 1);
    for (int i = 1; i <= 10; i++) {
        audio_start_match_music(&a, i);
        assert(a.current_game_track >= 0 && a.current_game_track < 2);
        assert(a.has_music_game[a.current_game_track]);
    }
    unsigned int previous = a.music_game[0].frameCount;
    int unloaded = raylib_stub_music_unloaded;
    assert(audio_set_music_folder(&a, "/missing") == -1);
    raylib_stub_files.count = 0;
    assert(audio_set_music_folder(&a, "/empty") == -1);
    char *bad[] = {"broken.mp3"};
    raylib_stub_files = (FilePathList){1, 1, bad};
    assert(audio_set_music_folder(&a, "/corrupt") == -1);
    assert(a.music_game[0].frameCount == previous && a.current_music == 2);
    assert(raylib_stub_music_unloaded == unloaded);

    char *one[] = {"only.mp3"};
    raylib_stub_files = (FilePathList){1, 1, one};
    assert(audio_set_music_folder(&a, "/one") == 1);
    for (int i = 1; i <= 10; i++) {
        audio_start_match_music(&a, i);
        assert(a.current_game_track == 0);
    }
    char *many[12];
    for (int i = 0; i < 12; i++) many[i] = "song.mp3";
    raylib_stub_files = (FilePathList){12, 12, many};
    assert(audio_set_music_folder(&a, "/many") == 10);
    assert(a.game_track_count == 10);
    assert(audio_set_music_folder(&a, "") == 10);
    assert(a.game_track_count == 10 && a.current_music == 2);
    audio_close(&a);
    assert(raylib_stub_music_unloaded == (int)raylib_stub_music_loaded);
    assert(raylib_stub_attached == raylib_stub_detached);

    GameSettings s, restored;
    storage_load_settings(&s);
    assert(!s.music_folder[0]);
    assert(s.effect_stereo == 0.5f && s.effect_bass == 0 && s.effect_reverb == 0.5f);
    const char *folder = "C:\\My Music\\日本語 = mix";
    strcpy(s.music_folder, folder);
    s.bgm_volume = 0.63f;
    s.effect_stereo = 0.81f; s.effect_bass = 0.67f; s.effect_reverb = 0.93f;
    storage_save_settings(&s);
    storage_load_settings(&restored);
    assert(strcmp(restored.music_folder, folder) == 0);
    assert(fabsf(restored.bgm_volume - 0.63f) < 0.001f);
    assert(fabsf(restored.effect_stereo - 0.81f) < 0.001f);
    assert(fabsf(restored.effect_bass - 0.67f) < 0.001f);
    assert(fabsf(restored.effect_reverb - 0.93f) < 0.001f);
    memset(s.music_folder, 'x', sizeof(s.music_folder) - 1);
    s.music_folder[sizeof(s.music_folder) - 1] = '\0';
    storage_save_settings(&s);
    storage_load_settings(&restored);
    assert(strcmp(restored.music_folder, s.music_folder) == 0);
    s.music_folder[0] = '\0';
    storage_save_settings(&s);
    storage_load_settings(&restored);
    assert(!restored.music_folder[0]);

    FILE *f = fopen("settings.txt", "w");
    assert(f);
    fputs("music_folder=", f);
    for (int i = 0; i < 2000; i++) fputc('x', f);
    fputs("\nrounds=7\n", f);
    fclose(f);
    storage_load_settings(&restored);
    assert(!restored.music_folder[0] && restored.rounds == 7);
    f = fopen("settings.txt", "w");
    assert(f);
    fputs("music_folder=/Music with spaces\r\nrounds=9\n", f);
    fclose(f);
    storage_load_settings(&restored);
    assert(strcmp(restored.music_folder, "/Music with spaces") == 0 && restored.rounds == 9);
    f = fopen("settings.txt", "w");
    assert(f);
    fputs("effect_stereo=nan\neffect_bass=-1\neffect_reverb=inf\n", f);
    fclose(f);
    storage_load_settings(&restored);
    assert(restored.effect_stereo == 0 && restored.effect_bass == 0 && restored.effect_reverb == 1);
    puts("All custom music folder / settings checks passed.");
    return 0;
}
