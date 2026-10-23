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
