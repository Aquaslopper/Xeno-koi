# ATRIUM PLEX — Open-Ended Brainstorm & Build Roadmap

> **Status:** Active Brainstorming & Concept Phase  
> **Incubator Folder:** `plexus/` (staged separately before production integration into `src/`)  
> **Visual Anchor:** `assets/plexus.png`  
> **Main Menu Entry:** `ATRIUM PLEX` (placed directly under `START MATCH`, entered via full loading transition)

---

## Core Vision

**ATRIUM PLEX** (*The Plexus*) is a biomechanical conditioning and sparring sanctuary inside **XENO / KOI** — not a static text tutorial or disconnected minigame list, but a living progression tree built around the **12 Months / 48 Cards** of the deck.

- **`START MATCH`** remains untouched as the primary Main Menu button.
- **`ATRIUM PLEX`** sits directly below `START MATCH` and transitions with the full loading screen into its own dedicated background (`assets/plexus.png`).
- Inside the Plexus, **tutorial, reflex training, solo sparring, deep stats/explanations, and long-term unlocks** (including the eventual true awakening of **`DEUS SUPREMUS`**) converge into one cohesive mode.
- Companion design notebooks: [`plexus/layout_concept.md`](layout_concept.md) (screen layout, 4-card spine & bottom bar flow), [`plexus/story.md`](story.md) (narrative & non-linear monthly art sequences), and [`plexus/training_games.md`](training_games.md) (training/minigame ideas & 48-card mapping).

---

## Step 1 — Name & Story / Lore *(Brainstorm in `plexus/story.md`)*

- [ ] Define the exact identity and non-linear art story of **ATRIUM PLEX** (*The Plexus*) in [`plexus/story.md`](story.md).
- [ ] Design **Sequence 00 (Introduction)** triggered the first time the player enters the Atrium Plex, plus the **12 Monthly Continuations** (`Months 01–12`).
- [ ] Connect the weird, non-linear visual/narrative arc from Month 1 to the apex seal of **`DEUS SUPREMUS`**.

---

## Step 2 — Visual Mockup & Menu Layout *(Mockup -> UI Brainstorm)*

- [ ] Review owner's visual mockup for the **ATRIUM PLEX** screen over `assets/plexus.png`.
- [ ] Map the `1280×720` layout while preserving visual breathing room for the artwork in `assets/plexus.png`:
  - Navigation controls (`BACK`, sub-views, node selection).
  - The progression tree structure (12 Months × 4 Cards + Monthly Capstone trials).
  - Detailed Stats & Codex/Explanation view inside the Plexus.

---

## Step 3 — Define the Training & Minigame Archetypes *(Brainstorm in `plexus/training_games.md`)*

- [ ] Define the core solo training mechanics (player vs. board, clock, and audio — no standard CPU turn loop):
  - **Interactive Tutorial Conditioning:** Learning suit matching, deck flips, captures, and yaku by playing hands-on scenarios.
  - **Speedbar Reflex Sparring:** 8 cards in hand vs. field cards with a draining speedbar — chaining fast matches to hold `reverb_synergy` at `100%` without hesitating too long.
  - **Additional Sparring Modes:** Yaku spotting/assembly, blind/memory reads, risk/Koi-Koi drills, and Monthly Boss/Capstone trials discovered during brainstorming.

---

## Step 4 — Map the 12 Months × 4 Cards Progression Tree *(Brainstorm in `plexus/training_games.md`)*

- [ ] **Month 1 (January — Pine, First 4 Cards + Top Trial):**
  - Card 1–4 interactive tutorial progression + Month 1 capstone trial at the top.
- [ ] **Months 2–12 (Cards 05–48 + Monthly Capstones):**
  - Define branching routes across the 12 months.
  - Assign training/minigame archetypes and modifiers to each month's 4 cards and top node.
  - Define progression rewards, detailed stat tracking, and the final **`DEUS SUPREMUS`** unlock condition.

---

## Step 5 — Step-by-Step Implementation *(Open-Ended, Scoped After Brainstorming)*

*Each build step below will be scoped inside `plexus/` and integrated into `src/` only after explicit owner approval:*

- [ ] **Step 5a — Main Menu Entry & Hub Shell:** Add `ATRIUM PLEX` under `START MATCH`, wire full loading screen transition, load `assets/plexus.png`, and build the initial screen shell.
- [ ] **Step 5b — Plexus Menu & Tree UI:** Implement the interactive node/tree interface from the approved mockup and persistent progress storage.
- [ ] **Step 5c — Solo Sparring Engine & Month 1 (Tutorial):** Build the solo training board loop (reusing board rendering, speedbar, and `reverb_synergy` audio response) and the first 4 tutorial cards + Month 1 top game.
- [ ] **Step 5d — Months 2–12, Detailed Stats Screen & Apex Unlocks:** Expand the remaining monthly branches, minigame variants, detailed stats/explanations view, and `DEUS SUPREMUS` progression hook.
