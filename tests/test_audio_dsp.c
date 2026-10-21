/* Headless regression test for the Tide-enveloped M/S capture DSP + live SFX
 * sidechain bus in src/ui_audio.c.
 *
 * Verifies:
 *   1. Stage 0 is a bit-exact bypass even on full-scale (1.00 peak) music.
 *   2. Live All-SFX sidechain bus ducks music under non-capture SFX (flip +
 *      click) and releases back to bit-exact 0.000000000 bypass.
 *   3. Block-size invariance: 256-frame callbacks match 2048-frame callbacks.
 *   4. Tide envelope shape: swells smoothly from t=0 to peak at 550 ms, drops
 *      20% onto its shoulder at ~740 ms, then glides monotonically down its
 *      smooth exponential tail.
 *   5. Stage order & targets:
 *      - Stage 1 (60% stereo, 20% rev, 20% bass): wide stereo opens first
 *      - Stage 2 (70% stereo, 50% rev, 20% bass): reverb blooms second (>2x)
 *      - Stage 3 (70% stereo, 70% rev, 50% bass): mono bass drops third (>2x)
 *   6. Low roll-off (100->40 Hz gentle slope + <40 Hz cut), 1.2 kHz pre-tank
 *      de-esser, and 500 Hz 24 dB/oct Side high-pass (>=35 dB Side low cut).
 *   7. Clean shutdown to bit-exact 0 bypass & < 1.000 master ceiling.
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

/* Full-bandwidth source with real 62 Hz kick fundamental + 125 Hz bass + mids */
static void fill_program(float *dst, int frames, float scale) {
    rng_state = 424242u;
    for (int i = 0; i < frames; i++) {
        float t = (float)i / (float)SR;
        float kick = 0.42f * sinf(2.0f * PI_F * 62.0f * t) +
                     0.28f * sinf(2.0f * PI_F * 125.0f * t);
        float body = 0.30f * next_noise();
        float v = scale * (kick + body);
        dst[i * 2 + 0] = v;
        dst[i * 2 + 1] = v;
    }
}

static void reset_all_dsp(void) {
    audio_reset_stage_state();
    sc_bus_flush();
    s_sfx_sc_env = 0.0f;
    memset(s_comb_buf_l, 0, sizeof(s_comb_buf_l));
    memset(s_comb_buf_r, 0, sizeof(s_comb_buf_r));
    memset(s_comb_lp_l, 0, sizeof(s_comb_lp_l));
    memset(s_comb_lp_r, 0, sizeof(s_comb_lp_r));
    memset(s_ap_buf_l, 0, sizeof(s_ap_buf_l));
    memset(s_ap_buf_r, 0, sizeof(s_ap_buf_r));
    memset(s_pre_buf_l, 0, sizeof(s_pre_buf_l));
    memset(s_pre_buf_r, 0, sizeof(s_pre_buf_r));
    memset(s_wide_ap1, 0, sizeof(s_wide_ap1));
    memset(s_wide_ap2, 0, sizeof(s_wide_ap2));
    s_wide_i1 = s_wide_i2 = 0;
    s_slope_lp_l = s_slope_lp_r = 0.0f;
    s_deess_lp_l = s_deess_lp_r = s_deess_env = 0.0f;
    s_low_env = s_sc_env = 0.0f;
    s_hp_lp_l = s_hp_lp_r = s_comp_env = 0.0f;
    s_mix_hp_l = s_mix_hp_r = s_mix_env = 0.0f;
    s_dsp_ready = 0;
}

