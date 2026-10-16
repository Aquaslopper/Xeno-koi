# Modes & Settings

How strongly the CPU plays has its own page: [Difficulty](difficulty.md). This page is everything
around the match itself - unlocks, rounds, options and the records the game keeps.

## Campaign and unlocks

- **Opponents 1 to 12** unlock in order. The next opponent unlocks when you win a match against the newest unlocked opponent with enough rounds for the difficulty you played.
- **Clearance rounds:**

  | Difficulty | Rounds needed to unlock the next opponent |
  |---|---:|
  | Easy | 9 or more |
  | Hard | 6 or more |
  | Master | 3 or more |

- A win with fewer rounds still counts as a match win, but it does **not** unlock the next opponent. The results screen tells you so.
- **[DEUS SUPREMUS](deus-supremus.md)** normally requires all twelve opponents to be cleared. The maintainer currently keeps it **open to everyone on purpose**, while the mode is being tested. See [Errata & Known Issues](../errata.md).

## Rounds

Choose 1 to 12 rounds on the Options screen; the `+` and `-` buttons stop at both ends
(`src/ui_screens.c:257-258`). With no `settings.txt` the game starts at **2** rounds
(`src/storage.c:32`), and the value is saved with your other settings when you press
BACK (SAVE). See [Errata & Known Issues](../errata.md) for why that is 2 and not 6.

## Options

These settings are saved in `settings.txt` next to the game:

<div class="figure-zoom" id="options-live"><div class="figure-zoom-scroll"><img src="../../assets/screenshots/options.webp" width="1920" height="1080" loading="lazy" alt="The System Configuration screen: Match Length at 9 Rounds, Ruleset YAKUZA on a red band, Difficulty EASY, Master Timer marked MASTER ONLY, then Terminal Color TEAL, Background 3: Castle with a small preview, Anim Speed at NORMAL, two volume sliders at 70% and 30%, Fullscreen BORDERLESS, Bloom MID, CRT Filter 2: MID, Display Gamma 1.40, and BACK (SAVE) at the bottom."></div><a class="figure-zoom-btn" href="#options-live">zoom the frame</a><a class="figure-zoom-btn" href="#">close</a></div>

*Every row the game has. The bands on the Ruleset row are its own colour code: green for Nintendo and KK-ADD-10, amber for FudaWiki, red for Tournament and Yakuza (`src/ui_screens.c:167-170`). This player's file differs from a fresh install on three rows only - 9 rounds instead of 2, Yakuza instead of KK-ADD-10, Castle instead of Harvest - plus BGM at 70%, Normal animation, Borderless and Bloom on.*

The left column is the label the screen shows. The right column is what you get before `settings.txt`
exists, read out of `storage_load_settings()` in `src/storage.c:32-46`:

| Setting | Options | On a fresh install |
|---|---|---|
| Match Length | 1 to 12 rounds | 2 |
| [Difficulty](difficulty.md) | Easy, Hard, Master, [DEUS SUPREMUS](deus-supremus.md) | Easy |
| Ruleset | Nintendo, FudaWiki, KK-ADD-10, Tournament, Yakuza. The row is painted green for Nintendo and KK-ADD-10, amber for FudaWiki, red for the other two (`src/ui_screens.c:167-170`); locked to Yakuza on [DEUS SUPREMUS](deus-supremus.md) | KK-ADD-10 |
| Master Timer | 15 sec or 30 sec, and the row only answers to clicks at Master. Against DEUS it reads **Deus Timer: 15 SEC (LOCKED)** | 15 |
| Background | Harvest, Hole, Arctic, Castle, City, Face, Realm — seven, because index 5 is his and both arrows step over it (`src/ui_screens.c:273-274`). The row shows a live 46 x 26 preview of the frame you are on | Harvest (0) |
| Terminal colour | Green, Amber, Teal, Crimson, Violet. The name list in `src/ui_screens.c:193` has a sixth, DARK, which the selector's `% 5` at `:272` never lands on: it is what playing DEUS sets, and leaving him puts you back on Teal | Teal (2) |
| Anim Speed | Slow, Normal, Fast | Slow |
| BGM Volume / SFX Volume | Sliders, shown as a percentage | 50% / 30% |
| Bloom Effect | Off, Mid, Full | Off |
| CRT Filter | 0: Off, 1: Low, 2: Mid, 3: High | 2: Mid |
| Display Gamma | 0.5 to 2.0, in 0.05 steps, or straight to any point on the slider | 1.40 |
| Fullscreen | Desktop: Windowed, Borderless. Browser: Windowed, Fullscreen (see below) | Windowed |

Nothing here is per-match: BACK (SAVE) writes `settings.txt` and returns you to the menu
(`src/ui_screens.c:296`), and the next match starts with what you left there.

### Fullscreen

- **Desktop builds:** the Options row switches the game between Windowed and Borderless, and the choice is saved with your other settings.
- **Browser build:** fullscreen belongs to the **page**, not the game. Use the **[ FULLSCREEN ]** button in the bar above the game, or the Options row inside the game — both do the same thing and both read back the browser's real state. The game canvas fills the screen with no browser chrome around it.
- To leave fullscreen, press **Esc**. The in-game Options row works as an exit too, which matters because the page bar is not visible while the canvas is fullscreen.
- While fullscreen is active, **Esc leaves fullscreen instead of opening the pause menu.** Leave fullscreen first, then press Esc again to pause.

## Stats and history

The game records statistics for **finished matches only**. Restarting, abandoning, or closing a match in progress does not count.

