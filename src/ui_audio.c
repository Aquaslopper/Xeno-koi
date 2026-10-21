#include "ui_audio.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ======================================================================
   Clean 3-stage studio signal chain on Raylib's mixer:

   1. SFX Bus (all 7 SFX streams loaded FIRST -> processed BEFORE music in
      every device callback with zero block latency):
        sfx_tap_N() feeds s_sc_bus[] without altering any SFX waveform.

   2. Music Bus (driven by 550 ms Tide swell -> -20% shoulder -> smooth tail):
        Stage 0: Bit-exact dry bypass (0 / 0 / 0)
        Stage 1: Wide Stereo ONLY            (Stereo 60%, Reverb  0%, Bass  0%)
        Stage 2: Wide Stereo + Reverb        (Stereo 70%, Reverb 50%, Bass  0%)
        Stage 3: Wide Stereo + Reverb + Bass (Stereo 70%, Reverb 70%, Bass 50%)

      Three clean, non-interfering textbook processors:
        [A] Minimum-Phase RBJ M/S High-Shelf Widener (> 500 Hz on Side, plus
            24 dB/oct 200 Hz Side safety cut & subtle high-Mid space tuck):
              Zero delay lines, zero static-allpass comb phasing, zero reverb
              tail on Stage 1, zero low-frequency phase cancellation.

        [B] Abbey Road Stereo Reverb (kick-free space ONLY):
              Pre-tank: 24 dB/oct Butterworth HP @ 550 Hz (ZERO kick in tank)
                -> stereo-linked split de-esser @ 1.2 kHz (-18 dBFS, 3:1,
                   zero post-boost)
                -> 12 dB/oct Butterworth LP @ 8.5 kHz
              -> 25 ms pre-delay -> 4 damped stereo combs -> 2 stereo allpasses
              -> post-tank 500 Hz HP on wet Side.

        [C] Minimum-Phase RBJ Mono Sub/Kick EQ + Slow Glue Comp (Mid ONLY):
              38 Hz sub-mud high-pass + wide RBJ bell boost centered at 82 Hz
              (Q = 0.62: slopes smoothly from 100->40 Hz, cuts <35 Hz mud,
              delivers real +6..+7.5 dB punch across 50-130 Hz with zero phase
              cancellation) + 35 ms slow-attack 2:1 low-band glue compressor.

   3. Music Group Sidechain & Master Bus:
      - Full music group ducks under ALL live SFX streams (3 ms atk / 85 ms rel).
      - 160 Hz HPF detector spike compressor (-1 dBFS, 2:1) + 0.95/0.99 ceiling.
   ====================================================================== */

#define AUDIO_SR          44100.0f
#define PI_F              3.14159265f

/* ---- tunables -------------------------------------------------------- */
#define SIDE_SHELF_HZ      500.0f   /* RBJ M/S High-Shelf stereo corner       */
#define SIDE_MAX_DB         10.0f   /* +6.0 dB at 60% (Stg1), +7.0 dB at 70%  */
#define SIDE_MONO_SAFE_HZ  130.0f   /* 24 dB/oct sub-bass mono lock on Side   */
#define MID_SPACE_TUCK_DB   -1.4f   /* subtle >600 Hz center pocket           */

#define REV_IN_HP_HZ       550.0f   /* 24 dB/oct pre-tank HP: ZERO kick in rev*/
#define REV_IN_LP_HZ      8500.0f   /* 12 dB/oct pre-tank LP: silky highs     */
#define DEESS_HZ          1200.0f   /* pre-tank split de-esser @ 1.2 kHz      */
#define DEESS_TH           0.125f   /* -18 dBFS threshold, 3:1, zero boost    */
#define PRE_DELAY_MS        25.0f   /* equal L/R pre-delay                    */
#define WET_REV_GAIN        0.92f   /* natural stereo hall balance            */

#define BASS_SUB_SHELF_HZ   36.0f   /* Pultec-style RBJ low-shelf cuts <40 Hz */
#define BASS_SUB_CUT_DB    -26.0f   /* -13.0 dB <36 Hz mud cut at 50% (Stg 3) */
#define BASS_BELL_HZ        76.0f   /* hits BOTH 48-65 Hz sub & 70-135 Hz kick*/
#define BASS_BELL_Q         0.58f   /* wide musical sub+kick curve, zero ring */
#define BASS_MAX_DB         15.5f   /* +7.75 dB at 50% (Stage 3 crest)        */
#define BASS_HEADROOM_DB    -3.2f   /* -1.6 dB >500 Hz space so sub dominates */
#define BASS_GLUE_HZ       150.0f   /* low-band glue detector crossover       */
#define BASS_GLUE_TH        0.660f  /* -3.6 dBFS slow glue catches loud peaks */

#define SFX_SC_THRESH      0.040f   /* -28 dBFS SFX duck threshold            */
#define SFX_SC_STRENGTH     1.60f   /* clean pocket across full music group   */
#define SFX_SC_ATK_S       0.003f   /* fast 3 ms attack catches card snap     */
#define SFX_SC_REL_S       0.085f   /* smooth 85 ms rebound                   */

#define TIDE_RISE_S        0.550f   /* 550 ms tide swell peaking post-thunder */
#define TIDE_DROP_S        0.190f   /* fast -20% shoulder drop like thunder   */
#define TIDE_SHOULDER      0.800f   /* 80% level after peak (-20% drop)       */
#define TIDE_TAIL_S        3.400f   /* smooth organic exponential tail        */
#define STAGE_OFF_TAU_S    0.450f   /* clean fade to 0 when stage resets      */

#define BUSCOMP_HP_HZ      160.0f   /* detector ignores sub/kick bass boost   */
#define BUSCOMP_TH         0.890f   /* ~-1 dBFS: catches true spikes only     */
#define LIMIT_KNEE         0.950f
#define LIMIT_CEIL         0.990f

/* Pure staged progression:
   Stage 1: Stereo ONLY            (60% stereo,  0% reverb,  0% bass)
   Stage 2: Stereo + Reverb        (70% stereo, 50% reverb,  0% bass)
   Stage 3: Stereo + Reverb + Bass (70% stereo, 70% reverb, 50% bass) */
