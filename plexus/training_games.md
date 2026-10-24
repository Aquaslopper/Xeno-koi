# ATRIUM PLEX — Training & Minigame Ideas (`training_games.md`)

> **Purpose:** Living brainstorm notebook for all **ATRIUM PLEX** training modes, solo sparring drills, interactive tutorials, and the **12-Month × 4-Card + Monthly Capstone** progression tree.  
> **Companion Roadmap:** [`plexus/roadmap_plexus.md`](roadmap_plexus.md)

---

## 1. Core Training Philosophy

- **No Dry Textbooks — Learn & Train by Playing:** Every node on the Plexus tree puts cards in the player's hand on the live table. Even the Month 1 Tutorial is played hands-on.
- **Solo Conditioning (No CPU Opponent Turn Delay):** In Sparring/Training modes, the player acts directly against the board, the deck, the draining **Speedbar**, and the **Audio Synergy** envelope.
- **Audio & Reflex Feedback (`reverb_synergy`):**
  - Playing matches quickly builds and holds `reverb_synergy` at **100%**.
  - Hesitating too long lets the energy decay — pushing the player to recognize suits and combinations by pure instinct.

---

## 2. Brainstormed Training & Minigame Archetypes

### Archetype A — Interactive Tutorial Drills *(Month 1 Foundation)*
- **Goal:** Teach the core mechanics of Hanafuda / Koi-Koi step by step across the 4 cards of Month 1, crowned by a first live test.
- **Candidate Mechanics:**
  - *Node 1:* Suit / Month recognition — matching hand cards to field cards of the same month.
  - *Node 2:* The Deck Flip — playing from hand, then drawing from the stock pile and resolving double captures.
  - *Node 3:* Card Ranks & Values — Chaff (`Kasu`), Ribbons (`Tanzaku`), Animals/Seeds (`Tane`), and Brights (`Hikari`).
  - *Node 4:* Forming a Yaku & the `KOI-KOI` vs. `SHOBU` decision.
  - *Month 1 Top Trial:* First timed solo clear combining all 4 fundamentals.

