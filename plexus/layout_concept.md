# ATRIUM PLEX — Screen Layout & Flow Concept (`layout_concept.md`)

> **Status:** Open-Ended Draft Concept (Nothing Set in Stone)  
> **Background Canvas:** `assets/plexus.png` (`1280×720`)  
> **Visual Layout Reference:** [`plexus/plexus_mockup.png`](plexus_mockup.png)  
> **Companion Files:** [`plexus/roadmap_plexus.md`](roadmap_plexus.md) · [`plexus/story.md`](story.md) · [`plexus/training_games.md`](training_games.md)

---

## 1. What the Atrium Plex Is *(The Lore Core)*

**ATRIUM PLEX** is a hidden biomechanical research and neural-conditioning facility inside the space-horror world of **XENO / KOI**.

Its purpose is singular: studying a dormant, anomalous entity lurking deep within the system — **`DEUS SUPREMUS`** — and conditioning the player's reflexes, pattern recognition, and tactical instinct to eventually face and defeat it.

---

## 2. Screen Composition Over `assets/plexus.png`

- **Left Half (Unobstructed Artwork):**
  - The biomechanical profile on the left side of `assets/plexus.png` remains clean and unobstructed, preserving the dark atmospheric weight of the artwork.
- **Right Synaptic Column (The 4-Card Month Spine):**
  - Aligned directly down the glowing vertical spire on the right side sit the **4 cards of the currently selected Month**, stacked vertically from top to bottom.
  - **`▲` / `▼` Month Switcher Triangles** above and below the 4-card stack (styled in the player's active UI theme color `COLOR_PHOSPHOR`, just like the rest of the HUD) switch between **whole Months** (`Month 1` ↔ `Month 2`, etc.), never single cards.
- **Center-Right Briefing Panel + Start Button:**
  - Selecting any of the 4 cards on the vertical spine displays its **Task / Training Briefing** in the panel to the left of the card column (explaining that card's specific goal, rules, or anomaly).
  - Directly underneath the right side of the briefing panel sits the **`START`** button to launch the selected card's training trial.

---

## 3. How Monthly Progression Works

- **Free Order Within a Month:**
  - Each of the 4 cards in the active Month has its own training goal.
  - The player can play and clear the 4 cards of the current Month in **any order**.
- **Month Gate:**
  - All 4 cards of the current Month must be finished before the next Month unlocks on the `▲` / `▼` selector.
- **Story Trigger (Plays Automatically Once, Re-Viewable in Logfile):**
  - **First-ever entry** into the Atrium Plex plays the **Introduction** (`Sequence 00`).
  - **Finishing all 4 cards of a Month** triggers the next piece of the weird, non-linear story/art sequence and unlocks the next Month's 4 cards.
  - Unlocked Months can be switched back and forth freely with `▲` / `▼`, while each story sequence plays automatically **only once** and is stored in the **Logfile** for re-viewing anytime.

---

## 4. Bottom Bar Controls *(Left -> Right)*

1. **`SAVE & BACK`:**
   - Saves all progress and triggers the full loading screen transition back to the Main Menu.
2. **`LOGFILE` *(Middle-Screen Window — [`plexus/plexus_logfile_mockup.png`](plexus_logfile_mockup.png))*:**
   - Clicking `LOGFILE` opens a central window (`244, 45, 740, 557`) divided into three regions:
     - **Top Story / Last Event Log (`y = 98..404`):** Records the information last given to the player, story sequences, and node events, with theme-colored `▲` / `▼` scroll arrows (and mouse-wheel support) to scroll back through earlier entries.
     - **Bottom-Left Stats Panel (`265, 431, 423, 150`):** Displays telemetry drawn from existing systems — average speedbar drop time, card drop overlap accuracy ("how close"), Speedcoin minipoints & best perfect streak, action-card audio effect synergy (`reverb_synergy` peak & `100%` hold duration), plus archived card count and match records.
     - **Bottom-Right 48-Card Atlas Map (`703, 431, 255, 151`):** Renders all 48 cards from `assets/atlas.png` (`12 × 4`); finished cards are highlighted at full brightness with a `COLOR_PHOSPHOR` border while unfinished cards remain dimmed.
3. **`TRAINING 1` / `TRAINING 2` / `TRAINING 3` *(Unlockable Minigame Slots)*:**
   - Over time, as the player successfully completes new training/minigame types inside the 4-card monthly progression, those minigames unlock as permanent buttons along the bottom bar so they can be chosen directly for free-play sparring anytime.
