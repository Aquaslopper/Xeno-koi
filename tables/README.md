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

  table_city/                              one table
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
