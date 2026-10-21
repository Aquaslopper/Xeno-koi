/* Headless regression test for the staged capture-reverb DSP in src/ui_audio.c.
 *
 * Guards the properties the effect is supposed to have:
 *   1. Stage 0 is a true bypass - no reverb, no EQ, no compression.
 *   2. The stages stack: side (stereo) energy at stage 1 < 2 < 3.
 *   3. Lows stay in the Mid: the Side below ~150 Hz is far below the Mid.
 *   4. Nothing clips, even driven hard at stage 3.
 *
 * Builds against tests/raylib_audio_stub/raylib.h, so it needs no raylib.
 * Run with ./tests/test_audio_dsp.sh
 */
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "../src/ui_audio.c"

#define SR              44100
#define WARMUP_FRAMES   (SR * 2)
#define TEST_FRAMES     (SR * 2)
#define NOISE_AMP       0.06f

#define BYPASS_MAX_DIFF   1e-6f
#define STAGE3_MIN_PEAK   0.30f   /* the effect must actually be doing something */
#define STAGE3_MAX_PEAK   1.0f
#define LOW_SIDE_MARGIN_DB  20.0    /* wet side: high band over low band */

static float buf[TEST_FRAMES * 2];
static float warm[WARMUP_FRAMES * 2];

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

static float peak(const float *v, int n) {
    float p = 0.0f;
    for (int i = 0; i < n; i++) if (fabsf(v[i]) > p) p = fabsf(v[i]);
    return p;
}

/* Run `frames` through the real processor with the given stage. */
static void run(float *pcm, int frames, int stage) {
    audio_set_capture_stage(stage);
    music_reverb_processor(pcm, (unsigned int)frames);
}

/* Wet-only band energies: subtract the stage-0 (dry) rendering first, so this
 * measures the effect and not the music. Low band = two cascaded one-poles at
 * 120 Hz, high band = the remainder. */
static float wet[TEST_FRAMES * 2];
static void wet_band_energies(const float *out, const float *dry, int frames,
                              double *side_lo, double *side_hi,
                              double *mid_lo, double *mid_hi) {
    const float k = 1.0f - expf(-2.0f * 3.14159265f * 120.0f / 44100.0f);
    float sl1 = 0, sl2 = 0, ml1 = 0, ml2 = 0;
    double sl = 0, sh = 0, ml = 0, mh = 0;
    for (int i = 0; i < frames; i++) {
        float side = 0.5f * (out[i * 2 + 0] - out[i * 2 + 1]) - 0.5f * (dry[i * 2 + 0] - dry[i * 2 + 1]);
        float mid  = 0.5f * (out[i * 2 + 0] + out[i * 2 + 1]) - 0.5f * (dry[i * 2 + 0] + dry[i * 2 + 1]);
        sl1 += (side - sl1) * k; sl2 += (sl1 - sl2) * k;
        ml1 += (mid  - ml1) * k; ml2 += (ml1 - ml2) * k;
        float s_hi = side - sl2, m_hi = mid - ml2;
        sl += (double)sl2 * sl2; sh += (double)s_hi * s_hi;
        ml += (double)ml2 * ml2; mh += (double)m_hi * m_hi;
    }
    *side_lo = sqrt(sl / frames); *side_hi = sqrt(sh / frames);
    *mid_lo  = sqrt(ml / frames); *mid_hi  = sqrt(mh / frames);
}

