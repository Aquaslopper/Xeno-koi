/* Headless regression test for the capture-reverb DSP in src/ui_audio.c.
 *
 * Guards the two properties that the stereo reverb kept losing:
 *   1. With no reverb requested samples must be unchanged after the eight-sample alignment.
 *   2. At full reverb the mid/high band (snare crack, hats, presence) must
 *      actually be present in the Side/stereo channel - not only the low end.
 *
 * Builds against tests/raylib_audio_stub/raylib.h, so it needs no raylib.
 * Run with ./tests/test_audio_dsp.sh
 */
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "../src/ui_audio.c"

#define SR              44100
#define WARMUP_FRAMES   (SR * 3)
#define TEST_FRAMES     (SR * 2)
#define NOISE_AMP       0.06f

/* Thresholds. Measured values at the time of writing are noted so a future
 * retune notices when it changes the stereo balance on purpose.
 * Side is compared against the full Mid (dry centre + wet), which is why the
 * number is negative: -15.1 dB with the old 2.8 kHz de-ess + 0.72 shelf,
 * -10.4 dB with the presence lift. Setting the bar at -12.5 dB fails the
 * old behaviour and passes the fixed one. */
#define BYPASS_MAX_DIFF   1e-6f
#define PRESENCE_MIN_DB   (-12.5f)

static float buf[TEST_FRAMES * 2];
static float warm[WARMUP_FRAMES * 2];
static float side[TEST_FRAMES];
static float mid[TEST_FRAMES];
static float lp1[TEST_FRAMES];
static float lp2[TEST_FRAMES];

static unsigned int rng_state = 12345u;

static float next_noise(void) {
    rng_state = (1103515245u * rng_state + 12345u) & 0x7fffffffu;
    return NOISE_AMP * (((float)rng_state / (float)0x7fffffff) * 2.0f - 1.0f);
}

/* Mono source: identical samples in both channels, like a centred snare. */
static void fill_mono(float *dst, int frames) {
    for (int i = 0; i < frames; i++) {
        float v = next_noise();
        dst[i * 2 + 0] = v;
        dst[i * 2 + 1] = v;
    }
}

static void split_channels(const float *src, float *side_out, float *mid_out, int frames) {
    for (int i = 0; i < frames; i++) {
        side_out[i] = 0.5f * (src[i * 2 + 0] - src[i * 2 + 1]);
        mid_out[i]  = 0.5f * (src[i * 2 + 0] + src[i * 2 + 1]);
    }
}

/* One-pole low-pass, same shape the DSP itself uses. */
static void onepole(const float *in, float *out, int frames, float cutoff) {
    float a = 1.0f - expf(-2.0f * 3.14159265f * cutoff / (float)SR);
    float acc = 0.0f;
    for (int i = 0; i < frames; i++) {
        acc += (in[i] - acc) * a;
        out[i] = acc;
    }
}

static float db(float x) {
    return 20.0f * log10f(x > 1e-12f ? x : 1e-12f);
}

int main(void) {
    int failures = 0;

    /* --- 1. No reverb: exact dry samples after the fixed phase-alignment delay. --- */
    fill_mono(buf, TEST_FRAMES);
    float original[TEST_FRAMES * 2];
    memcpy(original, buf, sizeof(original));
    music_reverb_processor(buf, TEST_FRAMES);

    float max_diff = 0.0f;
    for (int i = 0; i < TEST_FRAMES * 2; i++) {
        float d = fabsf(buf[i] - (i >= 2*WIDTH_DELAY ? original[i - 2*WIDTH_DELAY] : 0));
        if (d > max_diff) max_diff = d;
    }
    printf("bypass (reverb 0, 8-sample alignment): max |out - delayed in| = %.9f\n", (double)max_diff);
    if (max_diff > BYPASS_MAX_DIFF) {
        printf("  FAIL: the DSP alters dry samples after compensating for phase alignment\n");
        failures++;
    } else {
        printf("  ok\n");
    }

    /* --- 2. Full reverb: mid/high must show up as stereo width. --- */
    audio_set_reverb(1.0f);
    fill_mono(warm, WARMUP_FRAMES);
    music_reverb_processor(warm, WARMUP_FRAMES);   /* let the tank and the ramp settle */

    fill_mono(buf, TEST_FRAMES);
    music_reverb_processor(buf, TEST_FRAMES);

    split_channels(buf, side, mid, TEST_FRAMES);

    /* Presence band = low-passed at 5 kHz minus low-passed at 2 kHz. */
    onepole(side, lp1, TEST_FRAMES, 5000.0f);
    onepole(side, lp2, TEST_FRAMES, 2000.0f);
    double acc_s = 0.0;
    for (int i = 0; i < TEST_FRAMES; i++) {
        float band = lp1[i] - lp2[i];
        acc_s += (double)band * (double)band;
    }
    float side_presence = (float)sqrt(acc_s / (double)TEST_FRAMES);

    onepole(mid, lp1, TEST_FRAMES, 5000.0f);
    onepole(mid, lp2, TEST_FRAMES, 2000.0f);
    double acc_m = 0.0;
    for (int i = 0; i < TEST_FRAMES; i++) {
        float band = lp1[i] - lp2[i];
        acc_m += (double)band * (double)band;
    }
    float mid_presence = (float)sqrt(acc_m / (double)TEST_FRAMES);

    float ratio = db(side_presence) - db(mid_presence);
    printf("presence band (2-5 kHz): side %.1f dB, mid %.1f dB\n",
           (double)db(side_presence), (double)db(mid_presence));
    printf("presence-band side relative to mid = %+.1f dB (want >= %+.1f)\n",
           (double)ratio, (double)PRESENCE_MIN_DB);
    if (ratio < PRESENCE_MIN_DB) {
        printf("  FAIL: the mid/high band is missing from the stereo channel\n");
        failures++;
    } else {
        printf("  ok\n");
    }

    /* --- 3. Captures keep audible card feedback, including the drop fallback. --- */
    AudioAssets capture_audio = {0};
    capture_audio.has_sfx_drop = 1; /* capture.wav is optional; card_drop.wav is the fallback */
    audio_set_sfx_volume(&capture_audio, 0.10f);
    if (fabsf(raylib_stub_last_sound_volume - 0.40f) > 1e-6f) {
        printf("  FAIL: card SFX fell below the 40%% minimum\n");
        failures++;
    }
    int play_count = raylib_stub_play_sound_count;
    audio_play_capture(&capture_audio);
    if (raylib_stub_play_sound_count != play_count + 1) {
        printf("  FAIL: capture did not play the fallback card SFX\n");
        failures++;
    }
    audio_set_sfx_volume(&capture_audio, 0.0f);
    if (raylib_stub_last_sound_volume != 0.0f) {
        printf("  FAIL: the SFX mute setting was not preserved\n");
        failures++;
    }
    if (!failures) printf("capture card SFX: >= 40%% when enabled; mute still works\n  ok\n");

    if (failures) {
        printf("\n%d check(s) failed\n", failures);
        return 1;
    }
    printf("\nall audio DSP checks passed\n");
    return 0;
}