static const float k_stage_side[4] = { 0.00f, 0.60f, 0.70f, 0.70f };
static const float k_stage_rev[4]  = { 0.00f, 0.00f, 0.50f, 0.70f };
static const float k_stage_bass[4] = { 0.00f, 0.00f, 0.00f, 0.50f };

/* ---- RBJ cookbook biquads -------------------------------------------- */
typedef struct {
    float b0, b1, b2, a1, a2;
    float x1, x2, y1, y2;
} Biquad;

static void biquad_design(Biquad *q, float fc, int highpass) {
    float w  = 2.0f * PI_F * fc / AUDIO_SR;
    float cw = cosf(w), sw = sinf(w);
    float alpha = sw / (2.0f * 0.70710678f);
    float a0 = 1.0f + alpha;
    if (highpass) {
        q->b0 =  (1.0f + cw) * 0.5f / a0;
        q->b1 = -(1.0f + cw) / a0;
    } else {
        q->b0 =  (1.0f - cw) * 0.5f / a0;
        q->b1 =  (1.0f - cw) / a0;
    }
    q->b2 = q->b0;
    q->a1 = -2.0f * cw / a0;
    q->a2 = (1.0f - alpha) / a0;
    q->x1 = q->x2 = q->y1 = q->y2 = 0.0f;
}

static void biquad_set_lowshelf(Biquad *q, float cw, float sw, float gain_db) {
    float A  = powf(10.0f, gain_db / 40.0f);
    float al = sw * 0.70710678f;
    float sq = 2.0f * sqrtf(A) * al;
    float a0 =          (A + 1.0f) + (A - 1.0f) * cw + sq;
    q->b0 =       A * ( (A + 1.0f) - (A - 1.0f) * cw + sq) / a0;
    q->b1 =  2.0f*A * ( (A - 1.0f) - (A + 1.0f) * cw)      / a0;
    q->b2 =       A * ( (A + 1.0f) - (A - 1.0f) * cw - sq) / a0;
    q->a1 = -2.0f   * ( (A - 1.0f) + (A + 1.0f) * cw)      / a0;
    q->a2 =           ( (A + 1.0f) + (A - 1.0f) * cw - sq) / a0;
}

static void biquad_set_highshelf(Biquad *q, float cw, float sw, float gain_db) {
    float A  = powf(10.0f, gain_db / 40.0f);
    float al = sw * 0.70710678f;
    float sq = 2.0f * sqrtf(A) * al;
    float a0 =          (A + 1.0f) - (A - 1.0f) * cw + sq;
    q->b0 =       A * ( (A + 1.0f) + (A - 1.0f) * cw + sq) / a0;
    q->b1 = -2.0f*A * ( (A - 1.0f) + (A + 1.0f) * cw)      / a0;
    q->b2 =       A * ( (A + 1.0f) + (A - 1.0f) * cw - sq) / a0;
    q->a1 =  2.0f   * ( (A - 1.0f) - (A + 1.0f) * cw)      / a0;
    q->a2 =           ( (A + 1.0f) - (A - 1.0f) * cw - sq) / a0;
}

static void biquad_set_peaking(Biquad *q, float cw, float sw, float Q, float gain_db) {
    float A  = powf(10.0f, gain_db / 40.0f);
    float al = sw / (2.0f * Q);
    float a0 =  1.0f + al / A;
    q->b0 = (1.0f + al * A) / a0;
    q->b1 = (-2.0f * cw)    / a0;
    q->b2 = (1.0f - al * A) / a0;
    q->a1 = (-2.0f * cw)    / a0;
    q->a2 = (1.0f - al / A) / a0;
}

static float biquad_run(Biquad *q, float x) {
    float y = q->b0 * x + q->b1 * q->x1 + q->b2 * q->x2 - q->a1 * q->y1 - q->a2 * q->y2;
    q->x2 = q->x1; q->x1 = x;
    q->y2 = q->y1; q->y1 = y;
    return y;
}

