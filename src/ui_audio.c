#include "ui_audio.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ======================================================================
   Studio bus architecture on Raylib's mixer (raudio.c OnSendAudioDataToDevice):

   1. SFX Bus (all 7 SFX streams, loaded FIRST so Raylib runs them BEFORE music
      in every device callback with zero block latency):
        sfx_tap_N() -> writes live post-gain SFX signal into s_sc_bus[], leaving
        every SFX waveform 100% untouched.

   2. Music Bus:
      - Sidechain compressor keyed directly by s_sc_bus[] (3 ms attack, 115 ms
        release): every playing SFX carves its own transient pocket in the music
        and releases automatically to 0.00 dB once the SFX waveform ends.
      - Staged capture send -> tank -> M/S return -> mix (game tracks only):

          dry L/R ---------------------------------------------------+
            |                                                        |
            +-- HP 85 Hz -- LP 9 kHz -- pre-delay 25 ms              |
                  -- 4 damped combs -- 2 allpass diffusers -- wet    |
                                                                |    |
                       mid = (L+R)/2          side = (L-R)/2    |    |
                       mid : <220 Hz comp     side: 4th-order   |    |
                             + mono bass            HP 260 Hz   |    |
                             boost (x bass)         + 7 kHz     |    |
                                                    de-ess      |    |
                                                    (no boost)  |    |
                                                                v    v
                   dry (SFX-ducked) + wet_mid x rev + wet_side x stereo

   3. Master Bus (AttachAudioMixedProcessor on final Music + SFX mix):
      - 100 Hz high-passed detector bus compressor (catches spikes only above
        ~-1 dBFS at 2:1) + ceiling limiter (0.95 knee / 0.99 ceiling).
   ====================================================================== */

#define AUDIO_SR          44100.0f
#define PI_F              3.14159265f

/* ---- tunables -------------------------------------------------------- */
#define REV_IN_HP_HZ        85.0f   /* cut sub-rumble; keep 85-260 Hz body    */
#define REV_IN_LP_HZ      9000.0f   /* roll off brittle fizz > 9 kHz          */
#define REV_SIDE_HP_HZ     340.0f   /* 4th-order side HP: mono kick & bass    */
#define DEESS_HZ          7000.0f   /* side-only insurance de-esser split     */
#define DEESS_TH           0.025f   /* ignore normal air; catch harsh spikes  */
#define PRE_DELAY_MS        25.0f   /* equal L/R pre-delay (no Haas comb)     */
#define LOWCOMP_HZ         220.0f   /* mono low-body split on wet mid         */
#define LOWCOMP_TH         0.065f   /* keeps wet low end tight, no wobble     */
#define WET_MID_GAIN        0.90f   /* musical room return (-10..-6 dB W/D)   */
#define WET_SIDE_GAIN       1.35f   /* clear stereo bloom without drowning Mid*/
#define BASS_BOOST_GAIN     1.65f   /* natural stage 1, 2x mono weight stg 3  */
#define SFX_SC_THRESH      0.030f   /* SFX level where music bus duck starts  */
#define SFX_SC_STRENGTH     1.60f   /* ~2.5-4.5 dB pocket on whole music bus  */
#define SFX_SC_ATK_S       0.003f   /* fast 3 ms attack catches card snap     */
#define SFX_SC_REL_S       0.085f   /* smooth 85 ms rebound into the bloom    */
#define STAGE_ATTACK_S     0.032f   /* snappy swell on stage entry, no click  */
#define STAGE_TAIL_S       0.500f   /* clean room tail; fully off in ~3.3 s   */
#define BUSCOMP_HP_HZ      100.0f   /* master comp ignores bass weight        */
#define BUSCOMP_TH         0.890f   /* ~-1 dBFS: catches true spikes only     */
#define LIMIT_KNEE         0.950f
#define LIMIT_CEIL         0.990f

/* Per-event levels: stage 0 bypass, stages 1..3 as specified. */
static const float k_stage_rev[4]  = { 0.00f, 0.40f, 0.50f, 0.60f };
static const float k_stage_side[4] = { 0.00f, 0.20f, 0.40f, 0.50f };
static const float k_stage_bass[4] = { 0.00f, 0.20f, 0.20f, 0.40f };

/* ---- building blocks ------------------------------------------------- */
typedef struct {
    float b0, b1, b2, a1, a2;
    float x1, x2, y1, y2;
} Biquad;

/* RBJ cookbook Butterworth (Q = 0.7071). */
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

/* ---- reverb + M/S state ---------------------------------------------- */
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

static Biquad s_in_hp_l, s_in_lp_l, s_in_hp_r, s_in_lp_r;
static Biquad s_side_hp, s_side_hp2;
static int    s_dsp_ready;

