# FRONT-END & HUB RESTRUCTURE — Roadmap (`ROADMAP_MENU_HUB.md`)

> **Status:** Staged implementation; Stage 5 was owner-approved on 26 October 2026
> **Scope:** Title Screen, Main Game Hub (`CAMPAIGN` vs. `ATRIUM PLEX`), Campaign Hub
> **Deferred / separately gated:** `[DEUS MODE]` toggle (Stage 4)
> **Architecture Reference:** [`LEARN_THE_CODE.md`](LEARN_THE_CODE.md) · [`AGENTS.md`](AGENTS.md) · [`STATUS.md`](STATUS.md)
>
> This rewrite supersedes the previous draft of this roadmap. That draft described the
> right destination but several of its instructions would have broken the game as the
> code actually stands. Section 0 lists every conflict found, verified against `src/`.
> **No stage in this document authorises implementation by itself.** Each stage is a
> separate, separately approved, separately committed change, and a stage may only
> start after the previous stage's verification gate has passed.

---

## 0. Why the previous draft was unsafe (audit, verified against `src/`)

| # | Previous draft claimed / assumed | What the code actually does | Rule this rewrite follows |
|---|---|---|---|
| 1 | "Do not edit `src/ui_screens.c`" **and** "draw a cleaned 5-button Title Screen" in the same plan | The Title Screen *is* `src/ui_screens.c`: `screens_draw_start()` draws the 7 buttons at `{490, 320 + i*50, 300, 40}`, `screens_handle_start_click()` maps them. There is no second title screen to edit instead | Title cleaning is its own final cutover (Stage 3) done **only** in `src/ui_main.c` routing + `src/ui_hub.c` drawing. `src/ui_screens.c` stays untouched, so the old functions remain compiled, tested and available as a fallback |
| 2 | Profiles can live in `src/ui_hub.c` "reusing `storage_load_*` / `storage_save_*` unchanged" | The helpers a profile switch needs — `get_app_path()` (executable-dir vs `./` fallback) and `safe_write_text()` (`.tmp` + `remove`/`rename`) — are `static` inside protected `src/storage.c`. `ui_hub.c` cannot call them; copying files from `ui_hub.c` would duplicate that path logic and drift from it | Profiles are **removed from the hub stages** and deferred to Stage 5, which is blocked on an owner-approved `storage.*` API. No file copying in `ui_hub.c`, ever |
| 3 | Slot switching is "copy `slotM_*` into the live root files and reload" | Copy-only switching leaks: if the target slot has no `stats.txt`, the previous profile's live `stats.txt` survives and loads as the new profile's progress. Profile bleed is silent and permanent once re-saved | Stage 5 (if approved) must define per-file missing-file semantics (missing slot file ⇒ live file removed, loader defaults used) before any code is written |
| 4 | Register new filenames in `web/shell.html`'s "sync list" | One `files` array in `preRun` does **three** jobs: preload from `localStorage`, sync on `FS.rename`, and only `history.txt`/`stats_report.txt` are removed on `FS.unlink`. A filename registered for one job but not the others loads wrong, saves wrong, or resurrects after erase | Stage 5 must enumerate every exact filename (no wildcards — the shell uses `indexOf`) and cover preload + rename-sync + unlink-removal for each |
| 5 | `SCREEN_ATRIUM` exit and `SCREEN_STATS` Back can be sent to the new hubs from `src/ui_main.c` alone — stated without naming the mechanism | Today both are hard-coded: the Atrium branch calls `start_transition(SCREEN_START, 3, 0)` (SAVE & BACK **and** ESC), and the Stats branch calls `start_hub_transition(SCREEN_START, …)`. `plexus_handle_click()` only returns `ACT_GOTO_MENU`; the destination is `ui_main.c`'s decision. (The Options screen already solves this exact problem with the `options_return_screen` static) | Stage 2 adds one return-target static per affected screen in `src/ui_main.c`, modelled on `options_return_screen`. `src/ui_plexus.c` is not edited for routing |
| 6 | "`[ ATRIUM PLEX ]` → Card-back loader" and Campaign Hub via "CRT wipe", as if interchangeable | The two transitions are different machines: `start_transition()` is the 1.80 s + 2.20 s + 1.20 s fade/card-back loader used for game start, match results and Atrium entry/exit; `start_hub_transition()` is the 0.20 s + 0.16 s strobe/CRT wipe and **requires a `draw_hub_screen_state()` case for every screen it touches**, or the wipe draws nothing | Every route in §2 names its transition function explicitly. A hub-wipe route may not be enabled before its draw case exists |
| 7 | A hub-owned Deus snapshot/restore with "BACK always deactivates Deus" and "QUIT restores normal settings before writing `settings.txt`, so cold boot never starts in Deus" | Deus is forced in **four** places the hub does not own: `storage_load_settings()`, `main()` startup, `begin_new_match()`, and the Options difficulty selector in protected `src/ui_screens.c` (which applies the package on entry and a lossy colour/background reset on exit). The quit path (`ACT_QUIT`, window close) never calls `storage_save_settings()` at all, so "restore before writing" restores nothing; and `storage_save_settings()` is already called with Deus active from the campaign Start-Match path, so Deus in `settings.txt` is the *current, intended* persistence. The draft's snapshot also omitted `current_opponent`, which the Deus dossier sets to 12. A third snapshot system would also stack on top of the existing Plexus one (`plexus_sync_match_settings()` / `plexus_restore_match_settings()`, called by `start_hub_transition()` and `begin_new_match()`) | Stage 4 is **blocked on an owner decision** (§Stage 4). Until then: hub displays Deus state read-only, derived from `settings.difficulty == DIFFICULTY_DEUS`; no hub snapshot, no auto-deactivate, no cold-boot promise. Note `STATUS.md` (26 Oct 2026) records Deus as playable **only** via `OPTION → AI DIFFICULTY`; a second Deus entry point reverses that decision and needs explicit owner sign-off |
| 8 | Hub shows `SPINE XX/48` for Atrium Plex | The Spine masks (`s_finished_cards_mask`) are `static` inside `src/ui_plexus.c`. There is no getter in `src/ui_plexus.h`; `ui_hub.c` cannot read the count | Stage 1 either omits the number or lands a tiny read-only getter in `src/ui_plexus.{h,c}` as its own approved micro-change (§Stage 1.3). Cross-module `extern` of another file's statics is forbidden |
| 9 | Campaign overlay can simply "open" from the Campaign Hub | The overlay is anchored to `SCREEN_START`: `draw_hub_screen_state()` draws it only in the `SCREEN_START` case, the click handling (`campaign_handle_click()` return codes −1/1/2/3) lives in the `SCREEN_START` branch of the frame loop, and `campaign_overlay_open` / `hub_from_campaign` are woven through `start_hub_transition()` and the end-of-hold reset in `start_transition()` | Stage 1 generalises the existing branch in `src/ui_main.c` (draw case + one shared click path keyed on the current screen). `src/campaign.*` is not edited; the overlay's return-code contract is reused unchanged |

