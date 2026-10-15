# Errata & Known Issues

!!! warning "The handoff is legacy"
    `HANDOFF_KoiKoi_Giger_v2.pdf` (11 Oct 2026) was the original build spec for the first version of the game. **It is no longer the rulebook.** The rules have been changed and extended since, and the handoff has not been updated.

    This page lists where the handoff and the current game disagree. **Where they differ, the wiki and the code are correct.** Do not follow the handoff's rules.

## Rules that changed since the handoff

| Topic | Handoff says | Code does now | Where in code |
|---|---|---|---|
| Caller's own Koi-Koi | "Your own Koi-Koi never doubles your own score." (§7b) | Each of your Koi-Koi calls raises your own multiplier by one step (×1 → ×6, capped). | `koi_caller_multiplier()` in `src/rules.c` |
| Koi-Koi limit | Nintendo: one call per player. FudaWiki and KK-ADD-10: unlimited. | Tournament and Yakuza also allow one call, like Nintendo. | `finish_stock_turn()` in `src/engine.c` |
| KK-ADD-10 Tanzaku | Counts from 5 ribbons, as n − 4. Test case: all six ribbons = 12. | Counts from the 4th ribbon, as n − 3. All six ribbons now score **13** (10 for the combined set + 3). | `score_captures()` in `src/rules.c` (commented "was >= 5, n-4") |
| Viewings | Always on. | Disabled in Tournament and Yakuza. | `score_captures()` in `src/rules.c` |
| Yakuza ruleset | Not in the handoff. | Added: Tournament scoring plus Dealer's Privilege. | `apply_dealer_privilege()` in `src/engine.c` |
| First-dealer tie | §7a: "same month: redraw." The appendix uses rank 20/10/5/1 "for the first-dealer tie-break." | Same month: the higher card type deals. Only an identical month and type forces a redraw. | `engine_determine_first_dealer()` in `src/engine.c` |

## Defaults that differ from the handoff

| Setting | Handoff says | Code does now | Note |
|---|---|---|---|
| Rounds | 6 | `storage_load_settings()` sets **2** rounds when there is no `settings.txt`. | Fresh installs start at 2 rounds. |
| Ruleset | Nintendo | `storage_load_settings()` sets **KK-ADD-10** when there is no `settings.txt`. | Fresh installs start on KK-ADD-10, the maintainer's house rule. |

Both defaults only apply when `settings.txt` is missing. Once the file is saved, your chosen values are used.

## Files that changed since the handoff

| Topic | Handoff says | Code does now | Where in code |
|---|---|---|---|
| Saved files | "Saving: Settings and stats only, in text files next to the exe. No mid-match save." | Finished matches are recorded as well, in a `replays/` folder next to the exe (the ten newest automatic records plus copies the player saved by name). There is still **no mid-match save** - a match has to be played to the end to get a record, and unfinished matches get none. | `src/replay.c`, `src/ui_replay.c` |

## Known issues

1. **Deus Supremus is open to everyone, on purpose.** `src/campaign.c` defines `DEUS_UNLOCKED_FOR_TESTING` as `1`, so the final opponent does not require clearing all twelve. The maintainer keeps it open while testing the mode. This is intentional, not a bug. Revisit it when the maintainer decides the mode is ready.
2. **Card 47 label fixed.** It now reads `12-P3 Plain` (was `12-P2 Plain`, duplicating card 46). Scoring was never affected.
3. **FudaWiki's Koi-Koi take-back is not implemented.** The handoff mentions it. The game does not offer it.
4. **Repository test step.** `build.sh test` expects `tests/test_engine.c`, which is not in this repository, so the engine test step is skipped.
5. **Deus logic.** The Deus difficulty uses the Master decision logic with stronger card evaluation and 100% accuracy. It is not a separate AI.
6. **The browser build hands fullscreen to the page, not the game.** On desktop, Options → Fullscreen calls raylib's `ToggleBorderlessWindowed()`. On web that call resolves to Emscripten's `Module.requestFullscreen()`, which moves the canvas into a throwaway parent `<div>`, resizes it towards monitor device pixels and leaves its inline CSS size set. The canvas CSS box then no longer matches the size raylib sizes to the window, so the game is drawn offset and clicks land in the wrong place. The web shell now owns fullscreen: the `[ FULLSCREEN ]` button and the in-game Options row both toggle the canvas through `XenoKoiFullscreen` in `web/shell.html`, and `settings.fullscreen` is ignored and cleared on load in the browser build. A `fullscreen=1` saved by an earlier web session is therefore harmless.
   Browsers only grant fullscreen from a user gesture: the page button always qualifies, the in-game row works in Chrome, Firefox and Edge, and Safari may ignore the in-game row (the page button still works). `docs/reference/modes.md` describes the player-facing behaviour.

## README differences

The README's ruleset summary now states that Viewings are disabled in both Tournament and Yakuza, and that Yakuza uses Tournament scoring. The README's description of the streak multiplier matches the code.

## Verified against the code

The following handoff test cases were checked against the current code and still hold:

- Goko: 10 (Nintendo), 15 (FudaWiki), 10 (KK-ADD-10)
- Rain Man + 3 other Brights = Ame-shiko 7
- Rain Man + 2 other Brights = 0
- Curtain + Moon + Sake Cup = 10 (Nintendo)
- KK-ADD-10: Ino-shika-cho trio + 2 animals = 6
- Nintendo: all six ribbons = 10
- Nintendo: Ino-shika-cho trio + 2 animals = 7 (not 7 + 1)
- KK-ADD-10: nine Plains + Sake Cup = 1
- Teshi and Kuttsuki award 6 points
- Both players lucky = 0–0 draw (non-Yakuza)
- Exhaustion = 0–0 draw, and the dealer stays

The one handoff test case that no longer holds is KK-ADD-10 with all six ribbons, which is now 13.
