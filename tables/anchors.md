# Anchors — the numbers, and where each one is read from

Canvas **1280 × 720** (`src/ui_theme.h:45-46`). Machine-readable twin: `anchors.json`, which
`./tables/build.sh` asserts against `src/` on every run. Picture of all of it: `table-to-scale.png`.
Nothing here comes from a mockup.

## The table

| element | rect / geometry | source |
|---|---|---|
| CPU score strip | `12, 12, 255, 30` | `ui_board.c:975` |
| CPU lanes ×4 | `12, 46 + i·41.4, 255, 37.4` | `ui_board.c:983`, `:752` |
| player lanes ×4 | `12, 508 + i·41.4, 255, 37.4` | `ui_board.c:985` |
| you score strip | `12, 678, 255, 30` | `ui_board.c:976` |
| field slots row 0 / 1 | `434 + col·81, 170 | 310, 76, 124` | `ui_board.c:560-573` |
| overflow slots 16–23 | `434 + col·81 + 12, 184 / 324` | same |
| field bbox (**not drawn**) | `430, 132, 650, 400` | `ui_theme.h:72-75` |
| stock | `330, 262, 76, 124`, count text at `356, 392` | `ui_board.c:696`, `:1017` |
| status line | centred `710, 529`, 18 px | `ui_board_status.c:73-81` |
| turn timer | centred `640, 505`, 22 px | `ui_board.c:1122-1130` |
| CPU hand | 56 × 91, step 62, centred 710, `y 12` | `ui_board.c:992-996` |
| player hand | 88 × 143, step 96, centred 710, `y 566` (`552` hover) | `ui_board.c:526-529` |
| round + ruleset | `1160, 12, 112, 66` | `ui_combos.c:44` |
| CPU / your combos | `1160, 88 | 316, 112, 218` | `ui_combos.c:29-31` |
| PAUSE / RESTART | `1160, 560 | 610, 112, 40` | `ui_board.c:1184,1186` |

Text ink, derived from those draw calls (`DrawTextEx` y is the glyph top): the timer's glyphs span
≈505..527, the status line's ≈529..547 — and any status containing `KOI-KOI`, `SHOBU` or
`YAKU DETECTED` switches to 20 px `font_title` (`ui_board_status.c:73-81`). Neither line is shrunk
or clipped by the game, whatever its length. A status plaque can wrap one of the two lines, not
both; which one is a per-table decision (castle pass 1: the status line).

What is **painted** underneath (a composite that skips these looks wrong even when every frame is
right): `COLOR_PANEL_BG` black @ **178** on lanes, combos, buttons (`ui_theme.h:38`); the score
strips at **112** idle / **205** active plus an inset border when idle (`ui_board_status.c:23-28`);
**nothing at all behind the field** — cards sit straight on the backdrop.

Lane label sits at `x 16`, `y + 8.5`, 20 px — 4 px from the lane's own edge, and lanes are 4 px
apart. Minis inside: `17.324 × 30.652` at `x 54`, step `min(24, (255 − 54 − 17.324)/(n−1))`
(`ui_board.c:745, :775`).

## Free space, and the border it allows

A frame is drawn **outside** its rect. Free space on all four sides, per group:

| group | bbox | L | T | R | B | heaviest uniform | use instead |
|---|---|---|---|---|---|---|---|
| CPU block | `12, 12, 255, 196` | 12 | 12 | 163 | 300 | **12** | asymmetric `(12, 12, 44, 12)` |
| player block | `12, 508, 255, 200` | 12 | 300 | 163 | 12 | **12** | mirrored |
| right column | `1160, 12, 112, 522` | 80 | 12 | 8 | 26 | **8** | `(44, 12, 8, 12)` — thick inboard, thin at the edge |
| buttons | `1160, 560, 112, 90` | 80 | 26 | 8 | 70 | **8** | `(16, 9, 8, 9)` |
| deck | `330, 262, 76, 148` | 63 | 54 | 24 | 98 | **24** | `(18, 18, 18, 18)` |
| status | `480, 518, 460, 34` | — | 84 | — | 14 | **14** | vertical only |

