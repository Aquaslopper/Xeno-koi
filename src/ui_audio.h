#ifndef UI_AUDIO_H
#define UI_AUDIO_H

#include "raylib.h"

typedef struct {
    Music music_menu;
    int has_music_menu;

    Music music_game[10];
    int has_music_game[10];
    int current_game_track; /* 0..9, or -1 if none */

    Sound sfx_flip;
    Sound sfx_drop;
    Sound sfx_capture;
    Sound sfx_koi;
    Sound sfx_win;
    Sound sfx_lose;
    Sound sfx_click;
    int has_sfx_flip;
    int has_sfx_drop;
    int has_sfx_capture;
    int has_sfx_koi;
    int has_sfx_win;
    int has_sfx_lose;
    int has_sfx_click;

    float bgm_volume;
    float sfx_volume;
    int current_music; /* 0: none, 1: menu, 2: game */
} AudioAssets;

void audio_init(AudioAssets *a, float bgm_vol, float sfx_vol);
void audio_update(AudioAssets *a);

void audio_set_bgm_volume(AudioAssets *a, float volume);
void audio_set_sfx_volume(AudioAssets *a, float volume);

void audio_play_menu_music(AudioAssets *a);
void audio_start_match_music(AudioAssets *a, int track_1_to_10);
void audio_resume_match_music(AudioAssets *a);
int  audio_is_match_music_finished(const AudioAssets *a);
void audio_stop_music(AudioAssets *a);

void audio_play_flip(AudioAssets *a);
void audio_play_drop(AudioAssets *a);
void audio_play_capture(AudioAssets *a);
void audio_play_koi(AudioAssets *a);
void audio_play_win(AudioAssets *a);
void audio_play_lose(AudioAssets *a);
void audio_play_click(AudioAssets *a);

void audio_close(AudioAssets *a);

#endif /* UI_AUDIO_H */
