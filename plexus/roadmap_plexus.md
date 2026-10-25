# ATRIUM PLEX — Beta Roadmap (`roadmap_plexus.md`)

> **Status:** Active Beta — Hub & Foundation Phase
> **Canvas:** `assets/plexus.png` + `assets/plexus_normal.png` (`1280×720`)
> **Visual References:** [`plexus/plexus_mockup.png`](plexus_mockup.png) · [`plexus/plexus_logfile_mockup.png`](plexus_logfile_mockup.png)

---

## Core Engineering Rules (Non-Negotiable)

1. **Build With Existing Systems — Never Duplicate the Table:**
   - Every playable tutorial or minigame launched from Atrium Plex runs on the **real game engine and table** (`engine.c`, `rules.c`, `ui_board.c`, `ui_input.c`, `ui_speed.c`, `ui_audio.c`, `ui_screens.c`, and `SCREEN_GAME` in `ui_main.c`).
   - Never clone or rewrite a second board, input loop, or scoring system inside `ui_plexus.c`.
   - Core game behavior and protected mechanics (`src/rules.*`, `src/engine.*`, `src/cards.*`, `src/campaign.*`, `src/ai.*`, `src/storage.*`, `src/ui_screens.c`) stay untouched unless explicitly approved.
2. **48-Card Spine Only (`12 Months × 4 Cards`):**
   - Progression follows the 48 Hanafuda cards (`0..47`), 4 cards stacked per Month on the right-hand spine (`slot 3` bottom -> `slot 0` top). No `▲` / `▼` month browsing arrows and no going backward.
3. **Face-Down Lock, Sequential Face-Up Unlock & Irreversible `[NEXT]` Swing-In:**
   - Each month starts with the bottom card (`slot 3`) **face up** (unlocked) and the upper 3 cards (`slots 2..0`) **face down** (`assets/back.png`, locked).
   - Completing a face-up card illuminates it in the `LOGFILE` 48-card Atlas and **turns the next card above it face up** (unlocking it with a turn-around flip animation); unlocked cards in the active month remain face up without individual re-locking.
   - Once **all 4 cards of the current month are completed** (including the top card `slot 0`), a **`[NEXT]`** button appears above the top card (`{1072, 54, 96, 34}`); clicking **`[NEXT]`** swings in the next Month irreversibly (`slot 3` face up, `slots 2..0` face down), sealing prior months so special minigames cannot be endlessly replayed before their bottom-bar milestones (`Month 04` / `Month 08`).
   - **Beta Placeholder Behavior:** Cards that do not have a game built yet complete immediately when `START` is clicked so bottom-to-top card flips and `[NEXT]` month progression work during beta testing.

---

## Phase 1 — Hub Cleanup, `plexsave.txt` & Bottom Bar States *(First Priority)*

- [x] **1. Dedicated Persistence (`plexsave.txt`):**
  - Save and load Atrium Plex state in `plexsave.txt` directly from `src/ui_plexus.c` using the atomic `.tmp` + `rename()` pattern (leaving protected `src/storage.*` untouched).
  - Add `'plexsave.txt'` to `web/shell.html`'s `localStorage` file list so desktop and Web/WASM builds both persist on `SAVE & BACK`, `ESC`, and Main Menu `SAVE`.
  - Persist: `finished_cards_mask` (48-bit), selected month/slot, Plexus telemetry counters (speedbar drops, time sum, overlap sum, best streak, lifetime minipoints, audio triggers/peak/hold), and logfile progress.
- [x] **2. Bottom Bar Controls (`SAVE & BACK` | `LOGFILE` | `SPEEDCOIN` | `ACTION CARD` | `TRAINING`):**
  - **Locked Minigame Buttons (`SPEEDCOIN`, `ACTION CARD`):** Rendered in a smooth, non-flashy dark-wine / muted crimson locked state (`draw_plexus_locked_btn()`) until unlocked.
  - **`TRAINING` Normal Non-Campaign Match:** Clicking `TRAINING` displays the top briefing panel with placeholder info and the `START` button underneath, which starts a normal match using Option menu settings with no campaign awards (no clearance coins, sector unlocks, or campaign banners).