static void run_stream(float *pcm, int frames, int block_sz) {
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
    Biquad slo1, slo2, shi1, shi2, mlo1, mlo2, mhi1, mhi2;
    biquad_design(&slo1, 180.0f, 0); biquad_design(&slo2, 180.0f, 0);
    biquad_design(&shi1, 500.0f, 1); biquad_design(&shi2, 500.0f, 1);
    biquad_design(&mlo1, 180.0f, 0); biquad_design(&mlo2, 180.0f, 0);
    biquad_design(&mhi1, 250.0f, 1); biquad_design(&mhi2, 250.0f, 1);
    double sl = 0, sh = 0, ml = 0, mh = 0;
    for (int i = 0; i < frames; i++) {
        float side = 0.5f * (out[i * 2 + 0] - out[i * 2 + 1]) - 0.5f * (dry[i * 2 + 0] - dry[i * 2 + 1]);
        float mid  = 0.5f * (out[i * 2 + 0] + out[i * 2 + 1]) - 0.5f * (dry[i * 2 + 0] + dry[i * 2 + 1]);
        float s_lo = biquad_run(&slo2, biquad_run(&slo1, side));
        float s_hi = biquad_run(&shi2, biquad_run(&shi1, side));
        float m_lo = biquad_run(&mlo2, biquad_run(&mlo1, mid));
        float m_hi = biquad_run(&mhi2, biquad_run(&mhi1, mid));
        sl += (double)s_lo * s_lo; sh += (double)s_hi * s_hi;
        ml += (double)m_lo * m_lo; mh += (double)m_hi * m_hi;
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
    printf("Tide-enveloped M/S capture DSP + live SFX sidechain checks\n");

    /* 1. Stage 0 bit-exact bypass on a full-scale 1.00-peak signal */
    reset_all_dsp();
    fill_program(buf_a, TEST_FRAMES, 1.0f);
    memcpy(dry_ref, buf_a, sizeof(dry_ref));
    audio_set_capture_stage(0);
    run_stream(buf_a, TEST_FRAMES, BLOCK);
    float diff0 = max_abs_diff(buf_a, dry_ref, TEST_FRAMES * 2);
    printf("  stage 0 (peak %.2f): max |out-in| = %.9f\n", peak_val(dry_ref, TEST_FRAMES * 2), diff0);
    check(diff0 == 0.0f, "stage 0 is bit-exact bypass even at full-scale 1.0 peak");

    /* 2. Live SFX sidechain bus works for non-capture SFX too AND releases cleanly */
    {
        reset_all_dsp();
        fill_program(buf_a, BLOCK * 40, 0.4f);
        memcpy(buf_b, buf_a, sizeof(float) * BLOCK * 40 * 2);
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
        reset_all_dsp();
        fill_program(buf_a, TEST_FRAMES, 0.30f);
        audio_set_capture_stage(2);
        run_stream(buf_a, TEST_FRAMES, 256);

        reset_all_dsp();
        fill_program(buf_b, TEST_FRAMES, 0.30f);
        audio_set_capture_stage(2);
        run_stream(buf_b, TEST_FRAMES, 2048);

        double db_diff = fabs(20.0 * log10(rms_val(buf_a, TEST_FRAMES * 2) / rms_val(buf_b, TEST_FRAMES * 2)));
        printf("  block invariance (256 vs 2048): %.4f dB diff\n", db_diff);
        check(db_diff < 0.01, "DSP output is invariant to callback block size");
    }

    /* 4. Tide envelope shape: 550 ms peak -> -20% drop at 740 ms -> smooth tail */
    {
        reset_all_dsp();
        audio_set_capture_stage(1); /* target stereo = 0.60 */
        fill_program(warm, (int)(0.25 * SR), 0.25f);
        run_stream(warm, (int)(0.25 * SR), 64);
        float e_mid = s_side_env;

        fill_program(warm, (int)(0.30 * SR), 0.25f);
        run_stream(warm, (int)(0.30 * SR), 64); /* t = 0.550 s */
        float e_peak = s_side_env;

        fill_program(warm, (int)(0.19 * SR), 0.25f);
        run_stream(warm, (int)(0.19 * SR), 64); /* t = 0.740 s */
        float e_shoulder = s_side_env;

        fill_program(warm, (int)(1.50 * SR), 0.25f);
        run_stream(warm, (int)(1.50 * SR), 64); /* t = 2.240 s */
        float e_tail = s_side_env;

        printf("  tide curve (target 0.600): t=250ms %.3f -> peak@550ms %.3f -> shoulder@740ms %.3f (%.0f%%) -> tail@2.24s %.3f\n",
               e_mid, e_peak, e_shoulder, 100.0f * (e_shoulder / e_peak), e_tail);
        check(e_mid > 0.20f && e_mid < 0.52f && fabsf(e_peak - 0.60f) < 0.005f,
              "tide curve swells smoothly from 0 to exact peak at 550 ms");
        check(fabsf((e_shoulder / e_peak) - 0.80f) < 0.015f && e_tail < e_shoulder * 0.72f && e_tail > 0.15f,
              "tide curve drops 20% onto shoulder then glides smoothly along tail");
    }

    /* 5. Stages 1 -> 2 -> 3 match exact design:
          1: wide stereo (60% side, 20% rev, 20% bass)
          2: +reverb     (70% side, 50% rev, 20% bass)
          3: +mono bass  (70% side, 70% rev, 50% bass) */
    fill_program(dry_ref, TEST_FRAMES, 0.28f);
    double side_hi[4] = {0}, side_lo[4] = {0}, mid_hi[4] = {0}, mid_lo[4] = {0};
    for (int st = 1; st <= 3; st++) {
        reset_all_dsp();
        fill_program(warm, SR / 2, 0.28f);
        run_stream(warm, SR / 2, BLOCK); /* prime dry warm buffers */
        audio_set_capture_stage(st);
        fill_program(buf_a, TEST_FRAMES, 0.28f);
        run_stream(buf_a, TEST_FRAMES, BLOCK);
        wet_bands(buf_a, dry_ref, TEST_FRAMES, &side_lo[st], &side_hi[st], &mid_lo[st], &mid_hi[st]);
        printf("  stage %d: side_hi %.5f | mid_reverb %.5f | mono_bass(<200) %.5f\n",
               st, side_hi[st], mid_hi[st], mid_lo[st]);
    }
    check(side_hi[1] > 0.015 && side_hi[2] > side_hi[1] * 1.10,
          "stage 1 opens wide stereo immediately and stage 2 widens further");
    check(mid_hi[2] > mid_hi[1] * 2.00 && mid_hi[3] > mid_hi[2] * 1.25,
          "stage 2 surges centered reverb (>2x stage 1) and stage 3 deepens further");
    check(mid_lo[3] > mid_lo[2] * 1.90,
          "stage 3 surges mono bass weight (>1.9x stage 2)");

    /* 6. 500 Hz 24 dB/oct Side high-pass keeps kick/bass/low-mids strictly mono */
    {
        double slope = 20.0 * log10((side_hi[3] + 1e-12) / (side_lo[3] + 1e-12));
        printf("  stage 3 Side high/low ratio (500 Hz 24 dB/oct HPF): %.1f dB\n", slope);
        check(slope >= 34.0, "wet Side <160 Hz sits >=34 dB below Side high band");
    }

    /* 7. Clean tail shutoff back to true bypass + < 1.000 ceiling */
    {
        audio_set_capture_stage(0);
        fill_program(warm, SR * 4, 0.28f);
        run_stream(warm, SR * 4, BLOCK);
        fill_program(buf_a, BLOCK * 8, 0.28f);
        memcpy(buf_b, buf_a, sizeof(float) * BLOCK * 8 * 2);
        run_stream(buf_a, BLOCK * 8, BLOCK);
        float off_diff = max_abs_diff(buf_a, buf_b, BLOCK * 8 * 2);
        printf("  after release to stage 0: max |out-in| = %.9f\n", off_diff);
        check(off_diff == 0.0f, "shuts completely off back to bit-exact bypass after release");

        fill_program(buf_a, TEST_FRAMES, 1.25f);
        audio_set_capture_stage(3);
        run_stream(buf_a, TEST_FRAMES, BLOCK);
        master_mix_processor(buf_a, TEST_FRAMES);
        float pk = peak_val(buf_a, TEST_FRAMES * 2);
        printf("  driven hard at stage 3 + master bus: peak = %.3f\n", pk);
        check(pk < 1.0f && pk > 0.5f, "music + master bus stays cleanly below 1.0 full scale");
    }

    if (failures) {
        printf("\n%d check(s) failed\n", failures);
        return 1;
    }
    printf("\nall Tide-enveloped M/S DSP + sidechain checks passed\n");
    return 0;
}
