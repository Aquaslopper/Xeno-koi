# `tables/` — where a table is built

One folder for table designs: what the game actually draws, what a design wants to change, and the
tools that keep both honest. Open `table-to-scale.png` first.

## Look at these

| file | what it is |
|---|---|
| **`table-to-scale.png`** | the whole table at **1:1** (1280 × 720 + legend): every rect the game draws, the card pitches, the free space, the frozen anchors. Generated. |
| `table-to-scale.svg` | the same, vector — zoom it, annotate it, hand it to someone |
| `base.png` | the composite base: live geometry, the game's own fills, real cards from the atlas, no art of ours |
| `palettes.png` | all eight backdrops side by side with their sampled mean / highlight / swatch strip |
| `palettes.json` | those numbers, per backdrop |

## Read these

| file | what it is |
|---|---|
| `anchors.md` | every rect, gap, fill and rule a design may rely on, each with its source line |
| `anchors.json` | the same as data: rects, fills, card rects, budgets, free space, frozen anchors |
| `WORKFLOW.md` | **the procedure** — 8 steps, then 14 traps with the numbers that caused them |
| `CHECKLIST.md` | the gate to run before showing anyone a table |
| `TABLE_TEMPLATE.md` | copy this to `NN-<name>.md` to start a design |
| `refs/` | **examples** — inspiration only, never a measurement (read `refs/README.md`) |
| `designs/_schema.json` | manifest format for `tools/table_art.py build` (art units, rails, bezels) |
| `build.sh` | one command: anchors check + redraw map + redraw palettes |

## Designs

| file | state |
|---|---|
| `01-reliquary.md` | generic metal-bezel look, spec only |
| `02-table-city.md` | table_city: palette, budgets, card bezels. No art — the second pass was thrown away |

## Build

```sh
./tables/build.sh                       # anchors-vs-src check, redraw map, redraw palettes
./tables/build.sh base city             # + composite base on assets/city.jpg
python3 tools/table_map.py check        # just the drift check, exits non-zero on drift
python3 tools/table_art.py build tables/designs/<name>.json   # art: cut, size, compose, assert
```

Everything generated goes through the two tools under `tools/`, reads `tables/anchors.json`, and
never touches `src/`. `table_map.py` owns geometry (map, base, palettes); `table_art.py` owns art
(cut-out, asymmetric borders, card bezels, rails) and composites on the base, so there is exactly
one definition of where a card is.

## Rules

1. **Anchors come from `src/`**, never from a mockup or a screenshot. `./tables/build.sh` asserts it.
2. **Measure all four sides** of a panel before choosing a border. Uniform borders come out thin.
3. **Assert, don't eyeball**: the open interior of a frame equals the target rect ±2 px, checked in
   code, and the preview sits on live geometry.
4. **An example is not evidence.** Pictures in `refs/` show a direction; they never state a number,
   a position, or what the build does today. "The game doesn't look like the example" is not a
   finding — it is the point of the example. Facts come from `src/` and `anchors.json`. See
   `refs/README.md`, and trap 15 in `WORKFLOW.md`.
5. **Never composite on `docs/assets/readme/table-map.png`.** That figure is drawn with
   `board_field_slot_xy()`, which has no callers; the live field is `table_slot_rect()`, 81 px pitch
   against the figure's 96. Use `base.png`. Details in `anchors.md`.

## The idea

Every backdrop gets its own UI: eight backdrops plus the Deus variant, so **nine tables — one
layout, nine palettes and casing profiles.** The backdrop supplies the palette; the layout never
moves.