- [ ] **3. Spine Briefings & Month 01 Tutorial Spine:**
  - Month 01 (`January // Pine`): 4-card tutorial spine (`slot 3` bottom `01-P2 Plain` Standard Rules of Koi-Koi playable & replayable; `slot 2` second plain `01-P1 Plain` Speedcoin tutorial; `slot 1` Action Card Audio tutorial; `slot 0` Final Verification Match).
  - Month 02–12 remain `[PLACEHOLDER]` beta lock-on-start cards until built.

---

## Phase 2 — Clean Rebuild of Month 01 Tutorial on the Real Table

- [x] **Use `SCREEN_GAME` Directly Without Side Effects:**
  - Drive a real 1-round `SCREEN_GAME` deal on `RULESET_KK_ADD_10` and `DIFFICULTY_EASY` without copying table rendering or input handling.
  - Isolate tutorial runs from `settings.difficulty` (`MASTER`/`DEUS` turn timers and Deus setting overrides), Campaign stats/coins, replay recording, and Speedbar/Audio FX telemetry while leaving user `settings` and `DEUS` untouched.
- [x] **Fix UI Coordinates & Completion Guards:**
  - Match real `ui_screens.c` Decide hitboxes (`{280, 520, 320, 60}` and `{680, 520, 320, 60}`) and real `ui_board.c` left capture-lane bounds (`{9, 505, 261, 206}` around `x = 12, w = 255`).
  - Correct inaccurate or overflowing rules/button text inside the 5 existing pop-up panels (4 cards per monthly suit sharing the same plant, `KOI-KOI (CONTINUE)` / `SHOBU (BANK & END)` labels) and only award completion when the tutorial round finishes.

---

## Phase 3 — Speedcoin Minigame *(Atrium-Only Solo Ruleset on `RULESET_KK_ADD_10`)*

- [x] **1. Groundwork Rule Specification (`plexus/speedcoin_rule.md`):**
  - Solo highscore survival (`180s` countdown, uncapped `+10s` per `10` wave Speedcoins, `-10s` on bar drain or no-match table placement), `14`-card (`2×7`) field + `8`-card hand + `26`-card stock Waves, hidden `30`-Speedcoin per-Wave counter with continuous glowing/moving-dot Stock deck indicator for `Hand 2`, no CPU / no `KOI-KOI` screen, and a dedicated Speedcoin Ranklist + Logfile-stats final screen.
- [x] **2. Build Groundwork & Open `SPEEDCOIN` Button for Testing:**
  - Implement Speedcoin on the real `SCREEN_GAME` / `ui_speed.c` / `ui_board.c` / `ui_combos.c` / `ui_plexus.c` pipeline without touching protected core files.
  - Temporarily unlock the bottom-bar `SPEEDCOIN` button (`SPEEDCOIN_UNLOCKED_FOR_TESTING = 1` in `src/ui_plexus.c`) so the mode can be launched and tested directly from Atrium Plex.
- [x] **3. Playtesting & Fine-Tuning:**
  - Verified and locked in gameplay pace, `+10s / -10s` clock balance, frozen between-Wave round score screen, pre-Wave `3 -> 2 -> 1 -> START` countdown with full-width `// WAVE x //` header & left-to-right hand flip at `1`, `30`-MP `Hand 2` deck-glow indicator, `3`-miss-hit Reverse Card Deck anti-farming (`1`-match reset), and `3s` `GAME OVER` -> fade-to-black -> Highscore Ranklist transition.
- [x] **4. Full Atrium Plex Spine Integration *(Completed — `SPEEDCOIN_UNLOCKED_FOR_TESTING = 1` Kept Open Per Maintainer Directive)*:**
  - [x] **4a. Speedcoin & Atrium Plex Stats Integration:** Wire per-run and career Speedcoin / Speedbar / Audio FX / Archive telemetry end-to-end across `SPEEDCOIN` briefing, `LOGFILE`, the Speedcoin Result Screen, and `plexsave.txt` (including restart/abandon/quit persistence) while keeping `SPEEDCOIN_UNLOCKED_FOR_TESTING = 1`.
  - [x] **4b. Speedcoin Tutorial Game on Month 01 Second Plain Card (`slot 2`, `01-P1 Plain`):** Build the interactive Speedcoin tutorial on the second plain card directly above the `slot 3` Koi-Koi tutorial card on Month 01's spine.
  - [x] **4c. Spine Unlock Gating:** Sequential bottom-to-top face-up spine unlocks and Month 04 milestone gating wired (`SPEEDCOIN_UNLOCKED_FOR_TESTING = 1` intentionally kept open during Phase 4).

