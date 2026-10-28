#ifndef UI_MUSIC_METER_H
#define UI_MUSIC_METER_H

#include "ui_audio.h"

/* Shared existing music-level tap; no audio samples are modified. */
void music_meter_update(AudioAssets *audio, int enabled, int frozen,
                        float transition_alpha, float dt, int require_audible);
float music_meter_level(void);
void music_meter_detach(AudioAssets *audio);

#endif
