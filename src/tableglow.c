/* ============================================================================
   tableglow.c - the light that glides over the cards, on the table itself.

   READ THIS FIRST (it is the whole module in ten lines)
   -----------------------------------------------------
   A forged table draws its casing once, flat: assets/maps/<map>/overlay.png goes down as
   a single DrawTexture() and everything else is drawn over it. The cards on that table are
   different - board_draw_table_bezel_shine() in ui_board.c draws each bezel a second time
   through a fragment shader that reads the art's own brightness as a normal map and sweeps
   two point lights across it.

   This file is that trick, applied to the table. Same light model, same two orbits, but:
     - it samples the table overlay instead of a 95x136 bezel, so the tap radius is a
       setting in pixels rather than a fixed 1.45 texels;
     - the bezel shader's "solid interior" test (which keeps light off a transparent
       cavity) is useless on an opaque overlay, so the mask here is a brightness window:
       a pixel may catch light only if it is bright relative to its neighbourhood;
     - the three lights' drivers come from the music that is playing (see below).

   WHAT DRIVES IT  (three terms, no hidden adaptation)
   ---------------------------------------------------
     momentary LUFS  ->  brightness     (BS.1770-4: K-weighted, 400 ms, not integrated)
     BPM             ->  orbit speed     (onsets from the level envelope, median interval)
     high share      ->  bloom width     (one-pole split at 1.2 kHz, no FFT)

   No auto-gain, no normalisation, no reverb reading. Same audio in, same picture out.
   When no music is playing the light sits at brightness_base and bloom_base - it never
   goes black and never guesses.

   TUNING
   ------
   Every look knob is in the TableGlowSettings struct (see tableglow.h, each field has a
   comment and a sane range). The defaults are the TGLOW_DEF_* macros right below, so the
   header's comments and the numbers cannot drift apart. Rebuild to change them; F8 flips
   the effect on and off at runtime.

   Set TABLEGLOW_ENABLED to 0 to compile the whole thing out.

   THREADING
   ---------
   The analyser runs inside raylib's audio callback (its own processor on the music
   stream, chained after the game's reverb processor and the Action Card meter - raylib
   keeps a list of processors, it does not replace them). It publishes four integers
   through atomics and touches nothing else. All smoothing that affects the picture
   happens on the render side, so the callback stays a pure measurement.

   WHAT THE METER ACTUALLY HEARS  (stated plainly, because it is a real coupling)
   -----------------------------------------------------------------------------
   raylib appends processors and runs them in attach order (raudio.c). The reverb
   attaches when the music loads, this tap attaches later, so the samples measured here
   are the *audible mix*: the dry track plus the reverb return. Nothing in this file
   reads a reverb setting, calls a reverb function, or touches that processor - but the
   room/chorus sliders do move the measured loudness, tempo confidence and high share,
   and therefore the light. That is accepted on purpose: reacting to what the player
   hears beats reacting to a signal nobody hears, and the alternative (detaching the
   reverb, attaching this, re-attaching the reverb) would mean reaching into a module
   this one has no business touching. Same song + same settings still gives the same
   picture on every run: the measurement is a pure function of the audio it is handed.
   ========================================================================== */

#include "tableglow.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#define TABLEGLOW_ENABLED 1

/* ----------------------------------------------------------------------------
   DEFAULTS - one source of truth for the numbers documented in tableglow.h.
   -------------------------------------------------------------------------- */
#define TGLOW_DEF_ENABLED           1
#define TGLOW_DEF_RADIUS_PX         4.0f
#define TGLOW_DEF_CORE_UV          0.10f
#define TGLOW_DEF_BLOOM_UV         0.45f
#define TGLOW_DEF_BLACK_LO         0.015f
#define TGLOW_DEF_BLACK_HI         0.050f
#define TGLOW_DEF_LUM_LO            0.42f
#define TGLOW_DEF_LUM_HI            0.72f
#define TGLOW_DEF_MAX_ALPHA         0.88f
#define TGLOW_DEF_INTENSITY         1.00f
#define TGLOW_DEF_BRIGHT_BASE       0.55f
#define TGLOW_DEF_BRIGHT_PER_LUFS   0.45f
#define TGLOW_DEF_LUFS_FLOOR      (-40.0f)
#define TGLOW_DEF_LUFS_CEIL       (-10.0f)
#define TGLOW_DEF_LUFS_ATTACK_MS  120.0f
#define TGLOW_DEF_LUFS_RELEASE_MS 450.0f
#define TGLOW_DEF_BLOOM_BASE        0.85f
#define TGLOW_DEF_BLOOM_PER_HIGH    0.55f
#define TGLOW_DEF_HIGH_SMOOTH_MS  350.0f
#define TGLOW_DEF_SPEED_BASE        0.52f
#define TGLOW_DEF_BPM_REF          96.0f
#define TGLOW_DEF_BPM_FORCED        0.0f
#define TGLOW_DEF_BPM_MIN          60.0f
#define TGLOW_DEF_BPM_MAX         180.0f

