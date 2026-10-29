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

## Known-good reference

Nothing has been ported yet. `table_city` is still a spec: no `final/` set, no beta port, no entry
here. The file starts being used at the first port.
