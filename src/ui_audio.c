#include "ui_audio.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ======================================================================
   Capture reverb + stereo + mono bass, staged over a capture streak.

   The game only reports how many capture events are stacked:
       audio_set_capture_stage(0..3)
   Everything else lives here, so tuning means editing numbers in one block.

   Signal flow - one send, dry stays mono-safe:

     dry L/R ----------------------------------------------+
       |                                                    |
       +-- HP 250 -- LP 9k -- pre-delay 29 ms               |
             -- 4 combs -- 2 allpass -- wet L/R             |
                  |                                         |
          mid = (L+R)/2   side = (L-R)/2                     |
          mid : low-band comp, +bass shelf                   |
          side: HP 250 (2nd order), de-ess 7k, no boost      |
                  |                                         |
          wet_mid x rev + wet_side x stereo ------------------+
                                                             v
              dry + wet -> duck -> bus comp (HPF detector) -> limiter
   ====================================================================== */

#define AUDIO_SR 44100.0f
#define PI_F     3.14159265f

/* ---- tunables -------------------------------------------------------- */
#define REV_IN_HP_HZ      250.0f   /* low cut into the reverb send           */
#define REV_IN_LP_HZ     9000.0f   /* high cut into the reverb send          */
#define REV_SIDE_HP_HZ    250.0f   /* low cut on the wet side (mono lows)    */
#define DEESS_HZ         7000.0f   /* side-only ear protection               */
#define PRE_DELAY_MS       29.0f   /* 20-60 ms reads as space, not slapback  */
#define BASS_SHELF_HZ     120.0f   /* mono bass boost corner                 */
#define BASS_BOOST_DB       6.0f   /* boost at full bass stage               */
#define LOWCOMP_HZ        200.0f   /* low-band compressor on the wet mid     */
#define LOWCOMP_TH        0.155f   /* ~-16 dBFS                              */
#define BUSCOMP_HP_HZ     100.0f   /* detector low cut: bass drives nothing  */
#define BUSCOMP_TH        0.89f    /* ~-1 dBFS: spikes only                  */
#define LIMIT_KNEE        0.94f
#define LIMIT_CEIL        0.99f
#define DUCK_MAX_DB         2.5f   /* music dip under a capture SFX          */
#define STAGE_ATTACK_S     0.04f   /* soft entry, no click                   */
#define STAGE_TAIL_S       2.50f   /* the tail: stays up while cards fly     */

/* Per-event levels: stage 0 silent, 1..3 as designed. */
static const float k_stage_rev[4]  = { 0.00f, 0.40f, 0.50f, 0.60f };
static const float k_stage_side[4] = { 0.00f, 0.20f, 0.40f, 0.50f };
static const float k_stage_bass[4] = { 0.00f, 0.20f, 0.20f, 0.40f };

/* ---- building blocks ------------------------------------------------- */
typedef struct {
    float b0, b1, b2, a1, a2;
    float x1, x2, y1, y2;
} Biquad;

/* RBJ cookbook, Q = 0.7071 (Butterworth), designed for 44.1 kHz. */
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

/* One-pole coefficient for a -3 dB corner at fc. */
static float onepole_k(float fc) { return 1.0f - expf(-2.0f * PI_F * fc / AUDIO_SR); }

/* Soft-knee peak limiter: 1:1 below the knee, smoothly bounded above it. */
static float soft_knee_limit(float x, float knee, float max_val) {
    float ax = (x < 0.0f) ? -x : x;
    if (ax <= knee) return x;
    float span = max_val - knee;
    float u = (ax - knee) / (span + (ax - knee));
    float out = knee + span * u;
    return (x < 0.0f) ? -out : out;
}

/* ---- state ---------------------------------------------------------- */
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
static float s_mid_low_lp;
static float s_bass_lp;
static float s_low_env;
static float s_sc_env;
static float s_duck_env;
static float s_hp_lp_l, s_hp_lp_r;
static float s_comp_env;

static float s_rev_env, s_side_env, s_bass_env;

static volatile int   s_stage;
static volatile float s_duck_target;

void audio_set_capture_stage(int stage) {
    if (stage < 0) stage = 0;
    if (stage > 3) stage = 3;
    s_stage = stage;
}

