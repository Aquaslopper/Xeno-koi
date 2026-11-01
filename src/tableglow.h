/* tableglow.h - the light that glides over the cards, on the table itself.

   WHY THIS FILE EXISTS
   --------------------
   On a forged table every card is drawn twice: once normally, and once more through a
   small fragment shader that turns the bezel's own brightness into a fake normal map and
   sweeps two point lights across it (see board_draw_table_bezel_shine() in ui_board.c).
   The table art itself - assets/maps/<map>/overlay.png - gets nothing: it is a single
   DrawTexture() and sits flat under the cards. This module gives the table the same light,
   driven by the music that is playing.

   HOW TO TURN IT OFF
   ------------------
   F8 while a forged table is on screen, or set TABLEGLOW_ENABLED to 0 at the top of
   tableglow.c for a build with no cost at all.

   WHAT THE MUSIC DOES  (three terms, nothing hidden)
   --------------------------------------------------
     loudness (momentary LUFS)  ->  brightness
     tempo    (BPM, detected)   ->  orbit speed
     high share of the spectrum ->  bloom width

   There is no auto-gain, no adaptive normalisation and no reverb reading anywhere: the
   same audio always produces the same picture. Every number below is yours.

   THE SETTINGS
   ------------
   One struct, `tableglow`, defined in tableglow.c and declared here. Each field's comment
   says what it does and its sane range; the default lives in a TGLOW_DEF_* macro so the
   comment and the value can never drift apart.
*/

#ifndef TABLEGLOW_H
#define TABLEGLOW_H

#include "raylib.h"
#include "ui_audio.h"     /* AudioAssets - needed by tableglow_sync_music() */

typedef struct TableGlowSettings {
    /* ---- on / off ------------------------------------------------------------ */
    int   enabled;              /* 1 = draw the glow. F8 flips this at runtime.       */
    int   key_toggle;           /* key that flips it. KEY_F8 unless the game uses it. */

    /* ---- the light itself ---------------------------------------------------- */
    float radius_px;            /* how far the shader looks for a brightness gradient,
                                   in canvas pixels. 4..40, default 16.
                                   Too small: nothing lights. Too large: soft mush.     */
    float luminance_lo;         /* a pixel may catch light only if it is at least this
                                   share of its neighbourhood's brightest pixel. 0..1,
                                   default 0.42. Raise it to light fewer, sharper
                                   details; lower it for a broader glow.                */
    float luminance_hi;         /* where that ramp reaches full strength. Must be above
                                   luminance_lo. Default 0.72.                          */
    float max_alpha;            /* ceiling of the additive pass. 0..1, default 0.88.
                                   This is the single "how strong" knob.                */
    float intensity;            /* multiplier on top of the loudness term. 0..2,
                                   default 1.00. Use it to audition 0.5 quickly.        */

    /* ---- loudness -> brightness ---------------------------------------------- */
    float brightness_base;      /* glow with no music at all. 0..1, default 0.55.      */
    float brightness_per_lufs;  /* how much loudness adds at lufs_ceil_db. 0..1,
                                   default 0.45. brightness_base + this = 1.0 at the
                                   ceiling, so the light never clips past full.        */
    float lufs_floor_db;        /* loudness mapped to zero: -70..0, default -40.
                                   Narrow the gap to lufs_ceil_db for more movement.    */
    float lufs_ceil_db;         /* loudness mapped to full: -40..0, default -10.       */
    float lufs_attack_ms;       /* how fast brightness rises. 10..2000, default 120.   */
    float lufs_release_ms;      /* how slow it falls. 10..2000, default 450. Slower
                                   release = more breathing, less flicker.             */

    /* ---- high share -> bloom -------------------------------------------------- */
    float bloom_base;           /* halo radius multiplier for a dark tone. 0.2..2,
                                   default 0.85.                                       */
    float bloom_per_high;       /* added to that when the high band is all there is,
                                   0..1.5, default 0.55.                                */
    float high_smooth_ms;       /* how lazily the halo follows the tone. 20..2000,
                                   default 350.                                        */

    /* ---- tempo -> orbit speed -------------------------------------------------- */
    float speed_base;           /* orbit rate at bpm_ref, in radians on the light's
                                   circle per second. 0..3, default 0.52 - the same
                                   number the cards use.                               */
    float bpm_ref;              /* tempo at which speed_base applies. 40..200,
                                   default 96.                                         */
    float bpm_forced;           /* >0 pins the tempo and stops detection entirely.
                                   0 = detect. 0..300, default 0.                      */
    float bpm_min;              /* lower clamp on a detected tempo. Default 60.        */
    float bpm_max;              /* upper clamp. Default 180.                           */
} TableGlowSettings;

extern TableGlowSettings tableglow;

/* Call once per frame where an AudioAssets is in scope (ui_main.c). Attaches this module's
   own music analyser to the playing match streams and detaches it when they stop. It does
   not touch the Action Card meter's analyser; raylib chains processors. */
void tableglow_sync_music(AudioAssets *audio);

/* Draw the light over one forged table's overlay texture. Call immediately after the
   overlay has been drawn and before the cards (ui_board.c). `overlay` is whichever
   texture that frame used, normal or CRT casing, so the light follows the bake.
   Does nothing when the glow is off, so it is safe to call unconditionally. */
void tableglow_draw_overlay(Texture2D overlay, float dt);

#endif /* TABLEGLOW_H */
