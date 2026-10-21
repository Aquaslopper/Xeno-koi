/* Minimal raylib audio-surface stub for the headless audio DSP test.
   Only the types and calls used by src/ui_audio.c are declared here, so
   tests/test_audio_dsp.c can compile the real DSP without a raylib install.
   These stubs are never called by the test: only the stream processor is. */
#ifndef RAYLIB_STUB_H
#define RAYLIB_STUB_H

#include <stdbool.h>

static float raylib_stub_last_sound_volume;
static int raylib_stub_play_sound_count;

typedef struct rAudioBuffer rAudioBuffer;
typedef struct rAudioProcessor rAudioProcessor;

typedef struct AudioStream {
    rAudioBuffer *buffer;
    rAudioProcessor *processor;
    unsigned int sampleRate;
    unsigned int sampleSize;
    unsigned int channels;
} AudioStream;

typedef struct Sound {
    AudioStream stream;
    unsigned int frameCount;
} Sound;

typedef struct Music {
    AudioStream stream;
    unsigned int frameCount;
    bool looping;
    int ctxType;
    void *ctxData;
} Music;

typedef void (*AudioCallback)(void *bufferData, unsigned int frames);

static inline const char *GetApplicationDirectory(void) { return ""; }
static inline bool FileExists(const char *fileName) { (void)fileName; return false; }

static inline void InitAudioDevice(void) {}
static inline void CloseAudioDevice(void) {}

static inline Music LoadMusicStream(const char *fileName) { (void)fileName; Music m = {0}; return m; }
static inline void UnloadMusicStream(Music music) { (void)music; }
static inline void PlayMusicStream(Music music) { (void)music; }
static inline bool IsMusicStreamPlaying(Music music) { (void)music; return false; }
static inline void UpdateMusicStream(Music music) { (void)music; }
static inline void StopMusicStream(Music music) { (void)music; }
static inline void SetMusicVolume(Music music, float volume) { (void)music; (void)volume; }
static inline float GetMusicTimeLength(Music music) { (void)music; return 0.0f; }
static inline float GetMusicTimePlayed(Music music) { (void)music; return 0.0f; }

static inline Sound LoadSound(const char *fileName) { (void)fileName; Sound s = {0}; return s; }
static inline void UnloadSound(Sound sound) { (void)sound; }
static inline void PlaySound(Sound sound) { (void)sound; raylib_stub_play_sound_count++; }
static inline void SetSoundVolume(Sound sound, float volume) { (void)sound; raylib_stub_last_sound_volume = volume; }

static inline void AttachAudioStreamProcessor(AudioStream stream, AudioCallback process) { (void)stream; (void)process; }
static inline void DetachAudioStreamProcessor(AudioStream stream, AudioCallback process) { (void)stream; (void)process; }

#endif /* RAYLIB_STUB_H */
