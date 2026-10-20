> **Development handoff (last updated 20 October 2026).** This records the local source status and verification noted below; it is not a release or deployment record. For current rules, use the code in `src/` and the wiki in `docs/`. The original build spec, `HANDOFF_KoiKoi_Giger_v2.pdf`, is legacy; see `AGENTS.md` and `docs/errata.md`.
# XENO/KOI: BIOMECHANICAL EDITION — Project Status

# XENO/KOI: BIOMECHANICAL EDITION - STATUS REPORT
**Last updated: 20 October 2026.** This is a development handoff for the local source, not a release or deployment record. Code in `src/` defines behavior; the wiki in `docs/` explains the rules. See `AGENTS.md` for contributor guardrails and `docs/errata.md` for known issues.

## Status Summary
## Latest completed work

- Added click-to-select / click-to-play alongside existing dragging. Selection and matching field cards remain outlined; clicking the selected card again, right-clicking, or pressing Esc cancels selection. Small pointer movements are tolerated as clicks.
- Enlarged and brightened the ruleset label. Combo summaries now have fixed bounds and shortened preview names instead of overflowing.
- Made both combo panels open paginated details with full names, points, card images, and multiplier breakdowns. Gameplay, CPU/replay progress, animations, and the turn timer pause during inspection; closing the window consumes the input rather than playing through it.
- Added a one-shot hover shine to the existing panel flares. A hovered panel's flare brightens on entry and fades back to its normal dim level over 500 ms; staying over the panel does not keep it bright or retrigger the flash. Flare shape, motion, and random spawning are unchanged.
- Added headless table UI regression tests and a CI step to run them. Updated the controls documentation and regenerated the table map.
- Card data, matching rules, scoring, opponent logic, artwork, and the intentional Deus unlock were not changed.

## Verification last completed in this session

- Engine integrity suite: 500 full simulated matches passed.
- Browser tooltip regression test and engine-derived documentation table check passed.
- Panel-flare timing harness passed: the existing flare reached full alpha on hover, faded to baseline by 500 ms, did not retrigger while held, and flashed again after re-entry. This was compiled against Raylib API stubs and is not a graphical playtest.
- `git diff --check` passed.
- The Raylib table UI test and live graphical playtest were not run locally; Raylib headers are not installed in this environment.

## Still to verify

- Visually check the 500 ms panel-flare shine, its return to dim while the pointer remains over the panel, and re-entry behavior in the graphical build.
- Visually playtest click/drag coexistence, selection feedback, and panel readability.
- Check both combo windows, pagination, and timer pause/resume in live play and replay playback.
- Build Windows and WebAssembly for this change; they were not run locally, and CI has not run on this working branch.
- Refresh the stored gameplay screenshots when suitable captures are available. They predate the new sidebar; the generated table map is updated.

## Keeping this useful

Update the date, completed work, checks actually run, and remaining verification when handing off a meaningful change. Keep scoring explanations in the wiki rather than duplicating them here. Distinguish implemented, tested, and released work.

## Historical snapshot — retained for context

The following is an earlier session's report, not a description of the current checkout. Its test totals, file-size claims, build outputs, and archive paths are historical and have not been reverified. The original `HANDOFF_KoiKoi_Giger_v2.pdf` is also legacy.

### Earlier status summary

1. **Start Menu Order Updated**
   - Configured exact 6-button main menu structure:
