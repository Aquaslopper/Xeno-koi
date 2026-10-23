/* Exercise the REAL callback: numerical audio tests, not mocked DSP results. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/ui_audio.c"

#define MAX_FRAMES (96000 * 6)
static float signal[MAX_FRAMES * 2], original[MAX_FRAMES * 2];
static int sample_rate = 44100;

static void prepare(float chorus, float bass, float room, float action) {
    reset_music_dsp();
    audio_set_effects(chorus, bass, room);
    audio_set_reverb(action);
    memset(signal, 0, sample_rate * 2 * sizeof(float));
    music_reverb_processor(signal, sample_rate); /* Settle parameter smoothing in silence. */
}

static void tone(int frames, float hz, float level, float right) {
    for (int i = 0; i < frames; i++) {
        float v = level * sinf(6.28318530718f * hz * i / sample_rate);
        signal[2*i] = v; signal[2*i + 1] = v * right;
    }
}

static double energy(int start, int end, int channel) {
    double sum = 0;
    for (int i = start; i < end; i++) {
        double x = signal[2*i + channel]; sum += x*x;
    }
    return sum / (end - start);
}

static void finite_peak(int frames, float max) {
    for (int i = 0; i < frames*2; i++) assert(isfinite(signal[i]) && fabsf(signal[i]) <= max);
}

static void noise(int frames, float level) {
    unsigned int rng = 17;
    for (int i = 0; i < frames; i++) {
        rng = rng * 1664525u + 1013904223u;
        float x = level * ((rng >> 8) / 8388608.0f - 1);
        signal[2*i] = signal[2*i + 1] = x;
    }
}

static float bass_gain(float setting, float level, float hz) {
    prepare(0, setting, 0, 1);
    tone(sample_rate*2, hz, level, 0.5f);
    music_reverb_processor(signal, sample_rate*2);
    for (int i = sample_rate; i < 2*sample_rate; i++)
        assert(fabsf(signal[2*i] - 2*signal[2*i + 1]) < 1e-5f); /* Linked gain, no image shift. */
    finite_peak(2*sample_rate, 2.50001f);
    return 10*log10(energy(sample_rate, 2*sample_rate, 0) / (level*level*0.5));
}

static double bass_tail(float room) {
    prepare(0, 0, room, 1);
    tone(sample_rate, 40, 0.02f, 1);
    memset(signal + sample_rate*2, 0, sample_rate*4*sizeof(float));
    music_reverb_processor(signal, sample_rate*3);
    return energy(sample_rate + sample_rate/10, sample_rate*2, 0) + energy(sample_rate + sample_rate/10, sample_rate*2, 1);
}

/* +100% means exactly twice the existing return, not different modulation. */
static void chorus_return_is_doubled(void) {
    prepare(1, 0, 0, 1);
    s_chorus_phase = 0; s_chorus_index = 0;
    s_chorus_history[0][CHORUS_HISTORY-662] = s_chorus_history[1][CHORUS_HISTORY-662] = 0.04f;
    BiquadState reference_hp = {0};
    float expected = 2.0f*biquad_hp300(&reference_hp, 0.5f*0.04f);
    music_reverb_processor(signal, 1);
    printf("chorus_return_is_doubled: expected %.9g, actual %.9g\n", expected, signal[0]);
    assert(signal[0] == expected && signal[1] == -expected);
}

/* Maintainer chose generated Side, not channel-swap commutation or fixed pan. */
static void chorus_routing(void) {
    int failures[5] = {0};
    const char *names[5] = {"chorus_keeps_mono_sum", "chorus_creates_side", "chorus_no_ear_bias",
                           "chorus_off_is_bit_identical", "chorus_decorrelates"};
    for (int stereo = 0; stereo < 2; stereo++) {
        double previous = 0;
        for (int step = 0; step <= 4; step++) {
            float amount = step*0.25f;
            prepare(amount, 0, 0, 1); noise(sample_rate*3, 0.05f);
            /* Stereo fixture: one-second cyclic shift, outside the chorus taps. */
            if (stereo) for (int i = 0; i < sample_rate*3; i++)
                signal[2*i+1] = signal[2*((i+sample_rate)%(sample_rate*3))];
            memcpy(original, signal, sample_rate*6*sizeof(float));
            music_reverb_processor(signal, sample_rate*3);
            double input = 0, side = 0, left = 0, right = 0, cross = 0, sl = 0, sr = 0, error = 0;
            for (int i = sample_rate; i < sample_rate*3; i++) {
                double l = signal[2*i], r = signal[2*i+1], m = (original[2*i]+(double)original[2*i+1])/2;
                input += m*m; side += (l-r)*(l-r)/4;
                left += l*l; right += r*r; cross += l*r; sl += l; sr += r;
                error = fmax(error, fabs((l+r)/2-m));
            }
            double ratio = sqrt(side/input), balance = 10*log10(left/right), n = sample_rate*2;
            double correlation = (cross-sl*sr/n)/sqrt((left-sl*sl/n)*(right-sr*sr/n));
            printf("%d Hz %s %.2f: mono error %.9g, Side/input %.6f, correlation %.6f, L/R %+.6f dB\n",
                   sample_rate, stereo ? "stereo" : "mono", amount, error, ratio, correlation, balance);
            failures[0] += !(error < 1e-6);
            failures[1] += !stereo && step && !(ratio > 0.1*amount && ratio > previous);
            failures[2] += !stereo && !(fabs(balance) < 0.1);
            failures[3] += !step && memcmp(original, signal, sample_rate*6*sizeof(float)) != 0;
            failures[4] += !stereo && step == 4 && !(correlation < 0.9);
            previous = ratio;
        }
    }
    for (int i = 0; i < 5; i++) printf("%s: %s (%d failures)\n", names[i], failures[i] ? "FAIL" : "PASS", failures[i]);
    assert(failures[0]+failures[1]+failures[2]+failures[3]+failures[4] == 0);
}

static void chorus_behaviour(void) {
    for (int step = 1; step <= 4; step++) {
        float amount = step*0.25f;
        prepare(amount, 0, 0, 1);
        memset(signal, 0, sample_rate*2*sizeof(float));
        signal[0] = signal[1] = 0.1f;
        music_reverb_processor(signal, sample_rate);
        float voices = 0, tail = 0;
        for (int i = sample_rate/200; i < sample_rate; i++) {
            float x = fabsf(signal[2*i]);
            voices = fmaxf(voices, x);
            if (i > sample_rate/25) tail = fmaxf(tail, x);
        }
        printf("%d Hz chorus %.2f: delayed voices %.6f, tail after 40ms %.9g\n", sample_rate, amount, voices, tail);
        assert(voices > 1e-5); /* HP has IIR decay; history must have no delay feedback. */
        for (int c = 0; c < 2; c++) for (int i = 0; i < CHORUS_HISTORY; i++) assert(s_chorus_history[c][i] == 0);
        prepare(amount, 0, 0, 1);
        tone(sample_rate*4, 1000, 0.04f, 0.25f);
        music_reverb_processor(signal, sample_rate*4);
        double motion = 0;
        for (int i = sample_rate; i < sample_rate*2; i++) {
            double d = signal[2*i] - signal[2*(i+2*sample_rate)];
            motion += d*d;
        }
        printf("%d Hz chorus %.2f: modulation RMS %.6f\n", sample_rate, amount, sqrt(motion/sample_rate));
        assert(motion/sample_rate > 1e-8); /* Actually modulated, not just a static echo. */
        finite_peak(sample_rate*4, 2.50001f);
        /* Regression snapshot only, not correctness: same quiet dry frames, no limiting. */
        prepare(amount, 0, 0, 1);
        noise(sample_rate*3, 0.02f);
        memcpy(original, signal, sample_rate*6*sizeof(float));
        music_reverb_processor(signal, sample_rate*3);
        double dry_power = 0, wet_power = 0;
        for (int i = sample_rate; i < sample_rate*3; i++) {
            double dry = original[2*i], wet = signal[2*i] - dry;
            dry_power += dry*dry; wet_power += wet*wet;
        }
        double ratio = sqrt(wet_power/dry_power);
        printf("chorus_wet_level_regression_snapshot %d Hz %.2f: wet/dry %.6f, normalized %.6f (bounds 1.00..1.50)\n",
               sample_rate, amount, ratio, ratio/amount);
        assert(ratio > 1.00*amount && ratio < 1.50*amount); /* Prior snapshot bounds x2, not re-fitted. */
    }
}

