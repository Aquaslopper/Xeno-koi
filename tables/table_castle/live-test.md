# Live test — `<table>`

The record of every time this table was **ported into the game and played**. One entry per test,
newest first. It is the only place that says what broke in the real thing, so it has to be specific:
a symptom with a location, not a mood.

Why this file exists: a preview lies in ways a real round does not — a frame can sit perfectly on a
synthetic base and still land on the game's own text, a bezel can look right at rest and wrong while a
card flies, a panel can read well on the backdrop and vanish behind a glow. Those only show up live,
so they get written down here and become the next forge pass's work list.

---

## Template — copy this block per test

### `<YYYY-MM-DD>` · build `<short commit>` · `<beta | full>` port

- **Table id / selector:** what was picked, and where (Pancratium picker, `src/campaign.c:121`)
- **Tested:** which flows — deal, capture, koi-koi call, combo panel, drag/hover, pause/restart,
  window scaled, Action Card mode
- **Worked:** one line, so a regression is visible next time
- **Broke:** per item — *symptom* → *where* (rect / function / screen) → *expected*
  1.
  2.
- **Verdict:** forge again / accept / abandon
- **Next action:** what the next pass changes, in one line

---

### 2026-10-30 · beta port (`2378423`) · owner live test #1

- **Table id / selector:** `CASTLE (BETA)` — Pancratium → CARD TABLE picker, index 2
  (`settings.table_design`, `TABLE_DESIGN_CASTLE`). CRT = 0 and `CITY (BETA)` = 1 unchanged.
- **Tested:** played in-game — dealer select, capture lanes + quick-inventory hover, field/hand,
  PAUSE/RESTART hover, the skin in the Plex modes (Action Card, Speedcoin), switching back to
  CRT. (At port time: `./build.sh test` green and every `src/*.c` warning-free against raylib
  5.5 headers; no in-game screenshot exists from the build sandbox — no display/GL.)
- **Worked:** dealer select lands spot on; switching back to CRT is clean; the Speedcoin clock
  nests perfectly over the CPU panel art; all live text/numbers/cards in their right places.
- **Broke:**
  1. *Bezel on the quick-inventory preview* → `capture_info_draw`'s 106×172 card
     (`ui_capture_info.c`) wore a bezel over the CRT panel chrome → expected: no frame on a
     presentation draw; the old panel itself is fine.
  2. *Bezel too long, card on top* → the bezel's opaque leather mat (cavity 161.3 px around the
     143 px card) showed past the card → expected: hollow frame with the card behind the
     border (owner's words), like a picture in a frame.
  3. *PAUSE/RESTART hover box offset* → `ui_main.c` redraws a hitbox-sized fill+border on
     hover; inside the wider bronze cavity it reads as shifted right → expected: no box under
     castle (the casing is the box), keep the hover label.
  4. *Noted, not a break:* the skin follows the board into every mode (Plex, Action Card,
     Speedcoin) — one board renderer everywhere. Kept as-is; restricting it to Pancratium
     matches stays an owner option.
- **Verdict:** forge again (fine-tune). Fixes for 1–3 applied the same day; awaiting re-test.
- **Next action:** owner re-tests. Fixes in this follow-up: the fitter clears bezel cavities
  to alpha 0 (`cavity_cleared` in `final/_manifest.json`) and bezels now draw **over** their
  card at cavity height ≈ card height × 0.99 (the narrower cavity aspect makes the bronze lip
  overlap the long edges by ~2–3 px — comparison in `preview/castle-bezel-hollow.png`);
  the capture-info preview mutes the bezel (`board_set_castle_bezel_muted`); PAUSE/RESTART
  hover draws only the label under castle. Still open from pass 1: long status lines overflow
  the plaque (owner has not flagged it yet), timer crosses the plaque's top rail, turn cue is
  label brightness only (questions 2–3 in `README.md`), stock pile layers.

---

## Known-good reference

Beta port landed 2026-10-30 (see entry above): `final/` built by
`./tables/build.sh table_castle`, runtime copies in `assets/`, skin behind
`board_castle_skin_active()`, selectable as `CASTLE (BETA)`. Played once by the owner the same
day — dealer select, mode coverage and CRT switch-back confirmed good; three fine-tune fixes
(hollow bezels, preview mute, hover box) are in and awaiting the re-test.
