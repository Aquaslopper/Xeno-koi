/* Headless regression test for the staged capture DSP + live SFX sidechain bus
 * in src/ui_audio.c.
 *
 * Verifies:
 *   1. Stage 0 is a bit-exact bypass even on full-scale (1.00 peak) music.
 *   2. Block-size invariance: 256-frame callbacks match 2048-frame callbacks.
 *   3. Live SFX sidechain bus: ANY SFX stream (flip, drop, koi, click, ...)
 *      ducks the music proportionally to its waveform and releases back to
 *      exact bypass after the SFX ends (cannot stick).
 *   4. Stages 1 -> 2 -> 3 audibly stack across all three effects:
 *      reverb mid (1 < 2 < 3), stereo side (1 < 2 < 3), and mono low bass
 *      (<180 Hz, stage 3 > stage 1).
 *   5. Wet Side low band sits >=25 dB below its own high band (mono kick/bass).
 *   6. Clean shutdown: returning from stage 3 to stage 0 restores bit-exact
 *      bypass within 3 seconds instead of ringing indefinitely.
 *   7. Driven hard on both music and master mix buses, output stays < 1.000.
 *
 * Builds against tests/raylib_audio_stub/raylib.h. Run via ./tests/test_audio_dsp.sh
 */
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "../src/ui_audio.c"

#define SR              44100
#define BLOCK           512
#define WARMUP_FRAMES   (SR * 2)
#define TEST_FRAMES     (SR * 2)

static float buf_a[TEST_FRAMES * 2];
static float buf_b[TEST_FRAMES * 2];
static float dry_ref[TEST_FRAMES * 2];
static float warm[WARMUP_FRAMES * 2];
static float sfx_buf[BLOCK * 2];

static unsigned int rng_state = 12345u;

static float next_noise(void) {
    rng_state = (1103515245u * rng_state + 12345u) & 0x7fffffffu;
    return ((float)rng_state / (float)0x7fffffff) * 2.0f - 1.0f;
}

/* Full-bandwidth mono music source containing real low end (95 Hz + 145 Hz)
   plus broadband body so all bands (reverb, side, mono bass) are exercised. */
static void fill_program(float *dst, int frames, float scale) {
    rng_state = 424242u;
    for (int i = 0; i < frames; i++) {
        float t = (float)i / (float)SR;
        float bass = 0.45f * sinf(2.0f * PI_F * 105.0f * t) +
                     0.30f * sinf(2.0f * PI_F * 155.0f * t);
        float body = 0.25f * next_noise();
        float v = scale * (bass + body);
        dst[i * 2 + 0] = v;
        dst[i * 2 + 1] = v;
    }
}

static void run_blocks(float *pcm, int frames, int stage, int block_sz) {
    audio_set_capture_stage(stage);
    int pos = 0;
    while (pos < frames) {
        int n = (frames - pos < block_sz) ? (frames - pos) : block_sz;
        music_reverb_processor(pcm + pos * 2, (unsigned int)n);
        pos += n;
    }
}

static float max_abs_diff(const float *a, const float *b, int count) {
    float m = 0.0f;
    for (int i = 0; i < count; i++) {
        float d = fabsf(a[i] - b[i]);
        if (d > m) m = d;
    }
    return m;
}

static float peak_val(const float *v, int count) {
    float p = 0.0f;
    for (int i = 0; i < count; i++) if (fabsf(v[i]) > p) p = fabsf(v[i]);
    return p;
}

static double rms_val(const float *v, int count) {
    double s = 0.0;
    for (int i = 0; i < count; i++) s += (double)v[i] * v[i];
    return sqrt(s / (double)count);
}

static void wet_bands(const float *out, const float *dry, int frames,
                      double *side_lo, double *side_hi,
                      double *mid_lo, double *mid_hi) {
    const float k120 = 1.0f - expf(-2.0f * PI_F * 120.0f / (float)SR);
    const float k180 = 1.0f - expf(-2.0f * PI_F * 180.0f / (float)SR);
    float sl1 = 0, sl2 = 0, ml1 = 0, ml2 = 0;
    double sl = 0, sh = 0, ml = 0, mh = 0;
    for (int i = 0; i < frames; i++) {
        float side = 0.5f * (out[i * 2 + 0] - out[i * 2 + 1]) - 0.5f * (dry[i * 2 + 0] - dry[i * 2 + 1]);
        float mid  = 0.5f * (out[i * 2 + 0] + out[i * 2 + 1]) - 0.5f * (dry[i * 2 + 0] + dry[i * 2 + 1]);
        sl1 += (side - sl1) * k120; sl2 += (sl1 - sl2) * k120;
        ml1 += (mid  - ml1) * k180; ml2 += (ml1 - ml2) * k180;
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
    printf("staged capture DSP + live SFX sidechain checks\n");

    /* 1. Stage 0 bit-exact bypass on a full-scale 1.00-peak signal */
    audio_reset_stage_state();
    fill_program(buf_a, TEST_FRAMES, 1.0f);
    memcpy(dry_ref, buf_a, sizeof(dry_ref));
    run_blocks(buf_a, TEST_FRAMES, 0, BLOCK);
    float diff0 = max_abs_diff(buf_a, dry_ref, TEST_FRAMES * 2);
    printf("  stage 0 (peak %.2f): max |out-in| = %.9f\n", peak_val(dry_ref, TEST_FRAMES * 2), diff0);
    check(diff0 == 0.0f, "stage 0 is bit-exact bypass even at full-scale 1.0 peak");

    /* 2. Live SFX sidechain bus works for non-capture SFX too AND releases cleanly */
    {
        fill_program(buf_a, BLOCK * 40, 0.4f);
        memcpy(buf_b, buf_a, sizeof(float) * BLOCK * 40 * 2);
        /* Fire sfx_tap_0 (flip) and sfx_tap_6 (click) for 3 blocks, then silence */
        for (int b = 0; b < 40; b++) {
            if (b < 3) {
                for (int i = 0; i < BLOCK * 2; i++) sfx_buf[i] = 0.55f;
                sfx_tap_0(sfx_buf, BLOCK); /* flip */
                sfx_tap_6(sfx_buf, BLOCK); /* click */
            }
            music_reverb_processor(buf_a + b * BLOCK * 2, BLOCK);
        }
        double dipped = rms_val(buf_a + BLOCK * 2, BLOCK * 4);
        double orig   = rms_val(buf_b + BLOCK * 2, BLOCK * 4);
        double dip_db = 20.0 * log10(dipped / orig);
        float tail_diff = max_abs_diff(buf_a + 36 * BLOCK * 2, buf_b + 36 * BLOCK * 2, BLOCK * 4 * 2);
        printf("  live SFX sidechain: dip %.2f dB, post-release max |out-in| = %.9f\n", dip_db, tail_diff);
        check(dip_db < -2.0, "non-capture SFX (flip + click) sidechains the music");
        check(tail_diff == 0.0f, "sidechain releases completely back to bit-exact bypass");
    }

    /* 3. Block-size invariance (256 vs 2048 frames) */
    {
        audio_reset_stage_state();
        fill_program(warm, WARMUP_FRAMES, 0.30f);
        run_blocks(warm, WARMUP_FRAMES, 2, 256);
        fill_program(buf_a, TEST_FRAMES, 0.30f);
        run_blocks(buf_a, TEST_FRAMES, 2, 256);

        audio_reset_stage_state();
        memset(s_comb_buf_l, 0, sizeof(s_comb_buf_l));
        memset(s_comb_buf_r, 0, sizeof(s_comb_buf_r));
        memset(s_comb_lp_l, 0, sizeof(s_comb_lp_l));
        memset(s_comb_lp_r, 0, sizeof(s_comb_lp_r));
        memset(s_ap_buf_l, 0, sizeof(s_ap_buf_l));
        memset(s_ap_buf_r, 0, sizeof(s_ap_buf_r));
        memset(s_pre_buf_l, 0, sizeof(s_pre_buf_l));
        memset(s_pre_buf_r, 0, sizeof(s_pre_buf_r));
        s_dsp_ready = 0;

        fill_program(warm, WARMUP_FRAMES, 0.30f);
        run_blocks(warm, WARMUP_FRAMES, 2, 2048);
        fill_program(buf_b, TEST_FRAMES, 0.30f);
        run_blocks(buf_b, TEST_FRAMES, 2, 2048);

        double db_diff = fabs(20.0 * log10(rms_val(buf_a, TEST_FRAMES * 2) / rms_val(buf_b, TEST_FRAMES * 2)));
        printf("  block invariance (256 vs 2048): %.4f dB diff\n", db_diff);
        check(db_diff < 0.01, "DSP output is invariant to callback block size");
    }

    /* 4. Stages 1 -> 2 -> 3 stack across reverb mid, stereo side, AND mono bass */
    fill_program(dry_ref, TEST_FRAMES, 0.30f);
    double side_hi[4] = {0}, side_lo[4] = {0}, mid_hi[4] = {0}, mid_lo[4] = {0};
    for (int st = 1; st <= 3; st++) {
        fill_program(warm, WARMUP_FRAMES, 0.30f);
        run_blocks(warm, WARMUP_FRAMES, st, BLOCK);
        fill_program(buf_a, TEST_FRAMES, 0.30f);
        run_blocks(buf_a, TEST_FRAMES, st, BLOCK);
        wet_bands(buf_a, dry_ref, TEST_FRAMES, &side_lo[st], &side_hi[st], &mid_lo[st], &mid_hi[st]);
        printf("  stage %d: mid_hi %.5f | side_hi %.5f | mono_bass(<180) %.5f\n",
               st, mid_hi[st], side_hi[st], mid_lo[st]);
    }
    check(mid_hi[2] > mid_hi[1] * 1.15 && mid_hi[3] > mid_hi[2] * 1.12, "reverb Mid grows across stages 1 < 2 < 3");
    check(side_hi[2] > side_hi[1] * 1.80 && side_hi[3] > side_hi[2] * 1.18, "stereo Side grows across stages 1 < 2 < 3");
    check(mid_lo[3] > mid_lo[2] * 1.35, "stage 3 mono bass boost clearly lifts low body over stage 2");

    /* 5. Lows stay out of Side */
    {
        double slope = 20.0 * log10((side_hi[3] + 1e-12) / (side_lo[3] + 1e-12));
        printf("  stage 3 Side high/low ratio: %.1f dB\n", slope);
        check(slope >= 25.0, "wet Side low band sits >=25 dB below Side high band");
    }

    /* 6. Clean tail shutoff back to true bypass */
    {
        fill_program(warm, SR * 4, 0.30f);
        run_blocks(warm, SR * 4, 0, BLOCK);
        fill_program(buf_a, BLOCK * 8, 0.30f);
        memcpy(buf_b, buf_a, sizeof(float) * BLOCK * 8 * 2);
        run_blocks(buf_a, BLOCK * 8, 0, BLOCK);
        float off_diff = max_abs_diff(buf_a, buf_b, BLOCK * 8 * 2);
        printf("  after tail release to stage 0: max |out-in| = %.9f\n", off_diff);
        check(off_diff == 0.0f, "reverb shuts completely off back to exact bypass after release");
    }

    /* 7. Headroom on both music processor and master mix processor */
    {
        fill_program(buf_a, TEST_FRAMES, 1.25f);
        run_blocks(buf_a, TEST_FRAMES, 3, BLOCK);
        master_mix_processor(buf_a, TEST_FRAMES);
        float pk = peak_val(buf_a, TEST_FRAMES * 2);
        printf("  driven hard at stage 3 + master bus: peak = %.3f\n", pk);
        check(pk < 1.0f && pk > 0.5f, "music + master bus stays cleanly below 1.0 full scale");
    }

    if (failures) {
        printf("\n%d check(s) failed\n", failures);
        return 1;
    }
    printf("\nall staged capture DSP + sidechain checks passed\n");
    return 0;
}