A uniform border is always dragged down by its tightest side — measure all four, or a panel comes out
thin beside its neighbours.

Bands with nothing in them: `0, 208, 267, 300` (between the two left halves — the only place a
connector can live), `267, 208, 163, 300` (holds the stock), `0, 542, 1280, 24` (too short for
anything but a rule), `1160, 650, 112, 70`.

Seams inside the left blocks: score ends y 42, lane B starts y 46; lane P ends 669.6, you-score
starts 678. **8 px rails at y 40 and y 670** split them, drawn *after* the casing or it covers them.

Not possible: a frame per lane (37.4 tall, 4 px gaps, 4 px label inset).

## Cards

| card | size | where |
|---|---|---|
| field | 76 × 124 | `434 + col·81`, rows 170 / 310 |
| hand | 88 × 143 | step 96, centred 710, y 566 |
| CPU | 56 × 91 | step 62, centred 710, y 12 |
| choice | 176 × 286 | two-match prompt |
| mini | 23.324 × 36.652 | in the lanes — too small for a bezel |

`assets/atlas.png` is 2112 × 1144 = **12 × 4 cells of 176 × 286**, cell `(cid>>2)·176`,
`(cid&3)·286` (`ui_board.c:722`). Every card rect is a downscale of the same cell, so a bezel is a
per-size PNG drawn at the destination rect and **inside** it (81 px pitch for a 76 px card — a border
outside would overlap the neighbour). In code that is one textured quad replacing the
`DrawRectangleLinesEx` at `ui_board.c:722`. `assets/atlas_deus.png` has the same grid; Deus difficulty
swaps the texture only.

## Frozen

| thing | where | why |
|---|---|---|
| action-card stock anchor | `330, 262` (its own copy) | `ui_action.c:548`, hash-guarded file |
| action-card meter | `414, 170, 14, 264` | `ui_action.c:760` |
| card sizes | `ui_theme.h:48-63` | every mode reads them, including the frozen one |
| 12 pinned lines | `tools/action_guard.manifest` | must stay verbatim; 4 inside `board_draw_table()` |
| tutorial ghost hand | `ui_plexus.c:4482` | hardcodes an 8-card hand |

## The trap: two field layouts, one live

| | live | dead |
|---|---|---|
| function | `table_slot_rect()` `ui_board.c:560-573` | `board_field_slot_xy()` `board_layout.c:16-28` |
| step | **81 px**, `434 + col·81` | **96 px**, centred in 430..1080 |
| callers | `board_draw_table()` `ui_board.c:1033-1038` | none (only via `board_get_field_card_rect()`, also none) |

**And `tools/make_board_figure.py` draws the README's 1:1 figure with the dead one** (line 28,
`field_slots()`), so `docs/assets/readme/table-map.png` puts field cards 96 px apart where the game
draws them 81. Compositing on it makes correct frames look wrong — three rounds, 29 Oct 2026.
Never composite on it; use `base.png`.

## Dealing

`board_reset_field_slots()` `ui_board.c:575-603`: sorted by month, split `top = (n+1)/2`, centred
per row `c0 = (8−top)/2`. 8 cards → columns 2..5 both rows, 14 → 0..6, 16 → full. Slots are sticky
(`k_table_slot_order[]`). Card id is `(month−1)·4 + slot` (`cards.h:20`), so `{0,4,8,12,...}` is one
card per month, not four January cards.

Fit checks: hand 8 = `7·96 + 88 = 760` → x 330..1090; CPU 8 = 490 → x 465..955.

## Chokepoints — where a look is actually implemented

| surface | function | sites |
|---|---|---|
| every panel outline | `ui_draw_panel_border()` `ui_theme.c:583` | 76 |
| every card face / back / glow | `board_draw_card()` `ui_board.c:715`, `:728`, `:737` | all modes |
| palette | `TerminalTheme g_theme` `ui_theme.h:5-14` | everywhere (`theme_set()` cycles 5) |
