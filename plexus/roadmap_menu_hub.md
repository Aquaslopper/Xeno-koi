# FRONT-END & HUB RESTRUCTURE — Roadmap (`roadmap_menu_hub.md`)

> **Status:** Planned — Pre-Implementation Blueprint
> **Scope:** Title Screen, 3-Slot Profile System, Main Game Hub (`CAMPAIGN` vs. `ATRIUM PLEX`), Campaign Hub (`CAMPAIGN` / `RECORDS & STATS` / `[DEUS MODE]`)
> **Architecture Reference:** [`LEARN_THE_CODE.md`](../LEARN_THE_CODE.md) · [`AGENTS.md`](../AGENTS.md)

---

## Core Engineering Rules (Non-Negotiable)

1. **Zero Protected-File Edits (`guard-protected.yml` & Locked Tests):**
   - Do **not** edit `src/ui_screens.c`, `src/campaign.*`, `src/storage.*`, `src/ai.*`, `src/engine.*`, `src/rules.*`, `src/cards.*`, `src/ui_audio.c`, `docs/**`, or `tests/**`.
   - `tests/test_music_folder_ui.c` (`./build.sh test`) `#include`s `src/ui_screens.c` directly and asserts exact pixel clicks on `screens_handle_options_click()` across pages `0` and `1`; keeping `src/ui_screens.c` untouched preserves every locked test.
2. **Self-Contained Static Module (`src/ui_hub.h`, `src/ui_hub.c`):**
   - All new front-end screens, profile slot metadata, and `[DEUS MODE]` snapshot/restore state live in `src/ui_hub.h` + `src/ui_hub.c` using fixed static/stack storage only (**zero** `malloc` / `calloc` / `free`, matching `LEARN_THE_CODE.md` §0).
   - Screen routing and transitions connect through `src/ui_main.c` and enum extensions in unguarded `src/ui_screens.h`.
3. **Reuse Existing Rendering & Persistence Entry Points:**
   - Reuse `campaign_draw_overlay()` / `campaign_handle_click()` for Campaign Opponent Select and `ENTITY 13 // DEUS SUPREMUS`.
   - Reuse `screens_draw_stats()` / `screens_handle_stats_click()` (plus desktop `ui_replay_*`) for Campaign Records & Replays.
   - Reuse `storage_load_*` / `storage_save_*` and `plexus_load()` / `plexus_save()` unchanged by keeping the active profile mapped to the live root save files (`settings.txt`, `stats.txt`, `history.txt`, `stats_report.txt`, `plexsave.txt`).
4. **Desktop + WebAssembly Save Parity (`web/shell.html`):**
   - `web/shell.html` mirrors files to `window.localStorage` via an explicit filename list inside `preRun` (`FS.rename` / `FS.unlink`). Every profile slot filename added on desktop must be registered in `web/shell.html` so browser saves persist identically.
5. **CLI & Tooling Compatibility (`src/ui_main.c`):**
   - `--seed`, `--shot-frame`, `--window`, `--autostart`, `--options`, `--stats`, `--campaign`, and `--dealer` continue to work identically.

---

## Target Navigation Map

```text
1. TITLE SCREEN  (SCREEN_START)
   ├── START GAME   -> Profile / Continue flow -> MAIN GAME HUB
   ├── OPTION       -> System Configuration (SCREEN_OPTIONS)
   ├── SAVE         -> Saves active profile + slot mirror
   ├── CREDITS      -> Credits screen (SCREEN_CREDITS)
   └── QUIT         -> Deactivates transient Deus if needed, saves, exits

2. PROFILE SELECT / AUTO-CONTINUE  (inside START GAME flow)
   ├── Slot 01 / Slot 02 / Slot 03
   │   ├── CONTINUE      (loads selected profile -> MAIN GAME HUB)
   │   ├── NEW PROFILE   (initializes empty slot -> MAIN GAME HUB)
   │   └── ERASE SLOT    (confirmation modal before deleting slot data)
   └── BACK              (returns to TITLE SCREEN)

3. MAIN GAME HUB  (SCREEN_HUB_MAIN)
   ├── [ CAMPAIGN ]      -> CRT wipe to CAMPAIGN HUB (shows Sectors XX/12, Coins XX/36)
   ├── [ ATRIUM PLEX ]   -> Card-back loader to SCREEN_ATRIUM (shows Spine XX/48 + beginner hint at 00/48)
   └── [ BACK ]          -> Saves active profile, returns to TITLE SCREEN

4. CAMPAIGN HUB  (SCREEN_HUB_CAMPAIGN)
   ├── [ CAMPAIGN ]         -> Opens campaign_draw_overlay() (12 Bio-Opponents, or Deus dossier when Deus Mode is ON)
   ├── [ RECORDS & STATS ]  -> Opens SCREEN_STATS (BACK returns to SCREEN_HUB_CAMPAIGN)
   ├── [ DEUS MODE ]        -> Toggles the full 6-part DEUS package ON / OFF inside Campaign
   └── [ BACK ]             -> Automatically deactivates DEUS MODE (restoring normal settings/theme/bg) -> MAIN GAME HUB
```

---

## Stage 1 — Clean Title Screen & 3-Slot Profile Layer

- [ ] **1. Create `src/ui_hub.h` & `src/ui_hub.c`:**
  - Draw the cleaned 5-button Title Screen (`START GAME`, `OPTION`, `SAVE`, `CREDITS`, `QUIT`), removing `ATRIUM PLEX` and `STATS` from the root menu.
