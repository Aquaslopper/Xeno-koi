> **Development handoff (last updated 23 October 2026).** This records the local source status and verification noted below; it is not a release or deployment record. For current rules, use the code in `src/` and the wiki in `docs/`. The original build spec, `HANDOFF_KoiKoi_Giger_v2.pdf`, is legacy; see `AGENTS.md` and `docs/errata.md`.
# XENO/KOI: BIOMECHANICAL EDITION — Project Status

# XENO/KOI: BIOMECHANICAL EDITION - STATUS REPORT
**Last updated: 23 October 2026.** This is a development handoff for the local source, not a release or deployment record. Code in `src/` defines behavior; the wiki in `docs/` explains the rules. See `AGENTS.md` for contributor guardrails and `docs/errata.md` for known issues.

## Status Summary
## Latest completed work

### KK-ADD-10 gold minipoint counter: glyph glow, serie growth, and ghost-burst flash (23 October 2026)

- Upgraded `speed_bar_draw` and visual chain state in `src/ui_speed.c` (`RULESET_KK_ADD_10` only; scoring rules and `s_total` conversion untouched):
  - **Glyph-shaped golden glow (no circular halo):** additive `DrawTextEx` passes follow the exact digit strokes of the counter, scaling up in intensity and subtle shimmer as an unbroken gold-earning chain (`s_chain` / smoothed `s_chain_vis`, `0..5`) builds across consecutive turns.
  - **Slight serie growth & smooth reset:** font size eases up slightly across an unbroken chain (`22.0 px` base up to `27.5 px` at max chain, plus a `+2.2 px` flash pop) and smoothly shrinks back to `22.0 px` whenever the chain breaks (`speed_bar_consume()`, zero-point late drop, round transition, or match reset).
  - **Golden flash + larger transparent ghost burst behind the number:** every increase in `s_total` triggers a `0.55 s` bright golden flash on the digits plus a `0.42 s` expanding (`1.48x -> 2.10x`), optically centered transparent golden echo of the number directly behind it (with dark glyph contrast cushioning while the speed bar is visible behind the counter).

#### Checked
- `src/ui_speed.c` compiles cleanly under `-std=c99 -O2 -Wall -Wextra -Werror`; `RAYLIB_SRC=/tmp/raylib/src ./tests/test_table_ui.sh`, `RAYLIB_SRC=/tmp/raylib/src ./tests/test_music_folder.sh`, ASan/UBSan headless draw/chain harness, `python3 tools/gen_wiki_tables.py --check`, and `git diff --check` pass.
- **Touched:** `src/ui_speed.c`, `STATUS.md`. **Deliberately did not touch:** `src/rules.c`, `src/engine.c`, `src/cards.c`, `src/ui_audio.c`, `src/ui_main.c`, `src/ui_board.c`, `src/ui_screens.c`, `docs/`, or assets.

### Glowing card-back loading transition and 4 s winner banner (22 October 2026)

- Reworked `start_transition` in `src/ui_main.c` into a smooth three-stage transition while keeping Main Menu subscreen buttons (`OPTION`, `STATS`, `CREDITS`, `SAVE`, `QUIT`, and opening/closing Opponent Select) snappy and unchanged:
  1. **Slow smooth dim to full dark (`TRANS_OUT_DUR = 1.80f` smoothstep):** keeps the outgoing screen intact (including the Campaign opponent overlay on match start, the final Round Result combo screen on `MATCH RESULTS`, and the Pause overlay on abandon — fixing the 1-frame overlay vanish) while slowly fading outgoing BGM down to silence.
  2. **2.20 s mysterious card-back screen (`TRANS_HOLD_DUR = 2.20f`):** on full black, centers a small borderless `assets/back.png` (`64 x 104`) with soft dark-vignetted edges melting into the void, a very faint ambient phosphor whisper behind the card, subtle inner mist, and a focused glow on the `XENO/KOI` emblem at the lower-right corner before subsiding back into pitch black and switching screens/music cleanly.
  3. **Smooth reveal (`TRANS_IN_DUR = 1.20f` smoothstep):** lifts from black into `SCREEN_GAME`, `SCREEN_MATCH_RESULT`, `SCREEN_START`, or `SCREEN_STATS` as incoming BGM swells up (and fires the deferred win/lose SFX as `SCREEN_MATCH_RESULT` reveals).
- **Instant in-game match restart (`begin_new_match`):** clicking the board's `RESTART` button (`1160, 610`), pressing `R`, or confirming `RESTART MATCH` inside Pause now restarts the match immediately on `SCREEN_GAME` without popping up the `GAME PAUSED` overlay and without running the loading-card transition.
- Added a **4.00 s end-of-round / end-of-match winner banner** (`ROUND_BANNER_DUR = 4.00f`, `draw_round_end_banner` in `src/ui_main.c`) accompanied by a **subtle `0.26 s` low-tone sci-fi notification sound** (`init_banner_sfx` / `play_banner_sfx`, `110 Hz -> 147 Hz` stereo telemetry pulse scaled by `audio.sfx_volume`) when `PHASE_ROUND_END` begins: shows the round or final match winner, running/final score, and for `KK-ADD-10` the gold minipoints line (`GOLD MINIPOINTS: <mp> = +<pts> PTS` on match end, or running gold minipoints mid-match). `speed_bar_apply_match_bonus()` folds in once on the final round's rising edge so the match winner and score are exact; after 4 seconds, the full combo/breakdown overlay (`screens_draw_round_result`) eases in over `0.85 s` (`smooth01(overlay_alpha)`).
- Updated `.github/workflows/build.yml` to fetch MinGW packages from `mirrors.edge.kernel.org` (Fastly HTTPS CDN) instead of `azure.archive.ubuntu.com` and added concurrency cancellation for superseded branch runs.

#### Checked
- All 22 `src/*.c` compile cleanly under `-std=c99 -O2 -Wall -Wextra` (`-Werror` on `src/ui_main.c` and all non-replay modules) against Raylib 5.5 headers.
- Engine suite (`500` full simulated matches), `RAYLIB_SRC=/tmp/raylib/src ./tests/test_table_ui.sh`, `RAYLIB_SRC=/tmp/raylib/src ./tests/test_music_folder.sh`, `./tests/test_audio_dsp.sh`, `./tools/audio_guard.sh`, `python3 tools/gen_wiki_tables.py --check`, and `git diff --check` pass.
- Headless 60 Hz frame-stepping harness (`ASan + UBSan`) against `src/ui_main.c` verified instant Options round-trip, instant board `RESTART` with zero pause popup and zero loading transition, zero 1-frame pop on opponent choice and `MATCH RESULTS`, `1.80 s` slow dim -> `2.20 s` subtle card-back loading hold -> `1.20 s` reveal, `4.00 s` KK-ADD-10 winner + gold-points banner + banner notification SFX trigger before the `0.85 s` combo-screen fade-in, deferred win SFX, and smooth return to Main Menu.