---

## Phase 4 — Action Card Minigame & Remaining Hub Refinements

- [x] **1. Action Card Minigame (`plexus/action_card_rule.md` — Dedicated `KK-ADD-10` Table + Dual Audio Meters -> Human-Ear Fine-Tune -> Gameplay Rules -> Spine Integration):**
  - [x] **1a. Open `ACTION CARD` Button & Dedicated `KK-ADD-10` Table with Dual Vertical Meters:** Unlock `ACTION CARD` (`ACTION_CARD_UNLOCKED_FOR_TESTING = 1` in `src/ui_plexus.c`), launch a dedicated non-campaign `RULESET_KK_ADD_10` Action Card vs-CPU table, and render both the left rhythm-reactive music bar (`{414, 170, 14, 264}`, active theme color, independent of Options FX sliders, shifting to swing across the `90%` threshold mark as soon as the right bar rises above its third `75%` line) and the right soft-wine-red added-energy bar (`{1082, 170, 14, 264}`, 3 horizontal lines at `25%`/`50%`/`75%` with an enlarged `50%` middle trigger line, driving table-wide panel dot glow + dot speed, plus an upward-counting ceiling counter above the bar that ticks exclusively while pinned to the ceiling and pauses during drain; `src/ui_audio.c` untouched).
  - [x] **1b. Card/Deck Distraction Glow, Left-Meter Beat Symbols & Middle-Line (`< 50%`) Symbol Effects:** Extend the right-hand meter's 3-line perimeter border & flare-dot glow (`25%`/`50%`/`75%`, without washing out card faces; crisp `<= 1.05px` bright perimeter pixels) onto all field cards, both hands, and the stock deck as a deliberate visual distraction; display a `32x32` cycling symbol badge above the left-hand meter (`cx = 421, cy = 148`) and `24px` ceiling counter above the right-hand meter (`cx = 1089, y = 140`); activate the beat-chosen symbol once as soon as the right-hand meter drops below the enlarged middle horizontal line (`< 50%`) (**Skull**, **Hand**, **Reverse Arrow**, **One Eye**, and **Gold Coin** with enlarging/flashing directional vaporize VFX toward the recipient); and display Speedcoin & Action Card time numbers on per-round and match-end screens whenever `> 0`.
  - [x] **1c. Action Card Atrium Plex Integration (`ACTION_CARD_UNLOCKED_FOR_TESTING = 1` Kept Open Per Maintainer Directive):**
    - [x] **Telemetry, `KK-ADD-10` Lock & Options / Deus Deck Integration:** Link Action Card matches, W-L, best score, symbols fired, and claimed time numbers alongside Speedbar & Audio FX telemetry across `ACTION CARD` briefing, `LOGFILE`, and `plexsave.txt`; always enforce `RULESET_KK_ADD_10` when launched from the bottom bar while honoring Options menu Rounds and Difficulty (`EASY`, `HARD`, `MASTER` 15s/30s timer, and `DEUS` AI + 15s sudden-death timer + `atlas_deus` deck); keep `ACTION_CARD_UNLOCKED_FOR_TESTING = 1`.
    - [x] **Month 01 `slot 1` Tutorial & Spine Gating:** Build the Month 01 `slot 1` (`01-RP Poetry`) 3-round Easy Action Card tutorial card (freeze-on-start twin bio-meter & Five Horrors briefings with live meter highlights + `32x32` symbol badges, unlocking `slot 0` `01-B Crane` upon completing all 3 rounds) alongside Month 08 milestone gating (`ACTION_CARD_UNLOCKED_FOR_TESTING = 1` kept open).
- [x] **`TRAINING` Non-Campaign Match Mode:**
  - Launches a normal match from Option menu settings via `k_briefing_rect` + `k_start_rect` without Campaign awards.
- [x] **Plexus Telemetry & Highscore Ranklists (`plexsave.txt`):**
  - Persist Speedcoin ranklist and Action Card match/symbol/time stats alongside Speedbar and Audio FX telemetry in `plexsave.txt`.

---

## Phase 5 — Atrium Plex Story *(Later)*

- [ ] **Deferred Until Gameplay & Hub Are Solid:**
  - Introduce the non-linear biomechanical story sequences (`Sequence 00` intro + monthly fragments archived in `LOGFILE`) after Phases 1–4 are verified.