<div class="figure-zoom" id="stats-live"><div class="figure-zoom-scroll"><img src="../../assets/screenshots/stats.webp" width="1920" height="1080" loading="lazy" alt="The stats screen: twelve rows of numbers on the left under COMBAT TELEMETRY // STATS, and on the right the LAST 10 MATCHES as ten raw record lines, with START REPLAY, SAVE and LOAD under them and SAVE LOG, RESET STATS and BACK along the bottom."></div><a class="figure-zoom-btn" href="#stats-live">zoom the screen</a><a class="figure-zoom-btn" href="#">close</a></div>

*14 matches, 11 of them won. Read the Difficulty row against the log on the right: it says `Easy 10 | Hard 0 | Master 4` while two of the ten lines end in `deus` - there is no DEUS counter, because a match against him increments the Master total (`src/storage.c:355`). Every number on this screen is a field of `GameStats`, so the rows are exactly what `stats.txt` holds.*

<div class="figure-still"><img src="../../assets/screenshots/stats-log.webp" width="729" height="504" loading="lazy" alt="Close-up of the match log: the heading LAST 10 MATCHES and ten lines reading date, time, rounds, ruleset, difficulty, the two scores and the result, for example 2026-10-16 02:52;rounds=9;kk-add-10;easy;you=63;cpu=50;WIN."></div>

*Those are the records themselves, printed as stored. A line is `date time;rounds=N;ruleset;difficulty;you=;cpu=;outcome` (`src/storage.c:377`), which is also what `history.txt` holds, and every line is clickable: the rectangle that draws the highlight and the one that catches the click are the same box (`src/ui_replay.c:378` and `:323`), 400 game px wide (`ROW_W`, `:19`). The text is not clipped to it - on the longest line here it runs to x 1116, 66 px past the box - so the frame you can click is narrower than the line you read, and clicking the tail of a record does nothing.*

Tracked statistics include:

- Matches played, won, lost, and drawn
- Win rate
- Rounds won by you and by the CPU
- Total points for and against
- Highest round score (the best single round) and best award (the highest total in a match you
  won) - two different fields, `highest_round_score` and `best_award` in `src/storage.h:34-35`, and
  the second row is labelled only "Best Award"
- Koi-Koi calls by you and by the CPU
- Matches per difficulty. The row has three numbers and no DEUS column: a match against him is
  counted with Master (`src/storage.c:355`)
- Current and best win streak
- Yaku counts, which feed the favourite combo shown on the main menu

Match history is written to `history.txt`, and a report is written to `stats_report.txt`. Both are next to the game. `SAVE LOG` at the bottom left of the screen is what writes the report, and it says so on the screen for a moment (`src/ui_screens.c:345`). `RESET STATS` is the only red button and it asks first: a modal over the panel, `RESET ALL STATS & LOGS?`, `This action cannot be undone.`, CONFIRM or CANCEL (`src/ui_screens.c:346-353`).

### Replays

Every finished match is recorded while it is played, so it can be watched again later. The Stats screen lists the last ten matches on the right; click a line to select it and use the three buttons underneath:

- **[START REPLAY]** plays the selected match back on the board.
- **[SAVE]** asks for a name and copies the selected match to that name, so you can keep it or pass it on.
- **[LOAD]** lists the `.txt` files in the `replays/` folder; pick one and press **PLAY**. To load a replay someone shared with you, put its file in that folder first.

Records are written to the `replays/` folder next to the game as plain `.txt` files:

- The ten most recent matches are kept automatically and named after the date and time they finished, for example `2026-10-15_1548.txt`. A second match finishing in the same minute is stored as `..._2.txt`. When an eleventh match arrives, the oldest automatic record is deleted.
- Files made with **[SAVE]** are never deleted by the game. Give them the names you want.
- A record holds the match seed, the AI RNG starting state, the ruleset, round count, difficulty, master timer and opponent, plus the card choices and decisions from both sides. Playing it back feeds those choices into the same engine again, so the deals, captures and scores are the same.
- A record can only be made while the match is played. Existing log entries from before this feature, matches that were never finished (restarted, abandoned or closed), and [DEUS SUPREMUS](deus-supremus.md) matches lost on the turn timer have no record. A replay never counts towards statistics, streaks or unlocks, and the game never changes a record.
- While a replay runs, the status line shows `REPLAY - P TO PAUSE   ESC TO STOP`, or `REPLAY PAUSED` once it is frozen. **Esc** leaves the replay and returns to the Stats screen, and so do the **STOP** button - the lower of the two board buttons, which reads **RESTART** in a live match - and **STOP REPLAY** in the pause overlay. **P** or the **PAUSE** button freezes the board; the overlay then offers **RESUME** and **STOP REPLAY** rather than the match actions, because a replay cannot be restarted or abandoned.
- The choice, decision and round-result windows are not shown during a replay - the board, the capture piles and the combo panels are.
- A replay is watched, not skimmed: every round is dealt out from the stock pile with the round-start banner, and every hand card and stock card flies to the table before the engine takes the step. The flight is a little slower than a live turn and is followed by a beat to read the board, and it follows the animation speed set in Options. The engine step itself waits for the flight to land, so the board never gets ahead of what is drawn.
- A record with an unsupported replay-format version is refused with a short message. If a later engine change makes a valid record diverge, playback stops with a short message; incompatible replay changes must increment the format version.
- **The browser build has no replay buttons.** It cannot read or write files next to the game, so the Stats screen there shows the log list without the replay controls and without selection.
