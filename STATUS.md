> **Development handoff (last updated 20 October 2026).** This records the local source status and verification noted below; it is not a release or deployment record. For current rules, use the code in `src/` and the wiki in `docs/`. The original build spec, `HANDOFF_KoiKoi_Giger_v2.pdf`, is legacy; see `AGENTS.md` and `docs/errata.md`.
# XENO/KOI: BIOMECHANICAL EDITION — Project Status

# XENO/KOI: BIOMECHANICAL EDITION - STATUS REPORT
**Last updated: 20 October 2026.** This is a development handoff for the local source, not a release or deployment record. Code in `src/` defines behavior; the wiki in `docs/` explains the rules. See `AGENTS.md` for contributor guardrails and `docs/errata.md` for known issues.

## Status Summary
## Latest completed work

- Added click-to-select / click-to-play alongside existing dragging. Selection and matching field cards remain outlined; clicking the selected card again, right-clicking, or pressing Esc cancels selection. Small pointer movements are tolerated as clicks.
- Enlarged and brightened the ruleset label. Combo summaries now have fixed bounds and shortened preview names instead of overflowing.
- Made both combo panels open paginated details with full names, points, card images, and multiplier breakdowns. Gameplay, CPU/replay progress, animations, and the turn timer pause during inspection; closing the window consumes the input rather than playing through it.
- Added a one-shot hover shine to the existing panel flares. On entry, matching flares brighten; if none are active on that panel, the effect reuses an existing global flare slot there so the shine is visible immediately. For 400 ms, the streak reaches white at full opacity and grows from 1 px to 6 px thick (+5 px), then returns to baseline; staying over the panel does not retrigger it. The existing moving streak effect now allows five active dots globally instead of three.
- Added headless table UI regression tests and a CI step to run them. Updated the controls documentation and regenerated the table map.
- Card data, matching rules, scoring, opponent logic, artwork, and the intentional Deus unlock were not changed.

## Verification last completed in this session

- Engine integrity suite passed, including 500 full simulated matches.
- Browser tooltip regression passed, generated wiki tables are current, and `git diff --check` passed.
- Headless Raylib-API-stub timing harnesses passed, including the case where all five global flare slots were active on other panels: hover reached alpha 255 and 6 px thickness, faded to baseline by 400 ms, stayed dim while held, and flashed again after re-entry. These harnesses are temporary and are not a graphical playtest.
- Raylib headers are not installed locally, so the Raylib table UI test and live graphical playtest were not run.
- Windows and WebAssembly builds are tracked through the PR workflows; they were not run locally.

## Still to verify

- Visually check the 400 ms, 5 px growth of the panel flare, its return to baseline while the pointer remains over the panel, and re-entry behavior in the graphical build.
- Visually playtest click/drag coexistence, selection feedback, and panel readability.
- Check both combo windows, pagination, and timer pause/resume in live play and replay playback.
- Review the latest Windows and WebAssembly CI results on PR #56; those targets were not built locally.
- Refresh the stored gameplay screenshots when suitable captures are available. They predate the new sidebar; the generated table map is updated.

## Keeping this useful

Update the date, completed work, checks actually run, and remaining verification when handing off a meaningful change. Keep scoring explanations in the wiki rather than duplicating them here. Distinguish implemented, tested, and released work.

## Historical snapshot — retained for context

The following is an earlier session's report, not a description of the current checkout. Its test totals, file-size claims, build outputs, and archive paths are historical and have not been reverified. The original `HANDOFF_KoiKoi_Giger_v2.pdf` is also legacy.

### Earlier status summary

1. **Start Menu Order Updated**
   - Configured exact 6-button main menu structure:
