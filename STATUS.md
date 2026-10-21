# Project Status & Architecture

*Read this first when starting a new session. Keep this file under 100 lines and strictly factual.*

## Architecture Map
- `src/cards.c/h`: Static 48-card database, deck shuffle.
- `src/scoring.c/h`: 12 base yaku evaluation, cumulative scoring, KK-ADD-10 rule.
- `src/match.c/h`: Pure headless game state machine (turns, capture resolution, round transitions).
- `src/ai.c/h`: Bot evaluation heuristics for 6 campaign tiers + Deus.
- `src/campaign.c/h`: Campaign progression tracking, boss unlock states, rematch limits.
- `src/storage.c/h`: Binary save data (`save.dat`) read/write/reset, card stats, battle log.
- `src/ui_*.c/h`: Raylib rendering, animations, input handling, screen routing, audio playback (`ui_audio`: real-time All-SFX->Music sidechain bus + 3 independent minimum-phase studio stages on game music: Stage 1 RBJ >500 Hz M/S stereo width only, Stage 2 +550 Hz pre-filtered & 1.2 kHz de-essed stereo reverb, Stage 3 +Pultec-style <36 Hz sub-cut & 76 Hz mono sub/kick boost with slow glue + 550 ms Tide envelope + envelope peak limiter).
- `src/main.c`: Initialization, main game loop, screen state switching.
- `tools/gen_wiki_tables.py`: Parses C structs to verify/update documentation tables.

## Recent Changes
- Replaced the tangled single-tank audio processor with 3 non-interfering studio stages (`src/ui_audio.c`, `tests/test_audio_dsp.c`): Stage 1 widens stereo cleanly via an RBJ >500 Hz M/S high-shelf (zero comb allpasses, zero reverb tail on first hit), Stage 2 adds kick-free Abbey Road stereo reverb (24 dB/oct 550 Hz HPF + 1.2 kHz split de-esser + 8.5 kHz LPF before pre-delay/combs), and Stage 3 adds clean minimum-phase mono sub/kick weight (36 Hz low-shelf sub-mud cut + 76 Hz punch bell + 35 ms slow-attack glue + stereo-linked envelope limiter).
- Retained zero-latency live All-SFX->Music sidechain ducking (`sfx_tap_0..6` -> `s_sc_bus[]`), bit-exact Stage 0 bypass, and the 550 ms rise / -20% shoulder / 3.4 s tail Tide curve.
- Added combo guide card preview modal in `src/ui_screens.c`: clicking any card tile in the yaku reference opens a full-size card preview with name, type, month, and matching combos.

## Known Technical Debt / Active Bugs
- Large source files (`ui_screens.c`, `ui_board.c`) mix layout calculations and rendering logic; handle edits carefully without full-file rewrites.
- `DEUS_UNLOCKED_FOR_TESTING` macro in `campaign.h` is currently enabled (`1`).

## Roadmap / Next Steps
- Playtest campaign pacing and bot difficulty curves across all 6 stages.
- Prepare final release build packaging by disabling debug unlock macros when testing concludes.