Two further constraints the draft missed:

- **`./build.sh test` does not compile `src/ui_main.c` or a future `src/ui_hub.c`.** It
  builds the engine tests, the Koi-Koi trigger tool, and (only when raylib headers are
  present) `tests/test_table_ui.sh` / `tests/test_music_folder_ui.c`. A hub change can
  therefore pass `./build.sh test` and still break the game build. Every stage gate
  below includes a full `src/*.c` compile, which is what `build.sh linux|win|wasm`
  and CI's Windows build actually do.
- **Locked tests assert pixels.** `tests/test_music_folder_ui.c` `#include`s
  `src/ui_screens.c` and asserts exact `screens_handle_options_click()` coordinates
  on Options pages 0 and 1. Any stage that changes Options geometry, pagination or
  the Options Back rectangle breaks a locked test. Hub stages therefore change **no**
  existing screen's geometry; new screens get new rectangles in `src/ui_hub.c` only.

---

## 1. Core engineering rules (non-negotiable, all stages)

1. **Protected files stay untouched unless a stage says otherwise and the owner has
   approved that exact exception in advance** (`.github/workflows/guard-protected.yml`,
   `.github/CODEOWNERS`): `src/rules.*`, `src/engine.*`, `src/cards.*`,
   `src/campaign.*`, `src/storage.*`, `src/ai.*`, `src/ui_screens.c`,
   `src/ui_board_status.c`, `src/board_layout.*`, `src/ui_audio.c` (also pinned by
   `tools/audio_guard.sha256` — never edit, never re-record as part of a hub change),
   `docs/**`, `mkdocs.yml`, `overrides/**`, card/atlas art, `tests/**` and
   `tools/tests_guard.*`. Owner decisions already on record stay reversed only by the
   owner: `DEUS_UNLOCKED_FOR_TESTING` remains `1`; KK-ADD-10 remains as-is.
