# Live test — `table_castle`

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

### 2026-10-30 · owner-provided bezel (`daf55e4`) · owner live test #6 — screenshot verdict

- **Table id / selector:** `CASTLE (BETA)`, as before.
- **Tested:** full in-game screenshot (1920×1080) from the owner's machine, taken as
  ground truth and measured — RND tower, bezels on 8 hand cards + 8 CPU backs, field
  frames, stock, status plaque, PAUSE/RESTART.
- **Worked:** the owner-provided bezel — owner's words: *"now we got it right"*. Every
  hand card and CPU back wears it (fronts and backs, corner gems whole, card art clear);
  the field frames interlock readably; the stock reads as a stack of framed backs with
  the count (24) centred; the status line (440 px) fits inside the plaque rails;
  PAUSE/RESTART labels sit centred. One watch item only: the dim CPU SCORE text sits
  close to the panel's top-left corner ornament.
- **Broke:**
  1. *RND box still a little offset* → measured on the shipped overlay: cavity centres
     already agree (RND 1205.5 vs CPU 1205.0 / YOUR 1206.0), but the casing **left rails
     stagger** — RND x 1139 against combos x 1134/1135 — and in one contiguous tower the
     eye tracks the left rail line → expected: one shared outer-left edge for the tower
     (WORKFLOW trap 18).
- **Verdict:** bezel accepted; forge again for the tower only. Fix applied same day.
- **Next action:** owner re-tests. Fix: the rnd+combos tower shares **outer-left x 1134**
  in the record (rnd moves 5 px left; right edges stagger 1270 vs 1276 into the
  screen-edge ornaments), finals + overlay rebuilt, mode lines centre on the RND cavity
  cx **1200.9**. Combo lists stay left-aligned; PAUSE/RESTART island untouched. The
  lessons of this test series went into the workflow docs the same day (`AGENT_START.md`;
  WORKFLOW Delivered art + traps 18–19; CHECKLIST).

### 2026-10-30 · flood-cleared bezel (`e590b0b`) · owner live test #5 — bezel provided

- **Table id / selector:** `CASTLE (BETA)`, as before.
- **Tested:** the flood-clear fix from #4 — and, in parallel, the owner's own hand-made
  `bezzel.png` (95×136, transparent inside, only the border).