- [ ] **2. Non-Invasive 3-Slot Profile Persistence (`profiles.txt` + `slot{1..3}_*`):**
  - Track active slot (`1..3`) in `profiles.txt`.
  - **First-Run Auto-Migration:** If existing root save files (`settings.txt`, `stats.txt`, `history.txt`, `plexsave.txt`) exist without `profiles.txt`, automatically mirror them into **Slot 01** so existing Version 21 players lose zero progress.
  - Switching slots flushes the current slot's live files into `slotN_*`, copies the target slot's `slotM_*` into the live root files via atomic `.tmp` + `rename()`, and reloads `settings`, `stats`, `history`, `theme`, `background`, and `plexus_load()`.
  - Register `profiles.txt` and `slot{1..3}_*` in `web/shell.html` `localStorage` sync list.
- [ ] **3. Stage 1 Verification Gate (Separate Commit):**
  - `gcc -std=c99 -O2 -Wall -Wextra -Werror` on all touched files, `./build.sh test`, ASan + UBSan slot-migration/switch/erase test in `/tmp`.

---

## Stage 2 — Main Game Hub (`CAMPAIGN` vs. `ATRIUM PLEX`)

- [ ] **1. `SCREEN_HUB_MAIN` Layout & Clarity for New Players:**
  - Two side-by-side primary cards with live progress readouts and concise plain-English descriptions:
    - **`[ CAMPAIGN ]`** — 12 Bio-Opponents, 5 Hanafuda Rulesets & Career Records (`SECTORS XX/12 | COINS XX/36`).
    - **`[ ATRIUM PLEX ]`** — 48-Card Story Spine, Interactive Koi-Koi Tutorials, Speedcoin & Action Card (`SPINE XX/48`, displaying a `NEW PLAYERS: TUTORIALS INSIDE` badge while `00/48`).
  - `BACK` button auto-saves the active profile and transitions back to `SCREEN_START`.
- [ ] **2. Atrium Plex Return Routing (`src/ui_main.c`):**
  - Exiting `SCREEN_ATRIUM` via `SAVE & BACK` or `ESC` transitions back to `SCREEN_HUB_MAIN` (instead of `SCREEN_START`).
- [ ] **3. Stage 2 Verification Gate (Separate Commit):**
  - Compile `-Wall -Wextra -Werror`, `./build.sh test`, verify CRT hub transitions and card-back loading transitions in both directions.

---

## Stage 3 — Campaign Hub, Relocated `STATS` & Self-Contained `[ DEUS MODE ]`

- [ ] **1. `SCREEN_HUB_CAMPAIGN` Controls:**
  - **`[ CAMPAIGN ]`** — Opens `campaign_draw_overlay()` (`12 Bio-Opponents` normally; `ENTITY 13 // DEUS SUPREMUS` when `[DEUS MODE]` is ON).
  - **`[ RECORDS & STATS ]`** — Transitions to `SCREEN_STATS`; clicking `BACK` on `SCREEN_STATS` returns directly to `SCREEN_HUB_CAMPAIGN`.
  - **`[ DEUS MODE: OFF / ON ]`** — Dedicated transformation toggle:
    - **Activating ON:** Snapshots normal `(difficulty, ruleset, master_timer, terminal_color, background)` and applies the full **DEUS package**:
      1. `settings.difficulty = DIFFICULTY_DEUS` (`100%` optimal AI, `-150` block-discard penalty)
      2. `settings.ruleset = RULESET_YAKUZA`
      3. `settings.master_timer = 15` (`15.0s` sudden-death turn limit)
      4. `settings.terminal_color = 5` + `theme_set(5)` (`DARK` monochrome theme)
      5. `settings.background = 5` + `assets.current_bg = 5` (`assets/deus.png`)
      6. `atlas_deus` monochrome card deck (already driven automatically by `settings.difficulty == DIFFICULTY_DEUS` in `ui_board.c:606` and `ui_action.c:783`)
    - **Deactivating OFF:** Restores the exact snapshotted `(difficulty, ruleset, master_timer, terminal_color, background)` and updates `theme_set()` + `assets.current_bg`.
  - **`[ BACK ]`:** **Always deactivates `[ DEUS MODE ]` automatically** before saving and returning to `SCREEN_HUB_MAIN`, guaranteeing Deus theme/rules never leak into Main Hub, Atrium Plex, Title Screen, or next boot.
- [ ] **2. Campaign Match Return & Timeout Routing (`src/ui_main.c`):**
  - Campaign `SCREEN_MATCH_RESULT -> MAIN MENU`, Pause `ABANDON MATCH`, and `15.0s` Deus sudden-death timeout (`/// TIME EXPIRED ///`) return cleanly to `SCREEN_HUB_CAMPAIGN`.
  - Clean shutdown (`QUIT` / window close) restores normal non-Deus settings before writing `settings.txt` so cold boot never starts trapped in `DIFFICULTY_DEUS`.
- [ ] **3. Stage 3 Verification Gate (Separate Commit):**
  - Compile `-Wall -Wextra -Werror`, `./build.sh test`, ASan + UBSan test verifying `[DEUS MODE]` snapshot/restore across toggle, match completion, sudden-death timeout, match abandon, Campaign Hub exit, and shutdown.

---

## Stage 4 — Final Verification & Handoff

- [ ] Run `./build.sh test`, `./tools/tests_guard.sh`, `./tools/audio_guard.sh` (git-clean check), `python3 tools/gen_wiki_tables.py --check`, and `git diff --check`.
- [ ] Update `STATUS.md` with date, touched/untouched files, and checks actually run.
- [ ] Push branch and open PR against `main`.
