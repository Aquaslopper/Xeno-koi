#ifndef UI_LIGHT_PRESET_H
#define UI_LIGHT_PRESET_H

/* Shareable light-reaction presets for the 12 roaming table spotlights.
   Desktop reads the .txt files in presets/ next to the executable; the web
   build uses the compiled-in presets because it has only a couple of tracks.
   A preset only changes numbers inside src/ui_table_glow.c - it never touches
   the audio DSP, the background beam, the Action Card meter or the cards. */
typedef struct {
    char name[32];
    int lights;                                   /* 1..12 active dots */
    float strength, size_a, size_b, halo_a, halo_b, core_gain, exposure;
    float relief_min, relief_full, speed_a, speed_b, travel_x, travel_y;
    float idle, music_gain, level_gain, attack, release;
    float adapt_speed, beat_contrast;            /* read by the audio meter */
    float theme_mix, color_r, color_g, color_b;  /* base tint */
    float peak_r, peak_g, peak_b, color_shift;   /* music-driven colour blend */
    float bloom_gain, bloom_radius;
} LightPreset;

void light_preset_init(void);                    /* lazy, safe to call repeatedly */
void light_preset_reload(void);                  /* re-scan presets/ after editing a file */
int  light_preset_count(void);
const char *light_preset_name(int index);
int  light_preset_selected(void);
void light_preset_select(int index);
const LightPreset *light_preset_current(void);   /* UI thread only */

/* Fixed-point atomic snapshots for the real-time audio callback. The callback
   must never dereference light_preset_current() or read any file. */
float light_preset_adapt_speed(void);
float light_preset_beat_contrast(void);

#endif /* UI_LIGHT_PRESET_H */
