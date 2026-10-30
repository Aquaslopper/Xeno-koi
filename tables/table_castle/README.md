# `03` — table_`castle`

**One table = one folder.** Everything for this table lives here — spec, manifest record,
previews. The source art stays where the owner delivered it: `tables/assets_castle/`, untouched.

Status: **beta port — selectable in the Pancratium (`CASTLE (BETA)`), awaiting owner live
test.** `src/` touched: **yes** (beta port, see Port notes). CRT (index 0) unchanged.

Backdrop `assets/castle.jpg`, index **3** of
`{0 Harvest, 1 Hole, 2 Arctic, 3 Castle, 4 City, 5 Deus, 6 Face, 7 Realm}` (`src/ui_screens.c:326`).

Intent, one line: cast-bronze casing — rope edges, corner flourishes, dark leather interiors —
wrapping every panel group and card, with the gothic castle backdrop left bare behind the field,
exactly as the game paints it today.

## Source art (owner-delivered, never edited in place)

`tables/assets_castle/`: seven 1920×1080 RGBA layers, one UI group each, already cut out, no
baked text (`castle_upper_left`, `castle_lower_left`, `castle_right_column`, `castle_status`,
`castle_stock`, `castle_upper_hand`, `castle_lower_hand`), plus `castle.png` (the example, with
text) and `castle_clean.png` (the clean stack). Verified 2026-10-30: stacking the seven pieces
at ×⅔ reproduces `castle_clean.png` with mean |RGB diff| 1.36 — the pieces are consistent layers
of the clean plate. `castle.png` is an **example**: the text in it sits in the generator's own
layout, which is not the game's (next section).

## Measured fit — what is wrong as delivered, with numbers

Method: band-averaged luminance profiles on the 1920 px pieces (interior leather ≈ 16–30, bronze
well above; averaging kills the rope texture), every cavity enumerated and compared with
`../anchors.json`. All numbers at 1280×720. Picture: `preview/castle-as-delivered.png`.