- **Worked:** the flood-clear proved the border could survive (6.9 % → ~50 % solid,
  `preview/castle-bezel-transparent.png`) — and was then superseded same day: the owner
  provided the bezel as finished art. Settlement (owner's words, now workflow law): the
  transparent card bezel is **something the owner provides**; there needs to be one card
  bezel for every such table.
- **Broke:** nothing new live; the shipped bezel was manufactured by the wrong side.
- **Verdict:** ship the provided art, untouched.
- **Next action:** done in `daf55e4`: `assets_castle/castle_card_bezel.png` passed
  through by the fitter (pixel-identical, verified), its transparent cavity measured
  from alpha (cavity x 10..89, y 6..128; centre 49.5, 67.0; height 122) and mirrored
  into `ui_board.c`; ratio 0.94 kept; sandwich draw unchanged. Owner re-tests.

### 2026-10-30 · fine-tune #3 (`c16cd27`) · owner live test #4 — "transparent? NO"

- **Table id / selector:** `CASTLE (BETA)`, as before.
- **Tested:** deeper-grip sandwich (ratio 0.94) and tower cavity-centre axis from #3.
- **Worked:** the draw order (bezel → card → bezel) verified in source; numbers and
  placements as recorded. It still read wrong — see below.
- **Broke:**
  1. *Card still on top — and the bezel was never the bezel described* → an A–D variant
     sheet was rejected wholesale; owner's instruction: "Make the bezel transparent in
     the middle, only keep the borders, than applied it on the back and on the front."
     Opening the produced file on a checkerboard found why nothing worked: the shipped
     "hollow" bezel had been made by clearing a **rectangle** through it, which chopped
     the inward-sweeping corner ornaments and left a crumbling hairline (6.9 % solid) —
     and its transparency had been reported as fact for two rounds from centre-alpha
     numbers alone, without opening the asset. Recorded as WORKFLOW trap 17: *never
     state a property of an asset without opening the produced file in the same pass.*
- **Verdict:** forge again; the bezel had to be re-made the right way.
- **Next action:** centre-seeded flood-clear of the strip frame (border ring survives,
  incl. the ornaments sweeping in; RGB zeroed under cleared alpha), verified on a
  checkerboard (`preview/castle-bezel-transparent.png`); the fitter now governs every
  cut-and-place bezel this way. Owner responded by providing the bezel by hand (#5).

### 2026-10-30 · fine-tune #2 (`22a31b4`) · owner live test #3

- **Table id / selector:** `CASTLE (BETA)`, as before.
- **Tested:** re-test — sandwich bezels on faces, RND box centring from #2.
- **Worked:** the sandwich construction itself (backs still read correct); the grip was too
  shallow, see below.
- **Broke:**
  1. *Card still sits on top of the bezel* → at cavity height ≈ card height (× 0.99) the
     top/bottom rails overlap the card by <1 px and the sides by ~3 px, so the frame reads
     as a border around the card, not holding it → expected: the **card in the sandwich** —
     bronze visibly in front on every side.
  2. *RND box still offset right* → it was the box, not its text: outer-right placement
     puts the narrower RND cavity's centre ~5 px right of the combo panels' axis (left edge
     ~10 px right of theirs) inside one contiguous tower → expected: one centre axis.
- **Verdict:** forge again (fine-tune); fixes applied the same day, awaiting re-test.
- **Next action:** owner re-tests. Fixes: bezel ratio × 0.99 → **× 0.94** (the bronze band
  now lies mostly over the card — lip ~5 px at hand size, proportional elsewhere — while
  the silhouette stays just proud of the card; floor ~0.927. Comparison:
  `preview/castle-bezel-grip.png`); the rnd+combos tower's cavities aligned on **cx 1205.3**
  in the record (rnd moves 5.4 px left), finals + overlay rebuilt, and the mode lines now
  centre on that same axis. Buttons keep outer-right placement (separate island, labels
  game-centred at 1216) — flag if they now read off-axis against the tower.

### 2026-10-30 · beta port + fine-tune #1 (`85f5a41`) · owner live test #2

- **Table id / selector:** `CASTLE (BETA)`, as before.
- **Tested:** re-test after fine-tune #1 — hollow bezels on faces and backs, the RND/ruleset
  box, terminal-colour harmony.
- **Worked:** bezel direction confirmed (hollow, over the card); card backs read fully
  framed; the capture-info preview and PAUSE/RESTART hover fixes from #1 stand. Amber
  terminal colour makes the text read as one with the bronze — kept in mind (below).
- **Broke:**
  1. *Face side still above the bezel* → the delivered frame art is soft-alpha, so one pass
     leaves the frame faint against bright faces → expected: bezel applied **twice**, under
     and over the card (sandwich), only then does the border fully apply.
  2. *RND/ruleset content right-of-centre* → the mode cavity measures x 1151.7..1269.7, so
     text left-aligned at the game's x 1168 parks its empty margin on the left → expected:
     the two lines centred in the box.
- **Verdict:** forge again (fine-tune); fixes applied the same day, awaiting re-test.
- **Next action:** owner re-tests. Fixes: sandwich bezel in `board_draw_card` /
  `board_draw_card_back` (comparison: `preview/castle-bezel-sandwich.png`); mode lines
  centred on cavity cx 1211 under castle (`label_centered_x`, CRT untouched). **Amber, for a
  later owner decision:** DEUS already forces its theme colour per session
  (`storage.c`: `terminal_color 5` in memory, snapshot restored on save) — castle could
  force amber (1) while selected the same way. Not implemented; setting amber manually in
  Options already gives the effect.

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

Beta port landed 2026-10-30 and went through six owner live tests the same day: dealer
select, mode coverage (Plex/Action/Speedcoin) and CRT switch-back confirmed good from #1;
capture-info preview muted, PAUSE/RESTART hover box dropped, mode text centred. The card
bezel is **owner-provided** art (`assets_castle/castle_card_bezel.png`), drawn under and
over every card, and accepted live ("now we got it right", #6). The rnd+combos tower is
aligned on one outer-left edge (x 1134) after the owner's screenshot showed the RND left
rail indented; CRT unchanged throughout. Still open: README questions 2–3 (status
overflow, turn cue) and the amber-theme owner decision noted in #2.
