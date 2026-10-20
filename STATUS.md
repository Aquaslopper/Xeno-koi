> **Development handoff (last updated 20 October 2026).** This records the local source status and verification noted below; it is not a release or deployment record. For current rules, use the code in `src/` and the wiki in `docs/`. The original build spec, `HANDOFF_KoiKoi_Giger_v2.pdf`, is legacy; see `AGENTS.md` and `docs/errata.md`.
# XENO/KOI: BIOMECHANICAL EDITION — Project Status

# XENO/KOI: BIOMECHANICAL EDITION - STATUS REPORT
**Last updated: 20 October 2026.** This is a development handoff for the local source, not a release or deployment record. Code in `src/` defines behavior; the wiki in `docs/` explains the rules. See `AGENTS.md` for contributor guardrails and `docs/errata.md` for known issues.

## Background artwork update — 20 October 2026

- Baked the approved TOP-light, Medium 4.0 bump effect into the seven 1280×720 backgrounds: Harvest, Hole, Arctic, Castle, City, Face, and Realm.
- Applied final brightening gamma 1.20 (`output = input^(1/1.20)`) from the pre-gamma bumped images, replacing the stronger 1.40 trial.
- Verified all seven JPEGs decode and retain 1280×720 dimensions; `assets/deus.png` matches HEAD byte-for-byte. No game code, rules, card artwork, or shaders changed. Local preview/scratch outputs are excluded from the PR.
- Remaining verification: maintainer will test the backgrounds in-game and report back. Earlier UI work and its checks are recorded below; no new graphical game build was tested for this artwork update.

## Animated background lighting — 20 October 2026

- Shipped the seven original Medium 4.0 normal maps and added slow wandering surface light in the desktop/web background fog shaders, before fog/vignette. The baked gamma 1.20 JPGs are unchanged.
- Added capture pulses (0.6 seconds) and stronger koi-koi pulses (1 second) at existing player/CPU action feedback sites, independent of SFX volume. Overlapping pulses use the brighter envelope rather than summing. A match/capture uses one trigger, not separate duplicate effects.
- Deus bypasses the new light calculations and has no normal texture. Missing normal textures disable lighting for that background. Cards/UI are not shaded by this pass; menus retain their existing static drawing path.
- Checks: engine suite (500 matches), headless table input/combo suite, C99 `-Wall -Wextra` syntax checks of ui_board.c/ui_main.c against Raylib 5.5 headers, and diff whitespace check passed.
- Remaining: desktop/web shader compilation and graphical playtest, light intensity/speed tuning, and visual confirmation of capture/koi pulses. No live graphical build was run here; headless checks do not validate GPU rendering. Replay playback does not currently use the live action feedback hooks.

## Lighting correction after playtest — 20 October 2026

- Replaced the broad light falloff with a compact, soft-edged moving footprint. Event pulses now share that footprint instead of brightening the whole background.
- Increased localized surface response and removed the flat-plane specular contribution to emphasize ridges. Fog depth masking now uses unlit image luminance so added light cannot thin the fog.
- Checks: matching desktop/web shader bodies, numerical footprint checks at four timestamps (over 59% of the image outside the light), and diff whitespace check passed. Headless GPU compilation was attempted but unavailable (`libGL.so not loaded`); graphical desktop/web verification remains pending.
- No JPGs, normal maps, Deus handling, event timing, game rules, or UI code changed in this correction.

## Stronger localized lighting — 20 October 2026

- After maintainer feedback that the light remained too weak, increased diffuse gain from 0.30 to 0.90, ridge highlight gain from 0.65 to 1.80, and event multiplier from 2 to 4. Added a modest normal-dependent cool light component so dark surfaces can visibly catch the light.
- Moved the additive lighting blend after fog, before vignette; fog no longer attenuates the light by up to 88%. The compact spatial footprint remains unchanged, so this is not a global brightness increase. Deus still contributes zero new lighting.
- Verified desktop/web shader-body parity and diff whitespace. In-game visual testing remains pending; no GPU validation claimed.

## Stronger relief, slower light behind fog — 20 October 2026

