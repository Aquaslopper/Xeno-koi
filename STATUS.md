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
- Attached a real-time heavy stereo reverb + Haas ping-pong processor (`AttachAudioStreamProcessor` in `src/ui_audio.c`) to the music streams, driven by a dedicated bi-exponential capture curve (`reverb_energy -> reverb_mid -> reverb_curve` in `src/ui_board.c`) so music stereo width and reverb swell along a smooth rounded curve rather than spiking, and stack higher and longer as more cards are captured.
- Replaced `assets/back.png` with a dark matte/biomechanical black card-back texture featuring the vertical cyan `XENO/KOI` logo (`assets/audiowide.ttf`) at the bottom-right corner inside the cropped card bounds.
- Lit up all four left-hand miniature-card capture lane boxes (`B`, `A`, `R`, `P` in `src/ui_board.c`) briefly alongside the active player's score box whenever the turn changes.
- Increased ambient dark ash particles (`ASH_COUNT = 95`) and added a capture burst of dark particles (`ASH_BURST_MAX = 128`) that erupts from the taken table cards at the capture spike lag and trails slightly after the cards as they move into the capture lanes.
- Updated perimeter panel flares (`src/ui_theme.c`, `src/ui_board.c`) so all five dots on a held/dragged card (and its matching field cards) stay continuously visible until dropped, and made all perimeter dots 1 px smaller and 20% slower.
- Restored the default Display Gamma setting in `src/storage.c` to `1.00`.
- Added click-to-select / click-to-play alongside existing dragging. Selection and matching field cards remain outlined; clicking the selected card again, right-clicking, or pressing Esc cancels selection. Small pointer movements are tolerated as clicks.
- Enlarged and brightened the ruleset label. Combo summaries now have fixed bounds and shortened preview names instead of overflowing.
- Made both combo panels open paginated details with full names, points, card images, and multiplier breakdowns. Gameplay, CPU/replay progress, animations, and the turn timer pause during inspection; closing the window consumes the input rather than playing through it.
- Reworked the existing moving flares from a five-dot global pool into independent per-box pools: each bordered UI panel/button and audited direct-outline UI box gets 3–5 dots moving around its perimeter at varied speeds. On entry, only that box's dots flash white and grow by 3 px (1 px to 4 px total) for 400 ms; other boxes stay dim, and the pulse does not retrigger while held. Covered campaign portrait slots/buttons, option slider/ruleset/preview boxes, active turn boxes, and the cinematic notice, plus selected, matching, hovered, and animated card highlights (keyed so their dots follow card motion). Ordinary unhighlighted card-art borders remain plain to avoid crowding the play area.
- Added headless table UI regression tests and a CI step to run them. Updated the controls documentation and regenerated the table map.
- Card data, matching rules, scoring, opponent logic, artwork, and the intentional Deus unlock were not changed.

## Verification last completed in this session

- Engine integrity suite (`./build.sh test`) passed, including 500 full simulated matches.
- Headless table UI and combo-panel regression suite (`./tests/test_table_ui.sh`) passed against Raylib 5.5 headers.
- Strict C99 syntax check (`gcc -std=c99 -Wall -Wextra -Werror -fsyntax-only`) across all `src/*.c` passed.
- Generated wiki tables check (`python3 tools/gen_wiki_tables.py --check`), desktop/web `moving-lights` shader-block parity, and `git diff --check` passed.
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
