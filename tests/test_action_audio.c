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

static void prepare(float width, float bass, float room, float action) {
    reset_music_dsp();
    audio_set_effects(width, bass, room);
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
    finite_peak(2*sample_rate, 0.98001f);
    return 10*log10(energy(sample_rate, 2*sample_rate, 0) / (level*level*0.5));
}

static double bass_tail(float room) {
    prepare(0, 0, room, 1);
    tone(sample_rate, 40, 0.02f, 1);
    memset(signal + sample_rate*2, 0, sample_rate*4*sizeof(float));
    music_reverb_processor(signal, sample_rate*3);
    return energy(sample_rate + sample_rate/10, sample_rate*2, 0) + energy(sample_rate + sample_rate/10, sample_rate*2, 1);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    /* Hot stereo input stays bit-identical AFTER the fixed 64-sample alignment, even off. */
    for (int off = 0; off < 2; off++) {
        prepare(off ? 0 : 1, off ? 0 : 1, off ? 0 : 1, off ? 1 : 0);
        tone(sample_rate*2, 997, 0.95f, -0.4f);
        memcpy(original, signal, sample_rate*4*sizeof(float));
        music_reverb_processor(signal, sample_rate*2);
        assert(memcmp(original, signal + 2*WIDTH_DELAY, (sample_rate*2 - WIDTH_DELAY)*2*sizeof(float)) == 0);
    }

    for (int sr = 0; sr < 3; sr++) {
        sample_rate = sr == 0 ? 44100 : sr == 1 ? 48000 : 96000;
        /* Width must not borrow echoes from the tank, at any slider position. */
        for (int step = 1; step <= 4; step++) {
            prepare(step * 0.25f, 0, 0, 1);
            memset(signal, 0, sample_rate*2*sizeof(float));
            signal[0] = signal[1] = 0.1f;
            music_reverb_processor(signal, sample_rate);
            float echo = 0;
            for (int i = sample_rate/200; i < sample_rate; i++)
                echo = fmaxf(echo, fmaxf(fabsf(signal[2*i]), fabsf(signal[2*i+1])));
            printf("%d Hz width %.2f: impulse tail after 5ms %.9g\n", sample_rate, step*0.25f, echo);
            assert(echo < 1e-6f); /* Old width path emitted ~40-120ms slapback/tails. */
        }
        /* A mono source must widen into BOTH ears, with its mono centre preserved. */
        prepare(1, 0, 0, 1);
        noise(sample_rate*3, 0.02f);
        memcpy(original, signal, sample_rate*6*sizeof(float));
        music_reverb_processor(signal, sample_rate*3);
        double ll = 0, rr = 0, lr = 0, ss = 0;
        for (int i = sample_rate; i < sample_rate*3; i++) {
            double l = signal[2*i], r = signal[2*i + 1], side = (l-r)*0.5;
            assert(fabs((l+r)*0.5 - original[2*(i - WIDTH_DELAY)]) < 2e-7);
            ll += l*l; rr += r*r; lr += l*r; ss += side*side;
        }
        double balance = 10*log10(ll/rr), correlation = lr/sqrt(ll*rr);
        printf("%d Hz width: L/R %+.3f dB, correlation %.3f, Side/Mid %.3f\n", sample_rate, balance, correlation, sqrt(ss/(ll + rr - 2*ss)*2));
        assert(fabs(balance) < 0.5 && correlation > -0.03 && correlation < 0.05 && ss > 0.20*(ll+rr));
        /* Bass-heavy, music-like mono: the upper range must actually spread it,
           not merely pass the noise test with near-centred lows. */
        double previous_spread = 0;
        for (int step = 0; step < 3; step++) {
            float setting = step == 0 ? 0.25f : step == 1 ? 0.5f : 1;
            prepare(setting, 0, 0, 1);
            for (int i = 0; i < sample_rate*3; i++) {
                double t = 6.28318530718 * i / sample_rate;
                float envelope = 0.65f + 0.35f*cos(2*t);
                float x = envelope * (0.08f*sin(100*t) + 0.014f*sin(300*t)
                                      + 0.008f*sin(2000*t) + 0.006f*sin(5000*t));
                signal[2*i] = signal[2*i+1] = x;
            }
            memcpy(original, signal, sample_rate*6*sizeof(float));
            music_reverb_processor(signal, sample_rate*3);
            double mid_power = 0, side_power = 0;
            for (int i = sample_rate; i < sample_rate*3; i++) {
                double mid = 0.5*(signal[2*i] + signal[2*i+1]);
                double side = 0.5*(signal[2*i] - signal[2*i+1]);
                mid_power += mid*mid; side_power += side*side;
                assert(fabs(mid - original[2*(i-WIDTH_DELAY)]) < 2e-7);
            }
            double spread = sqrt(side_power/mid_power);
            printf("%d Hz bass-heavy width %.2f: Side/Mid %.3f\n", sample_rate, setting, spread);
            assert(spread > previous_spread*1.5);
            if (step == 2) assert(spread > 0.85 && spread < 1.05);
            previous_spread = spread;
            finite_peak(sample_rate*3, 0.98001f);
        }
        /* Single-frequency low/mid/high material cannot vanish or lean into one ear. */
        float freqs[] = {40, 1000, 8000};
        for (int j = 0; j < 3; j++) {
            prepare(1, 0, 0, 1);
            tone(sample_rate*3, freqs[j], 0.01f, 1);
            memcpy(original, signal, sample_rate*6*sizeof(float));
            music_reverb_processor(signal, sample_rate*3);
            double side_energy = 0;
            for (int i = sample_rate; i < sample_rate*3; i++) {
                float d = (signal[2*i] - signal[2*i + 1])*0.5f;
                side_energy += d*d;
                assert(fabsf((signal[2*i] + signal[2*i + 1])*0.5f - original[2*(i - WIDTH_DELAY)]) < 2e-7f);
            }
            balance = 10*log10(energy(sample_rate, sample_rate*3, 0)/energy(sample_rate, sample_rate*3, 1));
            printf("  mono %.0f Hz: balance %+.3f dB, side RMS %.6f\n", freqs[j], balance, sqrt(side_energy/(sample_rate*2)));
            assert(fabs(balance) < 0.5 && side_energy > 1e-9);
        }
        /* Overall balance and isolated tones missed spectral panning: test ALL
           centred tones at once, at several widths, then measure each ear/bin. */
        float mixed_hz[] = {100, 300, 1000, 3000, 8000, 15000};
        for (int step = 1; step <= 3; step++) {
            float setting = step == 3 ? 1.0f : step*0.25f;
            prepare(setting, 0, 0, 1);
            for (int i = 0; i < sample_rate*3; i++) {
                float x = 0;
                for (int j = 0; j < 6; j++) x += 0.01f*sinf(6.28318530718f*mixed_hz[j]*i/sample_rate);
                signal[2*i] = signal[2*i+1] = x;
            }
            music_reverb_processor(signal, sample_rate*3);
            for (int window = 0; window < 2; window++) {
                /* Measure from the aligned centre; allow <0.2 dB at FIR onset,
                   retaining the <0.1 dB settled per-frequency balance limit. */
                int begin = window == 0 ? WIDTH_DELAY : sample_rate;
                int end = window == 0 ? WIDTH_DELAY + sample_rate/10 : sample_rate*3;
                double worst = 0;
                for (int j = 0; j < 6; j++) {
                    double re[2] = {0}, im[2] = {0};
                    for (int i = begin; i < end; i++) {
                        double phase = 6.283185307179586*mixed_hz[j]*i/sample_rate;
                        for (int c = 0; c < 2; c++) {
                            re[c] += signal[2*i+c]*cos(phase);
                            im[c] += signal[2*i+c]*sin(phase);
                        }
                    }
                    double balance = 10*log10((re[0]*re[0]+im[0]*im[0])/(re[1]*re[1]+im[1]*im[1]));
                    worst = fmax(worst, fabs(balance));
                    if (fabs(balance) >= (window == 0 ? 0.2 : 0.1))
                        printf("Spectral balance: %d Hz, width %.2f, window %d, %.0f Hz: %+.4f dB\n",
                               sample_rate, setting, window, mixed_hz[j], balance);
                    assert(fabs(balance) < (window == 0 ? 0.2 : 0.1));
                }
                printf("%d Hz mixed tones, width %.2f, %s: worst spectral L/R %.4f dB\n",
                       sample_rate, setting, window == 0 ? "first 100ms" : "settled", worst);
            }
        }
        float quiet = bass_gain(1, 0.01f, 20), half = bass_gain(0.5f, 0.01f, 20);
        float loud = bass_gain(1, 0.8f, 20), high = bass_gain(1, 0.01f, 8000);
        printf("%d Hz bass: quiet %+.3f dB, half %+.3f dB, loud %+.3f dB, 8kHz %+.3f dB\n", sample_rate, quiet, half, loud, high);
        assert(fabsf(quiet - 10) < 0.15f && fabsf(half - 5) < 0.15f && fabsf(high) < 0.05f);
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
        int begin = sample_rate/2 + sample_rate/200, count = sample_rate/50;
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
    /* Do not make a source which is ALREADY anti-correlated even less mono-safe. */
    prepare(1, 0, 0, 1);
    noise(sample_rate*3, 0.02f);
    unsigned int rng = 91;
    double before_ll = 0, before_rr = 0, before_lr = 0;
    for (int i = 0; i < sample_rate*3; i++) {
        rng = rng*1664525u + 1013904223u;
        float mid = signal[2*i]*0.5f;
        float side = 0.015f*((rng >> 8)/8388608.0f - 1);
        signal[2*i] = mid+side; signal[2*i+1] = mid-side;
        if (i >= sample_rate) {
            before_ll += signal[2*i]*signal[2*i]; before_rr += signal[2*i+1]*signal[2*i+1];
            before_lr += signal[2*i]*signal[2*i+1];
        }
    }
    music_reverb_processor(signal, sample_rate*3);
    double ll = 0, rr = 0, lr = 0;
    for (int i = sample_rate; i < sample_rate*3; i++) {
        ll += signal[2*i]*signal[2*i]; rr += signal[2*i+1]*signal[2*i+1]; lr += signal[2*i]*signal[2*i+1];
    }
    printf("Already-wide source correlation: %.3f -> %.3f\n", before_lr/sqrt(before_ll*before_rr), lr/sqrt(ll*rr));
    assert(lr/sqrt(ll*rr) >= before_lr/sqrt(before_ll*before_rr) - 0.02);

    /* Slider steps are ramped, not sudden filter/gain jumps. */
    prepare(0, 0, 0, 1);
    for (int i = 0; i < sample_rate*2; i++) signal[i] = 0.05f;
    music_reverb_processor(signal, sample_rate);
    audio_set_effects(1, 1, 1);
    for (int i = 0; i < sample_rate*2; i++) signal[i] = 0.05f;
    music_reverb_processor(signal, sample_rate);
    assert(fabsf(signal[2*WIDTH_DELAY] - 0.05f) < 0.001f && fabsf(signal[2*WIDTH_DELAY + 1] - 0.05f) < 0.001f);
    finite_peak(sample_rate, 0.98001f);

    prepare(1, 1, 1, 1);
    noise(sample_rate*2, 1);
    music_reverb_processor(signal, sample_rate*2);
    finite_peak(sample_rate*2, 0.98001f);
    audio_set_reverb(0);
    memset(signal, 0, sample_rate*4*sizeof(float));
    music_reverb_processor(signal, sample_rate*2);
    tone(sample_rate, 1000, 0.1f, 1);
    memcpy(original, signal, sample_rate*2*sizeof(float));
    music_reverb_processor(signal, sample_rate);
    assert(memcmp(original, signal + 2*WIDTH_DELAY, (sample_rate - WIDTH_DELAY)*2*sizeof(float)) == 0); /* Action fades out. */

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
    assert(memcmp(original, signal + 2*WIDTH_DELAY, (sample_rate - WIDTH_DELAY)*2*sizeof(float)) == 0); /* Same DSP now dry. */
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
