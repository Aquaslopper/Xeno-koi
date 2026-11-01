# NEXT

What I am working on, in order, and what is already settled. Short on purpose: a long
document is a document nobody reads. When an item is done, delete it and move the next
one up. When a new agent arrives, this file plus `AGENTS.md` is the whole briefing.

Last updated: 1 November 2026.

---

## 1. The Koi-Koi tutorial (card 3 of Month 01) reads badly

The first thing a new player sees. It works, but it is a big text panel and a flat dim:
no pointer at the card you must click, no spotlight on the thing being described.

Wanted: coach marks. Dim the board, keep one rectangle lit, draw a ring on the card the
step is about, and one short line saying what to do. Advance on the click, not on a timer.

Rule: this is presentation only. No phase, no timer, no input gate, no rule changes.
Card 2 (Action Card) and card 3 (Speedcoin) tutorials are the frozen and pinned modes -
their panels can share a helper, but nothing in their sequence, pacing or logic changes.

## 1b. D-badge tour — built, waiting on one look

The token leaves the table and guides the first visit to the Plex: eight stops, one
sentence each, click to advance, then it is gone (persisted as `badge_seen` in
`plexsave.txt`). Copy is informational on purpose — it reports, it never jokes.

Not yet seen on a screen. Tuning knobs, all in `src/ui_plexus.c`: the flight beat
(`s_badge_blend += dt / 0.34f`), the scrim alpha (120), panel size in
`badge_panel_rect()`, and `badge_home()` at (664, 412). If a stop's subject is
wrong, only `k_badge_stops[]` changes.

## 2. Hover a Plex bottom button, the briefing panel explains it

Six buttons: SAVE & BACK, LOGFILE, SPEEDCOIN, ACTION CARD, TRAINING, OPTION. Most players
never learn what Speedcoin or Action Card are. Hovering one should make the briefing card
say what it is and how it scores. Replaces the "dropup menu" idea: no hidden layer, no
hover fly-outs, one state instead of six.

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
