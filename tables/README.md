# `tables/` — where a table is built

One layout, nine skins: each backdrop gets its own palette and casing profile. Nothing here touches
`src/`; the numbers all come from it.

**One table = one folder.** Everything built for a table — spec, manifest, raw renders, cut units,
previews, examples — stays inside `tables/table_<name>/`. Nothing about one table is written
elsewhere.

```
tables/
  AGENT_START.md                           an agent's first file: read order, hard rules
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
| `table_castle/` | castle (7) | beta ported (`CASTLE (BETA)`, index 2); owner live-tested — `table_castle/live-test.md` #1–#6; owner-provided card bezel accepted |
| `table_city/` | city (4) | spec + examples. No art, not ported, no live test |

A table's folder is only listed here once the folder exists; the state is the short truth
(`spec` → `beta ported` → `forged` → `abandoned`), and its detail lives in that folder's `README.md`
and `live-test.md`.

**Runtime assets: one folder per map.** The game reads a table from `assets/maps/<name>/` —
`overlay.png` (the static casing, 1280×720, drawn over the dimmed backdrop and under every live
element) and `bezel.png` (the one card frame, transparent inside, drawn under and over every
card at card scale). `assets/maps/<name>/` is the shipping copy; `tables/assets_<name>/` stays
the untouched owner delivery and `tables/table_<name>/final/` the built result. Since 30 Oct 2026
all seven map slots exist: `castle` is the forged pair, the other six hold copies of it until
their own maps are delivered.

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
6. **Delivered art is never 9-sliced or squished.** Finished pieces in `tables/assets_<name>/`
   (1920×1080 = 1.5× canvas) are measured compartment by compartment and fitted by cut-and-place:
   one uniform ×⅔ downscale, translation after that, a further uniform scale only where a measured
   cavity demands it — minimal, and recorded. `stamp()` is for generated single frames; a
   `cut-and-place` manifest builds through `build_cut_and_place` in `table_art.py` (crop, one
   uniform downscale, translate — never `stamp()`). See `WORKFLOW.md` → *Delivered art*.

## Status

Two art passes were generated and thrown away (29 Oct 2026). Nothing is kept but the data, the traps
and the tools. One table is implemented: `table_castle` (started 30 Oct 2026), the first with
finished art — owner-delivered pieces in `assets_castle/`, measured against the anchors and fitted by
cut-and-place (its internal rhythm is the generator's, not the game's, so no piece drops in whole) —
is **beta-ported**: selectable as `CASTLE` in the Pancratium (index 2), skin behind
`board_table_skin_active()` with CRT unchanged. Since 30 Oct 2026 the picker carries eight
entries — `CRT` (0, no skin), then one slot per map: `CITY` 1, `CASTLE` 2, `HOLE` 3,
`ARCTIC` 4, `HARVEST` 5, `FACE` 6, `REALM` 7. Slots 1 and 3–7 are **castle duplicates**: each
loads its own `assets/maps/<name>/{overlay,bezel}.png`, currently byte copies of the castle
pair, so the owner can replace one map's two files and that slot changes on its own. DEUS has
no slot (background 5 is his). It has been through six owner live tests
(`table_castle/live-test.md` #1–#6): the card bezel is **owner-provided** and accepted
("now we got it right", 30 Oct 2026), and the right-column tower is aligned on one outer-left
edge after the owner's screenshot showed the RND casing's left rail indented. What that loop
taught the forge is written into `WORKFLOW.md` (Delivered art, traps 16–19) and `CHECKLIST.md`;
an incoming agent starts at `AGENT_START.md`. `table_city` is a spec with examples. The five open decisions at the end
of `WORKFLOW.md` still stand for the other tables.