#### Needs maintainer verification
- Visually and audibly confirm on live desktop/web hardware: the `1.80 s` slow dim -> `2.20 s` subtle card-back/logo glow -> `1.20 s` reveal feel, instant `RESTART` behavior, and the `4.00 s` winner + KK-ADD-10 gold banner with its subtle low sci-fi notification tone.

**Touched:** `src/ui_main.c`, `.github/workflows/build.yml`, `STATUS.md`. **Deliberately did not touch:** `src/rules.c`, `src/engine.c`, `src/cards.c`, `src/ui_speed.c`, `src/ui_screens.c`, `src/ui_audio.c`, `src/campaign.c`, `src/storage.c`, `src/ai.c`, card/shader/audio assets, `docs/`, or the handoff PDF.

### Current Options and README gameplay captures — completed locally 22 Oct 2026

- Imported the four supplied 1920 x 1080 captures through `tools/make_screenshots.py --from`: `menu1.png` replaces the old Options frame, `menu2.png` adds the current page 2, and `ingame.png` / `ingame2.png` add the two README gameplay views. The importer only flattened/encoded them to q95 WebP; no capture was cropped, repainted, or recoloured.
- The Options wiki now displays both current pages. Captions explicitly call out the pictured saved examples rather than presenting them as defaults: page 1 shows Display Gamma 1.00, and page 2 shows the custom-music controls with Chorus 60%, Bass +20 dB, Space reverb 70%.
- README screenshots now use the repository-local `docs/assets/screenshots/ingame.webp` and `ingame2.webp` instead of external GitHub `user-attachments` URLs. The screenshot inventory and contributor instructions now cover all 11 frames and seven derived close-ups.

Checks on this section: `make_screenshots.py --check` (11 frames), the card/table generated-file checks, `mkdocs build --strict`, local Chromium decoding of both 1920 x 1080 Options images and their captions, README local-image checks, and `git diff --check` pass. No engine or audio code changed, so the previously recorded full-suite result is unchanged.

### Wiki freshness and browser-settings audit — completed locally 22 Oct 2026

- Regenerated the wiki card back, all 48 stale DEUS thumbnails, the DEUS background/atlas, and the dependent difficulty-chart SVG from the shipped source art. No pixel art was hand-edited. The new back is the matte-black texture with the vertical cyan XENO/KOI logo.
- `tools/make_card_images.py` now writes `tools/card_images.sha256`; `--check` rejects missing files, edited outputs, and source/generator drift. Docs CI installs Pillow and runs this check. A deliberate stale-output test was rejected as intended.
- Audited the commits since the previous wiki refresh. Existing coverage was confirmed for custom music, Options page 2, effect audition, chorus/reverb, thunder/ash, and the speed minigame. Added or corrected discoverability and accuracy notes: +20 dB bass (including the current 96 kHz +0.054 dB failure), ash and thunder feedback, the speed round-streak reset, and cross-links for minipoints/serie conversion.
- Fresh settings still default to **Display Gamma 1.00**. `tests/test_music_folder.c` now proves that default and separately proves a saved gamma of 1.35 survives a load. The browser still restores `xenokoi_settings.txt`; no saved-setting reset or migration was added. The old Options screenshot showed 1.40 and a pre-pagination panel, so its wiki reference was removed rather than presenting it as current; the current replacement is recorded in the capture update above.
- **WASM runtime check not completed:** Emscripten is absent locally, and latest main Docs & Web Build run 39723749408 fails before the WASM build. Latest successful main deployment found was run 39625965216 at 1bb9228. The surviving last-deployed artifact also could not be downloaded here because GitHub redirects to a storage host outside the sandbox allowlist. Static source checks and the real-browser wiki render do not substitute for a WASM boot; a fresh-browser-profile check remains required after CI deploys again.

Checks on this section: `gen_wiki_tables.py --check`, `make_card_images.py --check`, all other documentation art checks, `mkdocs build --strict`, local Chromium rendering of seven wiki pages (including every local image), `RAYLIB_SRC=/tmp/raylib/src ./tests/test_music_folder.sh`, `./tests/test_table_ui.sh`, and `git diff --check` pass. Full `RAYLIB_SRC=/tmp/raylib/src ./build.sh test` remains red with exit 134 at the already-recorded 96 kHz bass high-band assertion: measured +0.054 dB versus the unchanged <0.05 dB bound; the 500-match integrity suite and 44.1/48 kHz audio checks pass first.

### KK-ADD-10 speed minigame: 1000 ms timer bar and minipoints (22 October 2026)

- New module `src/ui_speed.c`/`ui_speed.h` (+~150 lines) adds the maintainer's minigame, **KK-ADD-10 only**. A theme-filled timer bar sits bottom-right below RESTART (`{1160, 662, 112, 40}`), arms at the start of each human hand turn when input unlocks, drains over **1000 ms** from left to right, and is gone the moment the hand card is dropped (stock flip never counts; the CPU is never graded; replays neither draw nor score it). Graded captures score `round(10 × s × a)`: `s = clamp(remaining / 500 ms, 0, 1)` (full speed half with half the bar left) and `a = clamp(drop_cover / 0.80, 0, 1)` where `drop_cover` is the dropped hand card's overlap with the target's painted slot (best match counts for all-four grabs and 2-match choices). A perfect catch (10) starts/continues a **serie**: every perfect catch in a serie of ≥2 gets +10, granted retroactively to the serie's first catch too (perfect 8-deal = 80 + 80 = 160); any other resolved hand play (plain drop, imperfect catch, timeout autoplay, cancelled 2-match choice) breaks it. Gold minipoints (`COLOR_AMBER`, fixed 22 px) draw over the bar and persist after it vanishes; new awards flash the bar (0.30 s) and pulse the number (0.60 s). At match end `speed_bar_apply_match_bonus()` adds `total / 10` to `total_score[0]` before `storage_record_match` and the win comparison, once per match. Far drops (card returns to hand) do not consume the bar. Serie resets at each new round; everything resets at dealer select. **Tuned live by the maintainer:** the bar lifetime went 300 ms → **1000 ms** ("triple it and round up") — the speed half simply re-references to 500 ms; no other constant moved.
- Hooks: `resolve_drop()` grades/consumes (`src/ui_input.c`, cover helper computes the intersection without new raylib deps), `src/ui_main.c` calls `speed_bar_update` after `input_unlocked`, applies the match bonus, and consumes on hand-choice cancel; `board_draw_table` draws the widget for `!replay_mode && ruleset == KK-ADD-10` only.
- The match result now shows the conversion in its own gold box below the board panel (`{400, 604, 480, 48}` in `screens_draw_match_result`, `src/ui_screens.c`): `SPEED MINIPOINTS: 260 = +26 PTS`, gently glinting, drawn only for KK-ADD-10 with `speed_bar_total() > 0` (a new accessor in `src/ui_speed.c`). The box is separate from the panel because the 12-round two-column ledger fills that window. The drop grading in `resolve_drop()` is now gated on `RULESET_KK_ADD_10` so other rulesets never accumulate invisible minipoints.

