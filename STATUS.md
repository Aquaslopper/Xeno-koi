> **Development handoff (last updated 16 October 2026).** This records the local source status and verification noted below; it is not a release or deployment record. For current rules, use the code in `src/` and the wiki in `docs/`. The original build spec, `HANDOFF_KoiKoi_Giger_v2.pdf`, is legacy; see `AGENTS.md` and `docs/errata.md`.
# XENO/KOI: BIOMECHANICAL EDITION — Project Status

# XENO/KOI: BIOMECHANICAL EDITION - STATUS REPORT
**Last updated: 16 October 2026.** This is a development handoff for the local source, not a release or deployment record. Code in `src/` defines behavior; the wiki in `docs/` explains the rules. See `AGENTS.md` for contributor guardrails and `docs/errata.md` for known issues.

## Status Summary
## Latest completed work

- Added click-to-select / click-to-play alongside existing dragging. Selection and matching field cards remain outlined; clicking the selected card again, right-clicking, or pressing Esc cancels selection. Small pointer movements are tolerated as clicks.
- Enlarged and brightened the ruleset label. Combo summaries now have fixed bounds and shortened preview names instead of overflowing.
- Made both combo panels open paginated details with full names, points, card images, and multiplier breakdowns. Gameplay, CPU/replay progress, animations, and the turn timer pause during inspection; closing the window consumes the input rather than playing through it.
- Added headless table UI regression tests and a CI step to run them. Updated the controls documentation and regenerated the table map.
- Card data, matching rules, scoring, opponent logic, artwork, and the intentional Deus unlock were not changed.

## Verification last completed in this session

- Engine integrity suite: 500 full simulated matches passed.
- Table input and combo-panel tests passed, including AddressSanitizer and UndefinedBehaviorSanitizer runs.
- Native and browser tooltip tests passed.
- Engine-derived documentation tables were current; strict MkDocs build passed.
- All C sources passed syntax checks with Raylib 5.5 headers and `-Wall -Wextra -Werror`.
- Generated table-map and card-image presence checks passed. These are not graphical playtests.

## Still to verify

- Visually playtest click/drag coexistence, selection feedback, and panel readability in the graphical build.
- Check both combo windows, pagination, and timer pause/resume in live play and replay playback.
- Confirm full Windows and WebAssembly build results. They were not run locally; this document does not claim a CI or deployment result.
- Refresh the stored gameplay screenshots when suitable captures are available. They predate the new sidebar; the generated table map is updated.

## Keeping this useful

Update the date, completed work, checks actually run, and remaining verification when handing off a meaningful change. Keep scoring explanations in the wiki rather than duplicating them here. Distinguish implemented, tested, and released work.

## Historical snapshot — retained for context

The following is an earlier session's report, not a description of the current checkout. Its test totals, file-size claims, build outputs, and archive paths are historical and have not been reverified. The original `HANDOFF_KoiKoi_Giger_v2.pdf` is also legacy.

### Earlier status summary

1. **Start Menu Order Updated**
   - Configured exact 6-button main menu structure:
