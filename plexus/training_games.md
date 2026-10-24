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

### Archetype B — Speedcoin (`KK-ADD-10` Solo Reflex & Accuracy Minigame)
- **Introduction Text:**
  > **"Warning: Synaptic countdown active. You have 1000ms per move to extract Speedcoins through rapid, precise sequencing.** Slow execution degrades the yield; collect 50 units to synthesize 2nd hand.
  >
  > Complete as many waves as possible"
- **Unlock & Placement:**
  - **Month 1, Card 2 from bottom (`slot = 2`, `cid = 2`):** First playable introduction to Speedcoin during Month 1 conditioning.
  - **Bottom Bar `SPEEDCOIN` Tab:** Unlocks permanently after all 4 cards of **Month 4** (Months 01–04) are completed.
  - Recurring conditioning variant across later months.
- **Solo Table Layout (`1280×720`, No Opponent):**
  - **No upper CPU card row** and **no CPU side panels**.
  - **Top-Left:** Big red **`03:00`** (`180s`) synaptic countdown timer with large visual indicators for `-1s` drain damage and `+10s` time injections.
  - **Left Side (1 Panel Only):** Unified player captured-card inventory (`Bright`, `Animal`, `Ribbon`, `Plain`) for the active wave.
  - **Right Side (1 Panel Only):** Unified player HUD showing Wave #, Hand (`1/2` or `2/2`), Total Score, Current Wave Combo Points, `2nd Hand Synthesis` progress (`0..50` Speedcoins), and active `KK-ADD-10` combos.
  - **Center Field (`2 rows × 7 cards = 14 cards`):**
    - Deals 2 rows of 7 cards (`14` on the table) + `8` cards in the player's hand (`22` cards dealt, `26` remaining in the stock pile).
    - When either 7-card row is cleared completely, it auto-refills with `7` cards from the stock pile.
    - *(Saved Design Note for Later Months / Variants: **`3 rows × 6 cards = 18 cards`** on the table + `8` in hand = `26` dealt, `22` in stock (`6 + 6 + 8 + 2` unlucky buried cards) — keep in reserve for higher-month escalations.)*
- **Wave & 2-Hand Cap Loop (No Koi-Koi Deck Flip):**
  - Pure Hand → Table reflex matching (no automatic stock flip between moves; if a hand card has no match on the table, the player places it onto the table).
  - **Speedbar (`1000ms` per move):**
    - Dropping a hand card onto a matching table card within `1000ms` awards **`1..10` Speedcoins** based on drop speed × card overlap accuracy.
    - **Timer Damage (`-1s`):** Every time the `1000ms` bar drains completely (`1.0s` without taking a card), **`-1 second`** is subtracted from the 3-minute timer.
    - **Timer Bonus (`+10s`):** Every **10 Speedcoins** earned adds **`+10 seconds`** to the 3-minute timer.
  - **Synthesizing the 2nd Hand (`50` Speedcoins, Cap at 2 Hands per Wave):**
    - A wave is capped at **2 Hands maximum** (Hand 1 = `8` cards, optional Hand 2 = `8` cards).
    - If the player collects **`>= 50` Speedcoins** during Hand 1, playing the 8th card synthesizes **Hand 2 (`+8` cards from the deck)** inside the same wave so captured cards keep accumulating for high-point `KK-ADD-10` combos (leaving the final unreached deck cards as the "unlucky" buried cards of that wave).
    - If the player collects **`< 50` Speedcoins** in Hand 1, or once Hand 2 runs dry: **Wave Over** → the 3-minute timer freezes, `KK-ADD-10` combos are tallied and banked into Total Score, all 48 cards reshuffle, and the next Wave begins.

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

---

## 3. The 12-Month × 4-Card Progression Tree *(Brainstorm Matrix)*

*Each month contains its 4 canonical cards (`CARDS[0..47]`) plus 1 Capstone Trial at the top (`60` total nodes across the Plexus).*

| Month | Suit / Theme | Card 1 | Card 2 | Card 3 | Card 4 | Monthly Top Trial |
| :---: | :--- | :--- | :--- | :--- | :--- | :--- |
| **01** | **January (Pine)** | Tutorial 1 *(TBD)* | Tutorial 2 *(TBD)* | Tutorial 3 *(TBD)* | Tutorial 4 *(TBD)* | Month 1 Capstone *(TBD)* |
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