static int failures = 0;
static void check(int ok, const char *what) {
    printf("  %s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) failures++;
}

int main(void) {
    printf("staged capture reverb\n");

    /* 1. stage 0 is a true bypass */
    fill_mono(buf, TEST_FRAMES);
    memcpy(warm, buf, sizeof(float) * TEST_FRAMES * 2);
    run(warm, TEST_FRAMES, 0);
    float diff = 0.0f;
    for (int i = 0; i < TEST_FRAMES * 2; i++) {
        float d = fabsf(warm[i] - buf[i]);
        if (d > diff) diff = d;
    }
    printf("  stage 0: max |out-in| = %.9f\n", diff);
    check(diff <= BYPASS_MAX_DIFF, "stage 0 leaves the music untouched");

    /* dry reference for the wet-only measurements */
    fill_mono(warm, WARMUP_FRAMES);
    run(warm, WARMUP_FRAMES, 0);
    fill_mono(buf, TEST_FRAMES);
    run(buf, TEST_FRAMES, 0);
    memcpy(wet, buf, sizeof(float) * TEST_FRAMES * 2);

    /* 2. stages stack: side energy grows 1 -> 2 -> 3 */
    double side_hi_of[4] = { 0 }, side_lo_of[4] = { 0 }, mid_hi_of[4] = { 0 };
    for (int stage = 1; stage <= 3; stage++) {
        fill_mono(warm, WARMUP_FRAMES);
        run(warm, WARMUP_FRAMES, stage);
        fill_mono(buf, TEST_FRAMES);
        run(buf, TEST_FRAMES, stage);
        double sl, sh, ml, mh;
        wet_band_energies(buf, wet, TEST_FRAMES, &sl, &sh, &ml, &mh);
        side_lo_of[stage] = sl; side_hi_of[stage] = sh; mid_hi_of[stage] = mh;
        printf("  stage %d: wet side %.6f (low %.3e / high %.6f), wet mid high %.6f\n",
               stage, sqrt(sl * sl + sh * sh), sl, sh, mh);
    }
    check(side_hi_of[2] > side_hi_of[1] * 1.2, "stage 2 opens more stereo than stage 1");
    check(side_hi_of[3] > side_hi_of[2], "stage 3 opens more stereo than stage 2");

    /* 3. the side carries the top, not the bottom */
    {
        double slope = 20.0 * log10((side_hi_of[3] + 1e-12) / (side_lo_of[3] + 1e-12));
        double width = 20.0 * log10((side_hi_of[3] + 1e-12) / (mid_hi_of[3] + 1e-12));
        printf("  stage 3: wet side high/low = %.1f dB, wet side/mid (high) = %.1f dB\n", slope, width);
        check(slope >= LOW_SIDE_MARGIN_DB, "side low band sits >=20 dB under its own high band");
        check(width <= 0.0, "side never exceeds the mid: no widened level jump");
    }

    /* 4. headroom: driven hard, the output still respects the ceiling */
    {
        fill_mono(warm, WARMUP_FRAMES);
        run(warm, WARMUP_FRAMES, 3);
        fill_mono(buf, TEST_FRAMES);
        for (int i = 0; i < TEST_FRAMES * 2; i++) buf[i] *= 8.0f;   /* ~0.5 peak noise, plus tone */
        for (int i = 0; i < TEST_FRAMES; i++) {
            float s = 0.9f * sinf(2.0f * 3.14159265f * 60.0f * (float)i / 44100.0f);
            buf[i * 2 + 0] += s;
            buf[i * 2 + 1] += s;
        }
        run(buf, TEST_FRAMES, 3);
        float pk = peak(buf, TEST_FRAMES * 2);
        printf("  stage 3, driven hard: output peak %.3f\n", pk);
        check(pk <= STAGE3_MAX_PEAK, "output stays under full scale");
        check(pk > STAGE3_MIN_PEAK, "the effect is audible at stage 3");
    }

    /* 5. the effect must not turn the music up: a card landing is not a volume jump */
    {
        double dry_rms, out_rms;
        fill_mono(warm, WARMUP_FRAMES);
        run(warm, WARMUP_FRAMES, 0);
        fill_mono(buf, TEST_FRAMES);
        run(buf, TEST_FRAMES, 0);
        dry_rms = sqrt(0.0);
        for (int i = 0; i < TEST_FRAMES * 2; i++) dry_rms += (double)buf[i] * buf[i];
        dry_rms = sqrt(dry_rms / (TEST_FRAMES * 2));
        fill_mono(warm, WARMUP_FRAMES);
        run(warm, WARMUP_FRAMES, 3);
        fill_mono(buf, TEST_FRAMES);
        run(buf, TEST_FRAMES, 3);
        out_rms = 0.0;
        for (int i = 0; i < TEST_FRAMES * 2; i++) out_rms += (double)buf[i] * buf[i];
        out_rms = sqrt(out_rms / (TEST_FRAMES * 2));
        double rise = 20.0 * log10(out_rms / dry_rms);
        printf("  stage 3 vs dry: %+.2f dB\n", rise);
        check(rise < 2.0, "stage 3 raises the music by less than 2 dB");
    }

    /* 6. stage 1 already produces some stereo */
    check(side_hi_of[1] > 0.0, "stage 1 produces some stereo");

    if (failures) {
        printf("\n%d check(s) failed\n", failures);
        return 1;
    }
    printf("\nall staged capture-reverb checks passed\n");
    return 0;
}