- **Left blocks.** Cavity *sizes* are right (lanes ≈257×35 vs the game's 255×37.4), but the stack
  *pitch* is ≈46.5 and irregular (cavity-top pitch 42.7 / 48.7 / 45.3 / 49.3) against the game's
  41.4, so the fourth lane's cavity sits **29.8 px** below the game's lane. No offset and no
  uniform scale can fit an irregular pitch — this is trap 5 verbatim. Per-lane placement is the
  only fit.
- **Right column.** Cavities sit ≈18 px left of the anchors and 6–15 px off vertically
  (RESTART +12.7); the two combo cavities are 204.7 / 203.3 tall against 218 px panels whose
  last text line ("CLICK TO VIEW", `ui_combos.c` r.y+197, 12 px) needs the full height.
- **Stock.** Cavity 91.3×146 around a 76×124 card (the piece is drawn ≈18 % large), and the count
  plaque's cavity is at y 406..429 while the game draws the count at (356, 392), 18 px.
- **Status.** The plaque cavity (458..897 × 498..527) wraps the **timer** line (glyphs ≈505..527),
  not the status line (529..547); its centre is x 678, the status text is centred x 710.
- **Hands.** **7 frames** at pitch 76.2 (CPU) / 116.4 (hand) against the game's **8 cards** at
  62 / 96. The strips cannot overlay a live hand (count, pitch, re-centring as cards leave);
  they are bezel stock — one frame per strip, tiled by the card draw. **No field-card piece was
  delivered** (76×124); the field bezel derives from the hand frame.

## Method: cut-and-place — never `stamp()`

Owner's rule: the art is scaled down to 1280×720 **uniformly** and is **not squished into the old
UI's hairlines**. `tools/table_art.py`'s `stamp()` does the forbidden thing — it 9-slices a frame
to force interior == rect — so castle is fitted differently: each compartment is cropped from its
piece at its natural outer band (the ×⅔ layer) and **translated only**, so its measured cavity
sits on the anchor rect. A uniform scale is applied in exactly three places, each justified by a
measured cavity size:

| unit | scale | why |
|---|---|---|
| stock frame | ×0.849 (=124/146) | cavity 91.3×146 → 77.5×124 on the 76×124 card |
| cpu / your combos | ×1.065 / ×1.072 (=218/204.7, 218/203.3) | cavity height → the 218 panel, keeping the last text line inside |
| field bezel | ×0.871 | hand frame scaled so cavity slack (+4.7 w) matches the hand bezel's |

`table.json` in this folder is the machine record (crop, measured cavity, anchor, alignment,
scale, placed cavity/outer per unit). It is **not** a table_art schema, and `table_art.py` now
refuses it loudly instead of building an empty "0 mismatches". The finals builder (crop at
1920 px → `final/`) is the next pass. After cut-and-place every unit is on-canvas; the assertion
table is reproduced under Units.

Right-column placement needs one sentence of its own: the anchors end at x 1272 with 8 px to the
canvas edge, but the art carries 8–13 px of border right of its cavities, so cavity-on-anchor
pushes ornament off-canvas (measured 1280–1285). The units are therefore placed with their
**outer right edge at 1278**; the cavities then end at x 1272 and every text the game draws there
(labels are clipped to 96 px from x 1168, so ≤ 1264) stays inside with ≥ 8 px to spare.

## Direction

Heavy, connected bronze: each lane keeps its own cast frame, and at the game's tighter pitch
(41.4 vs the art's 46.5) neighbouring frames interlock by 0.6–6 px, so a block reads as one
stacked casing — drawn top-to-bottom, each frame's top border covers the previous frame's bottom
shadow. Card bezels interlock the same way (hand 15 px, field ≈16 px, CPU 10 px), which is how
the delivered strips already look. Nothing is drawn behind the field; the cards' bezels are the
only furniture there.

## Palette (`tables/palettes.json` → `castle.jpg`, sampled)

| | value | use |
|---|---|---|
| mean | (37, 20, 15) `#25140f` | the felt; the leather interiors (sampled 34,30,20) sit just above it |
| highlight p99 | (206, 173, 134) `#cead86` | the bronze line — the art's metal is the backdrop's own highlight |
| mid | `#6d4b35` | frame body |
| accent | (255, 253, 233) max | rare glints only (6.06 % bright pixels) |

## Budget (`tables/anchors.md` free space → chosen = measured after placement)

| group | bbox | free L/T/R/B | border outside the rect (measured) | placed outer extent |
|---|---|---|---|---|
| CPU block | `12,12,255,196` | 12 / 12 / 163 / 300 | L 7–11, T 2–5, R 7–8, B 3–6 | x 1..275, y 7..212 |
| player block | `12,508,255,200` | 12 / 300 / 163 / 12 | L 6–7, T 2–5, R 12 (23 score, the `>>` knob), B 1–9 | x 5..290, y 503..717 |
| right column | `1160,12,112,522` | 80 / 12 / 8 / 26 | L 16–26, T 5–6, R 6, B 6–7 | x 1134..1278, y 7..541 |
| buttons | `1160,560,112,90` | 80 / 26 / 8 / 70 | L 21, T 4–5, R 6, B 5–6 | x 1139..1278, y 556..656 |
| deck | `330,262,76,124` | 63 / 54 / 24 / 98 | frame L 9, T 5, R 10, B 7; plaque separate | frame 321..416 × 257..393; plaque 329..407 × 387..417 |
| status | text 529..547 @ x 710 | top 84 / bottom 14 | plaque moved (+32, +25) onto the status glyphs | 433..982 × 494..565 |

Asymmetry holds the way `anchors.md` wants it: the weight is inboard (left blocks grow right,
right column grows left), thin only at the canvas edges.

## Layer

- [ ] runtime restyle
- [x] stamped art — cut-and-place variant, `table.json` is the record (builder next pass)

## Units (assertion run 2026-10-30 — cavity vs anchor, border outside the rect)

| unit | cavity measured | anchor | size diff | align | border L/T/R/B |
|---|---|---|---|---|---|
| cpu_score | 251.3×29.3 | 255×30 | (+3.7, +0.7) | left, centre-y | 11/5/8/6 |
| cpu_lane_0..3 | 256.7×34.7–36.7 | 255×37.4 | (−1.7, +0.7..+2.7) | left, centre-y | 7–8/2–5/7–8/3–6 |
| you_lane_0..3 | 260.7–262.7×32.7–36 | 255×37.4 | (−5.7..−7.7, +1.4..+4.7) | left, centre-y | 6–7/2–5/12/1–4 |
| you_score | 210.7×34.7 | 255×30 | (+44.3, −4.7) — cavity stops at the art's `>>` button; the score text is short and left-drawn | left, centre-y | 7/5/23/9 |
| rnd_panel | 118×66.7 | 112×66 | (−6.0, −0.7) | outer-right@1278, top | 16/5/6/7 |
| cpu_combos | 118×204.7 ×1.065 | 112×218 | (−13.7, 0.0) | outer-right@1278, top | 26/5/6/6 |
| your_combos | 118.7×203.3 ×1.072 | 112×218 | (−15.3, 0.0) | outer-right@1278, top | 26/6/6/7 |
| pause / restart | 120.7×36.7 / 121.3×37.3 | 112×40 | (−8.7..−9.3, +2.7..+3.3) | outer-right@1278, centre-y | 21/4–5/6/5–6 |
| stock_frame | 91.3×146 ×0.849 | 76×124 | (−1.5, 0.0) | cavity on card rect | 9/5/10/7 |
| stock_plaque | 64×22.7 | count text (356,392) 18 px | — | cavity → (336,390), centred under the deck (368) | 7/3/7/4 |
| status | 439.3×29.3 | glyphs 529..547, cx 710 | — | cavity centre → (710, 538) | flourish 57/29/53/13 |
| bezel_hand | 92.7×161.3, outer 111×174 | card 88×143 | slack (+4.7, +18.3), centred | per card | ≈9 top/bottom mat |
| bezel_cpu | 60.7×105.3, outer 72×114 | card 56×91 | slack (+4.7, +14.3), centred | per card | top lands at y ≈0.5 |
| bezel_field | 80.7×140.4 (hand frame ×0.871) | card 76×124 | slack (+4.7, +16.4), centred | per card | — |

Lane cavities are centred vertically on the 37.4 lane on purpose: the mini band (+3.4..+34.0) and
the 20 px label (+8.5..) then both clear the metal in every lane — top-aligned, the shortest
player cavity (32.7) would graze the minis by 1.3 px. Vertical neighbour overlap after
placement: lanes 0.6–6 px, right column ≤2 px — border shadows only, no cavity is ever covered.

## Previews

- `preview/castle-as-delivered.png` — pieces at ×⅔, red anchor rects: the drift, visible.
- `preview/castle-fit-flat.png` — after cut-and-place, flat canvas, red anchors: cavities on rects.
- `preview/castle-fit-table.png` — after cut-and-place on `assets/castle.jpg` with the
  live-geometry card layer from `base.png` (cards over bezels, minis in lanes). No text: the game
  draws it, and the Units table above is its fit proof.

## Port notes (beta port — done 2026-10-30, the stage that touches `src/`)

- **Picker:** castle is appended as index 2, `CASTLE (BETA)` (`TABLE_DESIGN_CASTLE`,
  `src/storage.h`; names in `src/campaign.c`). CRT stays 0 and `CITY (BETA)` stays the
  placeholder at 1, so existing saves and indices are undisturbed. Final naming/ordering is
  still the owner's call; selection itself works now.
- **Finals:** `./tables/build.sh table_castle` builds `final/` via `build_cut_and_place`
  (`tools/table_art.py`): each recorded compartment is cropped at native 1920×1080, given ONE
  uniform ×⅔ downscale (plus the recorded per-compartment uniform scale), and translated so
  its measured cavity lands on the anchor — no 9-slice, no non-uniform resizing. Static
  panels composite into `final/table_castle_overlay.png`; bezels stay separate at native
  resolution, and `_manifest.json` records each bezel's cavity-in-texture constants for the
  game code to mirror. Runtime copies: `assets/table_castle_overlay.png` +
  `assets/table_castle_bezel.png` (= `final/bezel_hand.png`).
- **Runtime:** one predicate, `board_castle_skin_active()` (`src/ui_board.c`), true only when
  the textures loaded and castle is selected. `board_draw_table` refreshes the selection flag
  each frame (the predicate itself reads no `settings` global, so the headless table-UI tests
  link the sidebar path without stubbing it). The overlay draws right after the backdrop
  dim/gradient/ash, below every live element. While active, the CRT `COLOR_PANEL_BG` fills
  and `ui_draw_panel_border` hairlines are skipped on the score strips, capture lanes, combo
  sidebar and PAUSE/RESTART — live text, counts, minis, flash feedback, glows, tooltips and
  the hover hairline are untouched. Interiors stay the art's opaque leather. With castle not
  selected, every draw call is byte-for-byte the old CRT path.
- **Bezels:** one texture for every card size, drawn under the card inside
  `board_draw_card`/`board_draw_card_back`. It scales by card **height** (cavity height
  161.3 canvas px per 143 px of card, cavity centre on card centre) and its drawn width
  follows a flip's width squeeze, so frames turn with their cards during deal/stock flips
  and fly with them. Cards under 60 px tall (capture-lane minis) get no bezel. This is what
  makes 8-card hands work with a 7-frame delivered strip: no strip is ever placed.
  `final/bezel_cpu.png`/`bezel_field.png` are kept for the record; the hand frame at the
  other sizes lands within ~1 px of their measured cavities.
- The score strips carry the turn cue: when a strip is active, `board_draw_info_box` draws
  breathing rings (2.5 px on the box, 2 px at box−3), panel flares and a filled `>>` tag at
  x 236..261, box.y+5..+25 (`ui_board_status.c:32-57`) — under castle those rings would draw on
  the bronze, and on the YOU strip the tag lands 13 px right of the art's own `>>` button
  (placed 223..259; on the CPU strip it falls inside the plain cavity, 12..263). The idle/active
  fill alpha (112/205) also has no equivalent under opaque leather. Beta drops all of it:
  under castle the active state reads from the label colour alone (phosphor vs dim), which the
  code already switches. Whether that's enough of a turn cue is open question 3. The score
  *text* fits: worst case
  "YOU   SCORE: 999 [DEALER]" is 222 px at 16 px and the game shrink-fits it to its 210 px
  active allowance (right edge ≈226 vs cavity 223 — a ≤3 px graze in the worst case only;
  two-digit scores end ≈208).
- Status text is never shrunk or clipped (`board_draw_status_message`). Measured with the shipped
  fonts at draw size: `"YOUR TURN - DRAG OR CLICK A CARD TO SELECT"` 440 px against the 439 px
  cavity (at the limit), `"SELECTED - CLICK A MATCH OR EMPTY FIELD / ESC TO CANCEL"` 569 px
  (the plaque's metal itself is 549 wide), the Audiowide callout
  `"CALL KOI-KOI TO CONTINUE OR SHOBU TO SCORE"` 626 px. Centred, so long lines overflow the
  plaque symmetrically onto metal/backdrop — still readable. The timer's glyphs (x ≤ 567..713,
  y 505..527) ride the plaque's top rail (outer top 494, cavity top 523). Beta accepts both as
  they are (open question 2); a shrink-fit for the status line on this table
  (`board_draw_info_box` already shrink-fits; the status line does not) is the alternative if
  the live test dislikes the overflow.
- Hitboxes stay the code rects: on the right column the visual frames extend 9–14 px inboard of
  the clickable rects (1160..1272), so the bronze lip is decoration, not button.
- The capture-info hover panel (`12,304,255,200`, `ui_capture_info.c` — the quick inventory) is
  **not** skinned in pass 1 and keeps working unchanged; no unit reaches it (blocks end y 212 /
  start y 503, its band is 304..504 — the player block's outer top at 503 just touches its
  bottom edge).
- Stock pile: the game stacks up to 6 card backs at −1 px each (`stock_count>>2`, in
  `board_draw_table`), reaching (324,256); the frame's outer starts (321,257), so the pile's back
  edge crosses the frame's inner line as it does today's 1 px edge. Each pile back also carries
  a bezel now, so up to 6 frames layer inside the static stock casing. Watch in live test.
- Frozen neighbours: `action_card_draw_stock_returns` / `action_card_draw_meters` and the guard
  manifest's pinned lines inside `board_draw_table()` — the port adds no lines inside them and
  removes none. `./tools/action_guard.sh --report` exits 0 after the port (pinned lines intact;
  only their line numbers moved).
- Bezel behaviour to check live: interlocked bezels while a card flies (field/hand), a hovered
  hand card (y 552) lifting its bezel into the status plaque's bottom ornament (plaque bottom
  565), CPU bezel tops at the canvas edge.

## Live test

The owner plays the beta port and the findings are written into `live-test.md` in this folder —
symptom, where, expected. Not in chat, not in memory: in the file, or the next pass rebuilds
the fault.

## Open questions for the owner

1. **Bezel treatment.** The previews centre each frame's cavity on its card — the frame sits
   *outside* the card rect and the card shows on a leather mat. `anchors.md`'s original
   prescription was a bezel *inside* the card rect. The delivered frames are drawn as outside
   casings, so pass 1 follows the art; say if you want the inside variant instead.
2. **Timer vs plaque.** With the plaque moved onto the status line, the turn timer
   (glyphs ≈505..527, centred 640) now crosses the plaque's top border. Acceptable, or does the
   timer get its own small plaque/moved treatment at port?
3. **Score-strip activity cue** under opaque leather (see Port notes) — recreate or drop?
