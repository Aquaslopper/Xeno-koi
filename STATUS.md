> **Development handoff (last updated 20 October 2026).** This records the local source status and verification noted below; it is not a release or deployment record. For current rules, use the code in `src/` and the wiki in `docs/`. The original build spec, `HANDOFF_KoiKoi_Giger_v2.pdf`, is legacy; see `AGENTS.md` and `docs/errata.md`.
# XENO/KOI: BIOMECHANICAL EDITION — Project Status

# XENO/KOI: BIOMECHANICAL EDITION - STATUS REPORT
**Last updated: 20 October 2026.** This is a development handoff for the local source, not a release or deployment record. Code in `src/` defines behavior; the wiki in `docs/` explains the rules. See `AGENTS.md` for contributor guardrails and `docs/errata.md` for known issues.

## Status Summary
## Latest completed work

### Moving lights over background bump maps — 20 October 2026

- Replaces the PR #58 Harvest "sweeping sun" prototype. Its normal texture was nearly flat (mean z 0.997), its height mask allowed a constant +22% on only 18% of the frame (the sweep cancelled itself out by construction: constant front elevation minus the l.z baseline), and its bake darkened Harvest — none of it read as a moving light.
- `tools/make_bg_normal_maps.py` derives real bump maps (`assets/<name>_normal.png`, RGB tangent-space normals with height in alpha) from each non-Deus background's art. Relief is auto-normalized to a target slope so every background gets equally strong bumps. Deus is excluded on purpose and keeps its own look.
- Both background shaders (`assets/depth_fog.fs`, `assets/depth_fog_web.fs`) light the bump with an ambient lift — every lit background is brighter than its art, so there is no night side — plus three wide soft lights drifting on independent slow paths. The diffuse term uses only the *tilted* part of the normal (`n.x*l.x + n.y*l.y`, the flat viewer-facing component projected out) and a relief mask (`smoothstep(0.12, 0.45, |n.xy|)`) keeps near-flat art at ambient, so the pools land on the raised ridges instead of washing the whole frame. An earlier version of this block used the full `dot(n, l)`: because a flat normal returns `l.z` (~0.5-1.0) under a low light, that alone multiplied nearly the whole frame by ~2.0-2.9 and the bump map moved the result by only ~5 luma on average — the light showed up on flat art, not on the bumps. Light height is now 0.35 (grazing) so ridges read stronger. `--check` enforces desktop/web parity of the moving-light block, and the tool mirrors its constants for offline previews.
- In-match drawing enables the effect per background whenever its normal map is loaded (`src/ui_board.c`); menus keep the static dimmed path. Fog values, vignette, cards, rules, scoring, AI, campaign, storage, and replay are untouched.
- Verification actually run: `python3 tools/make_bg_normal_maps.py --check`; `./build.sh test` (500 simulated matches); `./tests/test_table_ui.sh`; strict C99 `-Wall -Wextra -Werror` syntax check of all `src/*.c` against Raylib 5.5 headers; offline simulation of the shader math (`--preview`) — median pixel gain is now 1.10x (ambient only) instead of 2.1x, only 10-18% of pixels (the ridges) exceed 1.5x, mean background luma rises modestly instead of doubling (Harvest 26.8 -> 36), half the pixels do not move at all between samples 25 s apart, and the moving 19% shift by up to 157 luma.
- Still to verify: maintainer GPU retest on desktop/web — light travel and bump read on the ridges, strength under fog + bloom + CRT, and that Deus is visually unchanged. Tuning lives in the moving-light block of both shaders (keep the tool in sync).


- Added click-to-select / click-to-play alongside existing dragging. Selection and matching field cards remain outlined; clicking the selected card again, right-clicking, or pressing Esc cancels selection. Small pointer movements are tolerated as clicks.
- Enlarged and brightened the ruleset label. Combo summaries now have fixed bounds and shortened preview names instead of overflowing.
- Made both combo panels open paginated details with full names, points, card images, and multiplier breakdowns. Gameplay, CPU/replay progress, animations, and the turn timer pause during inspection; closing the window consumes the input rather than playing through it.
- Reworked the existing moving flares from a five-dot global pool into independent per-box pools: each bordered UI panel/button and audited direct-outline UI box gets 3–5 dots moving around its perimeter at varied speeds. On entry, only that box's dots flash white and grow by 3 px (1 px to 4 px total) for 400 ms; other boxes stay dim, and the pulse does not retrigger while held. Covered campaign portrait slots/buttons, option slider/ruleset/preview boxes, active turn boxes, and the cinematic notice, plus selected, matching, hovered, and animated card highlights (keyed so their dots follow card motion). Ordinary unhighlighted card-art borders remain plain to avoid crowding the play area.
- Added headless table UI regression tests and a CI step to run them. Updated the controls documentation and regenerated the table map.
- Card data, matching rules, scoring, opponent logic, artwork, and the intentional Deus unlock were not changed.

## Verification last completed in this session

- Engine integrity suite passed, including 500 full simulated matches.
- Browser tooltip regression passed, generated wiki tables are current, and `git diff --check` passed.
- Compiled `src/ui_theme.c` with `-Wall -Wextra -Werror` against a temporary Raylib API stub. Stub harnesses passed the 3–5-per-box bounds and independent storage, keyed flare continuity as card rectangles move, panel-scoped flash (other boxes stay dim), the 400 ms +3 px pulse, no retrigger while held, re-entry, and travel around all four perimeter sides. These are not a graphical playtest.
- Raylib headers are not installed locally, so the Raylib table UI test and live graphical playtest were not run.
- CI results for each pushed revision are tracked on PR #56.

## Still to verify

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