#### Checked
- `./tests/test_table_ui.sh` passes (headless table input/combo suite; now links `src/ui_speed.c`).
- Engine suite alone: `All engine & ruleset tests passed (500 full matches verified).` — `src/rules.c`, `src/engine.c`, and scoring untouched.
- All 22 `src/*.c` compile with `-Wall -Wextra` against real Raylib headers; no new warnings (only the pre-existing `snprintf` truncation warnings in `src/ui_replay.c`).
- `./build.sh test`'s action-audio step **fails at 96 kHz** (`test_action_audio.c:235` loud-bass assert). Verified identical on a clean `HEAD` worktree (`a9ebb9d`) — pre-existing, outside this change's scope; no audio file touched here.
- Full `./build.sh linux` link not runnable in this sandbox (no `libraylib_linux.a` / `libGL`); compile-only pass stands in.

#### Needs maintainer listening
- Play the minigame live: bar placement under RESTART, left-to-right drain direction, the tuned 1000 ms feel, gold number look/pulse, the match-board reveal box, and whether a great drop's worth (up to 10 minipoints → 1 point per 10) feels right over a match. Formula constants are isolated at the top of `src/ui_speed.c`.

**Touched:** `src/ui_speed.c`, `src/ui_speed.h` (new), `src/ui_input.c`, `src/ui_main.c`, `src/ui_board.c`, `tests/test_table_ui.sh`, `docs/rulesets/kk-add-10.md`, `docs/errata.md`, STATUS. **Deliberately did not touch:** `src/rules.c` / `src/engine.c` / `src/cards.c` / scoring logic, CPU AI, replay recording format, the thunder flash and ash wind, shaders, `storage.c` stats schema, generated wiki tables (no engine-derived values changed), the handoff PDF.

### Ambient ash rides a gusting wind (22 October 2026)

- The background ash (`g_ash`, `ash_draw_and_update` in `src/ui_board.c`) no longer flies a straight bottom-to-top line. A global wind now pushes it sideways: three incommensurate sine waves (`0.110 / 0.043 / 0.270 rad/s`, amplitudes `12 / 16 / 4` px/s) giving `wind_x` in `[-32, +32]` px/s that holds one direction for roughly 25-80 s before swinging the other way, plus a weak vertical gust (`+/-3.5 px/s`) that only breathes the rise rate. Each flake rides the wind differently via `wind_catch = 0.55 + 0.45 * sin(phase)` (`0.10..1.00`), so the field flows instead of translating as a sheet. Per-particle drift, flutter sway, wrapping, and respawn are unchanged.

#### Checked
- `./tests/test_table_ui.sh` passes (compiles `src/ui_board.c` against real Raylib headers, headless).
- Wind envelope sampled numerically over 1 h at 60 Hz: range `-31.7..+31.9` px/s (mean `-0.10`), 34 sustained right-blasts and 34 left (avg ~45 s each, max ~78 s); `wind_y` stays within `+/-3.5` px/s so the rise never reverses.

#### Needs maintainer listening
- Visually confirm on a live GPU (desktop and web) that the drift reads as wind: diagonal gusts, occasional sideways holds, no straight-up fall line, and no visible wrap seams at the screen edges.

**Touched:** `src/ui_board.c` (`ash_draw_and_update` only), STATUS. **Deliberately did not touch:** capture-burst ash (`ash_burst_draw_and_update`), taunt ash (`src/ui_taunt.c`), particle shapes/sizes/spawn rates, the thunder flash, shader files, rules/scoring, generated docs or images.

### Thunder-flash envelope: 500 ms sustain then slow roll-off (22 October 2026)

- The capture/Koi-Koi thunder flash no longer spikes and flashes off. `board_draw_table` (`src/ui_board.c`) now holds `flashAmount` at `1.0` for `0.50 s` after the trigger edge (`thunder_hold`), then rolls off exponentially from full (`tau = 0.50 s`, effectively invisible about 2 s into the fade). This replaces the old fast `3.2/s` drop to `0.40` plus `tau = 3.40 s` afterglow. A new capture or Koi-Koi call during the hold or fade snaps back to `1.0` and restarts the 500 ms sustain.

#### Checked
- `./tests/test_table_ui.sh` passes (compiles `src/ui_board.c` against real Raylib headers, headless).

#### Needs maintainer listening
- Visually confirm the new envelope on a live GPU (desktop and web): 500 ms full plateau, then a gentle ~2 s fade; retriggers sustain through capture streaks.

**Touched:** `src/ui_board.c` (one new static + the thunder decay block), STATUS. **Deliberately did not touch:** `assets/depth_fog.fs` / `assets/depth_fog_web.fs`, flash triggers, shader uniforms, `turn_flash_alpha`, the audio reverb curve, rules/scoring, generated docs or images. No shared mode flag.

### Double chorus return only (22 October 2026)

#### Checked numerically
- Test A `1db8e7b` (17 changed lines), fix B `3f65b8d` (2 changed lines: one production line). Literal before: `chorus_return_is_doubled: expected 0.0388091356, actual 0.0194045678` (DSP/full suite/ASan/UBSan exit 134). After: `chorus_return_is_doubled: expected 0.0388091356, actual 0.0388091356` at 44.1/48/96 kHz. All five routing properties still pass.
- Same quiet-noise fixture at 44.1 kHz maximum: Side/input RMS 0.571781→1.143562; peak 0.097416→0.146488; mono error 1.86264515e-09→3.7252903e-09. OFF stays byte-identical. Snapshot band explicitly scales by the requested factor two, 0.50–0.75→1.00–1.50; no fitting. Limiter remains unchanged and is inactive in this quiet fixture.
- B: DSP/full suite and ASan/UBSan end `All action-audio numerical checks passed.` (exit 0); wiki prints `docs are up to date with the engine source.` (exit 0); real-header strict C99 and whitespace exit 0. Final documentation checks: DSP/full suite/ASan/UBSan, wiki, real-header strict C99 and whitespace exit 0; strict MkDocs prints `INFO    -  Documentation built in 0.65 seconds`; stub-only C99 again exits 1 with `src/ui_theme.h:9:5: error: unknown type name 'Color'`. Previously disclosed stub-only graphics-header mismatch remains outside scope; no claim that command passed.
#### Matches the cited source
- Authority is the maintainer's +100% request: 1+100/100=2 linear return amplitude (+6.0206 dB), not twice acoustic power or apparent loudness. No external tuning claim. Source comparison confirms exactly one changed production line: multiply the existing filtered Side by 2; antisymmetric additions remain unchanged. Reconsider only on further maintainer instruction.
#### Needs maintainer listening
- Maintainer confirms the preceding routing works. This follow-up changes its strength only; no new listening outcome is claimed. Active linked limiting can still affect Mid on hot inputs.
**Touched:** one chorus-return multiplier, exact-gain test, explicitly scaled regression snapshot, STATUS/DSP reference. **Deliberately did not touch:** LFO/depth/rate/taps, filter coefficients, room, bass, limiter, other routing, UI/settings, SFX parameter, dependencies, gameplay/assets or existing history. No new shared mode flag.

