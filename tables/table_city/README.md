# 01 — table_city

Status: **spec only.** No art; the second pass was thrown away 29 Oct 2026. `src/` untouched.

Everything for this table lives in this folder (`base.png`, `table.json`, `raw/`, `final/`,
`preview/`, `refs/`). Nothing is written outside it.

Backdrop `assets/city.jpg`, index **4** of
`{0 Harvest, 1 Hole, 2 Arctic, 3 Castle, 4 City, 5 Deus, 6 Face, 7 Realm}` (`src/ui_screens.c:326`).
Already 1280 × 720, so it doubles as the composite base.

Intent: the city's own architecture instead of a wireframe — vertical buttresses, arched ribs, cable
bundles, glowing seams. Front-on and symmetric. First of nine tables, same layout, different skin.

Examples of the *direction* (not the layout): `refs/city-example-01.jpg`, `refs/city-example-02.jpg`.

The door is open: the Pancratium's `CARD TABLE:` picker already lists `CITY (BETA)` as a placeholder
(index 1 in `k_table_names[]`, `src/campaign.c`) and saves it to `settings.txt`. Selecting it changes
nothing yet — that is what "not forged" means.

## Palette, sampled from `assets/city.jpg`

| | value | use |
|---|---|---|
| mean | `(13, 12, 7)` | the felt; anything drawn must be brighter |
| highlight p99 | `(127, 121, 98)` | lit edges, rivet tops |
| warm accent | `(115, 92, 56)` | seam glow |
| mid metal | `(48, 48, 24)` / `(72, 72, 48)` | frame body, ribs |

83 % of the image is near-black — the UI has to carry its own light.

## Budget

Full table in `../anchors.md`. For this design: left blocks `(12, 12, 44, 12)`, right column
`(44, 12, 8, 12)` (80 px of free space is on its *left*, 8 on the right — a uniform border there is
the mistake that made the column read thin), buttons `(16, 9, 8, 9)`, deck `(18, 18, 18, 18)`,
status vertical-only. Score strips are split off the lanes by **8 px rails at y 40 and y 670**. The
connective element sits on **x 0**, flush with the border, bridging the CPU block (ends y 208) and
the player block (starts y 508) — not a spine through the middle.

## Cards

Per-size bezels inside the card rect: field 76 × 124, hand 88 × 143, CPU 56 × 91. The atlas is 12 × 4
cells of 176 × 286, so each is a downscale of one cell. Minis (17 × 31) carry nothing.

## Layer

Not decided. The direction in the examples — capsules, bevelled inner edges, panels connected by
piping — is rectangular-plus-rounding, which is a **runtime** change at `ui_draw_panel_border()` and
`board_draw_card()`. That would show the look in the real game at real rects with no assets at all,
and it sidesteps every stamped-art trap. Ornament (skulls, knots, a casing profile) would then be
stamped art on top.

## Open questions

1. Runtime restyle first (capsules + bevel), or art first?
2. If runtime: the teal `ui_draw_panel_border` line inside every frame has to go, or the result reads
   as a box inside a box.
3. Nine backdrops: one casing with a palette shift each, or bespoke ornament per backdrop?
