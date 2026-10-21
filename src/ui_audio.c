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

static float s_sc_env = 0.0f;
static float s_mid_bass_lp = 0.0f;
static float s_side_low_lp = 0.0f;
static float s_side_deess_lp = 0.0f;
static float s_side_presence_lp = 0.0f;
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

static float card_sfx_volume(float master_volume) {
    if (master_volume <= 0.0f) return 0.0f; /* Preserve the user's mute setting. */
    float volume = master_volume * 1.90f;
    if (volume < 0.40f) volume = 0.40f;
    return clamp_range(volume, 0.0f, 1.0f);
}

/* 2nd-order Butterworth High-Pass (150 Hz, 12 dB/oct, sr = 44100 Hz) */
static float biquad_hp150(BiquadState *s, float x) {
    const float b0 =  0.9850018f, b1 = -1.9700035f, b2 = 0.9850018f;
    const float a1 = -1.9697786f, a2 =  0.9702285f;
    float y = b0 * x + b1 * s->x1 + b2 * s->x2 - a1 * s->y1 - a2 * s->y2;
    s->x2 = s->x1; s->x1 = x;
    s->y2 = s->y1; s->y1 = y;
    return y;
}

/* 2nd-order Butterworth Low-Pass (5 kHz, 12 dB/oct, sr = 44100 Hz) */
static float biquad_lp5000(BiquadState *s, float x) {
    const float b0 =  0.0831599f, b1 = 0.1663197f, b2 = 0.0831599f;
    const float a1 = -1.0351712f, a2 = 0.3678107f;
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

        /* 1. Sidechain detector on dry music (3 ms attack = 0.00753, 80 ms release = 0.000283) */
        float dry_abs = 0.5f * ((dry_l < 0.0f ? -dry_l : dry_l) + (dry_r < 0.0f ? -dry_r : dry_r));
        float sc_rate = (dry_abs > s_sc_env) ? 0.007530f : 0.000283f;
        s_sc_env += (dry_abs - s_sc_env) * sc_rate;
        float sc_over = (s_sc_env > 0.08f) ? (s_sc_env - 0.08f) : 0.0f;
        float sc_dim = 1.0f / (1.0f + 2.2f * sc_over);

        /* 2. Reverb Input Bandpass (Lowered HP to 70Hz for more bass, raised LP to 8kHz for tight transients) */
        // Note: For optimal sub-bass response, consider lowering your biquad_hp150 filter coefficient to ~70Hz.
        float bp_in_l = biquad_lp5000(&s_in_lp_l, biquad_hp150(&s_in_hp_l, dry_l));
        float bp_in_r = biquad_lp5000(&s_in_lp_r, biquad_hp150(&s_in_hp_r, dry_r));

        /* --- SFX Injection Phase --- */
        // Calculate dynamic SFX injection based on your card_sfx_volume logic
        float sfx_scaling = card_sfx_volume(target) * 0.15f; // Keep it subtle (15% max blend)
        bp_in_l += dry_l * sfx_scaling;
        bp_in_r += dry_r * sfx_scaling;
        
        float slap_l = s_pre_buf_l[s_pre_idx_l];
        float slap_r = s_pre_buf_r[s_pre_idx_r];
        s_pre_buf_l[s_pre_idx_l] = clamp_range(bp_in_l * 0.65f + slap_r * 0.12f, -0.70f, 0.70f);
        s_pre_buf_r[s_pre_idx_r] = clamp_range(bp_in_r * 0.65f - slap_l * 0.12f, -0.70f, 0.70f);
        if (++s_pre_idx_l >= (k_pre_len_l / 2)) s_pre_idx_l = 0;
        if (++s_pre_idx_r >= (k_pre_len_r / 2)) s_pre_idx_r = 0;

        float tail_releasing = (target + 0.02f < s_reverb_cur) ? 1.0f : 0.0f;
        float feed_l = bp_in_l * 0.65f + slap_r * 0.15f;
        float feed_r = bp_in_r * 0.65f + slap_l * 0.15f;
        float feedback = 0.84f - 0.10f * tail_releasing;
        const float damp = 0.22f;
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

        acc_l *= 0.38f;
        acc_r *= 0.38f;

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

        float wet_l = acc_l + slap_l * 0.24f;
        float wet_r = acc_r + slap_r * 0.24f;

        /* 3. Mid (mono center) & Side (stereo) stage:
              - Mid is mono in the middle; gently boost mono bass (<140 Hz) when the reverb effect takes place
              - Side is stereo; cut lows (<180 Hz) from Side, lift the mids/highs so snares and presence
                actually register as width, and de-ess only true sibilance (>6 kHz) */
        float mono_mid = 0.5f * (dry_l + dry_r);
        s_mid_bass_lp += (mono_mid - s_mid_bass_lp) * 0.020f; /* ~140 Hz mono bass */

        float wet_mono   = 0.5f * (wet_l + wet_r);
        float wet_stereo = 0.5f * (wet_l - wet_r);
        s_side_low_lp += (wet_stereo - s_side_low_lp) * 0.025f; /* ~180 Hz Side low-cut */
        float side_hi = wet_stereo - s_side_low_lp;

        /* De-ess only true sibilance (3 ms attack, 80 ms release). The split used to sit at
           ~2.8 kHz, inside the snare crack, so every hit got pulled back out of the stereo
           field; ~6 kHz keeps the de-esser on sibilance and off the mid/high content. */
        s_side_deess_lp += (side_hi - s_side_deess_lp) * 0.575f; /* ~6 kHz split */
        float side_sibilance = side_hi - s_side_deess_lp;
        float sib_abs = (side_sibilance < 0.0f) ? -side_sibilance : side_sibilance;
        float deess_rate = (sib_abs > s_deess_env) ? 0.007530f : 0.000283f;
        s_deess_env += (sib_abs - s_deess_env) * deess_rate;
        float deess_gain = 1.0f / (1.0f + 5.0f * s_deess_env);
        float clean_wet_stereo = s_side_deess_lp + side_sibilance * deess_gain;

        /* Presence lift on the Side only. The reverb bloom is spectrally flat, but music is
           already much wider in the mids/highs than in the bass, so a flat side reads as
           "only the low end got stereo". Lifting everything above ~1.2 kHz puts the snare
           and presence into the width. The Mid path is untouched, so the centre keeps its
           exact level and the mono sum is unchanged. */
        s_side_presence_lp += (clean_wet_stereo - s_side_presence_lp) * 0.17f; /* ~1.2 kHz split */
        clean_wet_stereo += (clean_wet_stereo - s_side_presence_lp) * 0.70f;   /* ~+4.6 dB above 1.2 kHz */

        float out_l = dry_l;
        float out_r = dry_r;

        if (rev > 0.0005f) {
            /* Phase-safe, subtle stereo widening: keep the side contribution in-phase
               for both ears, but make the right channel smaller so it doesn't cancel. */
            float stereo_w = 0.30f + 0.24f * (rev * rev);
            float stereo_comp = 1.0f / (1.0f + 0.15f * stereo_w);
            float impact_boost = (target > s_reverb_cur) ? 1.18f : 1.0f;
            float wet_gain = rev * 0.76f * sc_dim * stereo_comp * impact_boost * (1.0f - 0.10f * tail_releasing);

            float dry_gain = 1.0f - rev * 0.12f;
            float mono_bass_boost = s_mid_bass_lp * (0.10f * rev);
            float rev_mid = wet_mono * wet_gain + mono_bass_boost;
            float rev_side = clean_wet_stereo * (wet_gain * stereo_w);

            out_l = dry_l * dry_gain + rev_mid + rev_side * 0.92f;
            out_r = dry_r * dry_gain + rev_mid + rev_side * 0.42f;
        }

        /* 4. Gentle Master Bus Compressor over everything (3 ms attack, 80 ms release):
              high threshold (0.60, gentle 2.0:1 ratio) so normal music is untouched and only spikes are caught */
        float pk_l = (out_l < 0.0f) ? -out_l : out_l;
        float pk_r = (out_r < 0.0f) ? -out_r : out_r;
        float pk = (pk_r > pk_l) ? pk_r : pk_l;
        float comp_rate = (pk > s_comp_env) ? 0.007530f : 0.000283f;
        s_comp_env += (pk - s_comp_env) * comp_rate;
        const float thresh = 0.70f;
        if (s_comp_env > thresh) {
            float over = s_comp_env - thresh;
            float target_lvl = thresh + over * 0.50f;
            float cg = target_lvl / s_comp_env;
            out_l *= cg;
            out_r *= cg;
        }
        pcm[f * 2 + 0] = soft_knee_limit(out_l, 0.72f, 0.88f);
        pcm[f * 2 + 1] = soft_knee_limit(out_r, 0.72f, 0.88f);
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
        SetSoundVolume(a->sfx_flip, card_sfx_volume(a->sfx_volume));
    }
    const char *p_drop = get_path("assets/sfx/card_drop.wav");
    if (FileExists(p_drop)) {
        a->sfx_drop = LoadSound(p_drop); a->has_sfx_drop = 1;
        SetSoundVolume(a->sfx_drop, card_sfx_volume(a->sfx_volume));
    }
    const char *p_cap = get_path("assets/sfx/capture.wav");
    if (FileExists(p_cap)) {
        a->sfx_capture = LoadSound(p_cap); a->has_sfx_capture = 1;
        SetSoundVolume(a->sfx_capture, card_sfx_volume(a->sfx_volume));
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
    float card_vol = card_sfx_volume(volume);
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
    s_reverb_target = 0.0f;
    s_reverb_cur = 0.0f;
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
    s_reverb_target = 0.0f;
    s_reverb_cur = 0.0f;
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
    s_reverb_target = 0.0f;
    s_reverb_cur = 0.0f;
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