2. **Gameplay is out of scope.** No stage touches phases, matching, scoring, the
   Koi-Koi trigger, AI, audio DSP, or the replay format. If a hub stage appears to
   need one of those, stop — the design is wrong, not the engine.
3. **Append-only enums.** New `ScreenState` values go **after** `SCREEN_MATCH_RESULT`
   in `src/ui_screens.h`. `SCREEN_START` remains `0`; no existing value is renumbered,
   and no existing `ScreenAction` value or button rectangle changes meaning.
4. **One state, one owner.** The single `GameState g`, the global `GameSettings
   settings`, `GameStats stats`, `MatchHistory history` and `BoardAssets assets` in
   `src/ui_main.c` remain the only copies. Hub state is file-scope `static` in
   `src/ui_hub.c`, fixed-size, **zero `malloc`/`calloc`/`free`** (the tree's only heap
   use stays the replay file table in `src/ui_replay.c`). `ui_hub.c` gets its own small
   static draw helpers built on raylib + `font_title`/`font_body` (declared `extern`
   exactly as `src/campaign.c` does); it must not `#include` another `.c` file, must
   not `extern` another file's statics, and its draw functions must be side-effect
   free — `draw_hub_screen_state()` is called into both render targets during wipes,
   with an off-screen mouse.
5. **Clicks only inside the existing input gate.** All hub click handling lives in the
   frame loop's `transition_dir == 0 && !hub_trans_active` region of `src/ui_main.c`.
   No input handling in draw code, no screen change except through
   `start_transition()` / `start_hub_transition()` (plus the one existing direct
   assignment for replay start, which hub code does not imitate).
6. **Persistence has exactly one writer per file.** `settings.txt`, `stats.txt`,
   `history.txt`, `stats_report.txt` are written only via `src/storage.c`;
   `plexsave.txt` only via `plexus_save()`. Hub code may *call* `storage_save_all()`,
   `storage_save_settings()` and `plexus_save()` the way existing `ui_main.c` branches
   do; it must not `fopen` save files itself. This is why profiles are Stage 5.
7. **Return targets are data, not constants.** Any screen reachable from two places
   gets a `*_return_screen` static in `src/ui_main.c`, set at entry, defaulting to
   `SCREEN_START` so CLI entry points (`--stats`, `--options`, `--campaign`,
   `--autostart`, `--dealer`) behave exactly as today when no hub was visited.
8. **Web parity is explicit.** Desktop-only replay UI stays behind
   `#ifndef __EMSCRIPTEN__`. Any new persisted filename is registered in
   `web/shell.html` for preload, `FS.rename` sync **and** `FS.unlink` removal by exact
   name. Hub draw/click code contains no platform `#ifdef`s except at the existing
   call sites in `src/ui_main.c`.
9. **Separate commits per stage.** Never bundle navigation, title cleaning, Deus and
   profiles. Update `STATUS.md` (date, touched / deliberately-not-touched, checks
   actually run) in the same commit as each stage, per `AGENTS.md`.

---

## 2. Target navigation map (revised)

Transition key: **[wipe]** = `start_hub_transition()` (0.36 s), **[loader]** =
`start_transition()` (fade + card-back, as today). Routes marked *(today)* already
exist and must not change behaviour.

