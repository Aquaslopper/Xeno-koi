# Modes, Difficulty & Settings

## Difficulty

| Difficulty | What it changes |
|---|---|
| **Easy** (default) | The CPU is weakest, and it calls Koi-Koi at random (50% of the time while it has 3 or more cards). Hand cards that can capture glow, and while you drag a card, field cards of the same month glow. |
| **Hard** | A stronger CPU. It needs at least 4 cards in hand before it will gamble on Koi-Koi, and it banks when you are already on the board. No glow hints. |
| **Master** | A strong CPU. It needs at least 3 cards in hand before gambling, and it banks instead of continuing when a card would complete one of your special combinations. A **turn timer** applies: 15 or 30 seconds, set in Options. If it runs out, a random card is played for you. |
| **Deus** | The 100% accuracy setting, against Entity 13. It uses the Master decision logic with stronger card evaluation, always plays under Yakuza rules, locks the ruleset selector, and has a fixed 15-second turn timer. |

The CPU's Koi-Koi decision compares the **banked award now** with the **projected award** if it continues. It continues only when the projected award clearly beats banking. Once its multiplier is capped at ×6, it has no more streak upside, so it banks.

### Difficulty curve

The one number the CPU carries per opponent is how often it plays its best move; below that it
blunders on purpose. That number climbs with the opponent and jumps with the difficulty, and the
jump is the bigger of the two - **Easy against opponent 12 is still weaker than Master against
opponent 01**. Which is why the chart is one column per opponent, not one rising curve.

Deus is not the thirteenth rung but a mode of its own: 100% in every band, with Yakuza rules, a
fixed 15-second timer and the mono atlas. The timer and the ruleset stay off the axis, because the
timer is your clock and the CPU scores with whichever ruleset you chose - neither moves his
accuracy. See [Errata & Known Issues](../errata.md) for what the rulesets do change.

<!-- BEGIN GENERATED: difficulty-curve -->

![Chart: chance of an optimal CPU move for each opponent on Easy, Hard and Master, with Deus above them all](../assets/difficulty-curve.svg)

| Opponent | Easy | Hard | Master |
|---|---:|---:|---:|
| 01 | 20% | 50% | 78% |
| 02 | 23% | 52% | 80% |
| 03 | 27% | 55% | 82% |
| 04 | 30% | 58% | 84% |
| 05 | 34% | 60% | 86% |
| 06 | 38% | 63% | 88% |
| 07 | 41% | 66% | 90% |
| 08 | 45% | 69% | 92% |
| 09 | 49% | 71% | 94% |
| 10 | 52% | 74% | 96% |
| 11 | 56% | 77% | 98% |
| 12 | 60% | 80% | 100% |
| **13 · Deus** | 100% | 100% | 100% |
| *Ramp, 01 to 12* | +40 | +30 | +22 |
<!-- END GENERATED: difficulty-curve -->

## Campaign and unlocks

- **Opponents 1 to 12** unlock in order. The next opponent unlocks when you win a match against the newest unlocked opponent with enough rounds for the difficulty you played.
- **Clearance rounds:**

  | Difficulty | Rounds needed to unlock the next opponent |
  |---|---:|
  | Easy | 9 or more |
  | Hard | 6 or more |
  | Master | 3 or more |

- A win with fewer rounds still counts as a match win, but it does **not** unlock the next opponent. The results screen tells you so.
- **Deus Supremus** normally requires all twelve opponents to be cleared. The maintainer currently keeps it **open to everyone on purpose**, while the mode is being tested. See [Errata & Known Issues](../errata.md).

## Rounds

Choose 1 to 12 rounds in Options. The default is set in the code, and the value is saved with your settings. See [Errata & Known Issues](../errata.md) for a note about the default.

## Options

These settings are saved in `settings.txt` next to the game:

| Setting | Options |
|---|---|
| Rounds | 1 to 12 |
| Difficulty | Easy, Hard, Master, Deus |
| Ruleset | Nintendo, FudaWiki, KK-ADD-10, Tournament, Yakuza (locked to Yakuza on Deus) |
| Background | Seven selectable backgrounds: Harvest, Hole, Arctic, Castle, City, Face, Realm (Deus has its own locked background) |
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
- A record can only be made while the match is played. Existing log entries from before this feature, matches that were never finished (restarted, abandoned or closed), and Deus matches lost on the turn timer have no record. A replay never counts towards statistics, streaks or unlocks, and the game never changes a record.
- While a replay runs, the status line shows `REPLAY - P TO PAUSE   ESC TO STOP`, or `REPLAY PAUSED` once it is frozen. **Esc** leaves the replay and returns to the Stats screen, and so do the **STOP** button - the lower of the two board buttons, which reads **RESTART** in a live match - and **STOP REPLAY** in the pause overlay. **P** or the **PAUSE** button freezes the board; the overlay then offers **RESUME** and **STOP REPLAY** rather than the match actions, because a replay cannot be restarted or abandoned.
- The choice, decision and round-result windows are not shown during a replay - the board, the capture piles and the combo panels are.
- A replay is watched, not skimmed: every round is dealt out from the stock pile with the round-start banner, and every hand card and stock card flies to the table before the engine takes the step. The flight is a little slower than a live turn and is followed by a beat to read the board, and it follows the animation speed set in Options. The engine step itself waits for the flight to land, so the board never gets ahead of what is drawn.
- A record with an unsupported replay-format version is refused with a short message. If a later engine change makes a valid record diverge, playback stops with a short message; incompatible replay changes must increment the format version.
- **The browser build has no replay buttons.** It cannot read or write files next to the game, so the Stats screen there shows the log list without the replay controls and without selection.