- Regenerated all seven animated-light normal maps at strength 6.0 (previously 4.0) from original pre-bake art. Added tools/make_background_normals.py for repeatable generation from source commit c188a05; requires Pillow/NumPy and that commit in local Git history.
- Halved wandering frequencies to 0.085/0.065, reduced diffuse/highlight gains to 0.65/1.30 and cool fill to 0.08, and restored lighting before fog. Localized event pulses also remain behind fog.
- Slightly relaxed fog on normal-lit backgrounds only: gain 1.35→1.20 and opacity cap 0.88→0.78. Deus and missing-normal fallback keep original fog values. Baked JPGs remain unchanged.
- Checks passed: desktop/web shader-body parity, RGB normal dimensions/unit-length tolerance, deterministic regeneration, unchanged JPG/Deus bytes, and diff whitespace. In-game visual assessment remains pending; no GPU test claimed.

## Broad directional light sweep — 20 October 2026

- Replaced the moving point-light pool with one distant directional light sweeping left/right and back at frequency 0.085 (about 74 seconds per full cycle). No circular footprint or vertical wandering remains.
- Subtracted the flat-plane diffuse response as well as flat specular response to emphasize changing normal-map contours without a uniform brightness wash. Event pulses boost the same contour light across the scene.
- Kept strength-6 normal maps, current gains, behind-fog blending, relaxed fog, and Deus exclusion. JPGs and gameplay/event hooks unchanged.
- Checks: desktop/web shader-body parity, sweep direction at four cycle positions, absence of spotlight falloff, and diff whitespace passed. Visual tuning still requires in-game testing.

## Continuous lighting correction — 20 October 2026

- Maintainer confirmed harsh fragmented relief after the directional-light change. Removed positive-only diffuse/specular ridge extraction and the additive cool-white highlight layer.
- Replaced it with continuous signed diffuse/soft-specular modulation of the image's own colors, capped at ±20% before event pulses. Broad left/right motion, strength-6 normals, behind-fog blending, and Deus exclusion remain unchanged.
- Checks: desktop/web shader-body parity, finite/bounded response over all seven normal maps at four sweep times, and diff whitespace passed. No graphical validation claimed; maintainer playtest remains required.

## Standard normal-mapped sun lighting — 20 October 2026

- Per maintainer request ("apply the effect the right way" per standard normal-mapping sources, and smoother rather than hard-edged), replaced the custom signed response/clamp/ridge math with the conventional model: decode normal map, distant sun direction sweeping left/right (fixed Z), Lambert `max(dot(n,l),0)` diffuse plus constant ambient, applied as `texel.rgb * illumination` and clamped. Removed specular terms and all ridge thresholds, so no embossed outlines remain.
- Softened edges by cross-filtering the sampled normal vectors over a 5-texel kernel and setting bilinear filtering + clamp wrap on the normal textures. Event flashes raise the same diffuse term (`0.80 + eventFlash`), keeping longer 1.2s/2s durations; lighting still blends before fog. Deus (no normal) keeps original fog path; JPGs and normal maps unchanged by this revision.
- Checks passed: desktop/web shader-body parity, old response math absent, sun-vector unit length at four cycle positions, strict C syntax with Raylib 5.5 header, table UI suite, unchanged JPG/Deus bytes, diff whitespace. GPU/in-game visual verification still outstanding.

## Travelling sun pool with night falloff — 20 October 2026

- Maintainer specified the spatial model exactly: when the light is left, the right is night; in the middle, both sides are night; when right, the left is night. Replaced the uniform sun illumination with a bright horizontal pool sweeping left/right (sunX = 0.5 + 0.45*sin(t*0.085)), wide smoothstep falloff (aspect-corrected 0.22–0.95), and night level 0.58 outside the pool. No whole-image ambient lift remains.
- Effect brightness raised: pool multiplier is 0.80 + 0.40*diffuse + eventFlash*2.2, peaking near 1.78x versus the previous 1.36x max. Speed, flash durations (1.2s/2s), softened normals, fog order, Deus exclusion unchanged.
- Checks: desktop/web shader-body parity, simulated illumination at left/middle/right sun positions showing night on the correct side (0.58 vs 1.60), strict C syntax, diff whitespace. Visual confirmation still pending.

## Stronger, slower flash decay — 20 October 2026

- Raised event flash peaks 0.18→0.25 (capture) and 0.32→0.45 (koi), extended durations to 3s/5s, and replaced the quick quadratic decay with smoothstep t²(3-2t), which eases off the peak and glides back to zero. Only board_flash_light envelope changed; shader, pool, sun speed, fog, and Deus handling untouched.
- Checks: strict C syntax, table UI suite, diff whitespace passed. Visual confirmation pending.

## Status Summary
## Latest completed work

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