static float clamp_range(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static float onepole_k(float fc) { return 1.0f - expf(-2.0f * PI_F * fc / AUDIO_SR); }
static float sample_tau(float tau_s) { return 1.0f - expf(-1.0f / (tau_s * AUDIO_SR)); }

static float soft_knee_limit(float x, float knee, float max_val) {
    float ax = (x < 0.0f) ? -x : x;
    if (ax <= knee) return x;
    float span = max_val - knee;
    float u = (ax - knee) / (span + (ax - knee));
    float out = knee + span * u;
    return (x < 0.0f) ? -out : out;
}

/* ---- live SFX sidechain bus ------------------------------------------ */
#define SFX_SLOT_COUNT 7
#define SC_BUS_MAX     16384

static float        s_sc_bus[SC_BUS_MAX];
static unsigned int s_sc_cur[SFX_SLOT_COUNT];
static unsigned int s_sc_max;
static unsigned int s_sc_read;
static float        s_sfx_fader[SFX_SLOT_COUNT] = { 0.95f, 0.95f, 0.95f, 0.50f, 0.50f, 0.50f, 0.50f };
static float        s_sfx_sc_env;

static void sfx_tap_slot(int slot, void *buffer, unsigned int frames) {
    const float *pcm = (const float *)buffer;
    if (!pcm || frames == 0) return;
    unsigned int base = s_sc_cur[slot];
    float gain = s_sfx_fader[slot];
    for (unsigned int f = 0; f < frames && (base + f) < SC_BUS_MAX; f++) {
        float al = fabsf(pcm[f * 2 + 0]);
        float ar = fabsf(pcm[f * 2 + 1]);
        float pk = (ar > al) ? ar : al;
        s_sc_bus[base + f] += pk * gain;
    }
    base += frames;
    if (base > SC_BUS_MAX) base = SC_BUS_MAX;
    s_sc_cur[slot] = base;
    if (base > s_sc_max) s_sc_max = base;
}

static void sfx_tap_0(void *b, unsigned int n) { sfx_tap_slot(0, b, n); }
static void sfx_tap_1(void *b, unsigned int n) { sfx_tap_slot(1, b, n); }
static void sfx_tap_2(void *b, unsigned int n) { sfx_tap_slot(2, b, n); }
static void sfx_tap_3(void *b, unsigned int n) { sfx_tap_slot(3, b, n); }
static void sfx_tap_4(void *b, unsigned int n) { sfx_tap_slot(4, b, n); }
static void sfx_tap_5(void *b, unsigned int n) { sfx_tap_slot(5, b, n); }
static void sfx_tap_6(void *b, unsigned int n) { sfx_tap_slot(6, b, n); }

static void sc_bus_flush(void) {
    if (s_sc_max > 0) {
        memset(s_sc_bus, 0, s_sc_max * sizeof(float));
        memset(s_sc_cur, 0, sizeof(s_sc_cur));
        s_sc_max = 0;
        s_sc_read = 0;
    }
}

static float sc_bus_read_sample(void) {
    if (s_sc_read >= s_sc_max) {
        if (s_sc_max > 0) sc_bus_flush();
        return 0.0f;
    }
    float v = s_sc_bus[s_sc_read];
    s_sc_bus[s_sc_read++] = 0.0f;
    if (s_sc_read >= s_sc_max) sc_bus_flush();
    return (v > 1.0f) ? 1.0f : v;
}

/* ---- dedicated DSP states -------------------------------------------- */
#define REVERB_COMB_COUNT 4
#define REVERB_COMB_MAX   5504
#define REVERB_AP_COUNT   2
#define REVERB_AP_MAX     640
#define PREDELAY_MAX      1400

static const int k_comb_len_l[REVERB_COMB_COUNT] = { 2903, 3571, 4219, 4967 };
static const int k_comb_len_r[REVERB_COMB_COUNT] = { 3251, 3917, 4603, 5381 };
static const int k_ap_len_l[REVERB_AP_COUNT]     = { 431, 163 };
static const int k_ap_len_r[REVERB_AP_COUNT]     = { 509, 211 };

static float s_comb_buf_l[REVERB_COMB_COUNT][REVERB_COMB_MAX];
static float s_comb_buf_r[REVERB_COMB_COUNT][REVERB_COMB_MAX];
static int   s_comb_idx_l[REVERB_COMB_COUNT];
static int   s_comb_idx_r[REVERB_COMB_COUNT];
static float s_comb_lp_l[REVERB_COMB_COUNT];
static float s_comb_lp_r[REVERB_COMB_COUNT];
static float s_ap_buf_l[REVERB_AP_COUNT][REVERB_AP_MAX];
static float s_ap_buf_r[REVERB_AP_COUNT][REVERB_AP_MAX];
static int   s_ap_idx_l[REVERB_AP_COUNT];
static int   s_ap_idx_r[REVERB_AP_COUNT];
static float s_pre_buf_l[PREDELAY_MAX];
static float s_pre_buf_r[PREDELAY_MAX];
static int   s_pre_idx_l, s_pre_idx_r, s_pre_len;

/* [A] RBJ High-Shelf M/S stereo width + mono-bass Side guard */
static Biquad s_side_shelf, s_mid_shelf;
static Biquad s_side_guard1, s_side_guard2;
static float  s_shelf_cw, s_shelf_sw;

/* [B] Pre-tank Abbey Road reverb filters (> 550 Hz, 1.2 kHz de-ess, < 8.5 kHz) */
static Biquad s_rev_hp_l1, s_rev_hp_l2, s_rev_hp_r1, s_rev_hp_r2;
static Biquad s_rev_lp_l,  s_rev_lp_r;
static Biquad s_rev_side_hp1, s_rev_side_hp2;
static float  s_deess_lp_l, s_deess_lp_r, s_deess_env;
static float  s_sc_env;

/* [C] Minimum-phase Pultec-style RBJ Mono Bass EQ + slow low-band glue */
static Biquad s_bass_sub_shelf;
static Biquad s_bass_bell;
static float  s_sub_cw,  s_sub_sw;
static float  s_bell_cw, s_bell_sw;
static float  s_glue_lp, s_bass_comp_env;

static int    s_dsp_ready;

/* Tide envelope state */
static float s_rev_env, s_side_env, s_bass_env;
static float s_rev_start, s_side_start, s_bass_start;
static float s_wave_t;

/* Music & Master bus spike protection state */
static float s_hp_lp_l, s_hp_lp_r;
static float s_comp_env, s_lim_env;

static volatile int s_stage;
static volatile int s_wave_trig;

void audio_set_capture_stage(int stage) {
    if (stage < 0) stage = 0;
    if (stage > 3) stage = 3;
    s_stage = stage;
    if (stage > 0) s_wave_trig = 1;
}

static void audio_reset_stage_state(void) {
    s_stage = 0;
    s_wave_trig = 0;
    s_wave_t = 0.0f;
    s_rev_env = s_side_env = s_bass_env = 0.0f;
    s_rev_start = s_side_start = s_bass_start = 0.0f;
}

static void dsp_init_once(void) {
    if (s_dsp_ready) return;
    float ws = 2.0f * PI_F * SIDE_SHELF_HZ / AUDIO_SR;
    s_shelf_cw = cosf(ws); s_shelf_sw = sinf(ws);
    memset(&s_side_shelf, 0, sizeof(Biquad));
    memset(&s_mid_shelf,  0, sizeof(Biquad));
    biquad_set_highshelf(&s_side_shelf, s_shelf_cw, s_shelf_sw, 0.0f);
    biquad_set_highshelf(&s_mid_shelf,  s_shelf_cw, s_shelf_sw, 0.0f);
    biquad_design(&s_side_guard1, SIDE_MONO_SAFE_HZ, 1);
    biquad_design(&s_side_guard2, SIDE_MONO_SAFE_HZ, 1);

    biquad_design(&s_rev_hp_l1,    REV_IN_HP_HZ, 1);
    biquad_design(&s_rev_hp_l2,    REV_IN_HP_HZ, 1);
    biquad_design(&s_rev_hp_r1,    REV_IN_HP_HZ, 1);
    biquad_design(&s_rev_hp_r2,    REV_IN_HP_HZ, 1);
    biquad_design(&s_rev_lp_l,     REV_IN_LP_HZ, 0);
    biquad_design(&s_rev_lp_r,     REV_IN_LP_HZ, 0);
    biquad_design(&s_rev_side_hp1, SIDE_SHELF_HZ, 1);
    biquad_design(&s_rev_side_hp2, SIDE_SHELF_HZ, 1);

    float wsub = 2.0f * PI_F * BASS_SUB_SHELF_HZ / AUDIO_SR;
    s_sub_cw = cosf(wsub); s_sub_sw = sinf(wsub);
    float wb = 2.0f * PI_F * BASS_BELL_HZ / AUDIO_SR;
    s_bell_cw = cosf(wb); s_bell_sw = sinf(wb);
    memset(&s_bass_sub_shelf, 0, sizeof(Biquad));
    memset(&s_bass_bell,      0, sizeof(Biquad));
    biquad_set_lowshelf(&s_bass_sub_shelf, s_sub_cw, s_sub_sw, 0.0f);
    biquad_set_peaking(&s_bass_bell, s_bell_cw, s_bell_sw, BASS_BELL_Q, 0.0f);

    s_pre_len = (int)(PRE_DELAY_MS * AUDIO_SR / 1000.0f);
    if (s_pre_len < 1) s_pre_len = 1;
    if (s_pre_len >= PREDELAY_MAX) s_pre_len = PREDELAY_MAX - 1;
    s_pre_idx_l = s_pre_idx_r = 0;
    s_dsp_ready = 1;
}

/* Menu music: receives real-time SFX sidechain ducking only. */
static void menu_music_processor(void *buffer, unsigned int frames) {
    float *pcm = (float *)buffer;
    if (!pcm || frames == 0) return;

    const float sc_atk = sample_tau(SFX_SC_ATK_S);
    const float sc_rel = sample_tau(SFX_SC_REL_S);

    for (unsigned int f = 0; f < frames; f++) {
        float key = sc_bus_read_sample();
        s_sfx_sc_env += (key - s_sfx_sc_env) * ((key > s_sfx_sc_env) ? sc_atk : sc_rel);
        if (s_sfx_sc_env <= SFX_SC_THRESH) {
            if (key == 0.0f && s_sfx_sc_env < 0.0002f) s_sfx_sc_env = 0.0f;
            continue;
        }
        float over = s_sfx_sc_env - SFX_SC_THRESH;
        float duck_g = 1.0f / (1.0f + SFX_SC_STRENGTH * over);
        pcm[f * 2 + 0] *= duck_g;
        pcm[f * 2 + 1] *= duck_g;
    }
}

/* Game music: 3 independent studio stages + live All-SFX sidechain ducking. */
static void music_reverb_processor(void *buffer, unsigned int frames) {
    float *pcm = (float *)buffer;
    if (!pcm || frames == 0) return;
    dsp_init_once();

    int stage = s_stage;
    if (stage < 0) stage = 0;
    if (stage > 3) stage = 3;

    const float t_rev  = k_stage_rev[stage];
    const float t_side = k_stage_side[stage];
    const float t_bass = k_stage_bass[stage];

    const float off_rel   = sample_tau(STAGE_OFF_TAU_S);
    const float tail_rel  = sample_tau(TIDE_TAIL_S);
    const float sfx_atk   = sample_tau(SFX_SC_ATK_S);
    const float sfx_rel   = sample_tau(SFX_SC_REL_S);
    const float deess_atk = sample_tau(0.002f), deess_rel = sample_tau(0.060f);
    const float bass_atk  = sample_tau(0.035f), bass_rel  = sample_tau(0.130f);
    const float wetsc_atk = sample_tau(0.005f), wetsc_rel = sample_tau(0.090f);

    const float k_glue  = onepole_k(BASS_GLUE_HZ);
    const float k_deess = onepole_k(DEESS_HZ);
    const float k_schp  = onepole_k(BUSCOMP_HP_HZ);
    const float comp_atk = sample_tau(0.025f), comp_rel = sample_tau(0.100f);

    if (s_wave_trig && stage > 0) {
        s_rev_start  = s_rev_env;
        s_side_start = s_side_env;
        s_bass_start = s_bass_env;
        s_wave_t = 0.0f;
        s_wave_trig = 0;
    }

    for (unsigned int f = 0; f < frames; f++) {
        float dry_l = pcm[f * 2 + 0];
        float dry_r = pcm[f * 2 + 1];

        /* Advance Tide envelope: 550 ms rise -> 190 ms -20% drop -> smooth tail. */
        if (stage == 0) {
            s_rev_env  -= s_rev_env  * off_rel;
            s_side_env -= s_side_env * off_rel;
            s_bass_env -= s_bass_env * off_rel;
            if (s_rev_env  < 0.0005f) s_rev_env  = 0.0f;
            if (s_side_env < 0.0005f) s_side_env = 0.0f;
            if (s_bass_env < 0.0005f) s_bass_env = 0.0f;
        } else {
            s_wave_t += 1.0f / AUDIO_SR;
            if (s_wave_t <= TIDE_RISE_S) {
                float u = s_wave_t / TIDE_RISE_S;
                float rise = sinf(0.5f * PI_F * u);
                s_rev_env  = s_rev_start  + (t_rev  - s_rev_start)  * rise;
                s_side_env = s_side_start + (t_side - s_side_start) * rise;
                s_bass_env = s_bass_start + (t_bass - s_bass_start) * rise;
            } else if (s_wave_t <= TIDE_RISE_S + TIDE_DROP_S) {
                float v = (s_wave_t - TIDE_RISE_S) / TIDE_DROP_S;
                float sh = TIDE_SHOULDER + (1.0f - TIDE_SHOULDER) * 0.5f * (1.0f + cosf(PI_F * v));
                s_rev_env  = t_rev  * sh;
                s_side_env = t_side * sh;
                s_bass_env = t_bass * sh;
            } else {
                s_rev_env  -= s_rev_env  * tail_rel;
                s_side_env -= s_side_env * tail_rel;
                s_bass_env -= s_bass_env * tail_rel;
                if (s_rev_env  < 0.0004f) s_rev_env  = 0.0f;
                if (s_side_env < 0.0004f) s_side_env = 0.0f;
                if (s_bass_env < 0.0004f) s_bass_env = 0.0f;
            }
        }

        /* Update smooth minimum-phase EQ coefficients every 16 samples (~0.36ms). */
        if ((f & 15u) == 0u && (s_side_env > 0.0f || s_bass_env > 0.0f)) {
            biquad_set_highshelf(&s_side_shelf,     s_shelf_cw, s_shelf_sw, SIDE_MAX_DB * s_side_env + BASS_HEADROOM_DB * s_bass_env);
            biquad_set_highshelf(&s_mid_shelf,      s_shelf_cw, s_shelf_sw, MID_SPACE_TUCK_DB * s_side_env + BASS_HEADROOM_DB * s_bass_env);
            biquad_set_lowshelf(&s_bass_sub_shelf,  s_sub_cw,   s_sub_sw,   BASS_SUB_CUT_DB * s_bass_env);
            biquad_set_peaking(&s_bass_bell,        s_bell_cw,  s_bell_sw,  BASS_BELL_Q, BASS_MAX_DB * s_bass_env);
        }

        /* Live SFX sidechain envelope */
        float sfx_key = sc_bus_read_sample();
        s_sfx_sc_env += (sfx_key - s_sfx_sc_env) * ((sfx_key > s_sfx_sc_env) ? sfx_atk : sfx_rel);
        if (sfx_key == 0.0f && s_sfx_sc_env < 0.0002f) s_sfx_sc_env = 0.0f;

        float dry_mid  = 0.5f * (dry_l + dry_r);
        float dry_side = 0.5f * (dry_l - dry_r);

        /* Keep filter states warm for click-free engagement */
        float guarded_s = biquad_run(&s_side_guard2, biquad_run(&s_side_guard1, dry_side));

        float rhp_l = biquad_run(&s_rev_hp_l2, biquad_run(&s_rev_hp_l1, dry_l));
        float rhp_r = biquad_run(&s_rev_hp_r2, biquad_run(&s_rev_hp_r1, dry_r));

        s_deess_lp_l += (rhp_l - s_deess_lp_l) * k_deess;
        s_deess_lp_r += (rhp_r - s_deess_lp_r) * k_deess;
        float hi_l = rhp_l - s_deess_lp_l;
        float hi_r = rhp_r - s_deess_lp_r;
        float hi_pk = fmaxf(fabsf(hi_l), fabsf(hi_r));
        s_deess_env += (hi_pk - s_deess_env) * ((hi_pk > s_deess_env) ? deess_atk : deess_rel);
        float deess_g = (s_deess_env > DEESS_TH)
                      ? (DEESS_TH + (s_deess_env - DEESS_TH) * 0.3333f) / s_deess_env
                      : 1.0f;

        float rev_in_l = biquad_run(&s_rev_lp_l, s_deess_lp_l + hi_l * deess_g);
        float rev_in_r = biquad_run(&s_rev_lp_r, s_deess_lp_r + hi_r * deess_g);

        float slap_l = s_pre_buf_l[s_pre_idx_l];
        float slap_r = s_pre_buf_r[s_pre_idx_r];
        s_pre_buf_l[s_pre_idx_l] = rev_in_l;
        s_pre_buf_r[s_pre_idx_r] = rev_in_r;
        if (++s_pre_idx_l >= s_pre_len) s_pre_idx_l = 0;
        if (++s_pre_idx_r >= s_pre_len) s_pre_idx_r = 0;

        int wet_active  = (s_side_env > 0.0f) || (s_rev_env > 0.0f) || (s_bass_env > 0.0f);
        int duck_active = (s_sfx_sc_env > SFX_SC_THRESH);

        /* Stage 0 with no active SFX: keep EQ states warm & return exact dry. */
        if (!wet_active && !duck_active) {
            s_side_shelf.x2     = s_side_shelf.x1;     s_side_shelf.x1     = guarded_s;
            s_side_shelf.y2     = s_side_shelf.y1;     s_side_shelf.y1     = guarded_s;
            s_mid_shelf.x2      = s_mid_shelf.x1;      s_mid_shelf.x1      = dry_mid;
            s_mid_shelf.y2      = s_mid_shelf.y1;      s_mid_shelf.y1      = dry_mid;
            s_bass_sub_shelf.x2 = s_bass_sub_shelf.x1; s_bass_sub_shelf.x1 = dry_mid;
            s_bass_sub_shelf.y2 = s_bass_sub_shelf.y1; s_bass_sub_shelf.y1 = dry_mid;
            s_bass_bell.x2      = s_bass_bell.x1;      s_bass_bell.x1      = dry_mid;
            s_bass_bell.y2      = s_bass_bell.y1;      s_bass_bell.y1      = dry_mid;
            continue;
        }

        /* [A] + [C] Pure minimum-phase M/S processing (zero parallel phase notch!):
           - Mid : Pultec-style RBJ 42 Hz sub-mud cut + 86 Hz punch bell
                   + 35 ms slow-attack low-band glue compressor
           - Side: RBJ High-Shelf stereo width above 500 Hz (sub/bass guarded) */
        float bass_mix = clamp_range(s_bass_env * 2.0f, 0.0f, 1.0f);
        float mid_sub  = biquad_run(&s_bass_sub_shelf, dry_mid);
        float mid_eq   = biquad_run(&s_bass_bell, biquad_run(&s_mid_shelf, mid_sub));

        /* Slow-attack (35 ms) 2:1 low-band glue active proportionally to Stage 3 */
        s_glue_lp += (mid_eq - s_glue_lp) * k_glue;
        float low_part  = s_glue_lp;
        float high_part = mid_eq - low_part;
        float la = fabsf(low_part);
        s_bass_comp_env += (la - s_bass_comp_env) * ((la > s_bass_comp_env) ? bass_atk : bass_rel);
        if (s_bass_env > 0.0f && s_bass_comp_env > BASS_GLUE_TH) {
            float g_raw = (BASS_GLUE_TH + (s_bass_comp_env - BASS_GLUE_TH) * 0.50f) / s_bass_comp_env;
            low_part *= 1.0f + (g_raw - 1.0f) * bass_mix;
        }
        float proc_mid = low_part + high_part;

        /* Blend dry_side -> guarded shelf_side smoothly so 0% is exact dry_side */
        float side_mix  = clamp_range(s_side_env * 3.0f, 0.0f, 1.0f);
        float side_base = dry_side + (guarded_s - dry_side) * side_mix;
        float proc_side = biquad_run(&s_side_shelf, side_base);

        float out_l = proc_mid + proc_side;
        float out_r = proc_mid - proc_side;

        /* [B] Stage 2+ Kick-Free Abbey Road Stereo Reverb (> 550 Hz ONLY). */
        if (s_rev_env > 0.0f) {
            float feed_l = rev_in_l * 0.62f + slap_r * 0.18f;
            float feed_r = rev_in_r * 0.62f + slap_l * 0.18f;
            const float feedback = 0.70f;
            const float damp = 0.42f;
            float acc_l = 0.0f, acc_r = 0.0f;

            for (int c = 0; c < REVERB_COMB_COUNT; c++) {
                int il = s_comb_idx_l[c];
                float o_l = s_comb_buf_l[c][il];
                s_comb_lp_l[c] = o_l * (1.0f - damp) + s_comb_lp_l[c] * damp;
                s_comb_buf_l[c][il] = feed_l + s_comb_lp_l[c] * feedback;
                if (++il >= k_comb_len_l[c]) il = 0;
                s_comb_idx_l[c] = il;
                acc_l += s_comb_lp_l[c];

                int ir = s_comb_idx_r[c];
                float o_r = s_comb_buf_r[c][ir];
                s_comb_lp_r[c] = o_r * (1.0f - damp) + s_comb_lp_r[c] * damp;
                s_comb_buf_r[c][ir] = feed_r + s_comb_lp_r[c] * feedback;
                if (++ir >= k_comb_len_r[c]) ir = 0;
                s_comb_idx_r[c] = ir;
                acc_r += s_comb_lp_r[c];
            }
            acc_l *= 0.42f;
            acc_r *= 0.42f;

            for (int a = 0; a < REVERB_AP_COUNT; a++) {
                int il = s_ap_idx_l[a];
                float bl = s_ap_buf_l[a][il];
                float inl = acc_l;
                acc_l = -inl + bl;
                s_ap_buf_l[a][il] = inl + bl * 0.48f;
                if (++il >= k_ap_len_l[a]) il = 0;
                s_ap_idx_l[a] = il;

                int ir = s_ap_idx_r[a];
                float br = s_ap_buf_r[a][ir];
                float inr = acc_r;
                acc_r = -inr + br;
                s_ap_buf_r[a][ir] = inr + br * 0.48f;
                if (++ir >= k_ap_len_r[a]) ir = 0;
                s_ap_idx_r[a] = ir;
            }

            float wet_l    = acc_l + slap_l * 0.28f;
            float wet_r    = acc_r + slap_r * 0.28f;
            float wet_mid  = 0.5f * (wet_l + wet_r);
            float wet_side = biquad_run(&s_rev_side_hp2, biquad_run(&s_rev_side_hp1, 0.5f * (wet_l - wet_r)));

            float key_dry = 0.5f * (fabsf(rev_in_l) + fabsf(rev_in_r));
            s_sc_env += (key_dry - s_sc_env) * ((key_dry > s_sc_env) ? wetsc_atk : wetsc_rel);
            float sc_over = (s_sc_env > 0.16f) ? (s_sc_env - 0.16f) : 0.0f;
            float rev_g   = (WET_REV_GAIN * s_rev_env) / (1.0f + 1.10f * sc_over);

            out_l += (wet_mid + wet_side * 1.25f) * rev_g;
            out_r += (wet_mid - wet_side * 1.25f) * rev_g;
        }

        /* Music group sidechain duck keyed by ALL live SFX streams. */
        if (duck_active) {
            float duck_g = 1.0f / (1.0f + SFX_SC_STRENGTH * (s_sfx_sc_env - SFX_SC_THRESH));
            out_l *= duck_g;
            out_r *= duck_g;
        }

        /* 160 Hz HPF spike compressor + clean stereo-linked envelope limiter
           (prevents waveshaper clipping/THD on loud -8 LUFS masters!). */
        s_hp_lp_l += (out_l - s_hp_lp_l) * k_schp;
        s_hp_lp_r += (out_r - s_hp_lp_r) * k_schp;
        float hi_out_l = out_l - s_hp_lp_l;
        float hi_out_r = out_r - s_hp_lp_r;
        float pk = fmaxf(fabsf(hi_out_l), fabsf(hi_out_r));
        s_comp_env += (pk - s_comp_env) * ((pk > s_comp_env) ? comp_atk : comp_rel);
        if (s_comp_env > BUSCOMP_TH) {
            float cg = (BUSCOMP_TH + (s_comp_env - BUSCOMP_TH) * 0.50f) / s_comp_env;
            out_l = s_hp_lp_l + hi_out_l * cg;
            out_r = s_hp_lp_r + hi_out_r * cg;
        }

        float full_pk = fmaxf(fabsf(out_l), fabsf(out_r));
        s_lim_env += (full_pk - s_lim_env) * ((full_pk > s_lim_env) ? 0.040f : 0.00025f);
        if (s_lim_env > 0.900f) {
            float lg = 0.900f / s_lim_env;
            out_l *= lg;
            out_r *= lg;
        }

        pcm[f * 2 + 0] = soft_knee_limit(out_l, LIMIT_KNEE, LIMIT_CEIL);
        pcm[f * 2 + 1] = soft_knee_limit(out_r, LIMIT_KNEE, LIMIT_CEIL);
    }
}

/* Master mix processor (Music + SFX post-fader sum): catches combined peaks and
   flushes any unconsumed SFX sidechain samples if music was stopped. */
static float s_mix_hp_l, s_mix_hp_r, s_mix_env;

static void master_mix_processor(void *buffer, unsigned int frames) {
    sc_bus_flush();
    float *pcm = (float *)buffer;
    if (!pcm || frames == 0) return;

    const float k_hp = onepole_k(BUSCOMP_HP_HZ);
    const float atk  = sample_tau(0.025f);
    const float rel  = sample_tau(0.100f);

    for (unsigned int f = 0; f < frames; f++) {
        float l = pcm[f * 2 + 0];
        float r = pcm[f * 2 + 1];
        s_mix_hp_l += (l - s_mix_hp_l) * k_hp;
        s_mix_hp_r += (r - s_mix_hp_r) * k_hp;
        float pk = fmaxf(fabsf(l - s_mix_hp_l), fabsf(r - s_mix_hp_r));
        s_mix_env += (pk - s_mix_env) * ((pk > s_mix_env) ? atk : rel);
        if (s_mix_env > BUSCOMP_TH) {
            float cg = (BUSCOMP_TH + (s_mix_env - BUSCOMP_TH) * 0.50f) / s_mix_env;
            l *= cg;
            r *= cg;
        }
        if (fabsf(l) > LIMIT_KNEE || fabsf(r) > LIMIT_KNEE || s_mix_env > BUSCOMP_TH) {
            pcm[f * 2 + 0] = soft_knee_limit(l, LIMIT_KNEE, LIMIT_CEIL);
            pcm[f * 2 + 1] = soft_knee_limit(r, LIMIT_KNEE, LIMIT_CEIL);
        }
    }
}

static char path_buf[512];
static const char* get_path(const char* sub) {
    const char *base = GetApplicationDirectory();
    if (base && base[0] != '\0') {
        snprintf(path_buf, sizeof(path_buf), "%s%s", base, sub);
        if (FileExists(path_buf)) return path_buf;
    }
    snprintf(path_buf, sizeof(path_buf), "./%s", sub);
    return path_buf;
}

void audio_init(AudioAssets *a, float bgm_vol, float sfx_vol) {
    memset(a, 0, sizeof(*a));
    a->current_game_track = -1;
    a->bgm_volume = (bgm_vol >= 0.0f && bgm_vol <= 1.0f) ? bgm_vol : 1.0f;
    a->sfx_volume = (sfx_vol >= 0.0f && sfx_vol <= 1.0f) ? sfx_vol : 0.5f;

    InitAudioDevice();

    /* Load all SFX FIRST so Raylib processes SFX before Music each audio block. */
    float card_vol = clamp_range(a->sfx_volume * 1.90f, 0.0f, 1.0f);
    for (int i = 0; i < 3; i++) s_sfx_fader[i] = card_vol;
    for (int i = 3; i < SFX_SLOT_COUNT; i++) s_sfx_fader[i] = a->sfx_volume;

    const char *p_flip = get_path("assets/sfx/card_flip.wav");
    if (FileExists(p_flip)) {
        a->sfx_flip = LoadSound(p_flip); a->has_sfx_flip = 1;
        SetSoundVolume(a->sfx_flip, card_vol);
        AttachAudioStreamProcessor(a->sfx_flip.stream, sfx_tap_0);
    }
    const char *p_drop = get_path("assets/sfx/card_drop.wav");
    if (FileExists(p_drop)) {
        a->sfx_drop = LoadSound(p_drop); a->has_sfx_drop = 1;
        SetSoundVolume(a->sfx_drop, card_vol);
        AttachAudioStreamProcessor(a->sfx_drop.stream, sfx_tap_1);
    }
    const char *p_cap = get_path("assets/sfx/capture.wav");
    if (FileExists(p_cap)) {
        a->sfx_capture = LoadSound(p_cap); a->has_sfx_capture = 1;
        SetSoundVolume(a->sfx_capture, card_vol);
        AttachAudioStreamProcessor(a->sfx_capture.stream, sfx_tap_2);
    }
    const char *p_koi = get_path("assets/sfx/koi_call.wav");
    if (FileExists(p_koi)) {
        a->sfx_koi = LoadSound(p_koi); a->has_sfx_koi = 1;
        SetSoundVolume(a->sfx_koi, a->sfx_volume);
        AttachAudioStreamProcessor(a->sfx_koi.stream, sfx_tap_3);
    }
    const char *p_win = get_path("assets/sfx/win.wav");
    if (FileExists(p_win)) {
        a->sfx_win = LoadSound(p_win); a->has_sfx_win = 1;
        SetSoundVolume(a->sfx_win, a->sfx_volume);
        AttachAudioStreamProcessor(a->sfx_win.stream, sfx_tap_4);
    }
    const char *p_lose = get_path("assets/sfx/lose.wav");
    if (FileExists(p_lose)) {
        a->sfx_lose = LoadSound(p_lose); a->has_sfx_lose = 1;
        SetSoundVolume(a->sfx_lose, a->sfx_volume);
        AttachAudioStreamProcessor(a->sfx_lose.stream, sfx_tap_5);
    }
    const char *p_click = get_path("assets/sfx/click.wav");
    if (FileExists(p_click)) {
        a->sfx_click = LoadSound(p_click); a->has_sfx_click = 1;
        SetSoundVolume(a->sfx_click, a->sfx_volume);
        AttachAudioStreamProcessor(a->sfx_click.stream, sfx_tap_6);
    }

    /* Load Music AFTER SFX so s_sc_bus holds the current block's live SFX audio. */
    const char *p_menu = get_path("assets/music/menu.mp3");
    if (FileExists(p_menu)) {
        a->music_menu = LoadMusicStream(p_menu);
        a->has_music_menu = 1;
        SetMusicVolume(a->music_menu, a->bgm_volume);
        AttachAudioStreamProcessor(a->music_menu.stream, menu_music_processor);
    }

    for (int i = 0; i < 10; i++) {
        char track_path[64];
        snprintf(track_path, sizeof(track_path), "assets/music/game%d.mp3", i + 1);
        const char *p_game = get_path(track_path);
        if (FileExists(p_game)) {
            a->music_game[i] = LoadMusicStream(p_game);
            a->music_game[i].looping = false;
            a->has_music_game[i] = 1;
            SetMusicVolume(a->music_game[i], a->bgm_volume);
            AttachAudioStreamProcessor(a->music_game[i].stream, music_reverb_processor);
        }
    }

    AttachAudioMixedProcessor(master_mix_processor);
}

void audio_set_bgm_volume(AudioAssets *a, float volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    a->bgm_volume = volume;
    if (a->has_music_menu) SetMusicVolume(a->music_menu, volume);
    for (int i = 0; i < 10; i++) {
        if (a->has_music_game[i]) SetMusicVolume(a->music_game[i], volume);
    }
}

void audio_set_sfx_volume(AudioAssets *a, float volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    a->sfx_volume = volume;
    float card_vol = clamp_range(volume * 1.90f, 0.0f, 1.0f);
    for (int i = 0; i < 3; i++) s_sfx_fader[i] = card_vol;
    for (int i = 3; i < SFX_SLOT_COUNT; i++) s_sfx_fader[i] = volume;
    if (a->has_sfx_flip) SetSoundVolume(a->sfx_flip, card_vol);
    if (a->has_sfx_drop) SetSoundVolume(a->sfx_drop, card_vol);
    if (a->has_sfx_capture) SetSoundVolume(a->sfx_capture, card_vol);
    if (a->has_sfx_koi) SetSoundVolume(a->sfx_koi, volume);
    if (a->has_sfx_win) SetSoundVolume(a->sfx_win, volume);
    if (a->has_sfx_lose) SetSoundVolume(a->sfx_lose, volume);
    if (a->has_sfx_click) SetSoundVolume(a->sfx_click, volume);
}

void audio_update(AudioAssets *a) {
    if (a->current_music == 1 && a->has_music_menu) {
        UpdateMusicStream(a->music_menu);
    } else if (a->current_music == 2 && a->current_game_track >= 0 && a->current_game_track < 10) {
        if (a->has_music_game[a->current_game_track]) {
            UpdateMusicStream(a->music_game[a->current_game_track]);
        }
    }
}

void audio_play_menu_music(AudioAssets *a) {
    audio_reset_stage_state();
    if (a->current_music == 1 && a->has_music_menu && IsMusicStreamPlaying(a->music_menu)) return;
    if (a->current_music == 2 && a->current_game_track >= 0 && a->current_game_track < 10) {
        if (a->has_music_game[a->current_game_track]) StopMusicStream(a->music_game[a->current_game_track]);
    }
    a->current_music = 1;
    if (a->has_music_menu) {
        SetMusicVolume(a->music_menu, a->bgm_volume);
        PlayMusicStream(a->music_menu);
    }
}

void audio_start_match_music(AudioAssets *a, int track_1_to_10) {
    audio_reset_stage_state();
    if (a->current_music == 1 && a->has_music_menu) StopMusicStream(a->music_menu);
    if (a->current_game_track >= 0 && a->current_game_track < 10) {
        if (a->has_music_game[a->current_game_track]) StopMusicStream(a->music_game[a->current_game_track]);
    }
    int idx = track_1_to_10 - 1;
    if (idx < 0) idx = 0;
    if (idx > 9) idx = 9;
    a->current_game_track = idx;
    a->current_music = 2;

    if (a->has_music_game[idx]) {
        a->music_game[idx].looping = false;
        SetMusicVolume(a->music_game[idx], a->bgm_volume);
        PlayMusicStream(a->music_game[idx]);
    }
}

void audio_resume_match_music(AudioAssets *a) {
    if (a->current_music == 2 && a->current_game_track >= 0 && a->current_game_track < 10) {
        if (a->has_music_game[a->current_game_track] && !IsMusicStreamPlaying(a->music_game[a->current_game_track])) {
            SetMusicVolume(a->music_game[a->current_game_track], a->bgm_volume);
            PlayMusicStream(a->music_game[a->current_game_track]);
        }
    }
}

int audio_is_match_music_finished(const AudioAssets *a) {
    if (a->current_music != 2 || a->current_game_track < 0 || a->current_game_track >= 10) return 0;
    if (!a->has_music_game[a->current_game_track]) return 0;
    Music m = a->music_game[a->current_game_track];
    if (!IsMusicStreamPlaying(m)) return 1;
    float len = GetMusicTimeLength(m);
    if (len > 0.0f && GetMusicTimePlayed(m) >= len - 0.15f) return 1;
    return 0;
}

void audio_stop_music(AudioAssets *a) {
    audio_reset_stage_state();
    if (a->current_music == 1 && a->has_music_menu) StopMusicStream(a->music_menu);
    if (a->current_music == 2 && a->current_game_track >= 0 && a->current_game_track < 10) {
        if (a->has_music_game[a->current_game_track]) StopMusicStream(a->music_game[a->current_game_track]);
    }
    a->current_music = 0;
}

void audio_play_flip(AudioAssets *a)    { if (a->has_sfx_flip) PlaySound(a->sfx_flip); }
void audio_play_drop(AudioAssets *a)    { if (a->has_sfx_drop) PlaySound(a->sfx_drop); }
void audio_play_capture(AudioAssets *a) {
    if (a->has_sfx_capture) PlaySound(a->sfx_capture);
    else if (a->has_sfx_drop) PlaySound(a->sfx_drop);
}
void audio_play_koi(AudioAssets *a)     { if (a->has_sfx_koi) PlaySound(a->sfx_koi); }
void audio_play_win(AudioAssets *a)     { if (a->has_sfx_win) PlaySound(a->sfx_win); }
void audio_play_lose(AudioAssets *a)    { if (a->has_sfx_lose) PlaySound(a->sfx_lose); }
void audio_play_click(AudioAssets *a)   { if (a->has_sfx_click) PlaySound(a->sfx_click); }

void audio_close(AudioAssets *a) {
    DetachAudioMixedProcessor(master_mix_processor);
    if (a->has_music_menu) {
        DetachAudioStreamProcessor(a->music_menu.stream, menu_music_processor);
        UnloadMusicStream(a->music_menu);
    }
    for (int i = 0; i < 10; i++) {
        if (a->has_music_game[i]) {
            DetachAudioStreamProcessor(a->music_game[i].stream, music_reverb_processor);
            UnloadMusicStream(a->music_game[i]);
        }
    }
    if (a->has_sfx_flip)    { DetachAudioStreamProcessor(a->sfx_flip.stream, sfx_tap_0); UnloadSound(a->sfx_flip); }
    if (a->has_sfx_drop)    { DetachAudioStreamProcessor(a->sfx_drop.stream, sfx_tap_1); UnloadSound(a->sfx_drop); }
    if (a->has_sfx_capture) { DetachAudioStreamProcessor(a->sfx_capture.stream, sfx_tap_2); UnloadSound(a->sfx_capture); }
    if (a->has_sfx_koi)     { DetachAudioStreamProcessor(a->sfx_koi.stream, sfx_tap_3); UnloadSound(a->sfx_koi); }
    if (a->has_sfx_win)     { DetachAudioStreamProcessor(a->sfx_win.stream, sfx_tap_4); UnloadSound(a->sfx_win); }
    if (a->has_sfx_lose)    { DetachAudioStreamProcessor(a->sfx_lose.stream, sfx_tap_5); UnloadSound(a->sfx_lose); }
    if (a->has_sfx_click)   { DetachAudioStreamProcessor(a->sfx_click.stream, sfx_tap_6); UnloadSound(a->sfx_click); }
    CloseAudioDevice();
}
