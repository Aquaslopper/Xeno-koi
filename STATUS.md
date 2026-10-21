> **Development handoff (last updated 21 October 2026).** This records the local source status and verification noted below; it is not a release or deployment record. For current rules, use the code in `src/` and the wiki in `docs/`. The original build spec, `HANDOFF_KoiKoi_Giger_v2.pdf`, is legacy; see `AGENTS.md` and `docs/errata.md`.
# XENO/KOI: BIOMECHANICAL EDITION — Project Status

# XENO/KOI: BIOMECHANICAL EDITION - STATUS REPORT
**Last updated: 21 October 2026.** This is a development handoff for the local source, not a release or deployment record. Code in `src/` defines behavior; the wiki in `docs/` explains the rules. See `AGENTS.md` for contributor guardrails and `docs/errata.md` for known issues.

## Status Summary
## Latest completed work

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

## Verification last completed in this session

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