```text
SCREEN_START  (Title — existing 7-button screen until Stage 3 cutover)
├── START MATCH ──[wipe]──▶ campaign overlay (until Stage 1) / SCREEN_HUB_MAIN (Stage 1+)
├── ATRIUM PLEX ──[loader]──▶ SCREEN_ATRIUM            (today; removed from Title in Stage 3)
├── OPTION ──[wipe]──▶ SCREEN_OPTIONS, returns to SCREEN_START   (today)
├── STATS ──[wipe]──▶ SCREEN_STATS, returns to SCREEN_START      (today; removed in Stage 3)
├── SAVE ──▶ storage_save_all() + plexus_save(), "ALL FILES SAVED"   (today, unchanged)
├── CREDITS ──[wipe]──▶ SCREEN_CREDITS ──[wipe]──▶ SCREEN_START      (today)
└── QUIT ──▶ plexus_save(), quit_requested                    (today, unchanged)

SCREEN_HUB_MAIN  (Stage 1)
├── [ CAMPAIGN ] ──[wipe]──▶ SCREEN_HUB_CAMPAIGN
│       readout: UNLOCKED SECTORS n/12 · COINS n/36  (from settings, §Stage 1.2)
├── [ ATRIUM PLEX ] ──[loader]──▶ SCREEN_ATRIUM, atrium_return = SCREEN_HUB_MAIN
│       readout: SPINE n/48 + NEW PLAYERS badge at 0/48 (needs §Stage 1.3 getter)
└── [ BACK ] ──[wipe]──▶ SCREEN_START   (no save beyond what existing routes do)

SCREEN_HUB_CAMPAIGN  (Stage 1; Deus row is display-only until Stage 4 is unblocked)
├── [ CAMPAIGN ] ──▶ campaign overlay over this screen (campaign_overlay_open = 1)
│       START MATCH ──[loader]──▶ SCREEN_GAME, match_origin = SCREEN_HUB_CAMPAIGN
│       X / BACK ──[wipe]──▶ overlay closed, stay on SCREEN_HUB_CAMPAIGN
├── [ RECORDS & STATS ] ──[wipe]──▶ SCREEN_STATS, stats_return = SCREEN_HUB_CAMPAIGN
├── [ DEUS MODE: ON/OFF ]  derived from settings.difficulty; display-only (Stage 4 gate)
└── [ BACK ] ──[wipe]──▶ SCREEN_HUB_MAIN

SCREEN_STATS ── BACK ──[wipe]──▶ stats_return_screen (default SCREEN_START)
SCREEN_ATRIUM ── SAVE & BACK / ESC ──[loader]──▶ atrium_return_screen (default SCREEN_START)
SCREEN_GAME ── Pause ABANDON (normal match only) ──[loader]──▶ match_origin (default SCREEN_START)
SCREEN_GAME ── Deus timeout ──[loader]──▶ match_origin; still no stats, no record (today's rule)
SCREEN_MATCH_RESULT ── MAIN MENU ──[wipe]──▶ match_origin (default SCREEN_START)
    (Plexus mode results keep returning to SCREEN_ATRIUM exactly as today.)
```

Invariants over this map:

- `match_origin` is a single static in `src/ui_main.c`, set where a match is launched
  (campaign overlay Start, and anywhere else that should return somewhere new —
  initially nowhere else), defaulting to `SCREEN_START`. It is **not**
  `campaign_overlay_open`: `start_transition()` clears the overlay flag at the end of
  its hold, long before the match ends.
- Tutorial / Speedcoin / Action Card / Training launches and results keep their
  existing `SCREEN_ATRIUM` routing in every branch (frame loop checks them before the
  normal-match branches). Hub routing must be added only to the normal-match paths.
- `options_return_screen` already implements this pattern; do not invent a second
  mechanism for Options.

---

## Stage 0 — Baseline (no behaviour change)

