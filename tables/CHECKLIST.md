# Checklist — run this before showing anyone the table

Every line below is a mistake that has already been made once. Tick in order; the order matters,
because an early "yes" that is really a "no" invalidates everything under it.

## A. Before drawing anything

- [ ] `./tables/build.sh` is clean — **anchors still match `src/`** (the `check` step exits non-zero
      on drift; do not work around it, re-derive the number and update `anchors.json`)
- [ ] the numbers I am about to use all come from `anchors.json`, not from a mockup, a screenshot,
      or the picture the user drew on
- [ ] I opened `tables/table-to-scale.png` and know where the panel is and how much free space it has
      **on all four sides**
- [ ] I decided the layer: **runtime restyle** (change the draw function, gets every rect exactly) or
      **stamped art** (a PNG at an anchor). A plain rectangle is always runtime.
- [ ] I am not treating an **example** as evidence: nothing I am about to assert came from a mockup
      (`tables/refs/`), and if a mockup's layout disagrees with `src/`, `src/` is right and the mockup
      is an example
- [ ] I know what the game already paints there (`anchors.json → fills`): black @ 178, info strips
      112 idle / 205 active, and **nothing at all** behind the field

## B. While drawing

- [ ] every frame is drawn **outside** its rect; its open interior equals the game's rect ±2 px
- [ ] all four borders are as thick as the free space allows — not the minimum that fits
- [ ] neighbouring panels have comparable weight (check them against each other, not alone)
- [ ] no per-lane frame (`37.4` tall, `4` px gaps, `4` px label inset — impossible)
- [ ] rails/seams are drawn **after** the casing they sit on
- [ ] connective art is on the **canvas edge**, never a spine through the middle of the left side
- [ ] metal is opaque, cavities are alpha 0, **the interior is cleared hard** (no 50–200 haze)
- [ ] opaque coverage is 40–55 %; low opaque + high mid = the key went soft, redo the cut-out
- [ ] every card size that moves is framed: field 76×124, hand 88×143, cpu 56×91 — minis 17×31 are not

## C. Verifying (this is the part that gets skipped)

- [ ] `python3 tools/table_map.py check` → `anchors in sync`
- [ ] previews are composited on **`tables/base.png`** or `base --backdrop <name>`, never on
      `docs/assets/readme/table-map.png` (dead 96 px layout — see `anchors.md`)
- [ ] one preview on the real backdrop **and** one on the flat canvas, both looked at
- [ ] card frames checked at the **real pitch** (field 81 px, hand 96, cpu 62) — a frame that looks
      right at a made-up pitch is not verified
- [ ] I can point at the exact pixel run of any claim I am about to make ("opaque from x 21 to 54",
      not "it's on the edge")
- [ ] the numbers I quote to the user are the measured ones; anything assumed is labelled assumed

## D. Before presenting

- [ ] previews are **standalone files** in the repo (not `/tmp`), and I presented the main one
- [ ] I said which of the user's points are fixed, one by one, in their words
- [ ] what is still wrong is stated first, not left for them to find
- [ ] `git diff --check` clean; `src/` untouched unless step A said runtime restyle
- [ ] if `src/` changed: `./build.sh test` green, and `./tools/action_guard.sh --report` for any
      shared draw function (Action Card mode is hash-frozen — 12 pinned lines outside `ui_action.*`,
      4 of them inside `board_draw_table()`)

## The one question that catches the most

> Can I show, with a number, that this frame is where I say it is?

If the answer is a picture and not a number, it is not verified yet.
