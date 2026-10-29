# `tables/` — where a table is built

One layout, nine skins: each backdrop gets its own palette and casing profile. Nothing here touches
`src/`; the numbers all come from it. Open `table-to-scale.png` first.

```
tables/
  table-to-scale.png    the whole table at 1:1 + legend        generated
  base.png              composite base, live geometry           generated
  palettes.png/.json    the 8 backdrops, sampled                generated
  anchors.json          every rect, fill and budget, as data
  anchors.md            the same numbers, with the source line beside each
  WORKFLOW.md           8 steps, 15 traps, 5 open decisions
  CHECKLIST.md          the gate to run before presenting
  build.sh              anchors-vs-src check + redraw

  designs/  _schema.json  manifest for tools/table_art.py
            _template.md  copy to <name>.md to start a table
            city.md       table_city: spec only, no art

  refs/     city-example-01.jpg  city-example-02.jpg   examples, not evidence
```

## Build

```sh
./tables/build.sh                  # check anchors, redraw map + palettes
./tables/build.sh base city        # + composite base on assets/city.jpg
./tables/build.sh svg              # + vector version of the map
python3 tools/table_map.py check   # only the drift check (exit 1 on drift)
python3 tools/table_art.py build tables/designs/<name>.json   # art: cut, size, compose, assert
```

`tools/table_map.py` owns geometry (map, base, palettes). `tools/table_art.py` owns art (cut-out,
asymmetric borders, card bezels, rails) and composites on that base — one definition of where a card
is, and never a second copy.

## Rules

1. **Anchors come from `src/`**, never from an example, a mockup or a screenshot. `build.sh` asserts it.
2. **An example is not evidence.** `refs/` shows a direction; it never states a number, a position, or
   what the build does today. "The game doesn't look like the example" is not a finding — that is what
   an example is for. See trap 15.
3. **Measure all four sides** before choosing a border. Uniform borders come out thin.
4. **Assert, don't eyeball**: interior == target rect ±2 px, checked in code; previews on live geometry.
5. **Never composite on `docs/assets/readme/table-map.png`** — it is drawn with dead code (96 px field
   pitch instead of 81). Use `base.png`.

## Status

Two art passes were generated and thrown away (29 Oct 2026). Nothing is kept but the data, the traps
and the tools. **No table design is implemented; `src/` is untouched; `assets/tables/` does not exist.**
Next: the five open decisions at the end of `WORKFLOW.md`.