### Routing-only chorus Side encode (22 October 2026)

- Supersedes the earlier chorus routing/strength claims below. A `74612cf` is tests only (70 changed lines); B `002a632` is the routing fix only (23). C is this documentation report (60 changed lines; its SHA will be linked in PR #79). No history rewrite.

#### Checked numerically

- Fixture: seed-17 uniform ±0.05 noise, 3 s, last 2 s measured after 1 s silent warmup; bass/room zero, action one. Stereo R is a 1 s cyclic shift of L. Measurements were reproduced on base and scratch fix before A, then byte-identical scratch/repository probe logs after B. Supplied seed-unspecified measurements were not substituted for observations.
- Literal base output: `44100 Hz mono 1.00: mono error 0.049168447, Side/input 0.000000, correlation 1.000000, L/R +0.000000 dB`. Base mono gain was +1.282836 dB. A: DSP, full suite and sanitized runs exit 134; `chorus_keeps_mono_sum: FAIL (8 failures)`, `chorus_creates_side: FAIL (4 failures)`, `chorus_decorrelates: FAIL (1 failures)`. Balance and OFF already pass on base; five failures cannot honestly be claimed.
- After B at 44.1 kHz, amounts 0/.25/.5/.75/1: Side/input RMS 0/0.143194/0.285808/0.428514/0.571781; mono max error 0/9.31322575e-10/1.86264515e-09/1.86264515e-09/1.86264515e-09; peaks 0.050000/0.062245/0.073414/0.084606/0.097416; mono level change rounds to 0.000000 dB throughout. Full table is in the DSP reference and B's commit message.
- At maximum, mono correlation is 0.507235; L/R balance −0.012432 dB. Stereo Side changes +0.347460/+1.264682 dB at .5/1; stereo mono error ≤3.7252903e-09. Across 44.1/48/96 kHz, all five named properties report `PASS (0 failures)`; worst mono-noise balance magnitude is 0.040105 dB. The strength criterion is normalized Side/input RMS, not an absolute signal level.
- `chorus_wet_level_regression_snapshot` now prints its measured value and unchanged normalized bounds 0.50..0.75. Maximum at 44.1 kHz is 0.571781 (previous routing approximately 0.580); normalized values across all tested rates/amounts span 0.570587–0.573206. This detects change, not correctness; no bounds were re-fitted. Removed the four mono-identical/fixed-pan checks explicitly; replaced exact-zero impulse output tail with zero delay-history feedback because the added HP has IIR decay.
- B check outputs: DSP and ASan/UBSan end `All action-audio numerical checks passed.`; full suite ends the same. All exit 0. Wiki tables, whitespace and strict C99 with real Raylib headers exit 0 (no output). The mandated stub-only all-source C99 command exits 1 on unchanged graphics declarations: `src/ui_theme.h:9:5: error: unknown type name 'Color'`; it is not a passing check. Final C checks: `./build.sh test`: `All engine & ruleset tests passed (500 full matches verified).`; DSP/ASan/UBSan: `All action-audio numerical checks passed.`; table UI: `All table input, campaign unlock, and combo panel tests passed.`; wiki, real-header C99 and whitespace exit 0; strict MkDocs: `INFO    -  Documentation built in 0.64 seconds`; stub-only C99 exit 1 with the same error.

#### Matches the cited source

- Opened the actual PDFs: Dattorro (1997), §1.3.7, “we use a quadrature oscillator to decrease the correlation”; Das (DAFx24), §1, “low frequencies are spread less than mid and high frequencies to maintain a strong phantom centre”; Kermit-Canfield/Abel (DAFx16), §2/§4.4, distinguish delay/comb coloration from their allpass cascade. These support the routing rationale, not the unchanged depth/rate/cutoff values or a quality verdict. Full links and attribution limits are in the DSP reference.
- Algebra: channel means form va/vb; generated S = amount × HP((va−vb)/2); outputs L+S, R−S preserve the pre-limiter Mid. Each 0.5 is a mean/half-difference, not a calibration. No normalization was added. The existing limiter can still change Mid when engaged; the quiet fixtures remain below its threshold. Noise balance is not a guarantee for every signal.

#### Needs maintainer listening

- Asked in PR #79: retain 264-sample depth/nominal 0.60 Hz? Accept measured maximum peak growth 0.050000→0.097416 without pre-scaling? Retain nominal 300 Hz HP or later request 150–200 Hz? No tuning while unanswered. Device-rate invariance and product character remain unconfirmed. `modes.md`/`errata.md` still describe the preceding routing; their correction needs authorization beyond C's specified two files. No review request.

**Touched:** chorus return plus one local HP state/reset; focused tests; this report and `docs/reference/audio-dsp.md`. No new shared function parameter or mode flag.

**Deliberately did not touch:** every existing DSP constant, LFO/depth/rate/taps, room, bass, limiter, UI/settings/routing outside the chorus return, existing NULL-only SFX parameter, dependencies, cards/rules/scoring/campaign/assets/PDF, or prior commits. Earlier limiter/reverb-rate/SFX packages remain out of scope. Outside the HP state/reset, chorus comment and return block, production source compares byte-identical to `39876c3`; every other production file is unchanged.

### Stronger, verified chorus range (22 October 2026)

- Maintainer reports no audible chorus, with or without other effects; clarified that Space reverb is audible with Test Effect ON. Inspected UI/startup binding, atomic controls and preview routing. The initial chorus had a quiet averaged return and slow/shallow modulation, and existing checks accepted a merely nonzero effect rather than a useful signal level.
- Changed only three chorus parameters: maximum parallel return 0.35 → 1.0 of the two-voice average, depth 3 → 6 ms, nominal rate 0.30 → 0.60 Hz. Direct signal, identical-channel processing, no-feedback routing and shared peak protection retained. Bass, space reverb, UI/settings/defaults/action/preview routing and all gameplay/assets/dependencies untouched.
- Added a real-callback quiet-noise wet/dry RMS acceptance check at 25/50/75/100% and 44.1/48/96 kHz. It fails the prior code; maximum now measures 0.579–0.580 with required bounds 0.50–0.75. Existing actual modulation, fixed-pan/channel-exchange, 40 ms tail, bypass, smoothing and chunk tests remain.
- Actual first-30-second bundled MP3 comparisons at identical quiet input levels and with no active limiter isolate processed-minus-dry chorus. Maximum layer: game1 −12.32 → −2.96 dB relative to dry; game3 −13.06 → −2.60 dB. Zero chorus has exactly zero difference. This quantifies a stronger modulated layer rather than inferring strength from overall volume. Full-level all-max peaks 0.96687/0.97471, L/R balance −0.370/−0.180 dB; low-level mono-fold commutation error <2e-9. Bass-only gains remain unchanged.
- Full engine/audio/settings/UI suite, ASan/UBSan DSP tests, strict C99, generated tables, strict docs and whitespace checks passed. Touched chorus tuning, focused test and docs only. Live listening on the maintainer's build remains unverified; these measurements are not a guarantee of perceived quality. Previous entry's chorus parameters/measurements are superseded.


### Chorus replaces widening; scalable space reverb (22 October 2026)

- Maintainer explicitly abandoned stereo widening and requested chorus plus a room-to-large-space reverb. Removed every width/covariance/power-budget state and the 64-sample alignment buffer. The interrupted, uncommitted fixed-width experiment/test is not shipped. All-off bypass is bit-identical at zero latency again.
- Added a separate feed-forward two-voice chorus **after** bass/reverb and before the shared peak limiter: nominal 15/23 ms delays, up to 3 ms modulation at 0.30 Hz, parallel return up to 35% with direct audio retained. Identical tap modulation and fixed gains in both channels avoid an auto-panning mechanism; chorus intentionally has cyclic pitch/phase movement. No chorus feedback or feed into the reverb tank. Fixed input/return staging reserves tank feedback headroom; a loud-bass state-headroom regression and instrumented shipped-music checks guard against internal clipping. Mono chorus fold-down matches processing the original mono fold-down, not the unprocessed original.
- Rebuilt the existing algorithmic tank as eight damped combs/four true all-pass diffusers per channel, with bounded interpolated taps. Room increases delay scale 0.65–2.2, predelay ~5–50 ms, feedback 0.55–0.93 and diffusion. Dimensions follow the room slider, not repeated action pulses. Deep reverb bass opens in Mid over the final 20%; Side stays high-passed so bass resonances do not pull the image. Shared wet ducking, capture/Koi-Koi envelope and bass boost/compression are retained. This is a room/hall algorithm, not convolution/shimmer or a physical acoustic simulation.
- Replaced the page-2 Stereo wide row with Chorus, relabelled Space reverb and updated its hint. Saved key is now `effect_chorus`, default off; legacy `effect_stereo` is ignored rather than enabling an unrelated effect. Bass defaults/format and reverb value are retained. Updated startup, UI, settings tests and docs; no new library or shared mode parameter.
- Real-callback impulse tests: at 25/50/100% room, first-reflection spacing and normalized decay increase. Broadband T60 estimates from reverse-integrated −30 dB energy are ~0.61/1.12/5.95 s at 44.1 kHz, 0.56/1.03/5.46 s at 48 kHz and 0.28/0.52/2.73 s at 96 kHz. This explicitly retains nominal 44.1 kHz sample-domain tuning; Raylib's public API still exposes no device-rate getter. Full-room 40 Hz mono balance stays within 0.35 dB. Existing presence threshold −12.5 dB unchanged; measured −11.1 dB.
- Chorus tests verify actual modulation, identical-channel/4:1-pan preservation, exact channel-exchange symmetry, no impulse tail after 40 ms, zero-latency bypass, chunking, transitions and audition output/routing. Existing bass endpoints/loud-bass/post-kick tests remain; new settings persistence/legacy-key and UI input checks pass. Full engine/audio/settings/UI suite, table UI, ASan/UBSan DSP/settings/UI, strict C99, generated tables, strict MkDocs and whitespace checks passed.
- Offline first-30-second shipped MP3 checks: all-max peaks 0.95938/0.97089, L/R balance −0.351/−0.184 dB (game1/game3), chorus mono-fold commutation error <1e-9, bass-only overall/band gains unchanged. CPU ~0.27–0.34 s per 30 s audio in this sandbox. Reviewed JUCE chorus/reverb references online for topology/math, with independent tunings/implementation and no copied JUCE code.
- Touched effect DSP, Options labels/binding, effect setting key/startup binding, focused tests and docs only. Deliberately did not change bass algorithm, music-folder/audition routing, BGM/SFX volume, action triggers, cards/rules/scoring/AI/campaign/assets or dependencies. Live listening/layout and full Windows/web builds remain unverified; measured checks do not establish perceived quality. Earlier stereo entries below are superseded history.


### Fixed-ear synthetic width removed (22 October 2026)

- Further listening feedback reported a broken/left-pulled image. Equal L/R RMS and nominal correlation had missed that the generated Hilbert Side retained one quadrature polarity regardless of input channel orientation. Added a channel-exchange test **before** changing DSP: the shipped implementation failed with maximum sample error 0.228065 at only 25% width. This demonstrates a fixed-side defect, not a claim that the player's exact playback setup has been reproduced.
- Removed the Hilbert coefficients/convolution and all synthetic Side generation from Mid. Width now scales existing dry and room Side, preserving the mono centre and source-aware power guards; mono remains mono with Reverb zero. Retained the strong upper range, existing bass compressor/limiter, reverb tank/filter parameters, and fixed 64-sample dry/bypass timing (~1.45 ms at 44.1 kHz). Width also expands the existing room Side; it no longer depends on fake phase stereo to sound wide.
- New exchange test passes with **zero** error at 25/50/100% width, bass off/on, hot peaks and 44.1/48/96 kHz (Reverb zero isolates width/bass). Mono noise/tones have zero added Side; all simultaneous mono-tone bins measure 0.0000 dB L/R, onset and settled. Replaced the old mono-to-stereo strength fixture with actual centre-heavy stereo parts: Side/Mid RMS 0.225/0.361/1.000 at 25/50/100% across all three rates, with unchanged strength/mono-preservation assertions. Width impulse output after 5 ms remains zero.
- Reprocessed both shipped MP3s offline: game3 source correlation 0.936 → width-only 0.214 (~39% Side energy, still far stronger than the original conservative ~7%); width-only L/R balance −0.104 dB. Game1 source −0.218 → −0.223. Aligned low-level mono-null error <6e-10; all-max peaks 0.95877/0.97248. Bass-only gains remain unchanged. These are numerical measurements, not a listening playtest.
- Full engine/audio/settings/UI suite, table UI, ASan/UBSan DSP checks, strict C99 syntax, generated wiki tables, strict MkDocs and whitespace checks passed. Reviewed online Airwindows MidSide encoding as an independent sum/difference reference; no copied implementation or dependency.
- Touched width routing, focused regressions and docs only. UI/settings/audition routing, bass processing, tank/delay/filter parameters, action triggers, rules/cards/scoring/AI/campaign/assets and dependencies unchanged. Live speaker/headphone confirmation, the player's exact track/settings, and complete Windows/web builds remain to verify. Earlier entries below describe superseded revisions.


### Stronger stereo width (22 October 2026)

- Follow-up listening feedback found the balanced width too subtle. Expanded the Hilbert filter from 17 to 129 taps for lower-mid coverage, increased the upper slider drive, and raised the maximum Side/Mid power budget from 0.49 to 1.0. Both dry channels and bypass now share 64-sample alignment (~1.45 ms at 44.1 kHz); no reverb/delayed-repeat feed was introduced. The source-aware guard remains, and adaptive covariance correction is blended with the room amount rather than colouring the already-quadrature FIR alone.
- Actual first-30-second bundled MP3 checks: game3 width-only correlation is now 0.183 versus the prior 0.860 (source 0.936), increasing Side energy from ~7% to ~41% compared with the previous maximum. Game1 is already wide/anti-correlated: source −0.218, new width-only −0.222. Aligned low-level mono-null error <1e-9; all-max peaks 0.96255/0.97343. Bass-only gains remain +0.058/+5.902 dB overall and +5.422/+6.425 dB in the four-pole 140 Hz band. These are offline numerical results, not live listening or LUFS measurements.
- Added bass-heavy modulated harmonic regression: maximum Side/Mid RMS 0.996–0.998 across 44.1/48/96 kHz, with each slider step required to increase spread meaningfully. Mono noise reaches equal Mid/Side power, near-zero correlation. Simultaneous-tone ear balance remains <0.027 dB settled; longer-filter startup is <0.143 dB over the first aligned 100 ms. Explicit onset bound changed from 0.1 to 0.2 dB for the stronger path; settled bound remains 0.1 dB. Width impulse output after 5 ms remains zero.
- Full engine/audio/settings/UI suite, table UI suite, ASan/UBSan DSP tests, strict C99 syntax, generated tables, strict MkDocs and whitespace checks passed. Offline DSP CPU cost ~0.28–0.46 s per 30 s audio in this sandbox, not a device-wide guarantee.
- Touched width DSP/guards, focused tests and docs. Bass/reverb algorithms and control tuning, UI/settings/preview routing, action triggers, rules/cards/scoring/AI/campaign/assets and dependencies were not changed. Live headphone/speaker listening and complete Windows/web builds remain to verify. Earlier work entries below describe their respective revisions, not the current filter length/range.


### Per-frequency stereo balance correction (22 October 2026)

- Reproduced the reported one-sided image: the independent-all-pass width path measured +13.388 dB L/R at 8 kHz and −10.710 dB at 100 Hz with six centred tones together. Existing aggregate and isolated-tone balance checks had missed this spectral tilt.
- Replaced that width path with a 17-tap antisymmetric, Hamming-windowed Hilbert FIR. Both dry channels (including bypass) have the same fixed eight-sample alignment (~0.18 ms at 44.1 kHz), ensuring quadrature per frequency without an audible echo. Removed the phase-shifting width IIR stages; retained the mono/Side guard and bass/reverb tuning. Bypass/mono tests now explicitly compare aligned samples, not zero-latency samples; chunking/ON/OFF timing stays constant.
- Added simultaneous-tone per-frequency ear measurements at 25/50/100% width and 44.1/48/96 kHz, both first-100-ms and settled windows. Worst differences: <0.09 dB on onset and <0.0121 dB settled. Existing impulse/no-echo, bass loudness, post-kick, safety and audition checks pass; reprocessed both shipped MP3s offline and checked aligned mono preservation. Full regression suite, ASan/UBSan, strict C99, generated tables, strict docs and whitespace checks passed.
- Touched width DSP, focused tests, and docs only. No new dependency, card/rules/scoring/AI/trigger/settings/UI change. Live listening with the revised Test Effect (Reverb=0 isolates width) and full platform/device checks remain to verify.


### Width delay / bass loudness correction (22 October 2026)

- Listening feedback exposed two issues the initial tests missed: width borrowed the long reverb/slapback return, and the 8:1 bass compressor ducked the entire track. Width now uses a separate pair of one-sample all-pass phase filters, with no reverb-tank feed. Bass compresses only its added shelf component (up to 4:1), over the unchanged dry music; removed the whole-track bus compressor. Linked peak limiting now starts at 0.88 and caps below 0.98 to leave more boost headroom. Reverb's intentional delay/tail, late bass return and existing mono/Side guards remain.
- Added regression checks for width impulse tails at all slider positions and 44.1/48/96 kHz, positive gain on loud bass, and no post-kick suppression of the high band. The no-echo check fails against the old implementation (~0.005 delayed output vs <7.2e-8 now after 5 ms). Loud 20 Hz input now gains +2.87…+2.89 dB instead of ~−7 dB; quiet input still reaches +10 dB. Existing presence, mono, channel balance, chunking and safety checks still pass.
- Re-decoded the shipped game1/game3 first 30 seconds offline: bass-only overall RMS increased +0.058/+5.902 dB over the identical bypass windows; bass-band RMS (four cascaded 140 Hz low-passes) increased +5.422/+6.425 dB. These are sample-domain measurements, not a live listening or LUFS claim.
- Scope: DSP routing/tuning, focused regression tests, and related docs only. No UI/settings format, gameplay, rules, cards, triggers, assets or dependency changes. Full regression suite, strict C99 syntax, sanitizer tests, generated-table/docs and whitespace checks passed. Remaining: player listening with the Test Effect switch (Reverb=0 isolates width/bass), live device-rate checks and complete platform builds.


### Action-audio audition switch (22 October 2026)

- Added **TEST EFFECT: ON/OFF** on Options page 2. It plays a real loaded match track through the existing callback at full action strength; sliders update live. OFF releases the effect smoothly while retaining the same stream/position for dry A/B comparison. Finished tracks advance; leaving the page or Back restores menu music. Preview state is not saved, BGM mute/volume stays respected, and missing music gets a visible warning. Browser audition uses bundled match tracks.
- Touched only Options UI/routing, a small audio preview helper, tests/stubs, and related docs. Did not change DSP maths, capture/Koi-Koi triggers, gameplay, saved-setting formats, or dependencies. Existing shared function signatures are unchanged.
- Verified full engine/audio/settings/UI regression suite, strict C99 syntax checking, ASan/UBSan DSP and UI tests, generated wiki tables, strict MkDocs build, and whitespace checks. Added tests assert wet versus dry sample output through the real callback, no restart on toggles, playlist advance, missing-track handling, and clearing test state on page/Back navigation. Live listening/layout and full Windows/web builds still need manual verification.


### Options page 2 / Action audio (22 October 2026)

- Added bottom-right arrows and page numbers. Page 1 retains its original settings; page 2 holds the music folder, the requested Audio effect options header/explanation, and saved Stereo wide, Bass boost, Reverb sliders. Browser page 2 exposes effect sliders but explicitly marks local folders desktop-only. Back saves both pages; folder modals block navigation/underlying inputs.
- Extended the existing action-triggered music callback, retaining capture/Koi-Koi triggers and the fixed delay tank. Added an RBJ +10 dB shelf before stereo-linked compression (up to 8:1), independent width/reverb controls, symmetric M/S decoding with original centre retained, an added-Side power/correlation guard, and full-band reverb whose bass return opens in the final 20%. Shared output gain prevents separate-ear compressor/limiter image shifts. Settings cross to the audio thread via lock-free atomic snapshots; filter resets happen on the audio thread. No dependencies were added.
- Verification passed: `./build.sh test` (500 simulated matches, playlist/settings/UI tests, existing DSP regression and new real-callback numerical suite), `./tests/test_table_ui.sh`, ASan/UBSan for new DSP/settings/UI tests, and strict C99 syntax checks against Raylib 5.5. Numerical tests cover 44.1/48/96 kHz sequences, exact bypass, L/R balance/mono fold-down, +10 dB bass/compression, late low-bass reverb admission, source anti-correlation, headroom, chunk sizes, smoothing, and transitions. The earlier DSP presence-band failure is now fixed without weakening its threshold (−2.0 dB vs required −12.5 dB).
- Additionally decoded/processed the first 30 seconds of shipped game1/game3 MP3s offline with Raylib's bundled decoder: no non-finite samples or active peaks above 0.88, low-level width mono-null error below 1e-9, and no worsening of game1's pre-existing −0.218 correlation. This is numerical testing, not a listening playtest. Online Raylib/RBJ/JUCE references, measurements and the explicit device-rate tuning limitation are recorded in `docs/reference/audio-dsp.md`.
- Touched Options UI, settings/audio/startup, related documentation and regression tests. Deliberately did not change rules, cards, scoring, campaign/Deus decisions, AI, artwork, SFX routing, or action-envelope trigger logic. No shared mode flag was introduced.
- Remaining: live desktop listening/layout, Windows filesystem/clipboard behavior, complete Windows/web builds and listening on different output-device rates. Raylib 5.5 exposes no public device-rate getter; inherited sample-domain tuning is nominally 44.1 kHz, not a claim of rate-invariant filter cutoffs/delay timing.


### Custom music folder (21 October 2026)

- Added desktop **Options → Music Folder** with a typed/pasted path, Apply/Enter, Cancel, and Use Default. Windows Explorer's surrounding path quotes are removed. Modal input blocks other Options controls; the browser retains its original controls and bundled audio.
- Persisted `music_folder` in `settings.txt`, including spaces and UTF-8 text. Loads up to 10 playable MP3s from the folder (no recursion), sorted by filename before loading; invalid files are skipped. Replacements are staged so rejected folders preserve current music. A missing/unusable saved folder falls back to shipped match tracks at launch without deleting the setting. Menu music, SFX, volume controls, and existing reverb processing remain separate/unchanged.
- Touched settings persistence, the audio playlist loader, Options UI/startup wiring, README/wiki instructions, and test/CI integration. Deliberately left card data, rules, scoring, campaign unlocks, AI, audio DSP, and artwork unchanged; added no libraries.
- Passed custom playlist/settings and modal input tests, including AddressSanitizer/UndefinedBehaviorSanitizer; headless table UI suite; strict C99 syntax checking of all game sources against Raylib 5.5; engine suite (500 matches); generated wiki-table check; strict MkDocs build using temporary dependencies; and `git diff --check`.
- **Full `./build.sh test` still exits 1:** the pre-existing DSP presence-band test reports −18.7 dB against its −12.5 dB threshold. Rebuilt/reran the test from unchanged HEAD source and reproduced the identical failure. This feature does not retune the DSP.
- Still to verify: live desktop playback/visual layout, Windows clipboard/filesystem behavior, and end-to-end Windows/web builds. Tests use synthetic Raylib file/audio calls; they verify playlist resource management and input/persistence, not MP3 decoding or audible playback.


- Refreshed wiki prose for cumulative per-tier campaign coins and the exact M-01–M-12 gates, the actual M-12 condition for DEUS, desktop-only replays, and recent table visual/audio feedback. Corrected the stale “newest opponent” unlock explanation; no game code, scoring, card art, or saved data changed.

- Added per-background normal-map lighting for the seven non-Deus table backgrounds (`src/ui_board.c`, `src/ui_board.h`, `assets/*_normal.png`, `assets/depth_fog.fs`, `assets/depth_fog_web.fs`; Deus remains unchanged):
  - One drifting, spinning 4-band light beam (32.1 s round-trip drift, spin 10% faster, nested bands `1.568 / 1.176 / 0.784 / 0.392` at `10%–20%` combined via `max()`) with a toned-down thunder-flash spike (`1.0 + 0.95 * flashAmount` plus `0.065` wide glow) triggered on every card capture (`captured_count` rising edge) and Koi-Koi call (`koi_called` rising edge) that sustains full for 500 ms and then rolls off exponentially from full (`tau = 0.50 s`, per the 22 October 2026 envelope change above; a new edge snaps back to full and restarts the sustain).
  - Smooth 3D relief with wrapped directional bias (`smoothstep(0.015, 0.30, length(n.xy)) * (0.55 + 0.45 * smoothstep(-0.30, 0.35, dot(n.xy, lightDir.xy)))`) so flat pixels return `0.0` without creating a center cutout at `lightUV` or hard shadow cutoffs on ridges.
  - Multi-tap two-ring light-only bloom kernel (`sampleRidgeEmit` at 7 px and 13 px) plus soft-knee highlight rolloff (`base + (1.0 - base) * (1.0 - exp(-lightAdd))`) so the moving background light glows smoothly around 3D ridges without washing black shadows into grey fog or affecting UI/cards, and softened lower-board depth fog (`0.80` gain, `0.55` cap, plus lighter lower gradient) so the lit artwork reads clearly at default gamma `1.00`.
- Attached a real-time stereo reverb + Haas widener (`AttachAudioStreamProcessor` in `src/ui_audio.c`) to the music streams, driven by a tight, hard-limited curve (`reverb_energy -> reverb_mid -> reverb_curve` in `src/ui_board.c`, `tau_mid = 0.20 s`, `tau_curve = 0.18 s`, `wet_gain = rev * 0.55f * sc_duck`) that fires audibly on the first capture (`~0.38`), builds into full synergy by the third stacked capture in a row (`1.00`), and ducks its high-passed tail under the dry music so it never smears or spikes.
- Rebalanced the capture reverb's stereo path so the mid/high band (snare crack, hats, presence) is actually audible as width: the Side de-esser split moved from ~2.8 kHz (inside the snare band) to ~6 kHz, the fixed `0.72` HF shelf came off, and a ~1.2 kHz one-pole presence lift (+~4.6 dB) was added to the Side only. Measured on `game1.mp3` and `game3.mp3`: Side gains `+4.5…+5.3 dB` across 2–4 kHz and `+5.9…+6.3 dB` at 4–8 kHz, bass Side moves by `≤0.3 dB`, and the Mid/centre channel is unchanged (`0.000 dB`), so the mono sum is identical. Added `tests/test_audio_dsp.c` with a Raylib-free stub header (`tests/raylib_audio_stub/raylib.h`) and `tests/test_audio_dsp.sh`; the test fails against the previous behaviour and now runs from `./build.sh test` and CI.
- Fixed missing card capture sound (`audio_play_capture` in `src/ui_audio.c` now falls back to `sfx_drop` when `assets/sfx/capture.wav` is absent, and `src/ui_main.c` also plays `audio_play_drop` when dropping a card into `PHASE_HAND_CHOICE`), and synced `prev_captured` / `prev_koi` across round resets in `board_draw_table`.
- Replaced `assets/back.png` with a dark matte/biomechanical black card-back texture featuring the vertical cyan `XENO/KOI` logo (`assets/audiowide.ttf`) at the bottom-right corner inside the cropped card bounds.
- Lit up all four left-hand miniature-card capture lane boxes (`B`, `A`, `R`, `P` in `src/ui_board.c`) and made the perimeter dots on both the active player's score box and all four capture-lane boxes glow brightly (`ui_theme_set_panel_flare_glow`) whenever the turn switches to that player.
- Scaled the perimeter dot brightness (`ui_theme_set_panel_flare_glow`) on the right-hand combo summary panels (`CPU COMBOS` and `YOUR COMBOS`) with `yaku_count` so the orbiting dots glow progressively brighter as more combos are collected.
- Enlarged the main menu `[XENO / KOI]` title by ~62% (`136.0f` font size with `font_title` rasterized at `96px` and trilinear mipmaps), enabled the 3D normal-map relief lighting and depth-fog shader on menu backgrounds (`draw_bg_if_loaded`), and added `ui_theme_draw_text_flares` (`src/ui_theme.c`) to trace smoothed 64-point glyph boundary loops so phosphor dots and streaks orbit directly along the letter contours.
- Expanded ambient flying dark particles (`ASH_COUNT = 155`) and card-capture burst particles (`ASH_BURST_MAX = 420`) with varied sizes (`1–10 px` ambient and up to `13 px` burst fragments) rendered as 8 rotating corrupted/jagged biomechanical shard shapes (`draw_corrupted_shard`), scaling the burst count and speed (`28 + (streak - 1) * 24` per card) as more cards are taken in a row.
- Updated perimeter panel flares (`src/ui_theme.c`, `src/ui_board.c`) so all five dots on a held/dragged card (and its matching field cards) stay continuously visible until dropped, and made all perimeter dots 1 px smaller and 20% slower.
- Updated default Option Menu settings in `src/storage.c` and `src/storage.h` to BGM Volume `100%` (`1.00f`), SFX Volume `50%` (`0.50f`), Bloom `MID` (`1`), CRT Filter `MID` (`2`), and Display Gamma `1.00`.
- Added click-to-select / click-to-play alongside existing dragging. Selection and matching field cards remain outlined; clicking the selected card again, right-clicking, or pressing Esc cancels selection. Small pointer movements are tolerated as clicks.
- Enlarged and brightened the ruleset label. Combo summaries now have fixed bounds and shortened preview names instead of overflowing.
- Made both combo panels open paginated details with full names, points, card images, and multiplier breakdowns. Gameplay, CPU/replay progress, animations, and the turn timer pause during inspection; closing the window consumes the input rather than playing through it.
- Reworked the existing moving flares from a five-dot global pool into independent per-box pools: each bordered UI panel/button and audited direct-outline UI box gets 3–5 dots moving around its perimeter at varied speeds. On entry, only that box's dots flash white and grow by 3 px (1 px to 4 px total) for 400 ms; other boxes stay dim, and the pulse does not retrigger while held. Covered campaign portrait slots/buttons, option slider/ruleset/preview boxes, active turn boxes, and the cinematic notice, plus selected, matching, hovered, and animated card highlights (keyed so their dots follow card motion). Ordinary unhighlighted card-art borders remain plain to avoid crowding the play area.
- Added headless table UI regression tests and a CI step to run them. Updated the controls documentation and regenerated the table map.
- Card data, matching rules, scoring, AI decision logic, artwork, and the intentional Deus unlock were not changed.
- Corrected campaign progression to derive open sectors from earned bronze/silver/gold totals (repairing stale saved unlock counts); capture card SFX now stay at or above 40% while SFX is enabled, with mute still respected.

## Wiki verification (21 October 2026)

- `python3 tools/gen_wiki_tables.py --check`, `python3 -m mkdocs build --strict` (via temporary installed dependencies), generated card/screenshot/flow/table-map checks, and `git diff --check` passed. The 12 campaign gate rows were compared directly with `UNLOCK_REQ_B/S/G` in `src/campaign.c`.
- `./build.sh test` passed the 500-match engine suite but **failed** the audio DSP presence-band check (Side −18.7 dB vs required −12.5 dB). No DSP implementation was changed in this docs-only update; see `docs/errata.md`.

## Verification last recorded for game changes

- Engine integrity suite (`./build.sh test`) passed, including 500 full simulated matches and the audio DSP/capture-SFX regression checks.
- Headless table UI, combo-panel, and campaign coin-unlock regression suite (`./tests/test_table_ui.sh`) passed against Raylib 5.5 headers.
- Strict C99 syntax check (`gcc -std=c99 -Wall -Wextra -Werror -fsyntax-only`) across all `src/*.c` passed.
- Generated wiki tables (`python3 tools/gen_wiki_tables.py --check`), strict MkDocs build, and `git diff --check` passed; the previously checked shader parity/simulation was unaffected.
- Offline full-pipeline shader simulation (normal-map lighting + depth fog + vignette + table dim + gamma 1.40) verified across all seven non-Deus backgrounds and capture flash states.

## Still to verify

- Visually check the moving/spinning 4-band background beam, 3D ridge gating, and capture thunder flash on a live GPU across desktop and web builds, and confirm Deus remains unchanged.
- Visually check the 400 ms, +3 px flare growth, perimeter movement (including the highlighted cards), speed variation, and return to baseline while the pointer remains over the panel.
- Visually playtest click/drag coexistence, selection feedback, and panel readability.
- Check both combo windows, pagination, and timer pause/resume in live play and replay playback.
- Refresh the stored gameplay screenshots when suitable captures are available. They predate the new sidebar; the generated table map is updated.

## Keeping this useful

Update the date, completed work, checks actually run, and remaining verification when handing off a meaningful change. Keep scoring explanations in the wiki rather than duplicating them here. Distinguish implemented, tested, and released work.

## Historical snapshot — retained for context

The following is an earlier session's report, not a description of the current checkout. Its test totals, file-size claims, build outputs, and archive paths are historical and have not been reverified. The original `HANDOFF_KoiKoi_Giger_v2.pdf` is also legacy.

### Earlier status summary

1. **Start Menu Order Updated**
   - Configured exact 6-button main menu structure:
