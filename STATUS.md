> **Development handoff (last updated 22 October 2026).** This records the local source status and verification noted below; it is not a release or deployment record. For current rules, use the code in `src/` and the wiki in `docs/`. The original build spec, `HANDOFF_KoiKoi_Giger_v2.pdf`, is legacy; see `AGENTS.md` and `docs/errata.md`.
# XENO/KOI: BIOMECHANICAL EDITION — Project Status

# XENO/KOI: BIOMECHANICAL EDITION - STATUS REPORT
**Last updated: 22 October 2026.** This is a development handoff for the local source, not a release or deployment record. Code in `src/` defines behavior; the wiki in `docs/` explains the rules. See `AGENTS.md` for contributor guardrails and `docs/errata.md` for known issues.

## Status Summary
## Latest completed work

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
  - One drifting, spinning 4-band light beam (32.1 s round-trip drift, spin 10% faster, nested bands `1.568 / 1.176 / 0.784 / 0.392` at `10%–20%` combined via `max()`) with a toned-down thunder-flash spike (`1.0 + 0.95 * flashAmount` plus `0.065` wide glow) triggered on every card capture (`captured_count` rising edge) and Koi-Koi call (`koi_called` rising edge) that peaks smoothly, drops to 40%, and decays slowly (`tau = 3.40 s`).
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