TableGlowSettings tableglow = {
    .enabled             = TGLOW_DEF_ENABLED,
    .key_toggle          = KEY_F8,
    .radius_px           = TGLOW_DEF_RADIUS_PX,
    .core_uv             = TGLOW_DEF_CORE_UV,
    .bloom_uv            = TGLOW_DEF_BLOOM_UV,
    .luminance_lo        = TGLOW_DEF_LUM_LO,
    .luminance_hi        = TGLOW_DEF_LUM_HI,
    .black_lo            = TGLOW_DEF_BLACK_LO,
    .black_hi            = TGLOW_DEF_BLACK_HI,
    .max_alpha           = TGLOW_DEF_MAX_ALPHA,
    .intensity           = TGLOW_DEF_INTENSITY,
    .brightness_base     = TGLOW_DEF_BRIGHT_BASE,
    .brightness_per_lufs = TGLOW_DEF_BRIGHT_PER_LUFS,
    .lufs_floor_db       = TGLOW_DEF_LUFS_FLOOR,
    .lufs_ceil_db        = TGLOW_DEF_LUFS_CEIL,
    .lufs_attack_ms      = TGLOW_DEF_LUFS_ATTACK_MS,
    .lufs_release_ms     = TGLOW_DEF_LUFS_RELEASE_MS,
    .bloom_base          = TGLOW_DEF_BLOOM_BASE,
    .bloom_per_high      = TGLOW_DEF_BLOOM_PER_HIGH,
    .high_smooth_ms      = TGLOW_DEF_HIGH_SMOOTH_MS,
    .speed_base          = TGLOW_DEF_SPEED_BASE,
    .bpm_ref             = TGLOW_DEF_BPM_REF,
    .bpm_forced          = TGLOW_DEF_BPM_FORCED,
    .bpm_min             = TGLOW_DEF_BPM_MIN,
    .bpm_max             = TGLOW_DEF_BPM_MAX
};

/* ============================================================================
   ANALYSIS  (pure C: no raylib, no settings, no screen)
   ----------------------------------------------------------------------------
   These constants define the *measurement*, not the look. They are fixed on purpose:
   changing them changes what the meter reads, not what the picture does with it.
   ========================================================================== */
/* ---- ANALYSIS BEGIN ---- */
#define TG_PI            3.14159265358979323846
#define TG_EPS           1e-12
#define TG_LUFS_BLOCKS   4      /* 4 x 100 ms = the 400 ms momentary window of BS.1770 */
#define TG_BPM_IVS       8      /* onset intervals kept for the tempo estimate          */
#define TG_ONSET_GAP     0.18f  /* refractory window: at most ~333 onsets per minute     */
#define TG_ONSET_FIRE    0.60f  /* an onset is a hit at 60% of the recent loudness peak  */
#define TG_ONSET_REARM   0.30f  /* ... and the next one needs the level back down to 30% */
#define TG_PEAK_TAU_S    1.0f   /* how fast "recent loudness" forgets                    */
#define TG_ENV_ON_MS     8.0f   /* fast envelope: 8 ms attack ...                        */
#define TG_ENV_OFF_MS   60.0f   /* ... 60 ms release                                     */
#define TG_SPLIT_HZ   1200.0f   /* low/high split for the bloom term                     */
#define TG_BPM_LO       40.0f   /* sanity clamp before the owner's bpm_min/bpm_max       */
#define TG_BPM_HI      240.0f

typedef struct { float b0, b1, b2, a1, a2; float x1, x2, y1, y2; } TgBiquad;

static double s_fs = 48000.0;         /* device rate, read from the stream at attach   */
static int    s_block_len = 4800;     /* samples per 100 ms block                      */

static int    s_sq_n;                 /* samples accumulated in the current block      */
static double s_sq_acc;               /* K-weighted squared sum, both channels         */
static double s_block_ms[TG_LUFS_BLOCKS];
static int    s_block_w, s_block_filled;

static TgBiquad s_shelf_l, s_shelf_r, s_hp_l, s_hp_r;

static float  s_fast_env;      /* short-window level                          */
static float  s_peak_hold;     /* recent loudness: decays over TG_PEAK_TAU_S   */
static double s_clock;                /* seconds of audio seen, not wall time          */
static double s_last_onset;
static float  s_iv[TG_BPM_IVS];
static int    s_iv_n, s_iv_w;
static float  s_bpm_now;
static int    s_onset_armed;   /* one onset per rise: see TG_ONSET_REARM */

