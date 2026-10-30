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

## Port status (this session)

Beta port complete: `TABLE_DESIGN_CITY` renders `assets/table_city_overlay.png` (1280×720) as the
forged casing above the dimmed backdrop and below all live elements; `assets/table_city_bezel.png`
draws as the per-card bezel sandwich in `board_draw_card` / `board_draw_card_back`. CRT panel
fills/hairlines are gated off while city is active (lanes, combo panels, pause/restart buttons,
hover boxes, capture-info bezel mute), mirroring the castle pattern (`src/ui_board.c`,
`src/ui_main.c`, `src/ui_capture_info.c`). Picker reads "CITY (BETA) · FORGED - IN LIVE TEST".

All of `tables/table_city/final/` (17 unit PNGs + overlay + `_manifest.json`) and both previews
were built in pass 1 as a **pure uniform resize ×0.6667** of the 1920×1080 art onto the 1280×720
canvas — no cut-and-place, no per-compartment rescale, no squash (tables/table_city/README.md).

## Known issues from pass 1 (pure resize) — expect these live, do not file as new bugs

The right column of the delivered art does not line up with the game's own anchors under a pure
resize, so the overlay's right-column panels sit offset from the live hitboxes beneath them:

1. **pause / restart ~200 px above their hitboxes.** Overlay pause cavity at canvas (1139,366,
   123×44) and restart at (1139,402,123×47); the game draws PAUSE/RESTART buttons at
   (1160,560,112×40) / (1160,610,112×40). Under pure resize the overlay text/frame is ~200 px
   above where the clickable buttons actually are. Symptom: clicking where the overlay says
   PAUSE does nothing; the real button is lower. Location: `board_draw_table` right-column.
   Expected: either accept the offset (owner decision A) or re-place by translation only
   (owner decision B) — see tables/table_city/README.md "alignment problem".
2. **stock_frame cavity larger than anchor.** Overlay stock_frame cavity (312,232,114×182) vs
   anchor (76×124). A stock card drawn inside looks small in the frame. Location:
   `board_draw_table` stock slot.
3. **status plaque shifted vs timer.** Overlay status at (351,539,664×93); pure-resize delta dx
   −24 / dy +45 vs anchor. Location: round timer / status area.
4. **bezel cavity ratio ~0.75 vs castle's accepted 0.94.** Port draws the city bezel at ratio
   0.94 (`board_draw_city_bezel`) so side rails sit slightly proud of the card; confirm grip
   live (cavity is narrower than the card: 99×190 vs ~88×143). Location: card frames.

## Known-good reference

First beta port: overlay draws at (0,0) behind all live elements, bezel sandwich renders on
face/back cards, lane/button/combo CRT fills gated off, capture-info preview mutes the city
bezel. Headless tests (`./build.sh test`) pass: engine, action_guard manifest, table UI,
music folder. Right-column alignment per the known issues above.
