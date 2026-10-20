> **Development handoff (last updated 20 October 2026).** This records the local source status and verification noted below; it is not a release or deployment record. For current rules, use the code in `src/` and the wiki in `docs/`. The original build spec, `HANDOFF_KoiKoi_Giger_v2.pdf`, is legacy; see `AGENTS.md` and `docs/errata.md`.
# XENO/KOI: BIOMECHANICAL EDITION — Project Status

# XENO/KOI: BIOMECHANICAL EDITION - STATUS REPORT
**Last updated: 20 October 2026.** This is a development handoff for the local source, not a release or deployment record. Code in `src/` defines behavior; the wiki in `docs/` explains the rules. See `AGENTS.md` for contributor guardrails and `docs/errata.md` for known issues.

## Status Summary
## Latest completed work

### Moving lights over background bump maps — 20 October 2026

- Replaces the PR #58 Harvest "sweeping sun" prototype. Its normal texture was nearly flat (mean z 0.997), its height mask allowed a constant +22% on only 18% of the frame (the sweep cancelled itself out by construction: constant front elevation minus the l.z baseline), and its bake darkened Harvest — none of it read as a moving light.
- `tools/make_bg_normal_maps.py` derives real bump maps (`assets/<name>_normal.png`) from each non-Deus background's art: RGB holds a tangent-space normal of a height field derived from the art, and **alpha holds the height field itself, which nothing samples** — the shaders light the RGB normal with plain tangent-space `N.L`, so no relief mask is baked any more (the mask was removed with the relief-only trick it existed to gate). Relief is auto-normalized to a target slope so every background gets equally strong bumps, so the maps are regenerated whenever the art is re-baked (they are currently derived from the darker backgrounds now on `main`). Deus is excluded on purpose and keeps its own look.
- The height field is a heavily blurred luminance (broad sigma 26 px, fine 13 px, 35/65 blend). The blur weight matters more than the slope: a luminance-derived gradient is only non-zero where the art changes, so at the previous light blur (3.6/1.6 px) half the frame was flat and *all* the relief sat in narrow bands along the art's contours — lighting that read as "lightning that is edging", a complaint the slope-only and mask-only fixes could not touch. At 26/13 the relief comes from the large forms (the Harvest dome, the spires, the ridges) and the art's own texture is no longer amplified into visible contour banding; `SLOPE_MAX` 3.0 clamps the steepest art edges so they cannot push a normal past ~72 degrees and strobe. `TARGET_SLOPE` 2.6 gives mean tilt 25 degrees, p90 56.
- Both background shaders (`assets/depth_fog.fs`, `assets/depth_fog_web.fs`) now do ordinary tangent-space normal mapping: an ambient floor (0.55) plus two *directional* lights — a warm key whose direction drifts on a slow lissajous and a cool fixed fill — with a Blinn-Phong glint (`SPEC_POWER` 48, `SPEC_GAIN` 0.30). The diffuse term is the full `max(dot(n, l), 0.0)`, so the bump actually steers the light and the highlight travels as the key light swings. This replaces the previous rig of three drifting *point* lights with a relief-only response and a mask, which could only ever pool around the map's steep pixels and never read as a lit surface. Because a flat normal returns `l.z` (~0.5-1.0) under a directional light, the ambient floor is what keeps the frame from washing out — 0.55 leaves shadowed slopes at ~0.8x and lit ones at ~1.7-2.0x, so the light and dark sides of the relief both read. `--check` enforces desktop/web parity of the moving-light block, and the tool mirrors its constants for offline previews.
- A capture (a match taken from the table) or a Koi-Koi call also flashes the background lights like a thunder: `bg_light_flash()` in `src/ui_board.c` spikes to 1.0 and decays linearly at 2.2/s until only 30% of the peak is left (~0.32 s), then eases into a long exponential tail (`tau` 0.55 s) back to rest, so the whole flash lasts ~2.5 s. The shader multiplies the entire lighting factor by `1 + FLASH_GAIN * flashAmount`. Both events are caught as rising edges (`ANIM_CAPTURE_FLY` becoming active, `koi_called[]` counting up), so every event flashes exactly once and SHOBU, which does not call Koi-Koi, does not flash. Deus has no normal map and is unaffected.
- In-match drawing enables the effect per background whenever its normal map is loaded (`src/ui_board.c`); menus keep the static dimmed path. Fog values, vignette, cards, rules, scoring, AI, campaign, storage, and replay are untouched.
- Verification actually run: `python3 tools/make_bg_normal_maps.py --check`; `./build.sh test` (500 simulated matches); `./tests/test_table_ui.sh`; strict C99 `-Wall -Wextra -Werror` syntax check of all `src/*.c` against Raylib 5.5 headers; GLSL block parse of both shaders; offline simulation of the shader math (`--preview`) against the current art — median pixel gain is 1.48x with a p10 of 0.73 and a p90 of 1.66 (shadowed and lit slopes both read), clipping 0.0-0.1%, mean background luma rises modestly instead of doubling (Harvest 13.4 -> 17.3), and 14-31% of pixels shift by more than 6 luma between samples 25 s apart as the key light drifts. At the flash peak the whole factor multiplies by 2.0x.
- Still to verify: maintainer GPU retest on desktop/web — light travel, bump read on the ridges, and that Deus is visually unchanged. Deliberately left alone: the relief is derived from luminance, so it describes where the art changes rather than a true surface; where the art has fine texture the gradient is small and that texture stays unlit. Tuning lives in the moving-light block of both shaders (keep the tool in sync): `AMBIENT` for how dark the shadow side goes, `KEY_GAIN`/`FILL_GAIN` for strength, `BLUR_BROAD`/`BLUR_DETAIL` for how broad the relief is, `TARGET_SLOPE` for how strong it is.


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
