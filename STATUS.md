# Status

**Branch:** `arena/a85c3416-xeno-koi`
**Head:** Studio bus routing, live all-SFX sidechain ducking, and M/S staged capture DSP on `src/ui_audio.{h,c}` + `src/ui_board.c`.

## Audio Architecture Summary

- **Live SFX -> Music Sidechain Bus (`src/ui_audio.c`)**:
  - All 7 SFX streams (`flip`, `drop`, `capture`, `koi`, `win`, `lose`, `click`) load before `music_menu` and `music_game[0..9]` so Raylib's `OnSendAudioDataToDevice` processes all active SFX before music in every device callback with zero block latency.
  - Non-destructive `sfx_tap_0..6` stream taps feed a real-time sidechain buffer (`s_sc_bus`) scaled by active SFX fader gain; `menu_music_processor` and `music_reverb_processor` duck music automatically (`3 ms` attack, `85 ms` release) for every playing SFX and release cleanly back to `0.00 dB` when the SFX waveform ends.
  - Removed the manual `audio_duck_music(float)` API (which previously only ducked captures and left `s_sfx_duck` latched at `1.0f`).
- **Staged Capture & Koi-Koi Chain (`src/ui_audio.c`, `src/ui_board.c`)**:
  - Driven solely by the player's own captures and Koi-Koi calls (`0` idle -> `1` reverb `40%` / stereo `20%` / bass `20%` -> `2` reverb `50%` / stereo `40%` / bass `20%` -> `3` reverb `60%` / stereo `50%` / bass `40%`).
  - Stage 0 is a bit-exact bypass (`max |out - in| == 0`).
  - Single-send M/S wet architecture: `85 Hz` HP + `9 kHz` LP -> equal `25 ms` L/R pre-delay -> 4 damped combs + 2 allpass diffusers -> M/S split (`Mid`: `<220 Hz` low-band compressor + mono bass boost; `Side`: 4th-order `340 Hz` HP + `7 kHz` insurance de-esser, no post-boost) + band-matched dry ducking + `100 Hz`-HPF master spike compressor/limiter (`AttachAudioMixedProcessor`).
  - Hold timer pauses while cards are still moving (`anim_is_blocking`, `motion_is_reflowing`, `cinematic_is_active`, `hitlag_timer`) and steps cleanly back down to `0` once play settles.
