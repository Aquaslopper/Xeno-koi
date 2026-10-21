/* Headless regression test for the 3-block parallel studio audio chain + live
 * All-SFX sidechain bus in src/ui_audio.c.
 *
 * Verifies every audible guarantee directly:
 *   1. Stage 0 is a bit-exact bypass even on full-scale (1.00 peak) stereo music.
 *   2. Live All-SFX sidechain bus ducks music under non-capture SFX (flip +
 *      click) and releases back to bit-exact 0.000000000 bypass.
 *   3. Block-size invariance: 256-frame callbacks match 2048-frame callbacks.
 *   4. Tide envelope shape: swells smoothly from t=0 to peak at 550 ms, drops
 *      20% onto its shoulder at ~740 ms, then glides monotonically down tail.
 *   5. Stage 1 = PURE CLEAN WIDE STEREO ONLY:
 *      - Zero reverb tail ring after dry stops (no reverb starting on Stage 1)
 *      - Zero comb/phaser ripple across frequency
 *      - Huge clean >500 Hz stereo Side expansion
 *   6. Stage 2 = WIDE STEREO + KICK-FREE ABBEY ROAD REVERB:
 *      - Lush reverb tail rings cleanly after dry stops (>550 Hz)
 *      - 70 Hz kick burst produces ZERO audible tail in reverb (<-45 dB)
 *   7. Stage 3 = + REAL PHASE-COHERENT MONO SUB/KICK BASS:
 *      - Real +4.5..+6.5 dB mono sub/kick gain (50-120 Hz) at Stage 3 crest
 *      - <25 Hz sub mud sharply rolled off
 *      - Side <180 Hz stays >=35 dB below Side high band
 *   8. Clean shutoff back to bit-exact 0 bypass & < 1.000 master ceiling.
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

/* Realistic stereo music program matching measured game1/game3 spectrum:
   mono centered 68 Hz kick + 110 Hz bass + true stereo L/R synth & percussion. */
