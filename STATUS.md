# Status

**Branch:** `arena/a85c3416-xeno-koi`
**Head:** Tide-enveloped M/S staging (`Stereo -> Reverb -> Mono Bass`), pre-tank `1.2 kHz` de-esser, `100->40 Hz` low roll-off, `500 Hz` Side HPF, and live all-SFX sidechain bus.

## Audio Architecture Summary

- **Live All-SFX -> Music Sidechain Bus (`src/ui_audio.c`)**:
  - All 7 SFX streams (`flip`, `drop`, `capture`, `koi`, `win`, `lose`, `click`) load before `music_menu` and `music_game[0..9]` so Raylib runs SFX before music every callback with zero block latency.
  - Non-destructive `sfx_tap_0..6` taps feed `s_sc_bus[]`; menu music and the full game music group (`dry + wet`) duck under every playing SFX (`3 ms` attack, `85 ms` release) and release back to `0.00 dB` automatically.
- **Tide Envelope & 3-Stage Chain (`src/ui_audio.c`, `src/ui_board.c`)**:
  - **Stage 0**: bit-exact dry bypass (`max |out - in| == 0`).
  - **Stage 1**: `Stereo 60%`, `Reverb 20%`, `Bass 20%` (wide stereo opens first).
  - **Stage 2**: `Stereo 70%`, `Reverb 50%`, `Bass 20%` (centered reverb blooms second).
  - **Stage 3**: `Stereo 70%`, `Reverb 70%`, `Bass 50%` (mono bass weight drops third).
  - **Tide Envelope**: smooth `550 ms` rise peaking right after the visual thunder flash settles onto its tail, followed by a `20%` post-crest drop (`190 ms`) and a smooth `3.4 s` exponential decay tail.
- **Wet Signal Chain (`src/ui_audio.c`)**:
  - Input: gentle `100 -> 40 Hz` roll-off + `40 Hz` `12 dB/oct` HPF -> pre-reverb split de-esser at `1.2 kHz` (`-18 dBFS`, `3:1`, `2 ms / 60 ms`, zero post-boost) -> `9.5 kHz` LP -> `25 ms` equal L/R pre-delay -> 4 damped combs + 2 allpasses.
  - M/S Return: Mid splits at `180 Hz` with a `35 ms` slow-attack `2:1` glue compressor (`-12 dBFS`) on mono low body; Side runs a mono-safe figure-8 allpass decorrelator + wet stereo through a `24 dB/oct` `500 Hz` Butterworth HPF.
