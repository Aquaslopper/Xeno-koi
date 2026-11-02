#ifndef UI_TABLE_GLOW_H
#define UI_TABLE_GLOW_H
#include "ui_audio.h"

void table_glow_draw(struct Texture overlay, float dt, int table_design);
void table_glow_unload(void);
/* UI-owned listener lifecycle; never changes the DSP or Action Card state. */
void table_glow_sync_music(AudioAssets *audio, int active, int paused,
                           float transition_alpha, float dt);
void table_glow_detach_music(AudioAssets *audio);

#endif