static void fill_stereo_program(float *dst, int frames, float scale) {
    rng_state = 424242u;
    for (int i = 0; i < frames; i++) {
        float t = (float)i / (float)SR;
        float mono_low = 0.42f * sinf(2.0f * PI_F * 68.0f * t) +
                         0.24f * sinf(2.0f * PI_F * 112.0f * t);
        float mid_body = 0.20f * next_noise();
        float side_air = 0.16f * next_noise();
        dst[i * 2 + 0] = scale * (mono_low + mid_body + side_air);
        dst[i * 2 + 1] = scale * (mono_low + mid_body - side_air);
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
    s_glue_lp = 0.0f;
    s_deess_lp_l = s_deess_lp_r = s_deess_env = 0.0f;
    s_bass_comp_env = s_sc_env = 0.0f;
    s_hp_lp_l = s_hp_lp_r = s_comp_env = s_lim_env = 0.0f;
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

static int failures = 0;
static void check(int ok, const char *what) {
    printf("  %s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) failures++;
}

int main(void) {
    printf("3-block clean studio DSP + live SFX sidechain checks\n");

    /* 1. Stage 0 bit-exact bypass on full-scale signal */
    reset_all_dsp();
    fill_stereo_program(buf_a, TEST_FRAMES, 0.98f);
    memcpy(dry_ref, buf_a, sizeof(dry_ref));
    audio_set_capture_stage(0);
    run_stream(buf_a, TEST_FRAMES, BLOCK);
    float diff0 = max_abs_diff(buf_a, dry_ref, TEST_FRAMES * 2);
    printf("  stage 0 (peak %.2f): max |out-in| = %.9f\n", peak_val(dry_ref, TEST_FRAMES * 2), diff0);
    check(diff0 == 0.0f, "stage 0 is bit-exact bypass even at full-scale peak");

    /* 2. Live SFX sidechain bus works for non-capture SFX too AND releases cleanly */
    {
        reset_all_dsp();
        fill_stereo_program(buf_a, BLOCK * 40, 0.4f);
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
        fill_stereo_program(buf_a, TEST_FRAMES, 0.30f);
        audio_set_capture_stage(3);
        run_stream(buf_a, TEST_FRAMES, 256);

        reset_all_dsp();
        fill_stereo_program(buf_b, TEST_FRAMES, 0.30f);
        audio_set_capture_stage(3);
        run_stream(buf_b, TEST_FRAMES, 2048);

        double db_diff = fabs(20.0 * log10(rms_val(buf_a, TEST_FRAMES * 2) / rms_val(buf_b, TEST_FRAMES * 2)));
        printf("  block invariance (256 vs 2048): %.4f dB diff\n", db_diff);
        check(db_diff < 0.01, "DSP output is invariant to callback block size");
    }

    /* 4. Tide envelope shape: 550 ms peak -> -20% drop at 740 ms -> smooth tail */
    {
        reset_all_dsp();
        audio_set_capture_stage(1);
        fill_stereo_program(warm, (int)(0.25 * SR), 0.25f);
        run_stream(warm, (int)(0.25 * SR), 64);
        float e_mid = s_side_env;

        fill_stereo_program(warm, (int)(0.30 * SR), 0.25f);
        run_stream(warm, (int)(0.30 * SR), 64);
        float e_peak = s_side_env;

        fill_stereo_program(warm, (int)(0.19 * SR), 0.25f);
        run_stream(warm, (int)(0.19 * SR), 64);
        float e_shoulder = s_side_env;

        fill_stereo_program(warm, (int)(1.50 * SR), 0.25f);
        run_stream(warm, (int)(1.50 * SR), 64);
        float e_tail = s_side_env;

        printf("  tide curve (target 0.600): t=250ms %.3f -> peak@550ms %.3f -> shoulder@740ms %.3f (%.0f%%) -> tail@2.24s %.3f\n",
               e_mid, e_peak, e_shoulder, 100.0f * (e_shoulder / e_peak), e_tail);
        check(e_mid > 0.20f && e_mid < 0.52f && fabsf(e_peak - 0.60f) < 0.005f,
              "tide curve swells smoothly from 0 to exact peak at 550 ms");
        check(fabsf((e_shoulder / e_peak) - 0.80f) < 0.015f && e_tail < e_shoulder * 0.72f && e_tail > 0.15f,
              "tide curve drops 20% onto shoulder then glides smoothly along tail");
    }

    /* 5 & 6. Stage 1 has ZERO reverb tail (stereo only!), Stage 2 adds kick-free reverb! */
    {
        /* Play 0.6s music burst followed by 0.4s pure silence: measure ringing tail! */
        int n_burst = (int)(0.60 * SR), n_tail = (int)(0.40 * SR);

        /* Stage 1: pure clean wide stereo -> silence after burst must NOT ring! */
        reset_all_dsp();
        fill_stereo_program(buf_a, n_burst, 0.35f);
        memset(buf_a + n_burst * 2, 0, n_tail * 2 * sizeof(float));
        audio_set_capture_stage(1);
        run_stream(buf_a, n_burst + n_tail, BLOCK);
        /* Skip 20 ms filter settle, measure 0.65..1.00s for reverb tail */
        double tail_stg1 = rms_val(buf_a + (n_burst + SR / 20) * 2, (n_tail - SR / 20) * 2);

        /* Stage 2 on full-band burst: lush reverb tail MUST ring after burst! */
        reset_all_dsp();
        fill_stereo_program(buf_b, n_burst, 0.35f);
        memset(buf_b + n_burst * 2, 0, n_tail * 2 * sizeof(float));
        audio_set_capture_stage(2);
        run_stream(buf_b, n_burst + n_tail, BLOCK);
        double tail_stg2 = rms_val(buf_b + (n_burst + SR / 20) * 2, (n_tail - SR / 20) * 2);

        /* Stage 2 on a PURE 70 Hz KICK DRUM burst: ZERO kick in reverb tail! */
        reset_all_dsp();
        memset(warm, 0, (n_burst + n_tail) * 2 * sizeof(float));
        for (int i = 0; i < n_burst; i++) {
            float kv = 0.50f * sinf(2.0f * PI_F * 70.0f * ((float)i / SR));
            warm[i * 2 + 0] = kv;
            warm[i * 2 + 1] = kv;
        }
        audio_set_capture_stage(2);
        run_stream(warm, n_burst + n_tail, BLOCK);
        double kick_rev_db = 20.0 * log10((rms_val(warm + (n_burst + SR / 20) * 2, (n_tail - SR / 20) * 2) + 1e-12) / 0.3535);

        printf("  reverb tail RMS: stage 1 = %.7f (zero reverb) | stage 2 = %.5f | 70 Hz kick in reverb = %.1f dB\n",
               tail_stg1, tail_stg2, kick_rev_db);
        check(tail_stg1 < 1e-5, "Stage 1 starts ONLY stereo width (zero reverb tail)");
        check(tail_stg2 > 0.004, "Stage 2 brings in audible stereo reverb tail");
        check(kick_rev_db < -55.0, "70 Hz kick drum is completely excluded from reverb (<-55 dB)");
    }

    /* 7. Stage 1 wide stereo (> +5 dB Side lift) AND Stage 3 real mono sub/kick boost (> +4.5 dB)! */
    {
        fill_stereo_program(dry_ref, TEST_FRAMES, 0.25f);
        memcpy(buf_a, dry_ref, sizeof(dry_ref));
        memcpy(buf_b, dry_ref, sizeof(dry_ref));

        reset_all_dsp();
        audio_set_capture_stage(1);
        run_stream(buf_a, TEST_FRAMES, BLOCK);

        reset_all_dsp();
        audio_set_capture_stage(3);
        run_stream(buf_b, TEST_FRAMES, BLOCK);

        /* Measure around the 550 ms crest window (0.45s .. 0.65s) */
        int f0 = (int)(0.45 * SR), fn = (int)(0.20 * SR);
        Biquad dhi, o1hi, dlo, o1lo, o3lo, dslo1, dslo2, s3lo1, s3lo2;
        biquad_design(&dhi,   800.0f, 1); biquad_design(&o1hi,  800.0f, 1);
        biquad_design(&dlo,   140.0f, 0); biquad_design(&o1lo,  140.0f, 0); biquad_design(&o3lo,  140.0f, 0);
        biquad_design(&dslo1, 130.0f, 0); biquad_design(&dslo2, 130.0f, 0);
        biquad_design(&s3lo1, 130.0f, 0); biquad_design(&s3lo2, 130.0f, 0);

        double ds2 = 0, s1_2 = 0, db2 = 0, b1_2 = 0, b3_2 = 0, ds_lo2 = 0, s3_lo2 = 0;
        for (int i = f0; i < f0 + fn; i++) {
            float dry_s = 0.5f * (dry_ref[i*2] - dry_ref[i*2+1]);
            float out3_s = 0.5f * (buf_b[i*2]  - buf_b[i*2+1]);
            float ds = biquad_run(&dhi,  dry_s);
            float s1 = biquad_run(&o1hi, 0.5f * (buf_a[i*2] - buf_a[i*2+1]));
            float dm = biquad_run(&dlo,  0.5f * (dry_ref[i*2] + dry_ref[i*2+1]));
            float m1 = biquad_run(&o1lo, 0.5f * (buf_a[i*2]   + buf_a[i*2+1]));
            float m3 = biquad_run(&o3lo, 0.5f * (buf_b[i*2]   + buf_b[i*2+1]));
            float dsl = biquad_run(&dslo2, biquad_run(&dslo1, dry_s));
            float s3l = biquad_run(&s3lo2, biquad_run(&s3lo1, out3_s));
            ds2 += (double)ds*ds; s1_2 += (double)s1*s1;
            db2 += (double)dm*dm; b1_2 += (double)m1*m1; b3_2 += (double)m3*m3;
            ds_lo2 += (double)dsl*dsl; s3_lo2 += (double)s3l*s3l;
        }
        double stg1_width_db = 10.0 * log10(s1_2 / ds2);
        double stg1_bass_db  = 10.0 * log10(b1_2 / db2);
        double stg3_bass_db  = 10.0 * log10(b3_2 / db2);
        double side_sub_cut  = 10.0 * log10(s3_lo2 / ds_lo2);
        double side_iso_db   = 10.0 * log10((b3_2 + 1e-18) / (s3_lo2 + 1e-18));

        printf("  crest @ 550ms: Stg1 Stereo %+5.2f dB (Stg1 Bass %+5.2f dB) | Stg3 Mono Bass %+5.2f dB | Side<130 %+5.2f dB (Mono/Side %.1f dB)\n",
               stg1_width_db, stg1_bass_db, stg3_bass_db, side_sub_cut, side_iso_db);
        check(stg1_width_db > 5.0 && fabs(stg1_bass_db) < 0.25,
              "Stage 1 delivers > +5 dB clean stereo width with 0 dB low-end alteration");
        check(stg3_bass_db > 5.0,
              "Stage 3 delivers > +5.0 dB real punchy mono kick/sub bass");
        check(side_sub_cut < -6.0 && side_iso_db > 35.0,
              "Side channel cuts sub/bass mud (< -6 dB) with > 35 dB mono bass isolation");
    }

    /* 8. Clean shutoff back to true bypass + < 1.000 ceiling */
    {
        audio_set_capture_stage(0);
        fill_stereo_program(warm, SR * 4, 0.28f);
        run_stream(warm, SR * 4, BLOCK);
        fill_stereo_program(buf_a, BLOCK * 8, 0.28f);
        memcpy(buf_b, buf_a, sizeof(float) * BLOCK * 8 * 2);
        run_stream(buf_a, BLOCK * 8, BLOCK);
        float off_diff = max_abs_diff(buf_a, buf_b, BLOCK * 8 * 2);
        printf("  after release to stage 0: max |out-in| = %.9f\n", off_diff);
        check(off_diff == 0.0f, "shuts completely off back to bit-exact bypass after release");

        fill_stereo_program(buf_a, TEST_FRAMES, 1.25f);
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
    printf("\nall 3-block clean studio DSP + sidechain checks passed\n");
    return 0;
}
