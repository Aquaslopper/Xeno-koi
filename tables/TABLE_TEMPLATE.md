# `NN` — table_`<name>`

Status: **spec / pass N / done.** `src/` touched: yes/no.

Backdrop: `assets/<name>.jpg`, background index **N** of
`{0 Harvest, 1 Hole, 2 Arctic, 3 Castle, 4 City, 5 Deus, 6 Face, 7 Realm}` (`src/ui_screens.c:326`).

One line of intent: what this backdrop's architecture suggests, and which of the 8 redesign points it
answers.

## Palette — sampled, never guessed

From `tables/palettes.json` (regenerate with `./tables/build.sh`).

| | value | use |
|---|---|---|
| mean | `#000000` | the felt; anything drawn must be brighter than this |
| highlight p99 | `#000000` | lit edges, rivet tops |
| mid | `#000000` | frame body, ribs |
| accent | `#000000` | seam glow |

Language: 3–5 nouns for the shapes this backdrop justifies (ribs, arches, cable bundles…).
Front-on and symmetric, or whatever the backdrop actually forces.

## Free space — the only place art can go

Copied from `anchors.json → free_space` / `budgets`, all four sides. A frame is drawn **outside** its
group, so the interior stays the game's rect.

| group | bbox | L | T | R | B | heaviest uniform border | chosen (asymmetric ok) |
|---|---|---|---|---|---|---|---|
| CPU block | `12, 12, 255, 196` | 12 | 12 | 163 | 300 | 12 | |
| player block | `12, 508, 255, 200` | 12 | 300 | 163 | 12 | 12 | |
| right column | `1160, 12, 112, 522` | 80 | 12 | 8 | 26 | 8 | |
| buttons | `1160, 560, 112, 90` | 80 | 26 | 8 | 70 | 8 | |
| deck | `330, 262, 76, 124` | 63 | 54 | 24 | 98 | 24 | |
| status | `480, 518, 460, 34` | — | 84 | — | 14 | 14 | |

Rules that are already decided: **no per-lane frame**; the score strips are split from the lanes by
**8 px rails at y 40 and y 670**, drawn after the casing; connective art goes on the **canvas edge**.

## Units

One row per art unit. Interior must equal the target rect ±2 px — `./tables/build.sh` does not check
art, so check it in the build tool before it writes `final/`.

| unit | file | final size | interior | placed at | draws over |
|---|---|---|---|---|---|
| | | | | | |

## Cards

| size | frame | interior | why |
|---|---|---|---|
| 76 × 124 field | | | 81 px pitch, 5 px of metal max |
| 88 × 143 hand | | | 96 px pitch |
| 56 × 91 cpu | | | 62 px pitch |
| 17 × 31 mini | none | — | 20 px label beside it, no room |

## Layer decision

- [ ] runtime restyle (`ui_draw_panel_border()` `src/ui_theme.c:583`, 76 call sites /
      `board_draw_card()` `src/ui_board.c:715`)
- [ ] stamped art at anchors
- both, and why:

## Build

```sh
./tables/build.sh                 # anchors in sync, map + palettes fresh
./tables/build.sh base <name>     # composite base on this backdrop
python3 tools/table_build.py <name>   # units, when art exists
```

## Open questions for the owner

1.