- [ ] Record `git status`, then run and note: `./build.sh test`,
      `./tools/tests_guard.sh`, `./tools/audio_guard.sh`,
      `python3 tools/gen_wiki_tables.py --check`, `git diff --check`, and one full
      compile of the game (e.g. `./build.sh linux` in an environment with raylib in
      `/tmp`), since `./build.sh test` alone does not compile `src/ui_main.c`.
- [ ] Capture the CLI matrix for later comparison: `--campaign` (overlay over Title),
      `--stats`, `--options`, `--autostart`, `--dealer`, `--window 1024x576`,
      `--shot-frame`. Note current destinations and return paths.
- [ ] Re-read `LEARN_THE_CODE.md` §5.3–5.4 (campaign overlay, files), §7.3–7.4
      (transitions, frame loop), §9.1–9.3 (Title/Options/Stats) and verify each
      anchor named in §0 above in the source. **Gate:** baseline checks green;
      nothing committed except, if needed, corrections to this roadmap.

## Stage 1 — Hub screens, reached from the existing Title (separate commit)

The Title keeps all 7 buttons and every existing route. Only `START MATCH` changes
destination: instead of opening the campaign overlay over the Title, it wipes to
`SCREEN_HUB_MAIN`. Every pre-hub route remains as a working fallback.

- [ ] **1.1 New module `src/ui_hub.{h,c}` (new files only).** Own `HubAction` enum in
      `ui_hub.h` (do not extend `ScreenAction`); draw + click functions for
      `SCREEN_HUB_MAIN` and `SCREEN_HUB_CAMPAIGN`; fixed static state; own static
      button/text helpers on raylib + `extern` fonts; background is a dim/panel
      treatment in theme colours — the fog-shader background path
      (`draw_bg_if_loaded()`) is `static` to `src/ui_screens.c` and is **not** reused
      or copied. No heap, no file I/O, no `screen` writes.
- [ ] **1.2 Readouts computed from data the hub may legally read:** sectors =
      `settings.unlocked_opponents` (1–12, labelled UNLOCKED, not "cleared"); coins =
      count of bits 0–35 of `settings.cleared_mask` over 36. Both are pure functions
      of the `GameSettings` passed in.
- [ ] **1.3 Spine readout (micro-change, separately approved):** add one read-only
      getter (e.g. `plexus_get_spine_count(void)`, standard-pass mask only) to
      `src/ui_plexus.{h,c}`. If this approval is not given, the Atrium card ships
      without a number — never reach into `ui_plexus.c` statics.
- [ ] **1.4 Enum + wiring in `src/ui_main.c` / `src/ui_screens.h`:** append
      `SCREEN_HUB_MAIN`, `SCREEN_HUB_CAMPAIGN` after `SCREEN_MATCH_RESULT`; add
      `draw_hub_screen_state()` cases **before** any route to them is enabled
      (the `SCREEN_HUB_CAMPAIGN` case draws the hub screen and then, when
      `campaign_overlay_open` is set, `campaign_draw_overlay()` over it); add a
      shared campaign-overlay click path usable from `SCREEN_START` (until Stage 3)
      and `SCREEN_HUB_CAMPAIGN`, preserving `campaign_handle_click()` codes:
      `−1` close (wipe to the *current* screen with overlay closed), `1` start match
      (`plexus_*_abort()` calls, `ai_set_opponent()`, `storage_save_settings()`,
      `start_transition(SCREEN_GAME, 1, …)`, `match_origin` set), `2`/`3` click sound
      only. `draw_hub_strobe_button()`'s Atrium/campaign font choice must be
      regression-checked for the new from-screens.
- [ ] **1.5 Campaign Hub buttons:** `[ CAMPAIGN ]` sets `campaign_overlay_open = 1`
      (no transition); `[ RECORDS & STATS ]` sets `stats_return_screen` then wipes to
      `SCREEN_STATS`; `[ BACK ]` wipes to `SCREEN_HUB_MAIN`. Deus row draws
      `settings.difficulty == DIFFICULTY_DEUS` state and is inert.
