# ATRIUM PLEX — Speedcoin Ruleset Specification (`speedcoin_rule.md`)

> **Status:** Finalized Groundwork Specification
> **Ruleset Scope:** Hidden Atrium-Plex-only mode running on `RULESET_KK_ADD_10` (never selectable in the Option menu)
> **Engine Basis:** Existing `src/engine.c` · `src/rules.c` (`KK-ADD-10`) · `src/ui_speed.c` · `src/ui_input.c` · `src/ui_board.c` · `src/board_layout.c` · `src/ui_combos.c` · `src/ui_main.c` · `src/ui_plexus.c`

---

## 1. Core Concept

1. **Single-Player Highscore Mode (No CPU):** Speedcoin is pure solo play against a `180`-second countdown clock. The goal is to score as many points as fast as possible across as many Waves (rounds) as the player can keep the clock alive.
2. **Atrium-Plex Exclusive Ruleset:** Speedcoin runs under **`RULESET_KK_ADD_10`**, **cannot** be selected in the Option menu (`0..4` public rulesets remain unchanged), and can **only** be launched inside **Atrium Plex** once unlocked.
3. **Zero Changes to Existing Input / Engine Mechanics:**
   - Card matching, `PHASE_HAND_CHOICE` (if it opens on a 2-match drop, the player simply wasn't accurate enough on a single target — leave completely untouched), stock flips, `KK-ADD-10` combo scoring (`score_captures` + `calculate_award`), and `ui_speed.c` Speedbar grading (`1.00 s` bar, `drop_cover()` overlap accuracy, `0..10` Speedcoins, and Perfect Serie `+10` bonuses) all remain **100% as they are**.
   - **No `KOI-KOI / SHOBU` screen:** `PHASE_DECIDE` never interrupts play; combos build automatically in `YOUR COMBOS` and bank when the Wave ends.

---

## 2. Table Layout Changes (`1280 × 720`, Solo Configuration)

Everything on the table stays in its standard position **except** CPU elements are removed and the top-left space becomes the Countdown Clock:

| Table Area | Standard Match | Speedcoin Mode |
| :--- | :--- | :--- |
| **Top Center (`y = 12`)** | `8` CPU hand cards | **Removed** (`hand_count[1] = 0`) |
| **Top-Left (`x = 12, w = 255`)** | `CPU SCORE` `{12, 12, 255, 30}` + 4 CPU capture lanes (`y = 46..211`) | **Replaced by big Countdown Timer panel** (`{12, 12, 255, 199}`), starting at **`180` seconds** |
| **Top-Right (`{1160, 88, 112, 218}`)** | `CPU COMBOS` panel | **Removed** |
| **Mode Box (`{1160, 12, 112, 66}`)** | `RND x/y` + Ruleset | **`WAVE x`** + **`SPEEDCOIN`** |
| **Center Field (`{430, 132, 650, 400}`)** | `8` initial cards (`2 × 4`) | **`14` initial cards (`2 rows × 7 cards` via `board_field_slot_xy`)** |
| **Stock Pile (`{330, 262}`)** | `24` cards at start | **`26` cards** at Wave start (`48 - 14 - 8`); **glows continuously with moving perimeter dots** once the Wave's `50`-Speedcoin Second Hand threshold is reached |
| **Player Hand (`y = 566`)** | `8` cards | **`8` cards** (`Hand 1`, plus optional `8`-card `Hand 2`) |
| **Bottom-Left (`x = 12, y = 508..708`)** | 4 Player capture lanes + `YOU SCORE` | **Unchanged** |
| **Right Sidebar (`x = 1160, y = 316..702`)** | `YOUR COMBOS`, `PAUSE`, `RESTART`, `Speedbar` `{1160, 662, 112, 40}` | **Unchanged** |

- **No Dealer Select (`PHASE_DEALER_SELECT`):** Waves deal immediately with `current_player = 0`.
- **No Boss Taunts:** `[ CPU TRANSMISSION ]` is disabled.

---

## 3. Wave & 48-Card Deck Flow

Each **Wave** behaves like a round in a match, repeating Wave after Wave (`Wave 1, Wave 2, ... Wave 5, Wave 10, ...`) until the countdown timer reaches `0`.

### 3.1 Wave Deal (`48` Cards Total)
- Shuffle all `48` cards (`0..47`):
  - **Table (Field):** **`14` cards** (`2` rows of `7` cards; redeal if all `4` cards of a single month land on the initial field).
  - **Player Hand (`Hand 1`):** **`8` cards** (`hand_count[0] = 8`, `hand_count[1] = 0`).
  - **Deck (`Stock`):** **`26` cards** (`stock_count = 26`).

### 3.2 Turn Loop (Repeat per Card)
1. **Player Turn (`PHASE_TURN_HAND`):** Once animations settle (`input_unlocked`), the `1.00 s` Speedbar starts draining.
2. **Player Plays 1 Card from Hand:**
   - Standard `ui_input.c` + `engine.c` + `ui_speed.c` resolution runs unchanged (including `PHASE_HAND_CHOICE` if the player drops inaccurately when `2` matches exist).
   - If the player places an unmatched card on the table (`0` matches), the card stays on the table and the Speedbar runs empty -> **`-10 seconds`** penalty (see §4).
3. **Deck Flips 1 Card to Table (`PHASE_TURN_DRAW`):**
   - `engine_reveal_stock()` flips `1` card from Stock onto the table, resolving `0 / 1 / 2 / 3` matches normally (not graded by Speedbar, never breaks Perfect Serie).
4. **Next Player Turn:** `current_player` stays `0`, `PHASE_TURN_HAND` resumes, and the Speedbar arms fresh (`1.00 s`).

### 3.3 Hidden Per-Wave `50`-Speedcoin Counter & Glowing Deck Indicator (`Hand 2`)
- **Hidden Background Math (Zero HUD Clutter):**
  - Total gold Speedcoins (`s_total` in `ui_speed.c`) carry over on screen across the whole run as usual.
  - **Each Wave tracks its own Speedcoins purely in the background** (`wave_coins = s_total - wave_start_coins`, reset to `0` at the start of every new Wave) — **no extra counters, progress bars, or numbers are drawn on screen**. Ending Wave 1 with `90` Speedcoins does **not** let `10` Speedcoins in Wave 2 trigger `Hand 2`; every Wave requires **`50` fresh Speedcoins earned in that Wave** (`wave_coins >= 50`).
- **Continuous Deck Glow + Moving Dots Indicator (Only Visual Cue):**
  - As soon as hidden `wave_coins >= 50` during `Hand 1`, the **Stock / Card Deck (`{330, 262, 76, 124}`)** lights up with a **continuous glow and moving perimeter dots** (`board_draw_card_glow` + `ui_theme_set_panel_flare_glow` + `ui_theme_draw_panel_flares`), visually signaling that the Second Hand is unlocked without overloading the screen.
  - Once `Hand 2` is dealt from the deck (or a new Wave begins), the deck's Second-Hand glow turns off.
- **End of `Hand 1` (`8` Cards Played, `18` Cards Left in Stock):**
  - **If `wave_coins >= 50`:** Deal **`8` new cards** from the `18`-card Stock into `hand[0]` (`10` cards remain in Stock). Play continues through all `8` cards of `Hand 2` + `8` Stock flips (`10 - 8 =` **`2` cards** left in Stock when `Hand 2` empties — a **3rd hand can never exist**).
  - **If `wave_coins < 50` (or after `Hand 2` empties):** The Wave ends, banks the Wave's `KK-ADD-10` combo award (`score_captures` + `calculate_award(base_points, 0, 0)`) into `YOU SCORE`, resets the per-Wave Speedcoin & `+10s` counters for the next Wave, and deals the **next Wave** as long as the countdown timer is `> 0`.

---

## 4. Countdown Timer (`180s`) & Per-Wave `±10s` Rules

- **Starting Time:** **`180.0` seconds**, displayed prominently inside the top-left panel (`{12, 12, 255, 199}`), with border/perimeter-dot flare feedback on `+10s` / `-10s` events.
- **No Upper Cap:** No hard ceiling — if fast play pushes the clock above `180.0` seconds, all earned time is kept.
- **When Time Ticks:** Counts down at `1×` real time during active gameplay (`SCREEN_GAME`, paused when `GAME PAUSED` or combo detail overlay is open).

### 4.1 `-10 Seconds` Penalties
1. **Speedbar Runs Out (`s_remaining` hits `0.0s` while thinking):**
   - When the `1.00 s` Speedbar drains to empty before the player drops a card -> **`-10.0 seconds`** from the countdown clock (applied once for that turn; dropping that turn's card afterward does not apply a second `-10s` on the same turn).
2. **No-Match Card Placed on Table:**
   - When the player places a card with no matching month onto the table while the Speedbar still had time remaining, the Speedbar immediately runs empty (`speed_bar_consume()`) -> **`-10.0 seconds`** from the countdown clock (and resets any active Perfect Serie).

### 4.2 `+10 Seconds` Bonus (Hidden Per-Wave Counter)
- Using the **same hidden per-Wave Speedcoin math (`wave_coins = s_total - wave_start_coins`, reset to `0` each Wave, with no extra HUD counter)** so leftover partial coins from a previous Wave never carry a cheap trigger into the next Wave:
  - Every **`10` Speedcoins earned within the current Wave** (`wave_coins` crossing `10, 20, 30, 40, ...` in that Wave, including Perfect Serie `+10` bonuses) silently triggers **`+10.0 seconds`** on the top-left countdown clock (`+10s` per `10`-coin step crossed in that Wave).

---

## 5. Run End & Dedicated Speedcoin Final Screen

When the countdown timer reaches **`0.0` seconds** (from normal ticking or a `-10s` penalty, clamped at `0.0s`):
1. Finalize/bank any completed `KK-ADD-10` combos in the current Wave and apply `speed_bar_apply_match_bonus()` (`+1 PT` per `10` total Speedcoins earned across the run) into **`YOU SCORE`**.
2. Transition to a **dedicated Speedcoin Result Screen** (instead of the standard `YOU vs CPU` Campaign match-result ledger) displaying:
   - **Run Summary:** Final Score (`YOU SCORE`), Waves reached, and total Speedcoins (`MP` + converted `+PTS`),
   - **Speedcoin Ranklist:** Persistent local Top Highscores stored in `plexsave.txt` (highlighting the current run when placed),
   - **Logfile Speedbar Stats:** The live/run Speedbar telemetry (`AVG SPEED`, `HOW CLOSE` match overlap %, `SPEEDCOIN` MP, and `BEST xN` serie),
   - **Controls:** `PLAY AGAIN` (restart Speedcoin immediately) and `BACK` / `ATRIUM PLEX` (return to `SCREEN_ATRIUM`).
3. **Campaign Isolation:** Main Menu Campaign files (`stats.txt`, `history.txt`, `stats_report.txt`, `replays/`, `cleared_mask`, `unlocked_opponents`) remain untouched; all Speedcoin ranklist and telemetry data saves in `plexsave.txt`.

---

## 6. Reference: Unchanged Speedbar Math (`src/ui_speed.c`)

- **Bar Window:** `1.00 s` (`<= 0.50 s` elapsed = `100%` speed `s = 1.0`; `0.50 s .. 1.00 s` decays linearly to `0.0`).
- **Overlap Accuracy:** `drop_cover()` (`>= 80%` overlap = `100%` accuracy `a = 1.0`; `< 80%` scales linearly `cover / 0.80`).
- **Catch Speedcoins:** `mp = round(10 * s * a)` (`0 .. 10`).
- **Perfect Serie (`mp == 10`):** `1st` = `+10` (`10`), `2nd` = `+30` (`40`), `3rd+` = `+20` each (`60, 80, ...`); resets on `< 10` catch, plain discard, bar empty, or Wave boundary.