### Archetype B — Speedbar Reflex Sparring *(Core Action Drill)*
> **SUPERSEDED by [SPEEDCOIN — Gameplay Rules](#2b-speedcoin-gameplay-rules) below.** The text in this entry (continuous bar, `reverb_synergy` goal, wave refills) is an early brainstorm and does **not** describe the approved game. It is kept only for history.

- **Setup:** No opponent. Field has open cards; player holds **8 cards** in hand.
- **Loop:**
  - The **Speedbar** runs down continuously.
  - Player must rapidly spot and play matching cards from hand onto the field.
  - Every fast capture refills time/momentum and pushes `reverb_synergy` up to **100%** (`reverb_hold` + smooth decay).
  - Thinking slightly too long drops the audio synergy and drains the bar, pressuring immediate visual recognition.
- **Possible Escalations (Later Months):**
  - Wave refills (new field cards spawn as pairs are cleared).
  - Decoy/dead cards on the field that must be ignored.
  - Sustaining `reverb_synergy >= 80%` for an entire wave to pass.

### Archetype C — Yaku Assembly & Route Spotting *(Tactical Puzzles)*
- **Setup:** Pre-arranged or semi-random board + hand with a limited number of turns or timer.
- **Goal:** Capture the exact cards needed to complete a target Yaku (*Akatan*, *Aotan*, *Ino-Shika-Cho*, *Sankou*, *Tane*, *Kasu*, etc.) before turns or time run out.

### Archetype D — Blind / Memory Conditioning *(Synaptic Recall)*
- **Setup:** Field cards are revealed briefly at the start of the wave, then flipped face-down (`back.png`).
- **Goal:** Match hand cards to the remembered positions on the field before the speedbar expires.

### Archetype E — Risk / Koi-Koi Edge Drills *(Decision Conditioning)*
- **Setup:** Mid-round board state where the player has just completed a small Yaku.
- **Goal:** Read the remaining unseen cards and opponent threat danger to make the optimal `KOI-KOI` vs. `SHOBU` call, or push a combo to a target multiplier within strict constraints.

### Archetype F — Monthly Capstone Trials *(Top of Each Month)*
- **Setup:** A special signature challenge at the apex of each 4-card monthly branch that fuses the skills trained in that month's 4 cards.

## 2b. SPEEDCOIN — Gameplay Rules

> **Status:** Approved game rules, not implemented yet. This section contains **only** the gameplay rules and is kept up to date as the rules are decided.
> SPEEDCOIN is the normal Koi-Koi table played solo, against a clock. It uses the existing table, cards, input and speed bar, the same way the Month 1 tutorial does.
> **Ruleset:** `KK-ADD-10` only, because of its high combo rate.
> **What the game is about:** making combos. Speedcoins earn a 2nd hand, and the 2nd hand makes bigger combos possible.

### Briefing text (shown before the game starts)

> "Warning: Synaptic countdown active. You have 1000ms per move to extract Speedcoins through rapid, precise sequencing. Slow execution degrades the yield; collect 50 units to synthesize 2nd hand.
>
> Complete as many waves as possible"

### Where it launches

- **Atrium Plex, Month 1, 2nd card from the bottom of the spine** (`slot = 2`, `cid = 2`; above the tutorial).
- **Bottom-bar `SPEEDCOIN` button**, which unlocks after Months 1–4 are completed.

### The table

1. **No AI.** There is no CPU opponent and no CPU turn.
2. **All AI parts are removed** from this table.
3. **Not needed on this table:** the AI's hand at the top, the AI's left-side panel and the AI's right-side panel (`CPU COMBOS`).
4. **The freed space is not used for more cards.** The table is **two rows of `7` cards** (`14` cards).
5. **Deal per wave:** `14` cards on the table + `8` cards in the player's hand = `22` dealt, `26` stay in the stock. The deal is plain random.
6. **Row refill:** when a row of the table is completely empty, it is refilled automatically with `7` cards from the stock, if cards remain.
7. **No deck card is turned over after a move.** The table only changes through the player's own plays and the row refill.

### The clock

| Clock | Rule |
| :--- | :--- |
| **Big red `03:00` (`180 s`)** | Top-left, where the CPU score and captures normally sit. Counts down while playing. Shows a clear cue when time is added (`+10s`) or removed (`-10s`). Frozen between waves. |
| **Per-card bar (`1000 ms`)** | The existing speed bar. Runs `1000 ms → 0 ms` once per card. It does not loop while idle. |
| **`-10s`** | Each time the speed bar runs out before a card is played, `10 s` is removed from the clock, once. The bar stays at `0 ms` until the card is played. |
| **`+10s`** | Every `10` Speedcoins earned adds `10 s` to the clock (see Speedcoins). |
| **Next bar starts** | Only after the card has been played. |

### One move

1. The player plays one hand card onto the table. The speed bar grades the drop (speed and card overlap) and awards Speedcoins.
2. If the hand card matches a card on the table, both are captured. If it matches nothing, it is placed on the table.
3. The speed bar for the next card starts.

### Speedcoins

Speedcoins are a **statistic**, not money. Like experience points, they only go up. They are never spent and never taken away. When they reach certain points they trigger an event, like a level-up. The two triggers are separate from each other.

- **A perfect hit is worth `10` Speedcoins:** full accuracy, in the upper half of the speed bar.
- **Trigger 1, time:** every `10` Speedcoins earned adds `10 s` to the clock.
- **Trigger 2, 2nd hand:** reaching `50` Speedcoins within one wave gives that wave a 2nd hand.

### Waves

- **Each wave is a new round.** All `48` cards are shuffled and dealt again.
- **The hand runs dry after `8` turns.** If the player made **at least `50` Speedcoins in that wave**, a **2nd hand of `8` cards** is dealt from the stock and the wave goes on. If not, the wave is over.
- **At most one 2nd hand per wave**, because the deck cannot supply a third.
- Two counts are kept, and neither ever goes down:
  - **Match total:** every Speedcoin earned keeps adding up until the match is over, and is counted at the end of the match.
  - **Wave count:** the Speedcoins earned in the current wave only. It starts at `0` with every new wave. Only this count decides the 2nd hand.

### Wave end

A wave ends when the player's hand runs dry: after Hand 1 if the wave count did not reach `50`, or after Hand 2. Then:

1. The clock freezes.
2. The `KK-ADD-10` combos of the wave are counted and banked.
3. The next wave starts (a new round, wave count at `0`). The match total and the remaining time carry on.

### Game over

When the clock reaches zero, the game is over. It is a minigame. The goal is to complete as many waves as possible.

---

## 3. The 12-Month × 4-Card Progression Tree *(Brainstorm Matrix)*

*Each month contains its 4 canonical cards (`CARDS[0..47]`) plus 1 Capstone Trial at the top (`60` total nodes across the Plexus).*

| Month | Suit / Theme | Card 1 | Card 2 | Card 3 | Card 4 | Monthly Top Trial |
| :---: | :--- | :--- | :--- | :--- | :--- | :--- |
| **01** | **January (Pine)** | Tutorial 1 *(TBD)* | Tutorial 2 *(TBD)* | **SPEEDCOIN** *(see 2b)* | Tutorial 4 *(TBD)* | Month 1 Capstone *(TBD)* |
| **02** | **February (Plum)** | *TBD* | *TBD* | *TBD* | *TBD* | Month 2 Capstone *(TBD)* |
| **03** | **March (Cherry)** | *TBD* | *TBD* | *TBD* | *TBD* | Month 3 Capstone *(TBD)* |
| **04** | **April (Wisteria)** | *TBD* | *TBD* | *TBD* | *TBD* | Month 4 Capstone *(TBD)* |
| **05** | **May (Iris)** | *TBD* | *TBD* | *TBD* | *TBD* | Month 5 Capstone *(TBD)* |
| **06** | **June (Peony)** | *TBD* | *TBD* | *TBD* | *TBD* | Month 6 Capstone *(TBD)* |
| **07** | **July (Bush Clover)** | *TBD* | *TBD* | *TBD* | *TBD* | Month 7 Capstone *(TBD)* |
| **08** | **August (Susuki)** | *TBD* | *TBD* | *TBD* | *TBD* | Month 8 Capstone *(TBD)* |
| **09** | **September (Chrysanthemum)** | *TBD* | *TBD* | *TBD* | *TBD* | Month 9 Capstone *(TBD)* |
| **10** | **October (Maple)** | *TBD* | *TBD* | *TBD* | *TBD* | Month 10 Capstone *(TBD)* |
| **11** | **November (Willow)** | *TBD* | *TBD* | *TBD* | *TBD* | Month 11 Capstone *(TBD)* |
| **12** | **December (Paulownia)** | *TBD* | *TBD* | *TBD* | *TBD* | **Apex / `DEUS SUPREMUS` Seal** *(TBD)* |

---

## 4. Open Brainstorm Scratchpad *(Notes & New Ideas)*

- **Detailed Stats & Explanations Inside the Plexus:**
  - Deep breakdown of player tendencies, reaction speeds, yaku completion rates, ruleset/yaku explanations, and conditioning ranks.
- **Progression & Unlocks:**
  - How clearing months unlocks branching routes, dossier lore, and ultimately **`DEUS SUPREMUS`**.