- [ ] **Gate:** full `src/*.c` compile with `-Wall -Wextra -Werror` on touched files,
      `./build.sh test` (with raylib headers present, so `test_table_ui.sh` and
      `test_music_folder_ui.c` actually run), `./tools/tests_guard.sh`,
      `git diff --check`; manual pass over the CLI matrix plus Title → Hub → Campaign
      overlay → match → result → MAIN MENU. **Touched (max):** `src/ui_hub.{h,c}`,
      `src/ui_main.c`, `src/ui_screens.h`, optionally `src/ui_plexus.{h,c}` (getter
      only), `STATUS.md`.

## Stage 2 — Return routing (separate commit)

- [ ] Add `stats_return_screen`, `atrium_return_screen`, `match_origin` statics to
      `src/ui_main.c` (all default `SCREEN_START`), set at entry points, consumed —
      never hard-coded — at: Stats BACK; Atrium SAVE & BACK **and** ESC (only when
      `plexus_close_subpanel()` returns 0 — a closed subpanel still consumes the
      ESC and stays in Atrium, as today); normal-match
      Pause ABANDON; Deus timeout; Match-Result MAIN MENU. Plexus-mode branches in
      the same handlers are not edited.
- [ ] Regression list (each is a current behaviour that must survive): Options entered
      from Atrium still returns to Atrium (`options_return_screen` untouched); replay
      start from Stats still enters `SCREEN_GAME` and STOP still lands on Stats, whose
      BACK then honours `stats_return_screen`; `--stats` / `--campaign` with no hub
      visited still return to Title; Atrium first-entry intro
      (`plexus_intro_blocking()`) unaffected by entry source.
- [ ] **Gate:** as Stage 1, plus a manual route-by-route pass over §2's map in both
      directions. **Touched (max):** `src/ui_main.c`, `STATUS.md`.

## Stage 3 — Clean Title cutover (separate commit, optional)

Only after Stages 1–2 have shipped and the hub routes are proven. This is the stage
the old draft put first; done first, it would have removed the only working routes
before their replacements existed.

- [ ] `src/ui_main.c` draws and routes the Title through `ui_hub` (5 buttons:
      `START GAME → SCREEN_HUB_MAIN`, `OPTION`, `SAVE`, `CREDITS`, `QUIT`, using the
      existing save/quit side effects verbatim) **instead of** calling
      `screens_draw_start()` / `screens_handle_start_click()` for `SCREEN_START`. The
      two paths are never both live for one click. `screens_draw_start()` stays
      compiled in untouched `src/ui_screens.c` as the revert path (deleting it would
      be a protected-file edit needing its own approval).
- [ ] `--campaign` still opens the overlay over the Title at boot
      (`campaign_overlay_open` over the hub-drawn Title); `--stats`, `--options`
      unchanged. Saved-"ALL FILES SAVED" timer behaviour preserved.
- [ ] **Gate:** as Stage 1, plus screenshot comparison of Title/Options/Stats/Credits
      and the full CLI matrix. Reverting this stage is one routing change in
      `src/ui_main.c`. **Touched (max):** `src/ui_main.c`, `src/ui_hub.{h,c}`,
      `STATUS.md`.

## Stage 4 — `[DEUS MODE]` toggle — ⛔ BLOCKED, owner decision required

**Do not implement until the owner answers, in writing on the PR:**

1. Is Deus still to be entered **only** via `OPTION → AI DIFFICULTY`
   (`STATUS.md`, 26 Oct 2026), or does the Campaign Hub become a second entry point?
2. Is hub-activated Deus persistent (today's semantics: `difficulty=deus` is saved and
   re-forced on load) or session-only? Session-only cannot be built on the current
   save paths — `storage_save_settings()` is called with Deus active from campaign
   start and Options — so choosing it means approving protected `storage.*` /
   `ui_screens.c` work as part of this stage.

If, and only if, a second persistent entry point is approved, the safe shape is:

- [ ] State is **derived**, never stored twice: ON ⇔ `settings.difficulty ==
      DIFFICULTY_DEUS`. The hub holds one file-scope snapshot
      `{valid, difficulty, ruleset, master_timer, terminal_color, background,
      current_opponent}` — including `current_opponent`, which the Deus dossier sets
      to 12 and which must be restored (or reset to the pre-Deus selection) on OFF.
