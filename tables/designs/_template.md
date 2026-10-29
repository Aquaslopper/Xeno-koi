# `NN` — table_`<name>`

Status: **spec / pass N / done.** `src/` touched: yes/no.

Backdrop `assets/<name>.jpg`, index **N** of
`{0 Harvest, 1 Hole, 2 Arctic, 3 Castle, 4 City, 5 Deus, 6 Face, 7 Realm}` (`src/ui_screens.c:326`).

Intent, one line: what this backdrop's architecture suggests, and which redesign points it answers.

## Direction

Read the examples in `refs/` for the **idea** only — never a number, never a position. Write down
what is being aimed at in words: the shapes, the way panels connect, the weight of the metal.

## Palette (from `palettes.json`, sampled — not eyeballed)

| | value | use |
|---|---|---|
| mean | | the felt; drawn art must be brighter |
| highlight p99 | | lit edges, rivet tops |
| mid | | frame body |
| accent | | seam glow |

## Budget (from `anchors.md` → free space; fill in the last column)

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

## Layer

- [ ] runtime restyle — `ui_draw_panel_border()` `ui_theme.c:583`, `board_draw_card()` `ui_board.c:715`
- [ ] stamped art — manifest `designs/<name>.json`, schema in `_schema.json`
- both, and why:

## Units

| unit | interior | border L/T/R/B | placed at |
|---|---|---|---|
| | | | |

## Open questions for the owner

1.
