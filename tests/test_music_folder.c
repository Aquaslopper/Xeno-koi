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
    assert(audio_set_music_folder(&a, "") == 11);
    assert(raylib_stub_attached == 11);
    assert(fabsf(raylib_stub_music_volume - 0.37f) < 0.001f);

    char *paths[] = {"/custom music/z.MP3", "/custom music/broken.mp3", "/custom music/b.ogg", "/custom music/a.wav"};
    raylib_stub_files = (FilePathList){4, 4, paths};
    a.current_music = 1;
    assert(audio_set_music_folder(&a, "/custom music") == 3);
    assert(a.game_track_count == 3 && a.current_music == 1);
    assert(strcmp(paths[0], "/custom music/a.wav") == 0);
    assert(strcmp(paths[1], "/custom music/b.ogg") == 0);
    assert(a.music_game[0].frameCount < a.music_game[1].frameCount);
    assert(raylib_stub_music_unloaded == 11 && raylib_stub_detached == 11);
    assert(raylib_stub_lists_unloaded == 1);
    audio_play_menu_music(&a);
    assert(a.current_music == 2 && a.current_game_track == 0);
    for (int i = 1; i <= AUDIO_MAX_TRACKS; i++) {
        audio_start_match_music(&a, i);
        assert(a.current_game_track >= 0 && a.current_game_track < 3);
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
    for (int i = 1; i <= AUDIO_MAX_TRACKS; i++) {
        audio_start_match_music(&a, i);
        assert(a.current_game_track == 0);
    }
    char *many[112];
    for (int i = 0; i < 112; i++) many[i] = "song.mp3";
    raylib_stub_files = (FilePathList){112, 112, many};
    assert(audio_set_music_folder(&a, "/many") == AUDIO_MAX_TRACKS);
    assert(a.game_track_count == AUDIO_MAX_TRACKS);
    audio_start_match_music(&a, AUDIO_MAX_TRACKS);
    assert(a.current_game_track == AUDIO_MAX_TRACKS - 1 && a.has_music_game[AUDIO_MAX_TRACKS - 1]);
    assert(audio_set_music_folder(&a, "") == 11);
    assert(a.game_track_count == 11 && a.current_music == 2 && !a.has_music_game[11]);
    audio_close(&a);
    assert(raylib_stub_music_unloaded == (int)raylib_stub_music_loaded);
    assert(raylib_stub_attached == raylib_stub_detached);

    GameSettings s, restored;
    storage_load_settings(&s);
    assert(!s.music_folder[0]);
    assert(s.effect_chorus == 0 && s.effect_bass == 0 && s.effect_reverb == 0.5f);
    /* Desktop and WASM share this loader: a missing settings file starts at neutral gamma. */
    assert(fabsf(s.gamma - 1.00f) < 0.001f);
    const char *folder = "C:\\My Music\\日本語 = mix";
    strcpy(s.music_folder, folder);
    s.bgm_volume = 0.63f;
    s.gamma = 1.35f;
    s.effect_chorus = 0.81f; s.effect_bass = 0.67f; s.effect_reverb = 0.93f;
    storage_save_settings(&s);
    storage_load_settings(&restored);
    assert(strcmp(restored.music_folder, folder) == 0);
    assert(fabsf(restored.bgm_volume - 0.63f) < 0.001f);
    /* A fresh-install default must not overwrite a player's saved browser/desktop choice. */
    assert(fabsf(restored.gamma - 1.35f) < 0.001f);
    assert(fabsf(restored.effect_chorus - 0.81f) < 0.001f);
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
    fputs("effect_chorus=nan\neffect_bass=-1\neffect_reverb=inf\n", f);
    fclose(f);
    storage_load_settings(&restored);
    assert(restored.effect_chorus == 0 && restored.effect_bass == 0 && restored.effect_reverb == 1);
    f = fopen("settings.txt", "w");
    assert(f);
    fputs("effect_stereo=1\neffect_bass=0.25\n", f);
    fclose(f);
    storage_load_settings(&restored);
    assert(restored.effect_chorus == 0 && restored.effect_bass == 0.25f);
    /* The abandoned width setting must not turn on a different effect. */
    puts("All custom music folder / settings checks passed.");
    return 0;
}