- [ ] Activation (overlay closed, on `SCREEN_HUB_CAMPAIGN` only): snapshot first, then
      apply exactly the existing package — `DIFFICULTY_DEUS` + `RULESET_YAKUZA` +
      timer 15 + `theme_set(5)` + `background = 5` / `assets.current_bg = 5` — then
      `storage_save_settings()`. Atlas selection needs no code
      (`src/ui_board.c` and `src/ui_action.c` already key off `difficulty`). `src/ai.*`
      is not touched; "100 % / −150" are consequences of `DIFFICULTY_DEUS`, already
      implemented.
- [ ] Deactivation restores the snapshot field-by-field (`theme_set()` +
      `assets.current_bg` reapplied) and saves. Restore may **never** run between
      match end and `storage_record_match()` / clearance-coin evaluation in
      `src/ui_main.c` — Deus results must be recorded as Deus (which also means: no
      clearance coin, as today). If the snapshot is invalid (booted into Deus, or
      difficulty changed via Options since), OFF is **disabled** and the row directs
      the player to Options — the hub must not fabricate "original" settings that
      `storage_load_settings()` already overwrote.
- [ ] **No auto-deactivate anywhere**: not on Campaign-Hub BACK, not on match return,
      abandon or timeout, not on QUIT. (The old draft's quit-time restore could not
      work — quit writes no settings — and mid-flow restores would corrupt match
      recording and fight `plexus_restore_match_settings()`, which
      `start_hub_transition()` already runs on non-game navigation.) Deus visibility
      on Title/Atrium is today's behaviour via Options and is not a "leak" to fix here.
- [ ] **Gate:** as Stage 1, plus manual matrix: toggle ON → dossier → match → result;
      ON → timeout (no stats/record); ON → abandon; ON → Options round-trip (snapshot
      invalidated, OFF disabled); cold boot into saved Deus. ASan+UBSan scratch checks
      in `/tmp` only — no new files in `tests/`. **Touched (max):** `src/ui_hub.{h,c}`,
      `src/ui_main.c`, `STATUS.md`.

## Stage 5 — 3-Slot Profiles — OWNER-APPROVED (26 October 2026)

The owner approved implementation by directing: “go on, start stage 5.” The earlier
profile draft remains rejected where it conflicts with this corrected checklist. The
implemented profile layout is the approved per-slot directory shape:
`profiles/slot01/`, `profiles/slot02/`, and `profiles/slot03/`.

- [x] **Storage-owned API:** `src/storage.*` owns slot initialization, create/load-by-
      switch/erase/flush, file staging, transactional replacement/rollback, and the
      `plexsave.txt` temp-file commit. Existing loaders keep the same filenames but
      resolve active working files under `profiles/`; profile code reuses
      `get_app_path()` and the storage-owned safe writer.
- [x] **Canonical files and failure rules:** Per-slot files are exactly
      `settings.txt`, `stats.txt`, `history.txt`, and `plexsave.txt`; the active
      working copies are `profiles/<canonical-file>` beside, not inside, `slot01..03`.
      Missing source files remove their live or slot counterparts. `stats_report.txt`
      remains at the app root, is regenerated after profile load, and is never
      slot-copied. Active-slot erase is rejected. `replays/` is untouched and remains
      shared; the UI does not describe replays as profile-owned.
- [x] **Safe switching and reload:** Profile changes are only actionable from the
      Title Options profile manager (or the first-launch profile manager), not while
      a match, replay, tutorial, or overlay is active. Switching flushes saves,
      updates the active slot transactionally, reloads settings/stats/history and
      Plexus state, reapplies unlocks/theme/background/audio/opponent/music-folder and
      fullscreen state, regenerates the derived report, and clears transient UI,
      overlay, options-page, return-target, and hub state.
