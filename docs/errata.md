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

1. **[DEUS SUPREMUS](reference/deus-supremus.md) is open to everyone, on purpose.** `src/campaign.c` defines `DEUS_UNLOCKED_FOR_TESTING` as `1`, so the final opponent does not require clearing all twelve. The maintainer keeps it open while testing the mode. This is intentional, not a bug. Revisit it when the maintainer decides the mode is ready.
2. **FudaWiki's Koi-Koi take-back is not implemented.** The handoff mentions it. The game does not offer it.
3. **[DEUS SUPREMUS](reference/deus-supremus.md) logic.** The DEUS SUPREMUS difficulty uses the Master decision logic with stronger card evaluation and 100% accuracy. It is not a separate AI.
4. **Fullscreen on the browser build belongs to the page, not the game.** The web shell owns it: the `[ FULLSCREEN ]` button and the in-game Options row both toggle the canvas, and `settings.fullscreen` is ignored and cleared on web. Browsers only grant fullscreen from a user gesture, so the page button always works while Safari may ignore the in-game row. Player-facing detail: [Modes & Settings](reference/modes.md).

## Campaign progression correction

A clearance previously advanced the saved opponent count directly, bypassing the dossier's silver/gold coin requirements; old saves also kept that inflated count. Unlocks are now recalculated from earned coins, so progress beyond bronze-only sectors requires the displayed silver and gold totals.

## Table UI updates

- **Alternative card control:** click a hand card, then a matching field card (or empty field if unmatched). Dragging remains available; matching and scoring are unchanged.
- **Combo-panel overflow corrected:** the right-column summaries now have fixed bounds and shortened names. Click either panel for paginated details, with play and the turn timer paused until dismissal. Existing gameplay screenshots predate this panel layout; the generated table map reflects it.

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