static float  s_lp_mono;
static double s_hi_acc, s_all_acc;

static float  s_env_att, s_env_rel, s_peak_decay, s_lp_a;

/* Published to the render thread. One integer each, scaled to keep the float precision
   that matters: LUFS as (db + 70) * 100, share as 0..10000, BPM as bpm * 100. */
static unsigned int s_pub_lufs, s_pub_high, s_pub_bpm, s_pub_live;
static unsigned int s_epoch, s_epoch_seen;

static void tg_bq_reset(TgBiquad *f) {
    f->x1 = f->x2 = f->y1 = f->y2 = 0.0f;
}

static float tg_bq_run(TgBiquad *f, float x) {
    float y = f->b0 * x + f->b1 * f->x1 + f->b2 * f->x2 - f->a1 * f->y1 - f->a2 * f->y2;
    f->x2 = f->x1; f->x1 = x;
    f->y2 = f->y1; f->y1 = y;
    return y;
}

/* K-weighting stage 1: high shelf, +4 dB above ~1.7 kHz. This is BS.1770-4's own
   design (the one that reproduces the published 48 kHz table exactly), not a generic RBJ
   shelf: a cookbook shelf with the same f0/gain/Q lands 0.26 dB low at 1 kHz, which the
   gated calibration in /tmp showed as a constant offset on every level. */
static void tg_design_shelf(TgBiquad *f, double fs) {
    const double f0 = 1681.974450955533;
    const double gain_db = 3.999843853973347;
    const double q = 0.7071752369554196;
    double K  = tan(TG_PI * f0 / fs);
    double Vh = pow(10.0, gain_db / 20.0);
    double Vb = pow(Vh, 0.4996667741545416);
    double a0 = 1.0 + K / q + K * K;
    f->b0 = (float)((Vh + Vb * K / q + K * K) / a0);
    f->b1 = (float)(2.0 * (K * K - Vh) / a0);
    f->b2 = (float)((Vh - Vb * K / q + K * K) / a0);
    f->a1 = (float)(2.0 * (K * K - 1.0) / a0);
    f->a2 = (float)((1.0 - K / q + K * K) / a0);
}

/* K-weighting stage 2: high pass at ~38 Hz. As in the standard (and the published table),
   the numerator stays 1, -2, 1 - only the denominator is normalised. */
static void tg_design_shp(TgBiquad *f, double fs) {
    const double f0 = 38.13547087602444;
    const double q = 0.5003270373238773;
    double K  = tan(TG_PI * f0 / fs);
    double a0 = 1.0 + K / q + K * K;
    f->b0 = 1.0f; f->b1 = -2.0f; f->b2 = 1.0f;
    f->a1 = (float)(2.0 * (K * K - 1.0) / a0);
    f->a2 = (float)((1.0 - K / q + K * K) / a0);
}

static float tg_clampf(float x, float lo, float hi) {
    return (x < lo) ? lo : (x > hi) ? hi : x;
}

/* Median of the intervals collected so far, or the last good tempo when the playing is
   too irregular to call. Sorting a copy keeps the ring order irrelevant. */
static float tg_bpm_from_intervals(void) {
    if (s_iv_n < 3) return s_bpm_now;
    float tmp[TG_BPM_IVS];
    memcpy(tmp, s_iv, sizeof(float) * (size_t)s_iv_n);
    for (int i = 1; i < s_iv_n; i++) {
        float key = tmp[i];
        int j = i - 1;
        while (j >= 0 && tmp[j] > key) { tmp[j + 1] = tmp[j]; j--; }
        tmp[j + 1] = key;
    }
    float lo = tmp[0], hi = tmp[s_iv_n - 1];
    if (lo <= 0.0f || hi > lo * 1.45f) return s_bpm_now;   /* irregular: do not chase it */
    float bpm = 60.0f / tmp[s_iv_n / 2];
    return tg_clampf(bpm, TG_BPM_LO, TG_BPM_HI);
}

static void tg_analysis_reset(void) {
    tg_bq_reset(&s_shelf_l); tg_bq_reset(&s_shelf_r);
    tg_bq_reset(&s_hp_l);    tg_bq_reset(&s_hp_r);
    s_sq_n = 0; s_sq_acc = 0.0;
    s_block_w = 0; s_block_filled = 0;
    for (int i = 0; i < TG_LUFS_BLOCKS; i++) s_block_ms[i] = 0.0;
    s_fast_env = 0.0f; s_peak_hold = 0.0f;
    s_clock = 0.0; s_last_onset = -1.0;
    s_iv_n = 0; s_iv_w = 0; s_bpm_now = 0.0f;
    s_onset_armed = 1;
    s_lp_mono = 0.0f;
    s_hi_acc = s_all_acc = 0.0;
}

static void tg_analysis_setup(double fs) {
    if (fs < 8000.0) fs = 48000.0;          /* a broken rate must not poison the maths */
    s_fs = fs;
    s_block_len = (int)(fs / 10.0);         /* 100 ms */
    if (s_block_len < 1) s_block_len = 1;
    tg_design_shelf(&s_shelf_l, fs); tg_design_shelf(&s_shelf_r, fs);
    tg_design_shp(&s_hp_l, fs);      tg_design_shp(&s_hp_r, fs);
    s_env_att   = (float)(1.0 - exp(-1.0 / (TG_ENV_ON_MS  * 0.001 * fs)));
    s_env_rel   = (float)(1.0 - exp(-1.0 / (TG_ENV_OFF_MS * 0.001 * fs)));
    s_peak_decay = (float)(1.0 - exp(-1.0 / (TG_PEAK_TAU_S * fs)));
    s_lp_a     = (float)(1.0 - exp(-2.0 * TG_PI * TG_SPLIT_HZ / fs));
    tg_analysis_reset();
}

static void tg_publish_block(void) {
    /* Momentary loudness: mean square over the filled blocks, both channels summed. */
    if (s_block_filled <= 0) return;
    double ms = 0.0;
    for (int i = 0; i < s_block_filled; i++) ms += s_block_ms[i];
    ms /= (double)s_block_filled;
    double lufs = -0.691 + 10.0 * log10(ms + TG_EPS);
    int enc = (int)((lufs + 70.0) * 100.0);
    if (enc < 0) enc = 0;
    if (enc > 7000) enc = 7000;
    __atomic_store_n(&s_pub_lufs, (unsigned int)enc, __ATOMIC_RELAXED);

    /* High share: how much of the signal's energy sits above the split. */
    float share = 0.0f;
    if (s_all_acc > TG_EPS) {
        share = (float)(sqrt(s_hi_acc) / sqrt(s_all_acc));
    }
    share = tg_clampf(share, 0.0f, 1.0f);
    __atomic_store_n(&s_pub_high, (unsigned int)(share * 10000.0f + 0.5f), __ATOMIC_RELAXED);
    s_hi_acc = s_all_acc = 0.0;

    /* Tempo. 0 means "not confident yet"; the render side falls back to bpm_ref. */
    float bpm = tg_bpm_from_intervals();
    __atomic_store_n(&s_pub_bpm, (unsigned int)(bpm * 100.0f + 0.5f), __ATOMIC_RELAXED);
    __atomic_store_n(&s_pub_live, 1u, __ATOMIC_RELAXED);
}

/* The audio callback. Stereo interleaved float, exactly what raylib hands its processors.
   Measurement only: no settings, no allocation, no locks, nothing that draws. */
static void tg_music_processor(void *buffer, unsigned int frames) {
    const float *pcm = (const float *)buffer;
    if (!pcm || frames == 0) return;

    unsigned int ep = __atomic_load_n(&s_epoch, __ATOMIC_RELAXED);
    if (ep != s_epoch_seen) { tg_analysis_reset(); s_epoch_seen = ep; }

    for (unsigned int i = 0; i < frames; i++) {
        float l = pcm[i * 2 + 0];
        float r = pcm[i * 2 + 1];
        if (!(l == l) || l > 8.0f || l < -8.0f) l = 0.0f;   /* NaN / stuck sample guard */
        if (!(r == r) || r > 8.0f || r < -8.0f) r = 0.0f;

        /* K-weighting, then energy. Both channels weigh 1.0 (BS.1770 stereo). */
        float wl = tg_bq_run(&s_hp_l, tg_bq_run(&s_shelf_l, l));
        float wr = tg_bq_run(&s_hp_r, tg_bq_run(&s_shelf_r, r));
        s_sq_acc += (double)wl * wl + (double)wr * wr;

        /* Onset detection on the plain mono mix: the beat lives in the low end. The
           reference is the recent loudness peak, not a mean - a mean lags on a decaying
           hit and latching a tail read every fast track as half its tempo. */
        float mono = 0.5f * (l + r);
        float am = fabsf(mono);
        s_fast_env += (am - s_fast_env) * (am > s_fast_env ? s_env_att : s_env_rel);
        if (s_fast_env > s_peak_hold) s_peak_hold = s_fast_env;
        else s_peak_hold -= s_peak_hold * s_peak_decay;

        /* High share: energy above the split, against the whole. */
        s_lp_mono += (mono - s_lp_mono) * s_lp_a;
        float hi = mono - s_lp_mono;
        s_hi_acc += (double)hi * hi;
        s_all_acc += (double)mono * mono;

        s_clock += 1.0 / s_fs;
        if (!s_onset_armed) {
            /* Wait for the level to come back down before listening for the next hit. */
            if (s_fast_env < s_peak_hold * TG_ONSET_REARM) s_onset_armed = 1;
        } else if (s_fast_env > 0.01f && s_fast_env > s_peak_hold * TG_ONSET_FIRE &&
                   (s_last_onset < 0.0 || (s_clock - s_last_onset) > TG_ONSET_GAP)) {
            if (s_last_onset >= 0.0) {
                s_iv[s_iv_w] = (float)(s_clock - s_last_onset);
                s_iv_w = (s_iv_w + 1) % TG_BPM_IVS;
                if (s_iv_n < TG_BPM_IVS) s_iv_n++;
            }
            s_last_onset = s_clock;
            s_onset_armed = 0;
        }

        if (++s_sq_n >= s_block_len) {
            s_block_ms[s_block_w] = s_sq_acc / (double)s_block_len;
            s_block_w = (s_block_w + 1) % TG_LUFS_BLOCKS;
            if (s_block_filled < TG_LUFS_BLOCKS) s_block_filled++;
            s_sq_n = 0;
            s_sq_acc = 0.0;
            tg_publish_block();
        }
    }
}
/* ---- ANALYSIS END ---- */

/* ============================================================================
   THE SHADER
   ----------------------------------------------------------------------------
   Same light model as the card bezel shine (ui_board.c, BEZEL_SHINE_BODY): read the
   brightness of a pixel and its four neighbours, turn the gradient into a normal, then
   light it with two orbiting point lights - a tight core and a soft bloom. Differences
   from the card version, all deliberate:

     - taps are clamped to the texture, so the edges of the art cannot wrap;
     - the same 8-neighbour opacity test as the card, for the same reason. Measured on
       the shipped art (the overlay.png of all seven maps): 69-76% of it is FULLY
       transparent and 1-10% is semi-transparent, so the light is kept inside the painted
       casing exactly as it is kept out of a card cavity;
     - but the ridge window is a GAIN here, not a gate, and the near-black floor is
       lowered to suit the art (u_shape.z/.w). Both because a table is not a bezel:
       measured on the opaque body, median brightness is 0.032 (realm), 0.042 (harvest),
       0.077 (city), 0.084 (castle), and the card's 0.08 floor threw away 46-83% of it,
       while its ridge gate left whole cells of the table with no light at all;
     - bloom is WIDER than the core here and its radius is scaled by u_tune.y. On the
       card the bloom lobe is narrower than the core, which reads as a tight halo on a
       90 px bezel and as nothing at all across 1280 px;
     - the head radius and the bloom radius are the owner's (core_uv, bloom_uv),
       handed over squared in u_shape.xy;
     - the final alpha is multiplied by the vertex alpha, which is max_alpha.

   Two sources: GL 3.3 for desktop and GLES2 for the browser build.
   ========================================================================== */