void audio_duck_music(float amount) {
    if (amount < 0.0f) amount = 0.0f;
    if (amount > 1.0f) amount = 1.0f;
    s_duck_target = amount;
}

static void dsp_init_once(void) {
    if (s_dsp_ready) return;
    biquad_design(&s_in_hp_l, REV_IN_HP_HZ, 1);
    biquad_design(&s_in_hp_r, REV_IN_HP_HZ, 1);
    biquad_design(&s_in_lp_l, REV_IN_LP_HZ, 0);
    biquad_design(&s_in_lp_r, REV_IN_LP_HZ, 0);
    biquad_design(&s_side_hp, REV_SIDE_HP_HZ, 1);
    biquad_design(&s_side_hp2, REV_SIDE_HP_HZ, 1);   /* 4th order: lows really leave the side */
    s_pre_len = (int)(PRE_DELAY_MS * AUDIO_SR / 1000.0f);
    if (s_pre_len < 1) s_pre_len = 1;
    if (s_pre_len >= PREDELAY_MAX) s_pre_len = PREDELAY_MAX - 1;
    s_pre_idx_l = s_pre_idx_r = 0;
    s_dsp_ready = 1;
}

/* Block-rate one-pole follower; the tail is what makes captures overlap. */
static void advance_env(float *cur, float target, float dt, float attack, float release) {
    float tau = (target > *cur) ? attack : release;
    float k = 1.0f - expf(-dt / tau);
    *cur += (target - *cur) * k;
}

