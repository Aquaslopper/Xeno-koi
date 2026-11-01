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
    int   key_debug;            /* key that shows the live numbers (LUFS, high share,
                                   BPM, final alpha) in the top-right corner. KEY_F7.
                                   Off by default; with it off nothing is drawn. This is
                                   how you tell "the meter is dead" from "the mapping is
                                   too timid" without guessing.                          */

    /* ---- the light itself ---------------------------------------------------- */
    float radius_px;            /* reach of the brightness gradient, in canvas pixels.
                                   2..40, default 12. Only the gradient now: the light is
                                   gated on the centre pixel's own alpha, not on its eight
                                   neighbours, so this no longer doubles as an opacity
                                   skirt. 12 px reads a moulding as a slope instead of as
                                   a 4 px speckle; the card uses 1.45 texels because a
                                   card is 90 px wide, this is 1280.                     */
    float core_gain;            /* strength of the sharp head. 0.02..2, default 0.20.  */
    float bloom_gain;           /* strength of the halo. 0..2, default 0.10.           */
    float core_uv;              /* radius of the sharp head's falloff, in texture UV.
                                   0.02..0.60, default 0.10 (= 128 px at 1280 wide).      */
    float bloom_uv;             /* radius of the broad halo around each head, in UV.
                                   0.05..1.20, default 0.45 (= 576 px). It must stay
                                   well above core_uv or the bloom is invisible: the card
                                   has bloom NARROWER than its core, which reads as a
                                   tight halo on a 90 px bezel and as nothing at all on
                                   a 1280 px table.                                     */
    float luminance_lo;         /* ABSOLUTE brightness where the ridge gain starts.
                                   0..0.5, default 0.050. This used to be a share of the
                                   neighbourhood's brightest pixel; on dark painted art that
                                   test is dominated by the divisor's floor, so flat panels
                                   scored higher than real mouldings and the light came out
                                   as speckle with no direction. Absolute brightness cannot
                                   invert like that: it is the art's own exposure.          */
    float luminance_hi;         /* where that gain reaches full strength. 0..1, default
                                   0.300. Keep it above luminance_lo.                     */
    float black_lo;             /* near-black floor: brightness at or below this gets no
                                   light at all. 0..0.2, default 0.015.
                                   The card uses 0.08, which is right for a bezel and
                                   wrong here: measured on the shipped overlays the
                                   OPAQUE table body has a median brightness of 0.03-0.08
                                   (realm 0.032, harvest 0.042, city 0.077, castle 0.084),
                                   so the card's floor threw away 46-83% of the table.   */
    float black_hi;             /* where that floor reaches full light. Must be above
                                   black_lo. Default 0.050.                              */
    float max_alpha;            /* ceiling of the additive pass. 0..1, default 0.35.
                                   This is the single "how strong" knob. It was 0.88,
                                   which on a dark painted table washed the score out.  */
    float intensity;            /* multiplier on top of the loudness term. 0..2,
                                   default 1.00. Use it to audition 0.5 quickly.        */

    /* ---- loudness -> brightness ---------------------------------------------- */
    float brightness_base;      /* glow with no music at all. 0..1, default 0.08.
                                   Keep this low: it is the part of the light the music
                                   does NOT move, and a high base is what makes an effect
                                   look like it ignores the music.                       */
    float brightness_per_lufs;  /* how much loudness adds at lufs_ceil_db. 0..1,
                                   default 0.92. brightness_base + this = 1.0 at the
                                   ceiling, so the whole range is used: 0.08 when the
                                   passage is at lufs_floor_db, 1.00 at lufs_ceil_db.    */
    float lufs_floor_db;        /* loudness mapped to zero: -70..0, default -32.
                                   A real track's momentary loudness swings inside a few
                                   dB. The old -40..-10 range squeezed a whole song into
                                   the top fifth of the mapping, which is why the light
                                   looked like it ignored the music.                     */
    float lufs_ceil_db;         /* loudness mapped to full: -40..0, default -10.
                                   Set the floor ~4 dB under your quietest passage and the
                                   ceiling ~2 dB over your loudest, and press F7 to watch
                                   the number while the track plays.                     */
    float lufs_attack_ms;       /* how fast brightness rises. 10..2000, default 120.   */
    float lufs_release_ms;      /* how slow it falls. 10..2000, default 450. Slower
                                   release = more breathing, less flicker.             */

    /* ---- high share -> bloom -------------------------------------------------- */
    float bloom_base;           /* halo RADIUS multiplier for a dark tone, applied to
                                   bloom_uv. 0.2..2, default 0.45,
                                                                                        */
    float bloom_per_high;       /* added to that when the high band is all there is,
                                   0..1.5, default 1.20 - so the halo radius runs from
                                   0.45x to 1.65x across a track's high end, instead of
                                   moving by a few percent and looking inert.           */
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
