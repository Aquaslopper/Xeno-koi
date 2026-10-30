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

### 2026-10-30 · beta port · **NOT YET PLAYED**

- **Table id / selector:** `CASTLE (BETA)` — Pancratium → CARD TABLE picker, index 2
  (`settings.table_design`, `TABLE_DESIGN_CASTLE`). CRT = 0 and `CITY (BETA)` = 1 unchanged.
- **Tested:** build only. `./build.sh test` green (tests lock, Action Card lockdown, engine
  500-match suite, headless table-UI / combo-panel / capture-info tests, music/settings), and
  every `src/*.c` type-checks warning-free against raylib 5.5 headers. **No in-game screenshot
  exists**: the build sandbox has no display/GL, so nothing was seen running. Every visual
  claim below is unverified until played.
- **Worked:** —
- **Broke:** — (nothing played yet)
- **Verdict:** awaiting owner live test.
- **Next action:** owner plays a round on `CASTLE (BETA)` and fills this entry in. Watch list,
  from the pass-1 measurements (`README.md` → Port notes):
  1. Long status lines (569–626 px) overflow the 439 px plaque symmetrically — readable?
  2. Turn timer glyphs cross the status plaque's top rail.
  3. Turn cue on the score strips is label brightness only; the CPU strip has no baked `>>`.
  4. Stock pile: up to 6 bezeled card backs layer inside the static stock frame.
  5. Bezels on interlocked field/hand rows: later cards' frames overlap earlier neighbours
     (the preview drew bezels as one global layer; the game draws per card).
  6. Bezels mid-flight and mid-flip (width follows the squeeze) — deal, stock flip, captures.
  7. Hovered hand card (y 552) lifting its bezel into the status plaque's bottom ornament.
  8. PAUSE/RESTART: hover hairline shows the true hitbox inside the wider bronze lip.
  9. Quick-inventory hover panel unchanged and clear of the casing (band 304..504).
  10. Combo sidebar text on leather; the paused combos-detail screen's cards wear bezels too.
  11. Speedcoin clock panel still draws its CRT fill over the CPU lane art.
  12. Dealer-select cards: bezels appear once the first table frame has run (skin flag is
      refreshed per table frame), so the very first dealer pick after choosing castle may
      briefly show CRT hairlines.

---

## Known-good reference

Beta port landed 2026-10-30 (see entry above): `final/` built by
`./tables/build.sh table_castle`, runtime copies in `assets/`, skin behind
`board_castle_skin_active()`, selectable as `CASTLE (BETA)`. Build and headless tests are
green; it has not been played yet. This section gets its first real entry after the owner's
first live test.