static float s_side_deess_lp;
static float s_deess_env;
static float s_mid_lp1, s_mid_lp2;
static float s_low_env;
static float s_sc_env;
static float s_rev_env, s_side_env, s_bass_env;

/* Master bus processor state */
static float s_hp_lp_l, s_hp_lp_r;
static float s_comp_env;

static volatile int s_stage;

void audio_set_capture_stage(int stage) {
    if (stage < 0) stage = 0;
    if (stage > 3) stage = 3;
    s_stage = stage;
}

static void audio_reset_stage_state(void) {
    s_stage = 0;
    s_rev_env = s_side_env = s_bass_env = 0.0f;
}

static void dsp_init_once(void) {
    if (s_dsp_ready) return;
    biquad_design(&s_in_hp_l, REV_IN_HP_HZ, 1);
    biquad_design(&s_in_hp_r, REV_IN_HP_HZ, 1);
    biquad_design(&s_in_lp_l, REV_IN_LP_HZ, 0);
    biquad_design(&s_in_lp_r, REV_IN_LP_HZ, 0);
    biquad_design(&s_side_hp,  REV_SIDE_HP_HZ, 1);
    biquad_design(&s_side_hp2, REV_SIDE_HP_HZ, 1);
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

/* Game music: live SFX sidechain + staged capture send -> M/S return -> mix. */
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

    /* All rates are true per-sample constants (invariant across block sizes). */
    const float stg_atk   = sample_tau(STAGE_ATTACK_S);
    const float stg_rel   = sample_tau(STAGE_TAIL_S);
    const float sfx_atk   = sample_tau(SFX_SC_ATK_S);
    const float sfx_rel   = sample_tau(SFX_SC_REL_S);
    const float deess_atk = sample_tau(0.003f), deess_rel = sample_tau(0.080f);
    const float low_atk   = sample_tau(0.020f), low_rel   = sample_tau(0.150f);
    const float wetsc_atk = sample_tau(0.005f), wetsc_rel = sample_tau(0.090f);

    const float k_deess = onepole_k(DEESS_HZ);
    const float k_midlp = onepole_k(LOWCOMP_HZ);
    const float k_schp  = onepole_k(BUSCOMP_HP_HZ);
    const float comp_atk = sample_tau(0.025f), comp_rel = sample_tau(0.100f);

    for (unsigned int f = 0; f < frames; f++) {
        float dry_l = pcm[f * 2 + 0];
        float dry_r = pcm[f * 2 + 1];

        /* Advance stage envelopes per sample for exact timing at any block size. */
        s_rev_env  += (t_rev  - s_rev_env)  * ((t_rev  > s_rev_env)  ? stg_atk : stg_rel);
        s_side_env += (t_side - s_side_env) * ((t_side > s_side_env) ? stg_atk : stg_rel);
        s_bass_env += (t_bass - s_bass_env) * ((t_bass > s_bass_env) ? stg_atk : stg_rel);
        if (t_rev  == 0.0f && s_rev_env  < 0.0008f) s_rev_env  = 0.0f;
        if (t_side == 0.0f && s_side_env < 0.0008f) s_side_env = 0.0f;
        if (t_bass == 0.0f && s_bass_env < 0.0008f) s_bass_env = 0.0f;

        /* Live SFX sidechain envelope */
        float sfx_key = sc_bus_read_sample();
        s_sfx_sc_env += (sfx_key - s_sfx_sc_env) * ((sfx_key > s_sfx_sc_env) ? sfx_atk : sfx_rel);
        if (sfx_key == 0.0f && s_sfx_sc_env < 0.0002f) s_sfx_sc_env = 0.0f;

        /* 1. Reverb input EQ + pre-delay always stay warm so entry never clicks. */
        float bp_l = biquad_run(&s_in_lp_l, biquad_run(&s_in_hp_l, dry_l));
        float bp_r = biquad_run(&s_in_lp_r, biquad_run(&s_in_hp_r, dry_r));

        float slap_l = s_pre_buf_l[s_pre_idx_l];
        float slap_r = s_pre_buf_r[s_pre_idx_r];
        s_pre_buf_l[s_pre_idx_l] = bp_l;
        s_pre_buf_r[s_pre_idx_r] = bp_r;
        if (++s_pre_idx_l >= s_pre_len) s_pre_idx_l = 0;
        if (++s_pre_idx_r >= s_pre_len) s_pre_idx_r = 0;

        int wet_active  = (s_rev_env > 0.0f) || (s_side_env > 0.0f) || (s_bass_env > 0.0f);
        int duck_active = (s_sfx_sc_env > SFX_SC_THRESH);

        /* Stage 0 with no active SFX: true bit-exact bypass. */
        if (!wet_active && !duck_active) {
            continue;
        }

        float out_l = dry_l;
        float out_r = dry_r;

        if (wet_active) {
            /* 2. Tank (4 parallel damped combs + 2 series allpass diffusers). */
            float feed_l = bp_l * 0.62f + slap_r * 0.18f;
            float feed_r = bp_r * 0.62f + slap_l * 0.18f;
            const float feedback = 0.68f;
            const float damp = 0.40f;
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

            float wet_l = acc_l + slap_l * 0.30f;
            float wet_r = acc_r + slap_r * 0.30f;

            /* 3. M/S split on the wet return: Mid keeps body, Side gets width. */
            float wet_mid  = 0.5f * (wet_l + wet_r);
            float wet_side = 0.5f * (wet_l - wet_r);

            /* Mid: 2-pole low split (<220 Hz) -> low-band compressor (no wobble)
               -> mono bass controlled independently by k_stage_bass[], upper body
               controlled independently by k_stage_rev[]. */
            s_mid_lp1 += (wet_mid - s_mid_lp1) * k_midlp;
            s_mid_lp2 += (s_mid_lp1 - s_mid_lp2) * k_midlp;
            float mid_low  = s_mid_lp2;
            float mid_high = wet_mid - mid_low;

            float la = fabsf(mid_low);
            s_low_env += (la - s_low_env) * ((la > s_low_env) ? low_atk : low_rel);
            if (s_low_env > LOWCOMP_TH) {
                mid_low *= (LOWCOMP_TH + (s_low_env - LOWCOMP_TH) * 0.35f) / s_low_env;
            }
            float mid_out = mid_high * (WET_MID_GAIN * s_rev_env) +
                            mid_low  * (BASS_BOOST_GAIN * s_bass_env);

            /* Side: 4th-order 340 Hz HP (kick/bass out of Side) -> 7 kHz insurance
               de-esser -> unity (no post-boost). */
            float side_hp = biquad_run(&s_side_hp2, biquad_run(&s_side_hp, wet_side));
            s_side_deess_lp += (side_hp - s_side_deess_lp) * k_deess;
            float sib = side_hp - s_side_deess_lp;
            float sa = fabsf(sib);
            s_deess_env += (sa - s_deess_env) * ((sa > s_deess_env) ? deess_atk : deess_rel);
            float deess_g = (s_deess_env > DEESS_TH)
                          ? (DEESS_TH + (s_deess_env - DEESS_TH) * 0.35f) / s_deess_env
                          : 1.0f;
            float side_out = (s_side_deess_lp + sib * deess_g) * (WET_SIDE_GAIN * s_side_env);

            /* 4. Band-matched dry-keyed duck on the wet tail so loud sections stay clear. */
            float key_dry = 0.5f * (fabsf(bp_l) + fabsf(bp_r));
            s_sc_env += (key_dry - s_sc_env) * ((key_dry > s_sc_env) ? wetsc_atk : wetsc_rel);
            float sc_over = (s_sc_env > 0.12f) ? (s_sc_env - 0.12f) : 0.0f;
            float sc_dim  = 1.0f / (1.0f + 1.25f * sc_over);

            out_l += (mid_out + side_out) * sc_dim;
            out_r += (mid_out - side_out) * sc_dim;
        }

        /* 5. Music group sidechain duck (keyed by ALL live SFX streams): ducks the
           entire Music group (dry + wet tail together) so every SFX cuts through
           even at full Stage 3 reverb, then releases cleanly into the bloom. */
        if (duck_active) {
            float duck_g = 1.0f / (1.0f + SFX_SC_STRENGTH * (s_sfx_sc_env - SFX_SC_THRESH));
            out_l *= duck_g;
            out_r *= duck_g;
        }

        /* 5. Gentle spike-only bus compressor (100 Hz HPF detector) + ceiling limiter. */
        s_hp_lp_l += (out_l - s_hp_lp_l) * k_schp;
        s_hp_lp_r += (out_r - s_hp_lp_r) * k_schp;
        float pk = fmaxf(fabsf(out_l - s_hp_lp_l), fabsf(out_r - s_hp_lp_r));
        s_comp_env += (pk - s_comp_env) * ((pk > s_comp_env) ? comp_atk : comp_rel);
        if (s_comp_env > BUSCOMP_TH) {
            float cg = (BUSCOMP_TH + (s_comp_env - BUSCOMP_TH) * 0.50f) / s_comp_env;
            out_l *= cg;
            out_r *= cg;
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
