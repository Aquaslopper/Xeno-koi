# Modes, Difficulty & Settings

## Difficulty

| Difficulty | What it changes |
|---|---|
| **Easy** (default) | The CPU is weakest, and it calls Koi-Koi at random (50% of the time while it has 3 or more cards). Hand cards that can capture glow, and while you drag a card, field cards of the same month glow. |
| **Hard** | A stronger CPU. It needs at least 4 cards in hand before it will gamble on Koi-Koi, and it banks when you are already on the board. No glow hints. |
| **Master** | A strong CPU. It needs at least 3 cards in hand before gambling, and it banks instead of continuing when a card would complete one of your special combinations. A **turn timer** applies: 15 or 30 seconds, set in Options. If it runs out, a random card is played for you. |
| **Deus** | The 100% accuracy setting, against Entity 13. It uses the Master decision logic with stronger card evaluation, always plays under Yakuza rules, locks the ruleset selector, and has a fixed 15-second turn timer. |

The CPU's Koi-Koi decision compares the **banked award now** with the **projected award** if it continues. It continues only when the projected award clearly beats banking. Once its multiplier is capped at ×6, it has no more streak upside, so it banks.

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
