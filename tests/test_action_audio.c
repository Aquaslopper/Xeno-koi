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

/* A separate chorus must process both channels identically, not auto-pan. */
static void chorus_channel_swap(void) {
    for (int step = 1; step <= 3; step++) for (int bass = 0; bass <= 1; bass++) {
        float setting = step == 3 ? 1 : step*0.25f;
        for (int swap = 0; swap < 2; swap++) {
            prepare(setting, bass, 0, 1);
            for (int i = 0; i < sample_rate; i++) {
                double t = 6.283185307179586*i/sample_rate;
                float mid = 0.6f*sin(100*t) + 0.12f*sin(3000*t);
                float side = 0.05f*sin(300*t) + 0.02f*sin(8000*t);
                signal[2*i+swap] = mid+side;
                signal[2*i+1-swap] = mid-side;
            }
            music_reverb_processor(signal, sample_rate);
            if (!swap) memcpy(original, signal, sample_rate*2*sizeof(float));
            else {
                float error = 0;
                for (int i = 0; i < sample_rate; i++) for (int c = 0; c < 2; c++)
                    error = fmaxf(error, fabsf(signal[2*i+c] - original[2*i+1-c]));
                printf("%d Hz chorus %.2f, bass %d: channel-swap error %.9g\n", sample_rate, setting, bass, error);
                assert(error < 2e-7f);
            }
        }
    }
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
            assert(signal[2*i] == signal[2*i+1]);
        }
        printf("%d Hz chorus %.2f: delayed voices %.6f, tail after 40ms %.9g\n", sample_rate, amount, voices, tail);
        assert(voices > 1e-5 && tail == 0); /* No feedback/slapback tail. */
        prepare(amount, 0, 0, 1);
        tone(sample_rate*4, 1000, 0.04f, 0.25f);
        music_reverb_processor(signal, sample_rate*4);
        double motion = 0;
        for (int i = sample_rate; i < sample_rate*2; i++) {
            assert(fabsf(signal[2*i] - 4*signal[2*i+1]) < 2e-7f);
            double d = signal[2*i] - signal[2*(i+2*sample_rate)];
            motion += d*d;
        }
        printf("%d Hz chorus %.2f: modulation RMS %.6f, fixed pan preserved\n", sample_rate, amount, sqrt(motion/sample_rate));
        assert(motion/sample_rate > 1e-8); /* Actually modulated, not just a static echo. */
        finite_peak(sample_rate*4, 0.98001f);
        /* Nonzero output is not enough: the wet layer must have a useful level.
           Quiet noise avoids peak limiting; subtract the exact same dry frames. */
        prepare(amount, 0, 0, 1);
        noise(sample_rate*3, 0.02f);
        memcpy(original, signal, sample_rate*6*sizeof(float));
        music_reverb_processor(signal, sample_rate*3);
        double dry_power = 0, wet_power = 0;
        for (int i = sample_rate; i < sample_rate*3; i++) {
            double dry = original[2*i], wet = signal[2*i] - dry;
            dry_power += dry*dry; wet_power += wet*wet;
            assert(signal[2*i] == signal[2*i+1]);
        }
        double ratio = sqrt(wet_power/dry_power);
        printf("%d Hz chorus %.2f: wet/dry RMS %.3f\n", sample_rate, amount, ratio);
        assert(ratio > 0.50*amount && ratio < 0.75*amount);
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
        finite_peak(sample_rate*6, 0.98001f);
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
    finite_peak(sample_rate*4, 0.98001f);
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
        chorus_channel_swap();
        chorus_behaviour();
        space_dimensions();
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
    /* Slider steps are ramped, not sudden filter/gain jumps. */
    prepare(0, 0, 0, 1);
    for (int i = 0; i < sample_rate*2; i++) signal[i] = 0.05f;
    music_reverb_processor(signal, sample_rate);
    audio_set_effects(1, 1, 1);
    for (int i = 0; i < sample_rate*2; i++) signal[i] = 0.05f;
    music_reverb_processor(signal, sample_rate);
    assert(fabsf(signal[0] - 0.05f) < 0.001f && fabsf(signal[1] - 0.05f) < 0.001f);
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
