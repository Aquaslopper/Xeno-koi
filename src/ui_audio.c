#include "ui_audio.h"
#include <stdio.h>
#include <string.h>

/* Real-time stereo reverb + subtle Haas widener attached to music streams.
   Driven by a smooth, hard-limited synergy curve via audio_set_reverb(amount):
   starts subtle on the first capture, blooms on stacked captures, and ducks its
   tail under the dry music so it never smears or spikes. */
#define REVERB_COMB_COUNT 4
#define REVERB_COMB_MAX   5504
#define REVERB_AP_COUNT   2
#define REVERB_AP_MAX     640
#define REVERB_PREDELAY_MAX 6400

static const int k_comb_len_l[REVERB_COMB_COUNT] = { 2903, 3571, 4219, 4967 };
static const int k_comb_len_r[REVERB_COMB_COUNT] = { 3251, 3917, 4603, 5381 };
static const int k_ap_len_l[REVERB_AP_COUNT]     = { 431, 163 };
static const int k_ap_len_r[REVERB_AP_COUNT]     = { 509, 211 };
static const int k_pre_len_l = 3607; /* ~75 ms stereo slapback L */
static const int k_pre_len_r = 5897; /* ~123 ms stereo slapback R */

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

static float s_pre_buf_l[REVERB_PREDELAY_MAX];
static float s_pre_buf_r[REVERB_PREDELAY_MAX];
static int   s_pre_idx_l = 0;
static int   s_pre_idx_r = 0;

typedef struct {
    float x1, x2;
    float y1, y2;
} BiquadState;

static BiquadState s_in_hp_l, s_in_hp_r;
static BiquadState s_in_lp_l, s_in_lp_r;
static BiquadState s_out_hp_l, s_out_hp_r;
static BiquadState s_out_lp_l, s_out_lp_r;

static float s_sc_env = 0.0f;
static float s_mid_bass_lp = 0.0f;
static float s_side_low_lp = 0.0f;
static float s_side_deess_lp = 0.0f;
static float s_deess_env = 0.0f;
static float s_comp_env = 0.0f;

static volatile float s_reverb_target = 0.0f;
static float s_reverb_cur = 0.0f;

void audio_set_reverb(float amount) {
    if (amount < 0.0f) amount = 0.0f;
    if (amount > 1.0f) amount = 1.0f;
    s_reverb_target = amount;
}

