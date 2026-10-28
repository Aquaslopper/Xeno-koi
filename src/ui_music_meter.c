#include "ui_music_meter.h"
#include <math.h>

static unsigned int s_music_meter_raw = 0u;
static unsigned int s_music_meter_epoch = 0u;
static int s_music_tap_attached = 0;
static float s_music_meter_vis = 0.0f;

static void music_meter_processor(void *buffer, unsigned int frames) {
    const float *pcm = (const float *)buffer;
    if (!pcm || frames == 0) return;
    static float fast_ms = 0.0f, lo_rms = 0.12f, hi_rms = 0.24f;
    static unsigned int last_ep = 0;
    unsigned int ep = __atomic_load_n(&s_music_meter_epoch, __ATOMIC_RELAXED);
    if (ep != last_ep) { fast_ms = 0.0f; lo_rms = 0.12f; hi_rms = 0.24f; last_ep = ep; }
    for (unsigned int f = 0; f < frames; f++) {
        float l = pcm[f * 2 + 0], r = pcm[f * 2 + 1], p2 = 0.5f * (l * l + r * r);
        if (!(p2 >= 0.0f) || p2 > 4.0f) p2 = 0.0f;
        fast_ms += (p2 - fast_ms) * 0.00085f;
    }
    if (fast_ms < 1e-12f) fast_ms = 0.0f;
    float rms = sqrtf(fast_ms), c_sec = (float)frames / 44100.0f;
    if (rms > hi_rms) hi_rms += (rms - hi_rms) * (1.0f - expf(-c_sec / 0.05f));
    else              hi_rms += (rms - hi_rms) * (1.0f - expf(-c_sec / 0.90f));
    if (rms < lo_rms) lo_rms += (rms - lo_rms) * (1.0f - expf(-c_sec / 0.06f));
    else              lo_rms += (rms - lo_rms) * (1.0f - expf(-c_sec / 0.90f));
    float gate = (hi_rms > 0.015f) ? fminf(1.0f, (hi_rms - 0.015f) / 0.075f) : 0.0f;
    float floor_r = lo_rms * 0.88f, span = fmaxf(0.025f, hi_rms - floor_r);
    float beat = fminf(1.0f, fmaxf(0.0f, (rms - floor_r) / span));
    float norm = gate * (0.08f + 0.92f * powf(beat, 1.20f));
    __atomic_store_n(&s_music_meter_raw, (unsigned int)(norm * 1000000.0f), __ATOMIC_RELAXED);
}

static void sync_music_meter_tap(AudioAssets *audio, int attach) {
    if (!audio || attach == s_music_tap_attached) return;
    __atomic_add_fetch(&s_music_meter_epoch, 1u, __ATOMIC_RELAXED);
    __atomic_store_n(&s_music_meter_raw, 0u, __ATOMIC_RELAXED);
    for (int i = 0; i < AUDIO_MAX_TRACKS; i++) {
        if (!audio->has_music_game[i]) continue;
        if (attach) AttachAudioStreamProcessor(audio->music_game[i].stream, music_meter_processor);
        else        DetachAudioStreamProcessor(audio->music_game[i].stream, music_meter_processor);
    }
    s_music_tap_attached = attach;
    if (!attach) s_music_meter_vis = 0.0f;
}

void music_meter_update(AudioAssets *audio, int enabled, int frozen,
                        float transition_alpha, float dt, int require_audible) {
    sync_music_meter_tap(audio, enabled ? 1 : 0);
    if (!enabled || frozen || !audio || dt <= 0.0f) return;
    float raw = __atomic_load_n(&s_music_meter_raw, __ATOMIC_RELAXED) / 1000000.0f;
    float target = (audio->current_music == 2) ? raw * (1.0f - transition_alpha) : 0.0f;
    if (require_audible && (audio->bgm_volume <= 0.0f || audio->current_game_track < 0 ||
                            audio->current_game_track >= AUDIO_MAX_TRACKS ||
                            !audio->has_music_game[audio->current_game_track] ||
                            !IsMusicStreamPlaying(audio->music_game[audio->current_game_track]))) target = 0.0f;
    s_music_meter_vis += (target - s_music_meter_vis) *
        (1.0f - expf(-dt / ((target > s_music_meter_vis) ? 0.035f : 0.120f)));
}

float music_meter_level(void) { return s_music_meter_vis; }

void music_meter_detach(AudioAssets *audio) { sync_music_meter_tap(audio, 0); }
