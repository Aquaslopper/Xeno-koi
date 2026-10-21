# Project Status & Architecture

*Read this first when starting a new session. Keep this file under 100 lines and strictly factual.*

## Architecture Map
- `src/cards.c/h`: Static 48-card database, deck shuffle.
- `src/scoring.c/h`: 12 base yaku evaluation, cumulative scoring, KK-ADD-10 rule.
- `src/match.c/h`: Pure headless game state machine (turns, capture resolution, round transitions).
- `src/ai.c/h`: Bot evaluation heuristics for 6 campaign tiers + Deus.
- `src/campaign.c/h`: Campaign progression tracking, boss unlock states, rematch limits.
- `src/storage.c/h`: Binary save data (`save.dat`) read/write/reset, card stats, battle log.
- `src/ui_*.c/h`: Raylib rendering, animations, input handling, screen routing, audio playback (`ui_audio`: real-time All-SFX->Music sidechain bus + Tide-enveloped staged music chain [60/20/20 -> 70/50/20 -> 70/70/50] with clean >500 Hz M/S stereo width, 250 Hz pre-filtered & 1.2 kHz de-essed MONO reverb tank, and 40–250 Hz MONO bass shelf/slope/glue + envelope peak limiter).
- `src/main.c`: Initialization, main game loop, screen state switching.
- `tools/gen_wiki_tables.py`: Parses C structs to verify/update documentation tables.

## Recent Changes
- Fixed staged music DSP architecture in `src/ui_audio.c` and `tests/test_audio_dsp.c`: replaced stereo reverb with a calibrated strictly MONO reverb tank (`wet_side == 0`) high-passed at `250 Hz` (`24 dB/oct`) with `1.2 kHz` pre-tank split de-essing (`0` post-boost), aligned mono bass processing to `40–250 Hz` (`250 Hz` low-shelf boost, `100->40 Hz` gentle slope, `<40 Hz` cut, `35 ms` slow glue), kept clean M/S Side expansion strictly above `500 Hz`, and restored `60/20/20 -> 70/50/20 -> 70/70/50` stage scaling.
- Retained zero-latency live All-SFX->Music sidechain ducking (`sfx_tap_0..6` -> `s_sc_bus[]`), bit-exact Stage 0 bypass, and the 550 ms rise / -20% shoulder / 3.4 s tail Tide curve.
- Added combo guide card preview modal in `src/ui_screens.c`: clicking any card tile in the yaku reference opens a full-size card preview with name, type, month, and matching combos.

## Known Technical Debt / Active Bugs
- Large source files (`ui_screens.c`, `ui_board.c`) mix layout calculations and rendering logic; handle edits carefully without full-file rewrites.
- `DEUS_UNLOCKED_FOR_TESTING` macro in `campaign.h` is currently enabled (`1`).

## Roadmap / Next Steps
- Playtest campaign pacing and bot difficulty curves across all 6 stages.
- Prepare final release build packaging by disabling debug unlock macros when testing concludes.