static void space_dimensions(void) {
    int previous_onset = 0;
    double previous_late = 0, previous_decay = 0;
    for (int step = 0; step < 3; step++) {
        float room = step == 0 ? 0.25f : step == 1 ? 0.5f : 1;
        prepare(0, 0, room, 1);
        memset(signal, 0, sample_rate*12*sizeof(float));
        signal[0] = signal[1] = 0.03f;
        music_reverb_processor(signal, sample_rate*6);
        int onset = 0;
        for (int i = 1; i < sample_rate; i++)
            if (fabsf(signal[2*i]) + fabsf(signal[2*i+1]) > 1e-7f) { onset = i; break; }
        double early = energy(sample_rate/5, sample_rate, 0) + energy(sample_rate/5, sample_rate, 1);
        double late = energy(sample_rate*4, sample_rate*6, 0) + energy(sample_rate*4, sample_rate*6, 1);
        double relative = late/fmax(early, 1e-30);
        /* Reverse-integrated wet impulse energy: extrapolate -30 dB to T60.
           Exclude the original dry impulse; decay is not just wet loudness. */
        double total = 0, remaining, decay = 0;
        for (int i = 1; i < sample_rate*6; i++)
            total += signal[2*i]*signal[2*i] + signal[2*i+1]*signal[2*i+1];
        remaining = total;
        for (int i = 1; i < sample_rate*6; i++) {
            remaining -= signal[2*i]*signal[2*i] + signal[2*i+1]*signal[2*i+1];
            if (remaining <= total*0.001) { decay = 2.0*(i-onset)/sample_rate; break; }
        }
        printf("%d Hz room %.2f: reflection %.2f ms, estimated T60 %.2f s, late/early %.9g\n",
               sample_rate, room, 1000.0*onset/sample_rate, decay, relative);
        assert(onset > previous_onset && relative > previous_late && decay > previous_decay);
        if (step == 2) assert(decay > 1.0);
        previous_onset = onset; previous_late = relative; previous_decay = decay;
        finite_peak(sample_rate*6, 2.50001f);
    }
    /* Full-band room bass must not let one tank resonance pull the image. */
    prepare(0, 0, 1, 1);
    tone(sample_rate*4, 40, 0.02f, 1);
    music_reverb_processor(signal, sample_rate*4);
    double balance = 10*log10(energy(sample_rate*2, sample_rate*4, 0)
                              / energy(sample_rate*2, sample_rate*4, 1));
    printf("%d Hz full room, 40 Hz mono: L/R %+.3f dB\n", sample_rate, balance);
    assert(fabs(balance) < 0.5);
    /* Loud boosted bass needs internal feedback headroom, not clipped tank states. */
    prepare(0, 1, 1, 1);
    tone(sample_rate*4, 40, 0.6f, 1);
    music_reverb_processor(signal, sample_rate*4);
    float state_peak = 0;
    for (int c = 0; c < REVERB_COMB_COUNT; c++) for (int i = 0; i < REVERB_COMB_MAX; i++)
        state_peak = fmaxf(state_peak, fmaxf(fabsf(s_comb_buf_l[c][i]), fabsf(s_comb_buf_r[c][i])));
    printf("%d Hz loud bass/full room: feedback-state peak %.6f\n", sample_rate, state_peak);
    assert(state_peak < 0.79f); /* Safety clamp is 0.80; normal operation must not hit it. */
    finite_peak(sample_rate*4, 2.50001f);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    /* Hot stereo input stays bit-identical with zero latency when effects are off. */
    for (int off = 0; off < 2; off++) {
        prepare(off ? 0 : 1, off ? 0 : 1, off ? 0 : 1, off ? 1 : 0);
        tone(sample_rate*2, 997, 0.95f, -0.4f);
        memcpy(original, signal, sample_rate*4*sizeof(float));
        music_reverb_processor(signal, sample_rate*2);
        assert(memcmp(original, signal, sample_rate*4*sizeof(float)) == 0);
    }

    for (int sr = 0; sr < 3; sr++) {
        sample_rate = sr == 0 ? 44100 : sr == 1 ? 48000 : 96000;
        chorus_return_is_doubled();
        chorus_routing();
        chorus_behaviour();
        space_dimensions();
        float off = bass_gain(0, 0.01f, 20);
        float quiet = bass_gain(1, 0.01f, 20), half = bass_gain(0.5f, 0.01f, 20);
        float loud = bass_gain(1, 0.8f, 20), high = bass_gain(1, 0.01f, 8000);
        printf("%d Hz bass: quiet %+.3f dB, half %+.3f dB, loud %+.3f dB, 8kHz %+.3f dB\n", sample_rate, quiet, half, loud, high);
        assert(fabsf(off) < 1e-5f);
        assert(fabsf(quiet - 20) < 0.15f && fabsf(half - 10) < 0.15f && fabsf(high) < 0.06f);
        assert(loud > 0.5f && loud < quiet - 2); /* Bass is louder, but added bass is controlled. */
        /* A preceding loud kick must not hold down the rest of the music. */
        prepare(0, 1, 0, 1);
        for (int i = 0; i < sample_rate; i++) {
            float hz = i < sample_rate/2 ? 40 : 8000;
            float level = i < sample_rate/2 ? 0.6f : 0.05f;
            signal[2*i] = signal[2*i+1] = level*sinf(6.28318530718f*hz*i/sample_rate);
        }
        music_reverb_processor(signal, sample_rate);
        double real = 0, imag = 0;
        int begin = sample_rate/2 + sample_rate/4, count = sample_rate/50;
        for (int i = begin; i < begin+count; i++) {
            double phase = 6.283185307179586 * 8000 * i / sample_rate;
            real += signal[2*i]*cos(phase); imag += signal[2*i]*sin(phase);
        }
        double vocal_gain = 20*log10(2*sqrt(real*real+imag*imag)/(count*0.05));
        printf("%d Hz post-kick high-band gain %+.3f dB\n", sample_rate, vocal_gain);
        assert(vocal_gain > -0.2 && vocal_gain < 0.2);
        double low = bass_tail(0.8f), late = bass_tail(0.9f), full = bass_tail(1);
        printf("%d Hz reverb 40Hz tail: 80%% %.9f, 90%% %.9f, 100%% %.9f (%+.1f dB vs 80%%)\n", sample_rate, low, late, full, 10*log10(full/low));
        assert(late > low*2 && full > late*2 && full > low*30);
    }

    sample_rate = 44100;
    /* Slider steps are ramped, not sudden filter/gain jumps. */
    prepare(0, 0, 0, 1);
    for (int i = 0; i < sample_rate*2; i++) signal[i] = 0.05f;
    music_reverb_processor(signal, sample_rate);
    audio_set_effects(1, 1, 1);
    for (int i = 0; i < sample_rate*2; i++) signal[i] = 0.05f;
    music_reverb_processor(signal, sample_rate);
    assert(fabsf(signal[0] - 0.05f) < 0.001f && fabsf(signal[1] - 0.05f) < 0.001f);
    finite_peak(sample_rate, 2.50001f);

    prepare(1, 1, 1, 1);
    noise(sample_rate*2, 1);
    music_reverb_processor(signal, sample_rate*2);
    finite_peak(sample_rate*2, 2.50001f);
    audio_set_reverb(0);
    memset(signal, 0, sample_rate*4*sizeof(float));
    music_reverb_processor(signal, sample_rate*2);
    tone(sample_rate, 1000, 0.1f, 1);
    memcpy(original, signal, sample_rate*2*sizeof(float));
    music_reverb_processor(signal, sample_rate);
    assert(memcmp(original, signal, sample_rate*2*sizeof(float)) == 0); /* Action fades out. */

    /* The callback must be independent of audio device chunk sizes. */
    prepare(0.7f, 0.4f, 0.9f, 1);
    noise(sample_rate, 0.02f);
    music_reverb_processor(signal, sample_rate);
    memcpy(original, signal, sample_rate*2*sizeof(float));
    prepare(0.7f, 0.4f, 0.9f, 1);
    noise(sample_rate, 0.02f);
    for (int i = 0; i < sample_rate;) {
        int count = (i % 503) + 1;
        if (i + count > sample_rate) count = sample_rate - i;
        music_reverb_processor(signal + 2*i, count); i += count;
    }
    for (int i = 0; i < sample_rate*2; i++) assert(fabsf(signal[i] - original[i]) < 1e-6f);
    /* Music transitions reset the tank on the audio thread, not from the UI. */
    AudioAssets a = {0};
    audio_play_menu_music(&a);
    audio_start_match_music(&a, 1);
    memset(signal, 0, sample_rate*2*sizeof(float));
    music_reverb_processor(signal, sample_rate);
    for (int i = 0; i < sample_rate*2; i++) assert(signal[i] == 0);
    /* Audition uses real match streams / the same callback, never menu music DSP. */
    a.game_track_count = 2;
    a.has_music_game[0] = a.has_music_game[1] = 1;
    a.current_music = 1;
    int plays = raylib_stub_music_play_count;
    audio_preview_effect(&a, 1);
    assert(a.current_music == 2 && a.current_game_track == 0);
    assert(raylib_stub_music_play_count == plays + 1);
    assert(__atomic_load_n(&s_reverb_target, __ATOMIC_RELAXED) == 1000000u);
    audio_preview_effect(&a, 0);
    assert(a.current_game_track == 0 && raylib_stub_music_play_count == plays + 1);
    assert(__atomic_load_n(&s_reverb_target, __ATOMIC_RELAXED) == 0);
    audio_set_effects(1, 0, 0);
    audio_preview_effect(&a, 1);
    memset(signal, 0, sample_rate*2*sizeof(float));
    music_reverb_processor(signal, sample_rate);
    noise(sample_rate*2, 0.02f);
    memcpy(original, signal, sample_rate*4*sizeof(float));
    music_reverb_processor(signal, sample_rate*2);
    assert(memcmp(original, signal, sample_rate*4*sizeof(float)) != 0); /* Actual wet samples. */
    audio_preview_effect(&a, 0);
    memset(signal, 0, sample_rate*4*sizeof(float));
    music_reverb_processor(signal, sample_rate*2);
    noise(sample_rate, 0.02f);
    memcpy(original, signal, sample_rate*2*sizeof(float));
    music_reverb_processor(signal, sample_rate);
    assert(memcmp(original, signal, sample_rate*2*sizeof(float)) == 0); /* Same DSP now dry. */
    raylib_stub_music_playing = false;
    audio_preview_effect(&a, 0);
    assert(a.current_game_track == 1); /* Continue when the test track ends. */
    a.game_track_count = 0;
    audio_preview_effect(&a, 1);
    assert(a.current_music == 1 && __atomic_load_n(&s_reverb_target, __ATOMIC_RELAXED) == 0);
    audio_set_effects(NAN, -1, INFINITY); /* Public setters must reject bad values. */
    assert(__atomic_load_n(&s_effect_settings, __ATOMIC_RELAXED) == (1000u << 20));
    puts("All action-audio numerical checks passed.");
    return 0;
}