static float clamp_range(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

/* 2nd-order Butterworth Low-Cut (72 Hz, 12 dB/oct) matching the reverb EQ curve */
static float biquad_hp72(BiquadState *s, float x) {
    const float b0 =  0.9927726f, b1 = -1.9855451f, b2 = 0.9927726f;
    const float a1 = -1.9854929f, a2 =  0.9855974f;
    float y = b0 * x + b1 * s->x1 + b2 * s->x2 - a1 * s->y1 - a2 * s->y2;
    s->x2 = s->x1; s->x1 = x;
    s->y2 = s->y1; s->y1 = y;
    return y;
}

/* 2nd-order Butterworth High-Cut (3.6 kHz, 12 dB/oct) matching the reverb EQ curve */
static float biquad_lp3600(BiquadState *s, float x) {
    const float b0 =  0.0477663f, b1 = 0.0955325f, b2 = 0.0477663f;
    const float a1 = -1.2937273f, a2 = 0.4847923f;
    float y = b0 * x + b1 * s->x1 + b2 * s->x2 - a1 * s->y1 - a2 * s->y2;
    s->x2 = s->x1; s->x1 = x;
    s->y2 = s->y1; s->y1 = y;
    return y;
}

/* Soft-knee peak limiter: 1:1 linear below knee, smoothly compressed above knee */
static float soft_knee_limit(float x, float knee, float max_val) {
    float ax = (x < 0.0f) ? -x : x;
    if (ax <= knee) return x;
    float span = max_val - knee;
    float u = (ax - knee) / (span + (ax - knee));
    float out = knee + span * u;
    return (x < 0.0f) ? -out : out;
}

static void music_reverb_processor(void *buffer, unsigned int frames) {
    float *pcm = (float *)buffer;
    if (!pcm || frames == 0) return;
    float target = clamp_range(s_reverb_target, 0.0f, 1.0f);

    for (unsigned int f = 0; f < frames; f++) {
        float rate = (target > s_reverb_cur) ? 0.0018f : 0.0012f;
        s_reverb_cur += (target - s_reverb_cur) * rate;
        float rev = clamp_range(s_reverb_cur, 0.0f, 1.0f);
        float dry_l = pcm[f * 2 + 0];
        float dry_r = pcm[f * 2 + 1];

        /* 1. Sidechain detector on dry music (dims the reverb during loud beats, no heavy dry ducking) */
        float dry_abs = 0.5f * ((dry_l < 0.0f ? -dry_l : dry_l) + (dry_r < 0.0f ? -dry_r : dry_r));
        float sc_rate = (dry_abs > s_sc_env) ? 0.008f : 0.00035f;
        s_sc_env += (dry_abs - s_sc_env) * sc_rate;
        float sc_over = (s_sc_env > 0.08f) ? (s_sc_env - 0.08f) : 0.0f;
        float sc_dim = 1.0f / (1.0f + 2.4f * sc_over);

        /* 2. Abbey Road Reverb Bandpass (72 Hz Low-Cut + 3.6 kHz High-Cut, flat 0 dB from 150 Hz..2 kHz)
              applied once going into the tank so bass never smears and highs never resonate/spike */
        float bp_in_l = biquad_lp3600(&s_in_lp_l, biquad_hp72(&s_in_hp_l, dry_l));
        float bp_in_r = biquad_lp3600(&s_in_lp_r, biquad_hp72(&s_in_hp_r, dry_r));

        float slap_l = s_pre_buf_l[s_pre_idx_l];
        float slap_r = s_pre_buf_r[s_pre_idx_r];
        s_pre_buf_l[s_pre_idx_l] = clamp_range(bp_in_l * 0.52f + slap_r * 0.20f, -0.70f, 0.70f);
        s_pre_buf_r[s_pre_idx_r] = clamp_range(bp_in_r * 0.52f - slap_l * 0.20f, -0.70f, 0.70f);
        if (++s_pre_idx_l >= k_pre_len_l) s_pre_idx_l = 0;
        if (++s_pre_idx_r >= k_pre_len_r) s_pre_idx_r = 0;

        float tail_releasing = (target + 0.02f < s_reverb_cur) ? 1.0f : 0.0f;
        float feed_l = bp_in_l * 0.52f + slap_r * 0.22f;
        float feed_r = bp_in_r * 0.52f + slap_l * 0.22f;
        float feedback = 0.70f - 0.10f * tail_releasing;
        const float damp = 0.44f;
        float acc_l = 0.0f, acc_r = 0.0f;

        for (int c = 0; c < REVERB_COMB_COUNT; c++) {
            int il = s_comb_idx_l[c];
            float out_l = s_comb_buf_l[c][il];
            s_comb_lp_l[c] = out_l * (1.0f - damp) + s_comb_lp_l[c] * damp;
            s_comb_buf_l[c][il] = clamp_range(feed_l + s_comb_lp_l[c] * feedback, -0.80f, 0.80f);
            if (++il >= k_comb_len_l[c]) il = 0;
            s_comb_idx_l[c] = il;
            acc_l += s_comb_lp_l[c];

            int ir = s_comb_idx_r[c];
            float out_r = s_comb_buf_r[c][ir];
            s_comb_lp_r[c] = out_r * (1.0f - damp) + s_comb_lp_r[c] * damp;
            s_comb_buf_r[c][ir] = clamp_range(feed_r + s_comb_lp_r[c] * feedback, -0.80f, 0.80f);
            if (++ir >= k_comb_len_r[c]) ir = 0;
            s_comb_idx_r[c] = ir;
            acc_r += s_comb_lp_r[c];
        }

        acc_l *= 0.40f;
        acc_r *= 0.40f;

        for (int a = 0; a < REVERB_AP_COUNT; a++) {
            int il = s_ap_idx_l[a];
            float buf_l = s_ap_buf_l[a][il];
            float in_l = acc_l;
            acc_l = -in_l + buf_l;
            s_ap_buf_l[a][il] = in_l + buf_l * 0.48f;
            if (++il >= k_ap_len_l[a]) il = 0;
            s_ap_idx_l[a] = il;

            int ir = s_ap_idx_r[a];
            float buf_r = s_ap_buf_r[a][ir];
            float in_r = acc_r;
            acc_r = -in_r + buf_r;
            s_ap_buf_r[a][ir] = in_r + buf_r * 0.48f;
            if (++ir >= k_ap_len_r[a]) ir = 0;
            s_ap_idx_r[a] = ir;
        }

        /* Post-tank Low-Cut (72 Hz) + High-Cut (3.6 kHz) */
        float wet_bp_l = biquad_lp3600(&s_out_lp_l, biquad_hp72(&s_out_hp_l, acc_l + slap_l * 0.28f));
        float wet_bp_r = biquad_lp3600(&s_out_lp_r, biquad_hp72(&s_out_hp_r, acc_r + slap_r * 0.28f));

        /* 3. Mid/Side (M/S) stage:
              - Mid keeps the warm low-mids; dry Mid lows get a gentle warmth boost (+1.2 dB)
              - Side has lows (<180 Hz) cut out so bass stays centered in Mid, and highs (>2.6 kHz)
                are dynamically de-essed before going into the gentle stereo spread */
        float dry_mid  = 0.5f * (dry_l + dry_r);
        float dry_side = 0.5f * (dry_l - dry_r);
        s_mid_bass_lp += (dry_mid - s_mid_bass_lp) * 0.020f; /* ~140 Hz Mid low-pass */

        float wet_mid  = 0.5f * (wet_bp_l + wet_bp_r);
        float wet_side = 0.5f * (wet_bp_l - wet_bp_r);
        s_side_low_lp += (wet_side - s_side_low_lp) * 0.025f; /* ~180 Hz Side low-cut */
        float side_hi = wet_side - s_side_low_lp;

        /* De-ess high frequencies in the stereo Side channel */
        s_side_deess_lp += (side_hi - s_side_deess_lp) * 0.30f; /* ~2.6 kHz split */
        float side_sibilance = side_hi - s_side_deess_lp;
        float sib_abs = (side_sibilance < 0.0f) ? -side_sibilance : side_sibilance;
        float deess_rate = (sib_abs > s_deess_env) ? 0.08f : 0.0015f;
        s_deess_env += (sib_abs - s_deess_env) * deess_rate;
        float deess_gain = 1.0f / (1.0f + 7.5f * s_deess_env);
        float clean_wet_side = s_side_deess_lp + side_sibilance * deess_gain * 0.60f;

        if (rev > 0.0005f) {
            /* Gentle stereo width (0.30 on 1st card -> 0.70 max on 3rd card) with loudness compensation */
            float stereo_w = 0.30f + 0.40f * (rev * rev);
            float stereo_comp = 1.0f / (1.0f + 0.22f * stereo_w);
            float wet_gain = rev * 0.78f * sc_dim * stereo_comp * (1.0f - 0.18f * tail_releasing);

            /* Gentle dry ducking (max 12%) + gentle Mid low boost */
            float dry_gain = 1.0f - rev * 0.12f;
            float out_mid  = dry_mid * dry_gain + s_mid_bass_lp * (0.12f * rev) + wet_mid * wet_gain;
            float out_side = dry_side * dry_gain + clean_wet_side * (wet_gain * stereo_w);

            float out_l = out_mid + out_side;
            float out_r = out_mid - out_side;

            /* 4. Master Glue Bus Compressor (catches spikes above 0.34, 3.2:1 ratio) + safety ceiling */
            float pk = (out_l < 0.0f ? -out_l : out_l);
            float pk_r = (out_r < 0.0f ? -out_r : out_r);
            if (pk_r > pk) pk = pk_r;
            float comp_rate = (pk > s_comp_env) ? 0.012f : 0.0003f;
            s_comp_env += (pk - s_comp_env) * comp_rate;
            const float thresh = 0.34f;
            if (s_comp_env > thresh) {
                float over = s_comp_env - thresh;
                float target_lvl = thresh + over * 0.31f;
                float cg = target_lvl / s_comp_env;
                out_l *= cg;
                out_r *= cg;
            }
            pcm[f * 2 + 0] = soft_knee_limit(out_l, 0.48f, 0.62f);
            pcm[f * 2 + 1] = soft_knee_limit(out_r, 0.48f, 0.62f);
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

    const char *p_menu = get_path("assets/music/menu.mp3");
    if (FileExists(p_menu)) {
        a->music_menu = LoadMusicStream(p_menu);
        a->has_music_menu = 1;
        SetMusicVolume(a->music_menu, a->bgm_volume);
        AttachAudioStreamProcessor(a->music_menu.stream, music_reverb_processor);
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
}
void audio_play_koi(AudioAssets *a)     { if (a->has_sfx_koi) PlaySound(a->sfx_koi); }
void audio_play_win(AudioAssets *a)     { if (a->has_sfx_win) PlaySound(a->sfx_win); }
void audio_play_lose(AudioAssets *a)    { if (a->has_sfx_lose) PlaySound(a->sfx_lose); }
void audio_play_click(AudioAssets *a)   { if (a->has_sfx_click) PlaySound(a->sfx_click); }

void audio_close(AudioAssets *a) {
    if (a->has_music_menu) {
        DetachAudioStreamProcessor(a->music_menu.stream, music_reverb_processor);
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
