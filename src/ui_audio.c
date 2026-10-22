#include "ui_audio.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* Action-triggered chorus, bass shelf/compression, and space reverb on match music.
   Driven by a smooth, hard-limited synergy curve via audio_set_reverb(amount):
   starts subtle on the first capture, blooms on stacked captures, and ducks its
   tail under the dry music so it never smears or spikes. */
#define CHORUS_HISTORY 2048
#define REVERB_COMB_COUNT 8
#define REVERB_COMB_MAX   4096
#define REVERB_AP_COUNT   4
#define REVERB_AP_MAX     1536
#define REVERB_PREDELAY_MAX 4096

/* Independently chosen tunings for the classic parallel-comb/serial-diffuser topology. */
static const int k_comb_len_l[REVERB_COMB_COUNT] = {1123, 1201, 1289, 1367, 1433, 1499, 1571, 1627};
static const int k_comb_len_r[REVERB_COMB_COUNT] = {1147, 1229, 1307, 1399, 1459, 1523, 1597, 1657};
static const int k_ap_len_l[REVERB_AP_COUNT] = {557, 443, 337, 227};
static const int k_ap_len_r[REVERB_AP_COUNT] = {577, 461, 359, 251};

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
static BiquadState s_bass_l, s_bass_r;
static float s_bass_env;
static float s_effect_cur[3] = {0.0f, 0.0f, 0.5f};
static BiquadState s_chorus_hp;
static float s_chorus_history[2][CHORUS_HISTORY], s_chorus_phase;
static int s_chorus_index;

#if __GCC_ATOMIC_INT_LOCK_FREE != 2
#error Audio controls require lock-free unsigned int atomics
#endif

/* C99 builds: lock-free, one-word snapshots between UI and audio threads.
   Never allocate, lock, or read GameSettings from the real-time callback. */
static unsigned int s_reverb_target;
static unsigned int s_effect_settings = 500u << 20;
static unsigned int s_reset_epoch;
static float s_reverb_cur;

static float clamp_range(float x, float lo, float hi) {
    if (!(x >= lo)) return lo; /* Includes NaN. */
    return x > hi ? hi : x;
}

void audio_set_reverb(float amount) {
    __atomic_store_n(&s_reverb_target, (unsigned int)(clamp_range(amount, 0, 1) * 1000000), __ATOMIC_RELAXED);
}

void audio_set_effects(float chorus, float bass, float reverb) {
    unsigned int c = (unsigned int)(clamp_range(chorus, 0, 1) * 1000 + 0.5f);
    unsigned int b = (unsigned int)(clamp_range(bass, 0, 1) * 1000 + 0.5f);
    unsigned int r = (unsigned int)(clamp_range(reverb, 0, 1) * 1000 + 0.5f);
    __atomic_store_n(&s_effect_settings, c | (b << 10) | (r << 20), __ATOMIC_RELAXED);
}

static void reset_music_dsp(void) {
    memset(s_comb_buf_l, 0, sizeof(s_comb_buf_l)); memset(s_comb_buf_r, 0, sizeof(s_comb_buf_r));
    memset(s_comb_idx_l, 0, sizeof(s_comb_idx_l)); memset(s_comb_idx_r, 0, sizeof(s_comb_idx_r));
    memset(s_comb_lp_l, 0, sizeof(s_comb_lp_l)); memset(s_comb_lp_r, 0, sizeof(s_comb_lp_r));
    memset(s_ap_buf_l, 0, sizeof(s_ap_buf_l)); memset(s_ap_buf_r, 0, sizeof(s_ap_buf_r));
    memset(s_ap_idx_l, 0, sizeof(s_ap_idx_l)); memset(s_ap_idx_r, 0, sizeof(s_ap_idx_r));
    memset(s_pre_buf_l, 0, sizeof(s_pre_buf_l)); memset(s_pre_buf_r, 0, sizeof(s_pre_buf_r));
    s_pre_idx_l = s_pre_idx_r = 0;
    s_in_hp_l = s_in_hp_r = s_in_lp_l = s_in_lp_r = (BiquadState){0};
    s_bass_l = s_bass_r = (BiquadState){0};
    s_chorus_hp = (BiquadState){0};
    s_sc_env = s_bass_env = s_reverb_cur = 0;
    memset(s_chorus_history, 0, sizeof(s_chorus_history));
    s_chorus_index = 0;
    s_chorus_phase = 0;
}

static float card_sfx_volume(float master_volume) {
    if (master_volume <= 0.0f) return 0.0f; /* Preserve the user's mute setting. */
    float volume = master_volume * 1.90f;
    if (volume < 0.40f) volume = 0.40f;
    return clamp_range(volume, 0.0f, 1.0f);
}

/* 2nd-order Butterworth High-Pass (300 Hz, 12 dB/oct, sr = 44100 Hz) */
static float biquad_hp300(BiquadState *s, float x) {
    const float b0 =  0.9702284f, b1 = -1.9404568f, b2 = 0.9702284f;
    const float a1 = -1.9395702f, a2 =  0.9413433f;
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

/* RBJ low shelf: +10 dB, 140 Hz, S=1, nominal 44.1 kHz. See docs/reference/audio-dsp.md.
   The callback is in DEVICE frames, not the MP3's sample rate (Raylib 5.5). */
static float bass_shelf10(BiquadState *s, float x) {
    float y = 1.00826505f*x - 1.97860751f*s->x1 + 0.97104258f*s->x2
            + 1.97884687f*s->y1 - 0.97906827f*s->y2;
    s->x2 = s->x1; s->x1 = x; s->y2 = s->y1; s->y1 = y;
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

/* Linear interpolation permits smooth room-size and chorus delay changes.
   All requested delays are >= 1 and below the corresponding ring capacity. */
static float delay_tap(const float *buffer, int capacity, int write, float delay) {
    float position = write - delay;
    if (position < 0) position += capacity;
    int index = (int)position;
    float fraction = position - index;
    return buffer[index] + fraction*(buffer[(index + 1) % capacity] - buffer[index]);
}

static void music_reverb_processor_impl(void *music_buffer, void *sfx_buffer, unsigned int frames) {
    float *pcm = (float *)music_buffer;
    float *sfx = (float *)sfx_buffer; // Cast the SFX stream
    if (!pcm || frames == 0) return;
    static unsigned int last_epoch;
    unsigned int epoch = __atomic_load_n(&s_reset_epoch, __ATOMIC_RELAXED);
    if (epoch != last_epoch) { reset_music_dsp(); last_epoch = epoch; }
    float target = __atomic_load_n(&s_reverb_target, __ATOMIC_RELAXED) / 1000000.0f;
    unsigned int settings = __atomic_load_n(&s_effect_settings, __ATOMIC_RELAXED);
    float controls[3] = { (settings & 1023u) / 1000.0f, ((settings >> 10) & 1023u) / 1000.0f, (settings >> 20) / 1000.0f };

    for (unsigned int f = 0; f < frames; f++) {
        float rate = (target > s_reverb_cur) ? 0.0018f : 0.0012f;
        s_reverb_cur += (target - s_reverb_cur) * rate;
        if (fabsf(target - s_reverb_cur) < 0.0001f) s_reverb_cur = target;
        float rev = clamp_range(s_reverb_cur, 0.0f, 1.0f);
        float dry_l = pcm[f * 2 + 0];
        float dry_r = pcm[f * 2 + 1];
        for (int i = 0; i < 3; i++) {
            s_effect_cur[i] += (controls[i] - s_effect_cur[i]) * 0.001f;
            if (fabsf(controls[i] - s_effect_cur[i]) < 0.0001f) s_effect_cur[i] = controls[i];
        }
        float chorus = s_effect_cur[0], bass = s_effect_cur[1], room = s_effect_cur[2];
        float bass_amount = bass * rev;
        float shelf_mix = (powf(10, bass_amount) - 1) / 2.16227766f; /* 0..20 dB before compression. */
        /* Parallel bass: compress ONLY the added shelf component. The original
           music never passes through this gain reduction, so kicks cannot duck vocals. */
        float added_l = (bass_shelf10(&s_bass_l, dry_l) - dry_l) * shelf_mix;
        float added_r = (bass_shelf10(&s_bass_r, dry_r) - dry_r) * shelf_mix;
        float bass_peak = fmaxf(fabsf(added_l), fabsf(added_r));
        s_bass_env += (bass_peak - s_bass_env) * (bass_peak > s_bass_env ? 0.00151f : 0.000567f);
        float bass_gain = s_bass_env > 0.80f ? powf(s_bass_env / 0.80f, (1 / (1 + 3*bass_amount)) - 1) : 1;
        if (bass_amount > 0.00001f) {
            dry_l += added_l * bass_gain;
            dry_r += added_r * bass_gain;
        }

        /* 1. Sidechain detector on dry music (3 ms attack = 0.00753, 80 ms release = 0.000283) */
        float dry_abs = 0.5f * ((dry_l < 0.0f ? -dry_l : dry_l) + (dry_r < 0.0f ? -dry_r : dry_r));
        float sc_rate = (dry_abs > s_sc_env) ? 0.007530f : 0.000283f;
        s_sc_env += (dry_abs - s_sc_env) * sc_rate;
        float sc_over = (s_sc_env > 0.08f) ? (s_sc_env - 0.08f) : 0.0f;
        float sc_dim = 1.0f / (1.0f + 2.2f * sc_over);

        /* Room dimensions depend on the saved slider, not the action pulse:
           size 0.65..2.2, feedback 0.55..0.93, increasingly diffuse late field. */
        float size = 0.65f + 1.55f*room;
        float diffusion = 0.50f + 0.15f*room;
        float bp_in_l = dry_l;
        float bp_in_r = dry_r;

        /* --- SFX Injection Phase --- */
        // Calculate dynamic SFX injection based on your card_sfx_volume logic
        // Note: sfx_buffer must hold at least `frames` stereo float frames when non-NULL.
        float sfx_scaling = card_sfx_volume(target) * 0.20f; // Keep it subtle (20% max blend)
        // If an active SFX stream buffer is passed, feed it directly into the reverb line
        if (sfx) {
            float sfx_l = sfx[f * 2 + 0];
            float sfx_r = sfx[f * 2 + 1];
            bp_in_l += sfx_l * sfx_scaling;
            bp_in_r += sfx_r * sfx_scaling;
        }
        
        float pre_delay = 220 + 1980*room; /* ~5..50 ms at nominal 44.1 kHz. */
        float early_l = delay_tap(s_pre_buf_l, REVERB_PREDELAY_MAX, s_pre_idx_l, pre_delay);
        float early_r = delay_tap(s_pre_buf_r, REVERB_PREDELAY_MAX, s_pre_idx_r, pre_delay);
        s_pre_buf_l[s_pre_idx_l] = bp_in_l;
        s_pre_buf_r[s_pre_idx_r] = bp_in_r;
        if (++s_pre_idx_l == REVERB_PREDELAY_MAX) s_pre_idx_l = 0;
        if (++s_pre_idx_r == REVERB_PREDELAY_MAX) s_pre_idx_r = 0;
        float tail_releasing = (target + 0.02f < s_reverb_cur) ? 1.0f : 0.0f;
        /* Low fixed input gain reserves feedback headroom, not an auto-gain. */
        float feed_l = early_l*0.020f + early_r*0.005f;
        float feed_r = early_r*0.020f + early_l*0.005f;
        float feedback = (0.55f + 0.38f*room) - 0.08f*tail_releasing;
        float damp = 0.40f - 0.20f*room;
        float acc_l = 0.0f, acc_r = 0.0f;

        for (int c = 0; c < REVERB_COMB_COUNT; c++) {
            int il = s_comb_idx_l[c];
            float out_l = delay_tap(s_comb_buf_l[c], REVERB_COMB_MAX, il, k_comb_len_l[c]*size);
            s_comb_lp_l[c] = out_l * (1.0f - damp) + s_comb_lp_l[c] * damp;
            s_comb_buf_l[c][il] = clamp_range(feed_l + s_comb_lp_l[c] * feedback, -0.80f, 0.80f);
            if (++il == REVERB_COMB_MAX) il = 0;
            s_comb_idx_l[c] = il;
            acc_l += s_comb_lp_l[c];

            int ir = s_comb_idx_r[c];
            float out_r = delay_tap(s_comb_buf_r[c], REVERB_COMB_MAX, ir, k_comb_len_r[c]*size);
            s_comb_lp_r[c] = out_r * (1.0f - damp) + s_comb_lp_r[c] * damp;
            s_comb_buf_r[c][ir] = clamp_range(feed_r + s_comb_lp_r[c] * feedback, -0.80f, 0.80f);
            if (++ir == REVERB_COMB_MAX) ir = 0;
            s_comb_idx_r[c] = ir;
            acc_r += s_comb_lp_r[c];
        }

        /* Restore the 20x input staging, then normalize eight returns (~1/sqrt(8)). */
        acc_l *= 7.0f;
        acc_r *= 7.0f;

        for (int a = 0; a < REVERB_AP_COUNT; a++) {
            int il = s_ap_idx_l[a];
            float buf_l = delay_tap(s_ap_buf_l[a], REVERB_AP_MAX, il, k_ap_len_l[a]*size);
            float in_l = acc_l;
            acc_l = buf_l - diffusion*in_l;
            s_ap_buf_l[a][il] = in_l + diffusion*acc_l;
            if (++il == REVERB_AP_MAX) il = 0;
            s_ap_idx_l[a] = il;

            int ir = s_ap_idx_r[a];
            float buf_r = delay_tap(s_ap_buf_r[a], REVERB_AP_MAX, ir, k_ap_len_r[a]*size);
            float in_r = acc_r;
            acc_r = buf_r - diffusion*in_r;
            s_ap_buf_r[a][ir] = in_r + diffusion*acc_r;
            if (++ir == REVERB_AP_MAX) ir = 0;
            s_ap_idx_r[a] = ir;
        }

        float wet_l = acc_l + early_l * 0.12f;
        float wet_r = acc_r + early_r * 0.12f;

        /* Stereo room return, with no width/covariance automation. */
        float wet_mono = 0.5f * (wet_l + wet_r);
        float wet_stereo = 0.5f * (wet_l - wet_r);
        float bass_late = clamp_range((room - 0.8f) * 5, 0, 1);
        bass_late *= bass_late * (3 - 2*bass_late);
        float room_mid_hp = biquad_hp300(&s_in_hp_l, wet_mono);
        float room_side_hp = biquad_hp300(&s_in_hp_r, wet_stereo);
        float room_mid = room_mid_hp + (wet_mono - room_mid_hp) * bass_late;
        /* Deep room bass stays centred; diffusion occupies the mid/high field. */
        float room_side = room_side_hp;
        /* Darker at low room settings; full-band at maximum. */
        float room_mid_lp = biquad_lp5000(&s_in_lp_l, room_mid);
        float room_side_lp = biquad_lp5000(&s_in_lp_r, room_side);
        room_mid = room_mid_lp + (room_mid - room_mid_lp) * room;
        room_side = room_side_lp + (room_side - room_side_lp) * room;
        float wet_gain = room * rev * 2.20f * sc_dim * (1 - 0.10f * tail_releasing);
        float dry_gain = 1 - 0.30f * room * rev;
        float out_l = dry_l, out_r = dry_r;
        if (room * rev > 0.00001f) {
            out_l = dry_l*dry_gain + (room_mid + room_side)*wet_gain;
            out_r = dry_r*dry_gain + (room_mid - room_side)*wet_gain;
        }

        /* Separate, feed-forward two-voice chorus AFTER the room. Return only
           generated Side from the existing quadrature taps; retain the input Mid.
           The linked output limiter below may still change that Mid. */
        s_chorus_history[0][s_chorus_index] = out_l;
        s_chorus_history[1][s_chorus_index] = out_r;
        float amount = chorus*rev;
        if (amount > 0.00001f) {
            float depth = 264*chorus; /* up to 6 ms at nominal 44.1 kHz */
            float d1 = 662 + depth*sinf(s_chorus_phase);
            float d2 = 1014 + depth*cosf(s_chorus_phase);
            /* Antisymmetric return: (L+R)/2 is unchanged before peak limiting. */
            float va = 0.5f*(delay_tap(s_chorus_history[0], CHORUS_HISTORY, s_chorus_index, d1)
                           + delay_tap(s_chorus_history[1], CHORUS_HISTORY, s_chorus_index, d1));
            float vb = 0.5f*(delay_tap(s_chorus_history[0], CHORUS_HISTORY, s_chorus_index, d2)
                           + delay_tap(s_chorus_history[1], CHORUS_HISTORY, s_chorus_index, d2));
            float voice_side = 2.0f*biquad_hp300(&s_chorus_hp, 0.5f*(va - vb)); /* Maintainer: +100% return. */
            out_l += amount*voice_side;
            out_r -= amount*voice_side;
        }
        if (++s_chorus_index == CHORUS_HISTORY) s_chorus_index = 0;
        s_chorus_phase += 6.28318530718f*0.60f/44100;
        if (s_chorus_phase >= 6.28318530718f) s_chorus_phase -= 6.28318530718f;

        /* Stereo-linked protection: one gain for both ears, no image shift.
           Dry bypass (including hot input) is zero-latency when all effects are off. */
        float peak = fmaxf(fabsf(out_l), fabsf(out_r));
        if (rev > 0.00001f && chorus + bass + room > 0.00001f && peak > 0.88f) {
            float gain = soft_knee_limit(peak, 0.88f, 0.98f) / peak;
            out_l *= gain; out_r *= gain;
        }
        pcm[f * 2 + 0] = out_l;
        pcm[f * 2 + 1] = out_r;
    }
}

static void music_reverb_processor(void *buffer, unsigned int frames) {
    music_reverb_processor_impl(buffer, NULL, frames);
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

static int compare_music_paths(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

int audio_set_music_folder(AudioAssets *a, const char *folder) {
    Music loaded[10] = {0};
    int count = 0, custom = folder && folder[0];
    FilePathList files = {0};
    if (custom) {
        if (!DirectoryExists(folder)) return -1;
        files = LoadDirectoryFilesEx(folder, ".mp3", false);
        if (files.count > 1) qsort(files.paths, files.count, sizeof(*files.paths), compare_music_paths);
    }
    unsigned int limit = custom ? files.count : 10;
    for (unsigned int i = 0; i < limit && count < 10; i++) {
        char path[64];
        snprintf(path, sizeof(path), "assets/music/game%u.mp3", i + 1);
        const char *name = custom ? files.paths[i] : get_path(path);
        if (!FileExists(name)) continue;
        Music m = LoadMusicStream(name);
        if (!IsMusicValid(m)) continue;
        m.looping = false;
        loaded[count++] = m;
    }
    if (custom) UnloadDirectoryFiles(files);
    if (custom && count == 0) return -1;

    /* Stage the replacement first so invalid folders leave playback untouched. */
    int was_match = a->current_music == 2;
    if (was_match) audio_stop_music(a);
    for (int i = 0; i < 10; i++) {
        if (a->has_music_game[i]) {
            DetachAudioStreamProcessor(a->music_game[i].stream, music_reverb_processor);
            UnloadMusicStream(a->music_game[i]);
        }
        a->has_music_game[i] = i < count;
        a->music_game[i] = loaded[i];
        if (i < count) {
            SetMusicVolume(loaded[i], a->bgm_volume);
            AttachAudioStreamProcessor(loaded[i].stream, music_reverb_processor);
        }
    }
    a->game_track_count = count;
    a->current_game_track = -1;
    if (was_match) audio_start_match_music(a, 1);
    return count;
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

    audio_set_music_folder(a, "");

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

void audio_preview_effect(AudioAssets *a, int enabled) {
    if (a->game_track_count == 0) { audio_play_menu_music(a); return; }
    if (a->current_music != 2) audio_start_match_music(a, 1);
    else if (audio_is_match_music_finished(a)) audio_start_match_music(a, a->current_game_track + 2);
    audio_set_reverb(enabled ? 1.0f : 0.0f);
}

void audio_play_menu_music(AudioAssets *a) {
    audio_set_reverb(0);
    __atomic_add_fetch(&s_reset_epoch, 1u, __ATOMIC_RELAXED);
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
    audio_set_reverb(0);
    __atomic_add_fetch(&s_reset_epoch, 1u, __ATOMIC_RELAXED);
    if (a->current_music == 1 && a->has_music_menu) StopMusicStream(a->music_menu);
    if (a->current_game_track >= 0 && a->current_game_track < 10) {
        if (a->has_music_game[a->current_game_track]) StopMusicStream(a->music_game[a->current_game_track]);
    }
    int idx = track_1_to_10 - 1;
    if (idx < 0) idx = 0;
    if (idx > 9) idx = 9;
    /* Compact playlists also handle a folder with just one usable song. */
    if (a->game_track_count > 0) idx %= a->game_track_count;
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
    audio_set_reverb(0);
    __atomic_add_fetch(&s_reset_epoch, 1u, __ATOMIC_RELAXED);
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
