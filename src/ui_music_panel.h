#ifndef UI_MUSIC_PANEL_H
#define UI_MUSIC_PANEL_H

#include "raylib.h"
#include "storage.h"
#include "ui_audio.h"

/* One music panel, shared by the Pancratium campaign picker and the Atrium Plex.
   Both hosts pass their own rectangle; the frame and border are the ones the
   Pancratium STAGE page uses (COLOR_PANEL_BG + ui_draw_panel_border). The audio
   and settings pointers are bound once by ui_main.c so neither hub has to carry
   an AudioAssets* through its click handler. */
void music_panel_bind(GameSettings *settings, AudioAssets *audio);

void music_panel_open(void);
void music_panel_close(void);
void music_panel_toggle(void);
int  music_panel_is_open(void);
/* Only the effect test is menu-only; STOP lasts until PLAY or the next match. */
void music_panel_sync(int hub_visible);

void music_panel_draw(Rectangle r, Vector2 vm);
/* Returns 1 when a click landed inside r (including the folder view). */
int  music_panel_handle_click(Rectangle r, Vector2 vm);

/* Frame-loop hooks. -1/0/1 follows the existing Options effect test. */
int  music_panel_effect_test(void);
/* 1 when STOP was pressed or the playlist finished with repeat off. */
int  music_panel_music_stopped(void);
/* 1-based pinned match-start track, or 0 when RANDOM is on (ui_main.c randomises).
   Also clears STOP: a new match always starts music. */
int  music_panel_match_track(const AudioAssets *a);
/* 1-based track after one ends: LOOP 1 repeats, RANDOM takes random_track,
   LOOP wraps to track 1, none plays the next in order. 0 = playlist ended. */
int  music_panel_next_track(const AudioAssets *a, int random_track);

#endif /* UI_MUSIC_PANEL_H */