#define TGLOW_SHADER_BODY(TEX, OUT)                                                         \
    "float bval(vec3 v) { return 0.45 * dot(v, vec3(0.299, 0.587, 0.114)) + 0.55 * max(max(v.r, v.g), v.b); }\n" \
    "vec2 uvc(vec2 uv) { return clamp(uv, vec2(0.0), vec2(1.0)); }\n"                       \
    "void main() {\n"                                                                       \
    "    vec2 uv = uvc(fragTexCoord);\n"                                                    \
    "    vec2 d  = u_phase.zw;\n"                                                           \
    "    vec4 c  = " TEX "(texture0, uv);\n"                                                \
    "    vec4 cl = " TEX "(texture0, uvc(uv - vec2(d.x, 0.0)));\n"                          \
    "    vec4 cr = " TEX "(texture0, uvc(uv + vec2(d.x, 0.0)));\n"                          \
    "    vec4 cu = " TEX "(texture0, uvc(uv - vec2(0.0, d.y)));\n"                          \
    "    vec4 cd = " TEX "(texture0, uvc(uv + vec2(0.0, d.y)));\n"                          \
    "    float diag_a = min(min(" TEX "(texture0, uvc(uv - d)).a,\n"                        \
    "                           " TEX "(texture0, uvc(uv + vec2(d.x, -d.y))).a),\n"         \
    "                       min(" TEX "(texture0, uvc(uv + vec2(-d.x, d.y))).a,\n"          \
    "                           " TEX "(texture0, uvc(uv + d)).a));\n"                      \
    "    float min8 = min(min(min(cl.a, cr.a), min(cu.a, cd.a)), diag_a);\n"                \
    "    float hc = bval(c.rgb);\n"                                                         \
    "    float hl = bval(cl.rgb), hr = bval(cr.rgb), hu = bval(cu.rgb), hd = bval(cd.rgb);\n" \
    "    float peak = max(max(max(hl, hr), max(hu, hd)), 0.12);\n"                          \
    "    float solid = smoothstep(0.91, 0.98, c.a) * smoothstep(0.85, 0.95, min8);\n"       \
    "    float lit   = solid * smoothstep(u_shape.z, u_shape.w, hc);\n"                     \
    "    float hi_edge = max(u_tune.w, u_tune.z + 0.001);\n"                                \
    "    float ridge = smoothstep(u_tune.z, hi_edge, hc / peak);\n"                         \
    "    float gain  = 0.35 + 0.65 * ridge;\n"                                              \
    "    vec2 g = vec2(hl - hr, hu - hd) / peak;\n"                                         \
    "    vec3 n = normalize(vec3(g * 2.8, 1.0));\n"                                         \
    "    vec2 t1 = (vec2(0.5 + 0.44 * cos(u_phase.x), 0.5 + 0.45 * sin(u_phase.x)) - uv) * vec2(0.70, 1.0);\n" \
    "    float d1 = dot(t1, t1);\n"                                                         \
    "    vec3 h1 = normalize(normalize(vec3(t1, 0.30)) + vec3(0.0, 0.0, 1.0));\n"           \
    "    float nd1 = max(dot(n, h1), 0.0);\n"                                               \
    "    vec2 t2 = (vec2(0.5 + 0.44 * cos(u_phase.y), 0.5 + 0.45 * sin(u_phase.y * 2.0)) - uv) * vec2(0.70, 1.0);\n" \
    "    float d2 = dot(t2, t2);\n"                                                         \
    "    vec3 h2 = normalize(normalize(vec3(t2, 0.30)) + vec3(0.0, 0.0, 1.0));\n"           \
    "    float nd2 = max(dot(n, h2), 0.0);\n"                                               \
    "    float str = fragColor.a * colDiffuse.a;\n"                                         \
    "    float bw  = max(u_tune.y, 0.05);\n"                                                \
    "    float s1 = 0.30 + 0.70 * pow(nd1, 4.5);\n"                                         \
    "    float s2 = 0.30 + 0.70 * pow(nd2, 4.5);\n"                                         \
    "    float core  = lit * gain * (exp(-d1 / u_shape.x) * s1 + exp(-d2 / u_shape.x) * s2) * 0.55;\n" \
    "    float bloom = lit * gain * (exp(-d1 / (u_shape.y * bw)) + exp(-d2 / (u_shape.y * bw))) * 0.35;\n" \
    "    float raw = (core + bloom) * u_tune.x;\n"                                          \
    "    float a = (1.0 - exp(-raw * 1.65)) * str;\n"                                       \
    "    " OUT " = vec4(fragColor.rgb, a);\n"                                               \
    "}\n"                                                                                   \

#ifdef __EMSCRIPTEN__
static const char *const k_tableglow_fs =
    "#version 100\n"
    "precision mediump float;\n"
    "varying vec2 fragTexCoord;\n"
    "varying vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "uniform vec4 u_phase;\n"
    "uniform vec4 u_tune;\n"
    "uniform vec4 u_shape;\n"
    TGLOW_SHADER_BODY("texture2D", "gl_FragColor");
#else
static const char *const k_tableglow_fs =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "uniform vec4 u_phase;\n"
    "uniform vec4 u_tune;\n"
    "uniform vec4 u_shape;\n"
    "out vec4 finalColor;\n"
    TGLOW_SHADER_BODY("texture", "finalColor");
#endif
#undef TGLOW_SHADER_BODY

/* ============================================================================
   RENDER SIDE
   ========================================================================== */
static Shader s_shader = { 0 };
static int    s_shader_tried;
static int    s_phase_loc = -1;
static int    s_tune_loc = -1;
static int    s_shape_loc = -1;
static float  s_phase_a, s_phase_b;
static float  s_bright_vis = -1.0f;      /* <0 = not seeded yet, snap on first frame */
static float  s_bloom_vis = -1.0f;

static void tg_shader_init(void) {
    if (s_shader_tried) return;
    s_shader_tried = 1;
    s_shader = LoadShaderFromMemory(NULL, k_tableglow_fs);
    if (s_shader.id > 0) {
        s_phase_loc = GetShaderLocation(s_shader, "u_phase");
        s_tune_loc  = GetShaderLocation(s_shader, "u_tune");
        s_shape_loc = GetShaderLocation(s_shader, "u_shape");
        TraceLog(LOG_INFO, "tableglow: linked - %s", TABLEGLOW_ENABLED ? "on" : "off");
    } else {
        TraceLog(LOG_WARNING, "tableglow: shader failed to load, table light disabled");
    }
}

