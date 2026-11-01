# NEXT

What I am working on, in order, and what is already settled. Short on purpose: a long
document is a document nobody reads. When an item is done, delete it and move the next
one up. When a new agent arrives, this file plus `AGENTS.md` is the whole briefing.

Last updated: 1 November 2026.

---

## 1. The Koi-Koi tutorial (card 3 of Month 01) — built, waiting on one look

Coach marks added in `src/ui_plexus.c`: dimmed board with spotlight cutout in
`tut_kk_focus_rect()`, compact non-overlapping panels in `tut_panel_rect_for_step()`,
pulsing phosphor rings and leader lines in `plexus_tutorial_draw_panel()`, and live play
callout pills (`1. CLICK THIS CARD` -> `2. MATCH HERE`) in
`plexus_tutorial_draw_board_guides()`. Advances on click only.

Rule: presentation only. No phase, no timer, no input gate, no rule changes. Not yet seen
on a screen.

## 1a. The other two tutorials, same language — built, waiting on one look

Speedcoin and Action Card now borrow the same coach marks as Koi-Koi instead of drawing
their own flat boxes. `tut_sc_focus_rect()` gives Speedcoin a real spotlight scrim
(CORE, SYNC, PARASITE/HAND2); the flat dim stays on `NO_HESITATION` on purpose, because
that step is reading the score screen. Each frozen step rings its subject and labels it:
`YOUR SCORE + CLOCK`, `SPEEDBAR`, `THE STOCK`, `AUDIO PULSE`, `RESERVOIR`. Action Card
keeps its flat dim — the meters frame the panel, so a cut-out there would light the middle
of the board and nothing else. Red stays on the parasite pile and the wine reservoir. The
`DrawRectangleLinesEx` outlines are gone; what a step asks for is unchanged. Not yet seen
on a screen.

## 1b. D-badge tour — built, waiting on one look

The token leaves the table and guides the first visit to the Plex: eight stops, one
sentence each, click to advance, then it is gone (persisted as `badge_seen` in
`plexsave.txt`). Copy is informational on purpose — it reports, it never jokes.

Flight interpolation fixed (`s_badge_prev` latched only when `s_badge_blend >= 1.0f` in
`draw_badge_tour()` instead of overwritten every frame) and
right-column panel clearance widened (`t.x - w - 62.0f` in `badge_panel_rect()`).
Not yet seen on a screen. Tuning knobs, all in `src/ui_plexus.c`: the flight beat
(`s_badge_blend += dt / 0.34f`), the scrim alpha (120), panel size in
`badge_panel_rect()`, and `badge_home()` at (664, 412). If a stop's subject is
wrong, only `k_badge_stops[]` changes.

## 2. Hover a Plex bottom button, the briefing panel explains it — built, waiting on one look

Six buttons: SAVE & BACK, LOGFILE, SPEEDCOIN, ACTION CARD, TRAINING, OPTION. Hovering
`k_bottom_rects[0..5]` in `plexus_draw_ui_chrome()` drives `k_briefing_rect` with what
the button or mode is and how it scores. No hidden layer, no hover fly-outs, one state
instead of six. Not yet seen on a screen.

## 2b. CRT casing for any table — built, waiting on one look

`settings.crt_tables`, toggled as `CRT CASING: ON/OFF` on STAGE // MAP SELECT, runs any
forged table in phosphor without replacing its art. The map's own overlay and bezel are
drawn through `k_crt_tint_fs` instead of straight, so silhouette, bevels and alpha stay the
map's and only the colour is remapped — CASTLE still looks like CASTLE. Pickers go through
`board_draw_table_overlay()`, so a thumbnail cannot promise brown and render green. Off by
default, saved as `crt_tables` in `settings.txt`. Not yet seen on a screen; if it reads too
dark, the three ramp stops are the first line of the shader to touch.

## 3. Month 02 onwards: my words, one card at a time

`k_story_beats[48]` in `src/ui_plexus.c`, index = month * 4 + slot. Every card already has
three agent-authored SIGNAL lines. They are placeholders and they are mine to replace.

- Month 01 is the tutorial month; all four cards are wired. Do not write there.
- Start at Month 02, card id 4. 44 cards from there.
- A beat is a string edit, never a code edit. `intrusion[0]` non-NULL makes the card
  playable; that is `story_beat_has_protocol()`, the only switch.
- ASCII only (fonts load codepoints 32-126). Nothing wraps: budgets are in
  `plexus/roadmap_story.md` section 5.

## 4. Month 01 card 4: the doorway into the story

Today it is the Koi-Koi tutorial on its own hard-coded branch. It should become a simple
match that opens the story. This needs a branch ABOVE the `== 3` test (the note in
`AGENTS.md` warns exactly about this), so it goes last, after the story text exists.

---

## Settled - do not reopen

- Menus: TITLE -> MAIN -> PANCRATIUM / ATRIUM PLEX. Max four buttons, BACK on every
  screen. The structure is done; it does not need another redesign.
- Deus stays open. `DEUS_UNLOCKED_FOR_TESTING` is 1 on purpose.
- Action Card mode is frozen (`src/ui_action.*`, plus the influence points listed by
  `./tools/action_guard.sh --report`).
- KK-ADD-10 is the house rule and stays as it is.
- PC only. No controller, no touch layout, no mobile web.
- The browser build stays slim; it ships `menu.mp3`, `game1.mp3`, `game3.mp3`
  (`WEB_MUSIC` in `build.sh`). Windows ships everything.
- Licence: MIT, see `LICENSE`. Music is mine, art is AI-generated under my direction.

## How to check a change

`./build.sh test` - engine suite, Koi-Koi trigger, guards, table UI, music/Options.
`./tools/action_guard.sh` - Action Card untouched.
`./tools/tests_guard.sh` - `tests/` untouched.
`python3 tools/gen_wiki_tables.py --check` - docs in step with the engine.

A visual claim that was not seen on a screen is written down as not verified. The build
sandbox has no display; mine does.