- [x] **Fresh initialization and metadata:** No legacy root-file import is performed.
      Root-level `settings.txt`, `stats.txt`, `history.txt`, and `plexsave.txt` are
      discarded; current saves live only under `profiles/`. `profiles.txt` remains at
      the app root and has a validated version-1 format, active slot 1–3, and slot
      mask. Invalid metadata falls back to single-profile behavior using the nested
      active working copies, not retired root saves.
- [x] **Web persistence registration:** `web/shell.html` explicitly registers the
      nested active working copies, every exact canonical slot path, `profiles.txt`,
      and root `stats_report.txt` for preload, `FS.rename` sync, and `FS.unlink`
      removal. Retired root-level LocalStorage save keys are deleted, not migrated.
- [x] **Scratch verification:** ASan+UBSan harnesses in `/tmp` covered migration,
      fresh profile creation, slot switching/isolation, missing-file removal, active
      erase rejection, inactive erase, corrupt-metadata fallback, retired-root-file
      discard, nested active working paths, derived-report exclusion, shared replay
      preservation, and injected partial-commit rollback. No files were added
      under `tests/`.
- [x] **Approved flow:** First launch follows Start Game → profile screen → Main Hub;
      with a saved active profile, Continue proceeds directly to Main Hub. The
      pre-approval direct-to-hub rule no longer applies.

**Runtime limitation:** The full `src/*.c` source set compiles, but `./build.sh linux`
can't complete its final link in this container because `/tmp/libraylib_linux.a` and
`libGL.so.1` are absent. A live desktop GUI pass and actual browser reload/persistence
pass remain unverified. See `STATUS.md` for commands and results.

## Stage 6 — Final verification & handoff

- [ ] `./build.sh test` (raylib headers present, so the table/music UI tests run),
      full `src/*.c` compile, `./tools/tests_guard.sh`, `./tools/audio_guard.sh`
      (proves `src/ui_audio.c` untouched), `git diff --check`. Docs generators only if
      a stage actually touched `docs/**` (none should):
      `python3 tools/gen_wiki_tables.py --check`, `python3 -m mkdocs build --strict`.
- [ ] Manual/web pass: `web/shell.html` build boots, saves persist across reload,
      replay controls correctly absent on web, present on desktop.
- [ ] `STATUS.md` updated per stage (date, completed work, checks actually run,
      touched / deliberately-not-touched). PR against `main`, one stage per PR,
      stating which protected-file exceptions (if any) were approved and by whom.

---

## Appendix — anchors verified for this rewrite (re-verify before each stage)

- Title draw/click: `screens_draw_start()` / `screens_handle_start_click()` in
  `src/ui_screens.c` (7 × `{490, 320 + i*50, 300, 40}`); called from
  `draw_hub_screen_state()` and the `SCREEN_START` branch of `src/ui_main.c`.
- Campaign overlay: `campaign_draw_overlay()` / `campaign_handle_click()` in
  `src/campaign.c` (codes −1/1/2/3; Deus dossier branch on `DIFFICULTY_DEUS`;
  `DEUS_UNLOCKED_FOR_TESTING 1`); drawn only in the `SCREEN_START` draw case today.
- Transitions: `start_transition()` (1.80/2.20/1.20 s; clears
  `campaign_overlay_open` at hold end) vs `start_hub_transition()`
  (0.20/0.16 s; calls `plexus_restore_match_settings()` for non-game targets) in
  `src/ui_main.c`; hub-wipe drawing via `draw_hub_screen_state()`.
- Deus forcing: `storage_load_settings()` (`src/storage.c`), startup and
  `begin_new_match()` (`src/ui_main.c`), Options difficulty selector
  (`src/ui_screens.c`); Plexus override pair `plexus_sync_match_settings()` /
  `plexus_restore_match_settings()` (`src/ui_plexus.c`).
- Persistence: `safe_write_text()` / `get_app_path()` are `static` in
  `src/storage.c`; `plexus_load()` / `plexus_save()` own `plexsave.txt`
  (`src/ui_plexus.c`); shell sync list in `web/shell.html` (`preRun`).
- Locked-test surface: `tests/test_music_folder_ui.c` includes `src/ui_screens.c`;
  `./build.sh test` does not compile `src/ui_main.c`.