/* One-pole approach with the owner's time constant. dt_ms <= 0 snaps. */
static float tg_approach(float now, float target, float tau_ms, float dt) {
    if (tau_ms <= 0.0f || dt <= 0.0f) return target;
    float k = 1.0f - expf(-(dt * 1000.0f) / tau_ms);
    return now + (target - now) * k;
}

void tableglow_draw_overlay(Texture2D overlay, float dt) {
#if !TABLEGLOW_ENABLED
    (void)overlay; (void)dt;
#else
    /* F8 works in both directions, so it is checked before the enabled guard. */
    if (IsKeyPressed(tableglow.key_toggle)) {
        tableglow.enabled = tableglow.enabled ? 0 : 1;
        TraceLog(LOG_INFO, "tableglow: %s", tableglow.enabled ? "on" : "off");
    }
    if (!tableglow.enabled) return;
    if (overlay.id <= 0 || overlay.width <= 0 || overlay.height <= 0) return;

    tg_shader_init();
    if (s_shader.id <= 0) return;

    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.05f) dt = 0.05f;

    /* ---- read the meters ---------------------------------------------------- */
    int live = (int)__atomic_load_n(&s_pub_live, __ATOMIC_RELAXED);
    float lufs  = (float)__atomic_load_n(&s_pub_lufs, __ATOMIC_RELAXED) / 100.0f - 70.0f;
    float share = (float)__atomic_load_n(&s_pub_high, __ATOMIC_RELAXED) / 10000.0f;
    float bpm_p = (float)__atomic_load_n(&s_pub_bpm, __ATOMIC_RELAXED) / 100.0f;
    if (!live) { lufs = tableglow.lufs_floor_db; share = 0.0f; }

    /* ---- loudness -> brightness --------------------------------------------- */
    float span = tableglow.lufs_ceil_db - tableglow.lufs_floor_db;
    float x = (span > 0.001f) ? (lufs - tableglow.lufs_floor_db) / span : 0.0f;
    x = tg_clampf(x, 0.0f, 1.0f);
    float bright_t = (tableglow.brightness_base + tableglow.brightness_per_lufs * x)
                     * tg_clampf(tableglow.intensity, 0.0f, 4.0f);
    bright_t = tg_clampf(bright_t, 0.0f, 1.0f);
    if (s_bright_vis < 0.0f) s_bright_vis = bright_t;
    else s_bright_vis = tg_approach(s_bright_vis, bright_t,
                                    (bright_t > s_bright_vis) ? tableglow.lufs_attack_ms
                                                              : tableglow.lufs_release_ms, dt);

    /* ---- high share -> bloom ------------------------------------------------ */
    float bloom_t = tableglow.bloom_base + tableglow.bloom_per_high * share;
    bloom_t = tg_clampf(bloom_t, 0.05f, 3.0f);
    if (s_bloom_vis < 0.0f) s_bloom_vis = bloom_t;
    else s_bloom_vis = tg_approach(s_bloom_vis, bloom_t, tableglow.high_smooth_ms, dt);

    /* ---- tempo -> orbit speed ----------------------------------------------- */
    float bpm = (tableglow.bpm_forced > 0.0f) ? tableglow.bpm_forced
               : (bpm_p > 0.0f) ? bpm_p : tableglow.bpm_ref;
    bpm = tg_clampf(bpm, tableglow.bpm_min, tableglow.bpm_max);
    float ref = (tableglow.bpm_ref > 1.0f) ? tableglow.bpm_ref : 96.0f;
    float rate = tableglow.speed_base * (bpm / ref);          /* rad/s on the light circle */
    s_phase_a = fmodf(s_phase_a + dt * rate, 6.28318530718f);
    s_phase_b = fmodf(s_phase_b - dt * rate * 0.7115f, 6.28318530718f);   /* the cards' 0.52 : 0.37 */

    /* ---- uniforms ------------------------------------------------------------ */
    float lo = tg_clampf(tableglow.luminance_lo, 0.0f, 1.0f);
    float hi = tg_clampf(tableglow.luminance_hi, 0.0f, 1.0f);
    if (hi <= lo) hi = lo + 0.001f;
    float phase[4] = { s_phase_a, s_phase_b,
                       tableglow.radius_px / (float)overlay.width,
                       tableglow.radius_px / (float)overlay.height };
    float tune[4]  = { s_bright_vis, s_bloom_vis, lo, hi };

    /* Radii are handed to the shader squared, because d1/d2 are squared UV distances:
       exp(-d/core_uv^2) is down to 1/e at exactly core_uv away from the head.
       z/w are the near-black floor, which is art-specific (see tableglow.h). */
    float core_uv  = tg_clampf(tableglow.core_uv,  0.02f, 0.60f);
    float bloom_uv = tg_clampf(tableglow.bloom_uv, 0.05f, 1.20f);
    float black_lo = tg_clampf(tableglow.black_lo, 0.0f, 0.20f);
    float black_hi = tg_clampf(tableglow.black_hi, 0.0f, 0.30f);
    if (black_hi <= black_lo) black_hi = black_lo + 0.001f;
    float shape[4] = { core_uv * core_uv, bloom_uv * bloom_uv, black_lo, black_hi };

    if (s_phase_loc >= 0) SetShaderValue(s_shader, s_phase_loc, phase, SHADER_UNIFORM_VEC4);
    if (s_tune_loc  >= 0) SetShaderValue(s_shader, s_tune_loc,  tune,  SHADER_UNIFORM_VEC4);
    if (s_shape_loc >= 0) SetShaderValue(s_shader, s_shape_loc, shape, SHADER_UNIFORM_VEC4);

    Color tint = { 250, 246, 238,
                   (unsigned char)(tg_clampf(tableglow.max_alpha, 0.0f, 1.0f) * 255.0f) };

    /* ---- draw --------------------------------------------------------------- */
    Rectangle src = { 0, 0, (float)overlay.width, (float)overlay.height };
    Rectangle dst = { 0, 0, 1280.0f, 720.0f };
    BeginBlendMode(BLEND_ADDITIVE);
    BeginShaderMode(s_shader);
    DrawTexturePro(overlay, src, dst, (Vector2){ 0, 0 }, 0.0f, tint);
    EndShaderMode();
    EndBlendMode();