/* ---- the processor --------------------------------------------------- */
static void music_reverb_processor(void *buffer, unsigned int frames) {
    float *pcm = (float *)buffer;
    if (!pcm || frames == 0) return;
    dsp_init_once();

    float dt = (float)frames / AUDIO_SR;
    int stage = s_stage;
    if (stage < 0) stage = 0;
    if (stage > 3) stage = 3;
    advance_env(&s_rev_env,  k_stage_rev[stage],  dt, STAGE_ATTACK_S, STAGE_TAIL_S);
    advance_env(&s_side_env, k_stage_side[stage], dt, STAGE_ATTACK_S, STAGE_TAIL_S);
    advance_env(&s_bass_env, k_stage_bass[stage], dt, STAGE_ATTACK_S, STAGE_TAIL_S);
    advance_env(&s_duck_env, s_duck_target, dt, 0.005f, 0.150f);

    const float rev    = s_rev_env;
    const float side_g = s_side_env;
    const float bass_g = powf(10.0f, (BASS_BOOST_DB * s_bass_env) / 20.0f);
    const float duck_g = powf(10.0f, (-DUCK_MAX_DB * s_duck_env) / 20.0f);

    const float k_deess = onepole_k(DEESS_HZ);
    const float k_midlp = onepole_k(LOWCOMP_HZ);
    const float k_bass  = onepole_k(BASS_SHELF_HZ);
    const float k_schp  = onepole_k(BUSCOMP_HP_HZ);

    /* envelope rates, expressed once per block */
    const float deess_atk = 1.0f - expf(-dt / 0.003f), deess_rel = 1.0f - expf(-dt / 0.080f);
    const float low_atk   = 1.0f - expf(-dt / 0.025f), low_rel   = 1.0f - expf(-dt / 0.180f);
    const float sc_atk    = 1.0f - expf(-dt / 0.003f), sc_rel    = 1.0f - expf(-dt / 0.080f);
    const float comp_atk  = 1.0f - expf(-dt / 0.030f), comp_rel  = 1.0f - expf(-dt / 0.100f);

    int active = (rev > 0.0005f) || (side_g > 0.0005f) || (s_bass_env > 0.0005f);

    for (unsigned int f = 0; f < frames; f++) {
        float dry_l = pcm[f * 2 + 0];
        float dry_r = pcm[f * 2 + 1];
        float out_l = dry_l * duck_g;
        float out_r = dry_r * duck_g;

        /* 1. EQ into the send: low cut, high cut. Runs every block so the
              pre-delay line is never fed stale or unfiltered samples. */
        float bp_l = biquad_run(&s_in_lp_l, biquad_run(&s_in_hp_l, dry_l));
        float bp_r = biquad_run(&s_in_lp_r, biquad_run(&s_in_hp_r, dry_r));

        /* 2. Pre-delay, same time both sides (no Haas slapback). */
        float slap_l = s_pre_buf_l[s_pre_idx_l];
        float slap_r = s_pre_buf_r[s_pre_idx_r];
        s_pre_buf_l[s_pre_idx_l] = bp_l;
        s_pre_buf_r[s_pre_idx_r] = bp_r;
        if (++s_pre_idx_l >= s_pre_len) s_pre_idx_l = 0;
        if (++s_pre_idx_r >= s_pre_len) s_pre_idx_r = 0;

        if (active) {
            /* 3. Tank. */
            float feed_l = bp_l * 0.50f + slap_r * 0.12f;
            float feed_r = bp_r * 0.50f + slap_l * 0.12f;
            const float feedback = 0.68f;
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
            acc_l *= 0.38f;
            acc_r *= 0.38f;

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

            float wet_l = acc_l + slap_l * 0.24f;
            float wet_r = acc_r + slap_r * 0.24f;

            /* 4. M/S split on the wet: mid keeps the body, side carries width. */
            float wet_mid  = 0.5f * (wet_l + wet_r);
            float wet_side = 0.5f * (wet_l - wet_r);

            /* mid: low-band compressor so the bloom cannot wobble, then bass shelf */
            s_mid_low_lp += (wet_mid - s_mid_low_lp) * k_midlp;
            float mid_low  = s_mid_low_lp;
            float mid_high = wet_mid - mid_low;
            float la = (mid_low < 0.0f) ? -mid_low : mid_low;
            s_low_env += (la - s_low_env) * ((la > s_low_env) ? low_atk : low_rel);
            if (s_low_env > LOWCOMP_TH) {
                mid_low *= (LOWCOMP_TH + (s_low_env - LOWCOMP_TH) * 0.5f) / s_low_env;
            }
            float mid_out = mid_high + mid_low;
            /* bass shelf below BASS_SHELF_HZ, on the mono mid only */
            s_bass_lp += (mid_out - s_bass_lp) * k_bass;
            mid_out += s_bass_lp * (bass_g - 1.0f);

            /* side: 2nd-order low cut, gentle de-ess, unity gain - no boost */
            float side_hp = biquad_run(&s_side_hp2, biquad_run(&s_side_hp, wet_side));
            s_side_deess_lp += (side_hp - s_side_deess_lp) * k_deess;
            float sib = side_hp - s_side_deess_lp;
            float sa = (sib < 0.0f) ? -sib : sib;
            s_deess_env += (sa - s_deess_env) * ((sa > s_deess_env) ? deess_atk : deess_rel);
            float deess_gain = 1.0f / (1.0f + 2.0f * s_deess_env);
            float side_out = s_side_deess_lp + sib * deess_gain;

            /* 5. Duck the tail under the dry music (textbook reverb ducking). */
            float dry_abs = 0.5f * ((dry_l < 0.0f ? -dry_l : dry_l) +
                                    (dry_r < 0.0f ? -dry_r : dry_r));
            s_sc_env += (dry_abs - s_sc_env) * ((dry_abs > s_sc_env) ? sc_atk : sc_rel);
            float sc_over = (s_sc_env > 0.08f) ? (s_sc_env - 0.08f) : 0.0f;
            float sc_dim = 1.0f / (1.0f + 1.6f * sc_over);

            float wet_gain = rev * sc_dim;
            float mid_wet  = mid_out  * wet_gain;
            float side_wet = side_out * (side_g * sc_dim);
            out_l += mid_wet + side_wet;
            out_r += mid_wet - side_wet;
        }

        /* 6. Bus compressor: detector high-passed, so bass cannot pump it.
              Only spikes above ~-1 dBFS are caught, at 2:1. */
        s_hp_lp_l += (out_l - s_hp_lp_l) * k_schp;
        s_hp_lp_r += (out_r - s_hp_lp_r) * k_schp;
        float hl = out_l - s_hp_lp_l, hr = out_r - s_hp_lp_r;
        float pk = fmaxf(fabsf(hl), fabsf(hr));
        s_comp_env += (pk - s_comp_env) * ((pk > s_comp_env) ? comp_atk : comp_rel);
        if (s_comp_env > BUSCOMP_TH) {
            float over = s_comp_env - BUSCOMP_TH;
            float cg = (BUSCOMP_TH + over * 0.50f) / s_comp_env;
            out_l *= cg;
            out_r *= cg;
        }

        pcm[f * 2 + 0] = soft_knee_limit(out_l, LIMIT_KNEE, LIMIT_CEIL);
        pcm[f * 2 + 1] = soft_knee_limit(out_r, LIMIT_KNEE, LIMIT_CEIL);
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

    const char *p_menu = get_path("assets/music/menu.mp3");
    if (FileExists(p_menu)) {
        a->music_menu = LoadMusicStream(p_menu);
        a->has_music_menu = 1;
        SetMusicVolume(a->music_menu, a->bgm_volume);
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

    const char *p_flip = get_path("assets/sfx/card_flip.wav");
    if (FileExists(p_flip)) {
        a->sfx_flip = LoadSound(p_flip); a->has_sfx_flip = 1;
        SetSoundVolume(a->sfx_flip, clamp_range(a->sfx_volume * 1.90f, 0.0f, 1.0f));
    }
    const char *p_drop = get_path("assets/sfx/card_drop.wav");
    if (FileExists(p_drop)) {
        a->sfx_drop = LoadSound(p_drop); a->has_sfx_drop = 1;
        SetSoundVolume(a->sfx_drop, clamp_range(a->sfx_volume * 1.90f, 0.0f, 1.0f));
    }
    const char *p_cap = get_path("assets/sfx/capture.wav");
    if (FileExists(p_cap)) {
        a->sfx_capture = LoadSound(p_cap); a->has_sfx_capture = 1;
        SetSoundVolume(a->sfx_capture, clamp_range(a->sfx_volume * 1.90f, 0.0f, 1.0f));
    }
    const char *p_koi = get_path("assets/sfx/koi_call.wav");
    if (FileExists(p_koi)) {
        a->sfx_koi = LoadSound(p_koi); a->has_sfx_koi = 1;
        SetSoundVolume(a->sfx_koi, a->sfx_volume);
    }
    const char *p_win = get_path("assets/sfx/win.wav");
    if (FileExists(p_win)) {
        a->sfx_win = LoadSound(p_win); a->has_sfx_win = 1;
        SetSoundVolume(a->sfx_win, a->sfx_volume);
    }
    const char *p_lose = get_path("assets/sfx/lose.wav");
    if (FileExists(p_lose)) {
        a->sfx_lose = LoadSound(p_lose); a->has_sfx_lose = 1;
        SetSoundVolume(a->sfx_lose, a->sfx_volume);
    }
    const char *p_click = get_path("assets/sfx/click.wav");
    if (FileExists(p_click)) {
        a->sfx_click = LoadSound(p_click); a->has_sfx_click = 1;
        SetSoundVolume(a->sfx_click, a->sfx_volume);
    }
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
    audio_set_capture_stage(0);
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
    audio_set_capture_stage(0);
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
    audio_duck_music(1.0f);   /* duck the music instead of turning the SFX up */
}
void audio_play_koi(AudioAssets *a)     { if (a->has_sfx_koi) PlaySound(a->sfx_koi); }
void audio_play_win(AudioAssets *a)     { if (a->has_sfx_win) PlaySound(a->sfx_win); }
void audio_play_lose(AudioAssets *a)    { if (a->has_sfx_lose) PlaySound(a->sfx_lose); }
void audio_play_click(AudioAssets *a)   { if (a->has_sfx_click) PlaySound(a->sfx_click); }

void audio_close(AudioAssets *a) {
    if (a->has_music_menu) {
        UnloadMusicStream(a->music_menu);
    }
    for (int i = 0; i < 10; i++) {
        if (a->has_music_game[i]) {
            DetachAudioStreamProcessor(a->music_game[i].stream, music_reverb_processor);
            UnloadMusicStream(a->music_game[i]);
        }
    }
    if (a->has_sfx_flip) UnloadSound(a->sfx_flip);
    if (a->has_sfx_drop) UnloadSound(a->sfx_drop);
    if (a->has_sfx_capture) UnloadSound(a->sfx_capture);
    if (a->has_sfx_koi) UnloadSound(a->sfx_koi);
    if (a->has_sfx_win) UnloadSound(a->sfx_win);
    if (a->has_sfx_lose) UnloadSound(a->sfx_lose);
    if (a->has_sfx_click) UnloadSound(a->sfx_click);
    CloseAudioDevice();
}
