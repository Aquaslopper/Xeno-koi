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

static float s_hp_lp_l = 0.0f;
static float s_hp_lp_r = 0.0f;
static float s_dry_env = 0.0f;

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

static float soft_limit(float x) {
    if (x > 1.35f) return 0.96f;
    if (x < -1.35f) return -0.96f;
    return x - (x * x * x) * 0.19f;
}

static void music_reverb_processor(void *buffer, unsigned int frames) {
    float *pcm = (float *)buffer;
    if (!pcm || frames == 0) return;
    float target = clamp_range(s_reverb_target, 0.0f, 1.0f);

    for (unsigned int f = 0; f < frames; f++) {
        /* Smooth audio-rate glide, strictly hard-limited to [0, 1] */
        float rate = (target > s_reverb_cur) ? 0.0020f : 0.0012f;
        s_reverb_cur += (target - s_reverb_cur) * rate;
        float rev = clamp_range(s_reverb_cur, 0.0f, 1.0f);
        float dry_l = pcm[f * 2 + 0];
        float dry_r = pcm[f * 2 + 1];

        /* Track dry music envelope for gentle sidechain ducking on the release tail */
        float dry_abs = 0.5f * ((dry_l < 0.0f ? -dry_l : dry_l) + (dry_r < 0.0f ? -dry_r : dry_r));
        float env_rate = (dry_abs > s_dry_env) ? 0.012f : 0.0008f;
        s_dry_env += (dry_abs - s_dry_env) * env_rate;

        /* Stereo ping-pong pre-delay / Haas widener */
        float slap_l = s_pre_buf_l[s_pre_idx_l];
        float slap_r = s_pre_buf_r[s_pre_idx_r];
        float in_gain = 0.45f + 0.48f * rev;
        float slap_fb = 0.28f + 0.16f * rev;
        s_pre_buf_l[s_pre_idx_l] = clamp_range(dry_l * in_gain + slap_r * slap_fb, -0.92f, 0.92f);
        s_pre_buf_r[s_pre_idx_r] = clamp_range(dry_r * in_gain - slap_l * slap_fb, -0.92f, 0.92f);
        if (++s_pre_idx_l >= k_pre_len_l) s_pre_idx_l = 0;
        if (++s_pre_idx_r >= k_pre_len_r) s_pre_idx_r = 0;

        /* Wide decorrelated stereo feed into the 4-comb tank; tighten feedback on release so tail never smears */
        float feed_l = (dry_l * 0.85f - dry_r * 0.30f) * in_gain + slap_r * 0.40f;
        float feed_r = (dry_r * 0.85f - dry_l * 0.30f) * in_gain + slap_l * 0.40f;
        float tail_releasing = (target + 0.02f < s_reverb_cur) ? 1.0f : 0.0f;
        float feedback = (0.74f + 0.10f * rev) - 0.12f * tail_releasing;
        const float damp = 0.30f;
        float acc_l = 0.0f, acc_r = 0.0f;

        for (int c = 0; c < REVERB_COMB_COUNT; c++) {
            int il = s_comb_idx_l[c];
            float out_l = s_comb_buf_l[c][il];
            s_comb_lp_l[c] = out_l * (1.0f - damp) + s_comb_lp_l[c] * damp;
            s_comb_buf_l[c][il] = clamp_range(feed_l + s_comb_lp_l[c] * feedback, -0.95f, 0.95f);
            if (++il >= k_comb_len_l[c]) il = 0;
            s_comb_idx_l[c] = il;
            acc_l += out_l;

            int ir = s_comb_idx_r[c];
            float out_r = s_comb_buf_r[c][ir];
            s_comb_lp_r[c] = out_r * (1.0f - damp) + s_comb_lp_r[c] * damp;
            s_comb_buf_r[c][ir] = clamp_range(feed_r + s_comb_lp_r[c] * feedback, -0.95f, 0.95f);
            if (++ir >= k_comb_len_r[c]) ir = 0;
            s_comb_idx_r[c] = ir;
            acc_r += out_r;
        }

        acc_l *= 0.40f;
        acc_r *= 0.40f;

        for (int a = 0; a < REVERB_AP_COUNT; a++) {
            int il = s_ap_idx_l[a];
            float buf_l = s_ap_buf_l[a][il];
            float in_l = acc_l;
            acc_l = -in_l + buf_l;
            s_ap_buf_l[a][il] = in_l + buf_l * 0.54f;
            if (++il >= k_ap_len_l[a]) il = 0;
            s_ap_idx_l[a] = il;

            int ir = s_ap_idx_r[a];
            float buf_r = s_ap_buf_r[a][ir];
            float in_r = acc_r;
            acc_r = -in_r + buf_r;
            s_ap_buf_r[a][ir] = in_r + buf_r * 0.54f;
            if (++ir >= k_ap_len_r[a]) ir = 0;
            s_ap_idx_r[a] = ir;
        }

        /* Gentle sub-bass blocker (~65 Hz) keeps low end clean while preserving full warm body */
        float raw_wet_l = acc_l + slap_l * 0.50f - slap_r * 0.22f;
        float raw_wet_r = acc_r + slap_r * 0.50f - slap_l * 0.22f;
        s_hp_lp_l += (raw_wet_l - s_hp_lp_l) * 0.009f;
        s_hp_lp_r += (raw_wet_r - s_hp_lp_r) * 0.009f;
        float wet_l = raw_wet_l - s_hp_lp_l;
        float wet_r = raw_wet_r - s_hp_lp_r;

        if (rev > 0.0005f) {
            /* Duck dry center signal so stereo reverb blooms clearly; duck release tail so it never smears */
            float tail_duck = (1.0f - 0.20f * tail_releasing) / (1.0f + s_dry_env * 0.35f * tail_releasing);
            float wet_gain = rev * 1.18f * tail_duck;
            float dry_gain = 1.0f - rev * 0.50f;
            pcm[f * 2 + 0] = soft_limit(dry_l * dry_gain + wet_l * wet_gain);
            pcm[f * 2 + 1] = soft_limit(dry_r * dry_gain + wet_r * wet_gain);
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
        SetSoundVolume(a->sfx_flip, a->sfx_volume);
    }
    const char *p_drop = get_path("assets/sfx/card_drop.wav");
    if (FileExists(p_drop)) {
        a->sfx_drop = LoadSound(p_drop); a->has_sfx_drop = 1;
        SetSoundVolume(a->sfx_drop, a->sfx_volume);
    }
    const char *p_cap = get_path("assets/sfx/capture.wav");
    if (FileExists(p_cap)) {
        a->sfx_capture = LoadSound(p_cap); a->has_sfx_capture = 1;
        SetSoundVolume(a->sfx_capture, a->sfx_volume);
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
    if (a->has_sfx_flip) SetSoundVolume(a->sfx_flip, volume);
    if (a->has_sfx_drop) SetSoundVolume(a->sfx_drop, volume);
    if (a->has_sfx_capture) SetSoundVolume(a->sfx_capture, volume);
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