#endif
}

/* ============================================================================
   MUSIC TAP
   ----------------------------------------------------------------------------
   raylib 5.5 keeps processors in a list (raudio.c: AttachAudioStreamProcessor appends),
   so this one lives beside the game's reverb processor and the Action Card meter. It
   detaches the same way, by function pointer. Nothing here reads or changes the mode,
   and nothing here detaches or reorders anybody else's processor - see "WHAT THE METER
   ACTUALLY HEARS" at the top of this file for the consequence of running last.
   ========================================================================== */
static int s_attached;
static unsigned int s_tap_sig;

static unsigned int tg_stream_signature(const AudioAssets *a) {
    unsigned int sig = 0u;
    for (int i = 0; i < AUDIO_MAX_TRACKS; i++) {
        if (!a->has_music_game[i]) continue;
        uintptr_t p = (uintptr_t)a->music_game[i].stream.buffer;
        sig ^= (unsigned int)(p >> 4) + (unsigned int)i * 2654435761u;
    }
    return sig;
}

static void tg_set_tap(AudioAssets *a, int on) {
    if (!a) return;
    unsigned int fs = 48000u;
    for (int i = 0; i < AUDIO_MAX_TRACKS; i++) {
        if (!a->has_music_game[i]) continue;
        if (a->music_game[i].stream.sampleRate > 0) fs = a->music_game[i].stream.sampleRate;
        if (on) AttachAudioStreamProcessor(a->music_game[i].stream, tg_music_processor);
        else    DetachAudioStreamProcessor(a->music_game[i].stream, tg_music_processor);
    }
    if (on) {
        tg_analysis_setup((double)fs);      /* K-weighting for the real device rate */
        __atomic_add_fetch(&s_epoch, 1u, __ATOMIC_RELAXED);
        s_epoch_seen = __atomic_load_n(&s_epoch, __ATOMIC_RELAXED) - 1u;  /* force reset */
    } else {
        __atomic_store_n(&s_pub_live, 0u, __ATOMIC_RELAXED);
        __atomic_store_n(&s_pub_lufs, 0u, __ATOMIC_RELAXED);
        __atomic_store_n(&s_pub_high, 0u, __ATOMIC_RELAXED);
    }
    s_attached = on;
}

void tableglow_sync_music(AudioAssets *audio) {
    if (!audio) return;
    int want = (audio->current_music == 2 && audio->game_track_count > 0);
    unsigned int sig = want ? tg_stream_signature(audio) : 0u;

    /* A custom music folder unloads and reloads the streams, which destroys any attached
       processor with them; the signature changes and we re-attach. */
    if (s_attached && (!want || sig != s_tap_sig)) tg_set_tap(audio, 0);
    if (want && !s_attached) tg_set_tap(audio, 1);
    s_tap_sig = sig;
}
