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

Choose 1 to 12 rounds in Options. The default is set in the code, and the value is saved with your settings. See [Errata & Known Issues](../errata.md) for a note about the default.

## Options

These settings are saved in `settings.txt` next to the game:

| Setting | Options |
|---|---|
| Rounds | 1 to 12 |
| [Difficulty](difficulty.md) | Easy, Hard, Master, [DEUS SUPREMUS](deus-supremus.md) |
| Ruleset | Nintendo, FudaWiki, KK-ADD-10, Tournament, Yakuza (locked to Yakuza on [DEUS SUPREMUS](deus-supremus.md)) |
| Background | Seven selectable backgrounds: Harvest, Hole, Arctic, Castle, City, Face, Realm ([DEUS SUPREMUS](deus-supremus.md) has his own locked background) |
| Terminal colour | Green, Amber, Teal, Crimson, Violet |
| CRT filter | Off, Low, Mid, High |
| Bloom | Off, Mid, Full |
| Gamma | 0.5 to 2.0 |
| Music and effects volume | 0.0 to 1.0 |
| Animation speed | Slow, Normal, Fast |
| Fullscreen | Desktop: Windowed, Borderless. Browser: Windowed, Fullscreen (see below) |

### Fullscreen

- **Desktop builds:** the Options row switches the game between Windowed and Borderless, and the choice is saved with your other settings.
- **Browser build:** fullscreen belongs to the **page**, not the game. Use the **[ FULLSCREEN ]** button in the bar above the game, or the Options row inside the game — both do the same thing and both read back the browser's real state. The game canvas fills the screen with no browser chrome around it.
- To leave fullscreen, press **Esc**. The in-game Options row works as an exit too, which matters because the page bar is not visible while the canvas is fullscreen.
- While fullscreen is active, **Esc leaves fullscreen instead of opening the pause menu.** Leave fullscreen first, then press Esc again to pause.

## Stats and history

The game records statistics for **finished matches only**. Restarting, abandoning, or closing a match in progress does not count.

Tracked statistics include:

- Matches played, won, lost, and drawn
- Win rate
- Rounds won by you and by the CPU
- Total points for and against
- Best award in a single round
- Koi-Koi calls by you and by the CPU
- Matches per difficulty
- Current and best win streak
- Yaku counts, which feed the favourite combo shown on the main menu

Match history is written to `history.txt`, and a report is written to `stats_report.txt`. Both are next to the game.

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
