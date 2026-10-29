# `NN` — table_`<name>`

**One table = one folder.** Copy this folder to `tables/table_<name>/` and everything for that table
lives here — spec, manifest, raw renders, cut units, previews, examples. Nothing about one table is
written outside its folder.

```
tables/table_<name>/
  README.md      this file: the spec
  table.json     the manifest `tools/table_art.py` builds from (schema in this template)
  base.png       composite base on this backdrop      ./tables/build.sh base <name>
  raw/           generator output, untouched
  final/         cut out, borders sized, interior == the game's rect
  preview/       composites + the assertion table
  refs/          examples of the direction (optional)
  live-test.md   every beta port and what happened in the real game (starts empty)
```

The folder is created **when the table is started** — `cp -r tables/_template tables/table_<name>` —
and never scaffolded ahead of time. An empty folder is not a plan.

If the table turns out to be a **runtime-only** restyle (step 3), delete `table.json`, `raw/` and
`final/`: the look then lives in the draw functions, `live-test.md` is its record, and the beta port
is its preview.

Status: **spec / pass N / done.** `src/` touched: yes/no.

Backdrop `assets/<name>.jpg`, index **N** of
`{0 Harvest, 1 Hole, 2 Arctic, 3 Castle, 4 City, 5 Deus, 6 Face, 7 Realm}` (`src/ui_screens.c:326`).

Intent, one line: what this backdrop's architecture suggests, and which redesign points it answers.

## Direction

Read `refs/` for the **idea** only — never a number, never a position. Write the aim in words: the
shapes, how panels connect, the weight of the metal.

## Palette (`tables/palettes.json`, sampled — not eyeballed)

| | value | use |
|---|---|---|
| mean | | the felt; drawn art must be brighter |
| highlight p99 | | lit edges, rivet tops |
| mid | | frame body |
| accent | | seam glow |

## Budget (`tables/anchors.md` → free space; fill in the last column)

| group | bbox | L | T | R | B | uniform | chosen |
|---|---|---|---|---|---|---|---|
| CPU block | `12, 12, 255, 196` | 12 | 12 | 163 | 300 | 12 | |
| player block | `12, 508, 255, 200` | 12 | 300 | 163 | 12 | 12 | |
| right column | `1160, 12, 112, 522` | 80 | 12 | 8 | 26 | 8 | |
| buttons | `1160, 560, 112, 90` | 80 | 26 | 8 | 70 | 8 | |
| deck | `330, 262, 76, 124` | 63 | 54 | 24 | 98 | 24 | |
| status | `480, 518, 460, 34` | — | 84 | — | 14 | 14 | |

Fixed: no per-lane frame; score strips split from the lanes by 8 px rails at y 40 / y 670, drawn
after the casing; connectors on the canvas edge.

## Port

The picker already exists: `GameSettings.table_design` + the Pancratium row (`CARD TABLE:`). A new
skin takes the next free index in `k_table_names[]` (`campaign.c:24`), and rendering reads
`settings->table_design`. Nothing else about a port changes.

## Live test

The owner plays the beta port and the findings are written into `live-test.md` in this folder —
symptom, where, expected. Not in chat, not in memory: in the file, or the next pass rebuilds the fault.

## Layer

- [ ] runtime restyle — `ui_draw_panel_border()` `ui_theme.c:583`, `board_draw_card()` `ui_board.c:715`
- [ ] stamped art — `table.json` in this folder
- both, and why:

## Units

| unit | interior | border L/T/R/B | placed at |
|---|---|---|---|
| | | | |

## Open questions for the owner

1.
