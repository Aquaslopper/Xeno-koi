# 02 — table_city (City of the Machine)

Status: **spec only.** No art exists — the second pass was thrown away on 29 October 2026 and its
lessons are in `WORKFLOW.md`. `src/` untouched.

Backdrop: `assets/city.jpg` — background index **4** in the Options list
(`{0 Harvest, 1 Hole, 2 Arctic, 3 Castle, 4 City, 5 Deus, 6 Face, 7 Realm}`,
`src/ui_screens.c:326`). The file is already 1280 × 720, so it doubles as the composite base.

Idea (owner, 29 Oct 2026): **every backdrop gets its own UI.** This is the first one — the metal
of the city's architecture instead of the phosphor wireframe. Same layout, different skin.

## Palette, sampled from `assets/city.jpg`

| | value | use |
|---|---|---|
| overall mean | `(13, 12, 7)` | the felt; anything drawn must be brighter than this |
| highlights, top 1 % | `(127, 121, 98)` | frame lit edges, rivet tops |
| warm accents | `(115, 92, 56)` | seam glow, amber in the joints |
| mid metal | `(48, 48, 24)` / `(72, 72, 48)` | frame body, ribs |
| shadow | `(24, 0, 0)` / `(0, 0, 0)` | recesses; 83 % of the image is pure black |

Language: vertical buttresses, arched ribs, cable bundles, glowing seams. Front-on and
symmetric — anything off-axis will not tile or mirror.

## Step 3 — the frame budget (done BEFORE any art)

A frame is drawn **outside** its group, so the interior stays exactly the game's rect and no text
ever lands on metal. Free space per group, from `anchors.md`:

| group | bbox | free L / T / R / B | uniform border | asymmetric instead |
|---|---|---|---|---|
| CPU block | `12, 12, 255, 196` | 12 / 12 / 163 / 300 | 12 | `(12, 12, 44, 12)` |
| player block | `12, 508, 255, 200` | 12 / 300 / 163 / 12 | 12 | `(12, 12, 44, 12)` |
| right column | `1160, 12, 112, 522` | 80 / 12 / 8 / 26 | 8 | `(44, 12, 8, 12)` |
| buttons | `1160, 560, 112, 90` | 80 / 26 / 8 / 70 | 8 | `(16, 9, 8, 9)` |
| deck | `330, 262, 76, 148` | 63 / 54 / 24 / 98 | 24 | `(18, 18, 18, 18)` |
| status | `480, 518, 460, 34` | — / 84 / — / 14 | 14 | vertical only |

**No per-lane frame:** 37.4 tall, 4 px gaps, 4 px label inset. The eight lanes are framed as one
group per side, with **8 px rails** at `y 40` and `y 670` separating the score strips from the lane
blocks. Rails are drawn *after* the casing.

**The connective element is on the canvas edge**, not in the middle of the left side — a pilaster at
`x 0` that is flush with the border and bridges the CPU block (ends y 208) and the player block
(starts y 508).

## Cards

Bezels are per-size PNGs drawn **inside** the card rect: field 76 × 124, hand 88 × 143, CPU 56 × 91.
The atlas is 12 × 4 cells of 176 × 286, so each is a downscale of the same cell. In the live code
that is one textured quad instead of the `DrawRectangleLinesEx` at `src/ui_board.c:722`; the card art
still fills the interior and the bezel rides the card's rect every frame, so flying and spinning are
free. Minis (23 × 37) are too small for a bezel.

## Palette, sampled from `assets/city.jpg`

| | value | use |
|---|---|---|
| overall mean | `(13, 12, 7)` | the felt; anything drawn must be brighter |
| highlights, top 1 % | `(127, 121, 98)` | lit edges, rivet tops |
| warm accents | `(115, 92, 56)` | seam glow |
| mid metal | `(48, 48, 24)` / `(72, 72, 48)` | frame body, ribs |

Language: vertical buttresses, arched ribs, cable bundles, glowing seams. Front-on and symmetric.

## Build (when art exists again)

Save generator output to `assets/tables/city/raw/`, derive `final/`, previews to `preview/` — see
`WORKFLOW.md` steps 5–7. Previews are composited on live geometry (a base drawn from
`table_slot_rect()` + `assets/back.png` + `assets/atlas.png`), never on `table-map.png`.

## Open decisions

1. Keep the teal inner `ui_draw_panel_border` line, or restyle/remove it — as long as it stays, the
   result reads as a box inside a box.
2. Connective element: generated pilaster, or extruded procedurally from the frame's own side profile?
3. The other eight backdrops: reuse one casing with a palette shift, or bespoke ornament each?
