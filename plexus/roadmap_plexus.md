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
   - Progression follows the 48 Hanafuda cards (`0..47`), 4 cards stacked per Month switched via `▲` / `▼`. No extra capstone nodes or parallel trees.
3. **Completion & Face-Down Lock Rule:**
   - Completing a card turns it **face down** (`assets/back.png`) on the spine, lights it up in the `LOGFILE` 48-card Atlas grid, and **locks** it.
   - **Tutorial Exemption:** Month 01 Card 4 (`cid == 3`, bottom slot — Standard Rules of Koi-Koi) stays unlocked after completion so the player can replay the tutorial anytime.
   - **Beta Placeholder Behavior:** Cards that do not have a game built yet lock and flip face down immediately when `START` is clicked so monthly gating (`all 4 cards -> next Month`) works during beta testing.

---

## Phase 1 — Hub Cleanup, `plexsave.txt` & Bottom Bar States *(First Priority)*

- [x] **1. Dedicated Persistence (`plexsave.txt`):**
  - Save and load Atrium Plex state in `plexsave.txt` directly from `src/ui_plexus.c` using the atomic `.tmp` + `rename()` pattern (leaving protected `src/storage.*` untouched).
  - Add `'plexsave.txt'` to `web/shell.html`'s `localStorage` file list so desktop and Web/WASM builds both persist on `SAVE & BACK`, `ESC`, and Main Menu `SAVE`.
  - Persist: `finished_cards_mask` (48-bit), selected month/slot, Plexus telemetry counters (speedbar drops, time sum, overlap sum, best streak, lifetime minipoints, audio triggers/peak/hold), and logfile progress.
- [x] **2. Bottom Bar Controls (`SAVE & BACK` | `LOGFILE` | `SPEEDCOIN` | `ACTION CARD` | `TRAINING`):**
  - **Locked Minigame Buttons (`SPEEDCOIN`, `ACTION CARD`):** Rendered in a smooth, non-flashy dark-wine / muted crimson locked state (`draw_plexus_locked_btn()`) until unlocked.
  - **`TRAINING` Normal Non-Campaign Match:** Clicking `TRAINING` displays the top briefing panel with placeholder info and the `START` button underneath, which starts a normal match using Option menu settings with no campaign awards (no clearance coins, sector unlocks, or campaign banners).
- [ ] **3. Spine Briefings & Month 02 Alignment:**
  - Month 01 (`January // Pine`): Tutorial spine (Card 4 playable & replayable; Cards 1–3 beta lock-on-start until built).
  - Month 02 (`February // Plum`): Assign as the upcoming **Speedcoin** minigame month (`KK-ADD-10`), replacing outdated Month 04 gating assumptions once built.

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
  - Solo highscore survival (`180s` countdown, uncapped `+10s` per `10` wave Speedcoins, `-10s` on bar drain or no-match table placement), `14`-card (`2×7`) field + `8`-card hand + `26`-card stock Waves, hidden `50`-Speedcoin per-Wave counter with continuous glowing/moving-dot Stock deck indicator for `Hand 2`, no CPU / no `KOI-KOI` screen, and a dedicated Speedcoin Ranklist + Logfile-stats final screen.
- [x] **2. Build Groundwork & Open `SPEEDCOIN` Button for Testing:**
  - Implement Speedcoin on the real `SCREEN_GAME` / `ui_speed.c` / `ui_board.c` / `ui_combos.c` / `ui_plexus.c` pipeline without touching protected core files.
  - Temporarily unlock the bottom-bar `SPEEDCOIN` button (`SPEEDCOIN_UNLOCKED_FOR_TESTING = 1` in `src/ui_plexus.c`) so the mode can be launched and tested directly from Atrium Plex.
- [ ] **3. Playtesting & Fine-Tuning:**
  - Test gameplay pace, `+10s / -10s` clock balance, Wave transitions, `Hand 2` deck-glow clarity, and the Ranklist + Logfile-stats result screen; iterate rules and feel until locked in.
- [ ] **4. Full Atrium Plex Spine Integration:**
  - Once fine-tuning is complete and approved, disable the testing unlock (`SPEEDCOIN_UNLOCKED_FOR_TESTING = 0`) and wire full spine card progression, briefings, and unlock gating.

---

## Phase 4 — Action Card Minigame & Remaining Hub Refinements

- [ ] **1. Action Card Minigame (`plexus/action_card_rule.md` -> Test Open -> Fine-Tune -> Integrate):**
  - Follow the same 4-step pipeline: write dedicated rule markdown (`plexus/action_card_rule.md`), build onto the real table & existing `reverb_synergy` envelope (`ui_board.c` / `ui_audio.c` untouched DSP) with `ACTION CARD` temporarily opened on the bottom bar for testing, fine-tune, and then integrate into spine progression.
- [x] **`TRAINING` Non-Campaign Match Mode:**
  - Launches a normal match from Option menu settings via `k_briefing_rect` + `k_start_rect` without Campaign awards.
- [ ] **Plexus Telemetry & Highscore Ranklists (`plexsave.txt`):**
  - Persist Speedcoin / Action Card ranklists alongside Speedbar and Audio FX telemetry in `plexsave.txt`.

---

## Phase 5 — Atrium Plex Story *(Later)*

- [ ] **Deferred Until Gameplay & Hub Are Solid:**
  - Introduce the non-linear biomechanical story sequences (`Sequence 00` intro + monthly fragments archived in `LOGFILE`) after Phases 1–4 are verified.
