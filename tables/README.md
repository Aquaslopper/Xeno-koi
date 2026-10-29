# `tables/` — where a table is built

One layout, nine skins: each backdrop gets its own palette and casing profile. Nothing here touches
`src/`; the numbers all come from it.

**One table = one folder.** Everything built for a table — spec, manifest, raw renders, cut units,
previews, examples — stays inside `tables/table_<name>/`. Nothing about one table is written
elsewhere.

```
tables/
  README.md  WORKFLOW.md  CHECKLIST.md     how to work
  anchors.md  anchors.json                 the numbers, with their source lines
  build.sh                                 anchors-vs-src check + redraw

  table-to-scale.png   the whole table at 1:1 + legend      generated
  base.png             composite base, live geometry, flat  generated
  palettes.png/.json   the 8 backdrops, sampled             generated

  table_city/                              one table — spec only, not ported
    README.md      the spec
    live-test.md   every beta port and what happened live (empty so far)
    refs/          city-example-01.jpg, city-example-02.jpg    direction, not evidence
    base.png       composite base on assets/city.jpg           ./tables/build.sh base city
    table.json     the manifest                                (not written yet — no art)
    raw/ final/ preview/                                       written by tools/table_art.py

  _template/                               cp -r when a table starts — never ahead of time
    README.md      the spec template
    table.json     the manifest template (doubles as its schema)
    live-test.md   the live-test record template
```

## Build

```sh
./tables/build.sh                  # anchors-vs-src check, redraw map + palettes
./tables/build.sh base city        # composite base -> tables/table_city/base.png
./tables/build.sh svg              # + vector version of the map
./tables/build.sh table_city       # build that table's art from table_city/table.json
python3 tools/table_map.py check   # only the drift check (exit 1 on drift)
```

`tools/table_map.py` owns geometry (map, base, palettes). `tools/table_art.py` owns art (cut-out,
asymmetric borders, card bezels, rails) and writes `final/` + `preview/` inside the table's own
folder — one definition of where a card is, and never a second copy.

## The tables

| folder | backdrop | state |
|---|---|---|
| `table_city/` | city (4) | spec + examples. No art, not ported, no live test |

A table's folder is only listed here once the folder exists; the state is the short truth
(`spec` → `beta ported` → `forged` → `abandoned`), and its detail lives in that folder's `README.md`
and `live-test.md`.

## Cleaning and cutting the artwork

Each `assets_<name>/` (and `assetes_castle/`) holds the full-table render at 1920 × 1080 — the
1280 × 720 canvas at 1.5×. Two steps turn one of those into something placeable, both in
`tools/table_slice.py`:

```sh
python3 tools/table_slice.py tables/assets_city     # clean + cut, with a report
python3 tools/table_slice.py --all                  # every asset folder
python3 tools/table_slice.py tables/assets_city --pad 24   # more breathing room
```

It writes, inside the folder it was given and nowhere else:

```
<name>_clean.png    the whole artwork, its baked-in text removed, same size as the source
pieces/NN-<area>.png  one piece per game area, padded, transparent
pieces.json         every piece's box in the source, its canvas placement, the rect it belongs to
preview/pieces.png  contact sheet of the pieces
preview/placed.png  the pieces put back at their own positions on the real backdrop
```

Three things it asserts instead of eyeballing:

- **the text is gone, the metal is not.** A glyph is a small bright mark over dark metal; bigger
  and brighter things (frames, cables, rivet clusters) fail the size test and stay. The emptied
  glyph is filled from the metal around it, not blurred — a blur puts the glyph back into its own
  hole. It never erases where the panel is bright, so nothing bright gets eaten.
- **no piece takes a neighbour's pixels, and none are left over.** `pieces.json` reports
  `lost_px: 0`.
- **the pieces rebuild the artwork.** `recompose_max_diff: 0`, i.e. pasting them back at their own
  boxes gives the cleaned original pixel for pixel. A cut that does not rebuild cannot be placed.

Cuts run only on the game's own area edges (`tables/anchors.json`), never tight around the art: a
piece keeps the art's outer ornament plus padding into the empty space around it, so nothing reads
as chopped when it is scaled into place. A gap between two cards in a row is **not** an area edge
and is not a cut — that would leave a piece with no rect to belong to.

## The forge loop

A table is made here and ported into the game, not the other way round:

```
 forge ─────────► beta port ─────────► live test ─────────► back to the forge
 tables/table_x/  src/, selectable      written into          fixes its README + live-test.md
                  in the Pancratium     live-test.md
```

A table is forged when a real round found nothing left to fix. The Pancratium picker
(`src/campaign.c:121`) is the door it comes in through. Details in `WORKFLOW.md`.

## Rules

1. **Anchors come from `src/`**, never from an example, a mockup or a screenshot. `build.sh` asserts it.
2. **An example is not evidence.** `refs/` shows a direction; it never states a number, a position, or
   what the build does today. "The game doesn't look like the example" is not a finding. See trap 15.
3. **Measure all four sides** before choosing a border. Uniform borders come out thin.
4. **Assert, don't eyeball**: interior == target rect ±2 px, checked in code; previews on live geometry.
5. **Never composite on `docs/assets/readme/table-map.png`** — it is drawn with dead code (96 px field
   pitch instead of 81). Use a `base.png`.

## Status

Two art passes were generated and thrown away (29 Oct 2026). Nothing is kept but the data, the traps
and the tools. **No table is implemented; `src/` is untouched.** `table_city` is a spec with examples.
Next: the five open decisions at the end of `WORKFLOW.md`.
