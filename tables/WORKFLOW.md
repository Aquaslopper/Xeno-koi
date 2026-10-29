# Building a table — the workflow, and the traps we already paid for

Procedure for adding a table design. Two attempts have been thrown away (29 October 2026); every
mistake is written down here with the number that caused it, so the third attempt is short.

Data: **`anchors.json`** (machine) and **`anchors.md`** (human, with the source line beside every
number). Visual truth: **`table-to-scale.png`**, the whole table at 1:1. Composite base:
**`base.png`**. Art: `assets/tables/`. Nothing touches `src/` until step 8 — and step 8 may never be
needed at all, see step 3.

Start every session with `./tables/build.sh`. If it does not print `anchors in sync`, stop and fix
that first: every number below it is then suspect.

---

## The three rules

1. **The mockup is a picture of a design. The layout is what `src/` already draws.** Anchors come
   from code, never from a mockup and never from a screenshot.
2. **Measure all four sides, not one.** A frame's thickness is decided by the tightest free space
   around it, and panels next to each other must end up with comparable weight or one looks broken.
3. **Assert, don't eyeball.** A frame counts as placed when its open interior equals the game's
   rect within 2 px, checked numerically, and when it visually sits on a base drawn from live code.

---

## Steps

### 1. Clear the build gate
`./build.sh test` stays locked until `LEARN_THE_CODE.md` has been read and its six questions
answered into `/tmp/ans.txt`, then `./build.sh --verify /tmp/ans.txt`. Do it first; it is also the
cheapest way to learn what the UI actually does.

### 2. Take the anchors from code
Open `tables/table-to-scale.png`. Work from `tables/anchors.json`. If a number is not there, find
its line and add it. **Do not** measure a screenshot and **do not** measure a mockup.

`./tables/build.sh` re-derives the map from the JSON and asserts the JSON against `src/`, so a stale
anchor fails loudly instead of quietly producing a wrong design. `tables/palettes.png` has the
sampled colours for all eight backdrops already.

### 3. Pick the layer before making art
- **Runtime restyle** — change `ui_draw_panel_border()` (`src/ui_theme.c:583`, 76 call sites) and/or
  `board_draw_card()` (`src/ui_board.c:715`). Every panel and card gets the design at its exact
  rect: no overlap, no scaling, no assets, nothing to mis-place. A plain rectangular frame should
  always be this.
- **Stamped art** — PNG units blitted at anchors. Only for shapes a rect cannot express: a bracket
  on the canvas edge, ornaments, a backdrop-specific casing with a profile.

Default: **runtime for frames, art for decoration.** If the design is "the panels are metal now",
stop at step 3 and write a style spec.

### 4. Check the frame budget before drawing (all four sides)
`anchors.md` has the table. Short version:

- capture lane: 37.4 tall, 4 px gaps, label 4 px from the edge → **no per-lane frame, ever**
- right column: 80 px free on the left, **8** on the right → a uniform border is stuck at 8
- left blocks: 12 left, **163** right → a uniform border is stuck at 12
- so: **uniform borders look thin.** Use asymmetric ones and put the weight on the inner side.

### 5. Make unit art (only if step 3 said art)
- Ask for **one empty frame**, never a multi-compartment panel. In five of seven tries the
  generator returned the wrong compartment count, and a wrong count cannot be scaled into place.
- State the aspect ratio of the **interior**, and "pure black background, no scenery".
- A frame is drawn **outside** its rect, so the interior equals the game rect and the game's text can
  never land on metal.
- Save generator output to `raw/`, never edit it in place; `final/` is derived.

### 6. Cut out — solid silhouette, not soft alpha

`tools/table_art.py` implements all of the below: `cut` for one unit, `build tables/designs/<name>.json`
for a whole design (schema in `tables/designs/_schema.json`). Do not re-implement it in a scratch
script — that is how the 96 px figure survived unnoticed.
"Transparent" means **cut out**, not see-through. The metal stays opaque; only the background and
the frame openings are transparent.

1. seed `max(r,g,b) > 26`
2. closing with a 13 px max/min filter — shadowed crevices fill solid, large openings survive
3. fill enclosed transparent blobs under ~90 px (dark highlights, not holes)
4. 0.7 px gaussian feather, snapped to 0/255 outside that
5. grade toward the backdrop's sampled palette (value, saturation, tint, lift)
6. **clear the interior outright**: a render leaves alpha 50–200 haze inside its own frame, which
   reads as a dark overlay on the game's text
7. sanity check: **40–55 % fully opaque**. Low opaque + high mid means the key went soft again

### 7. Compose on live geometry, then assert
**Never composite onto `docs/assets/readme/table-map.png`.** That figure is drawn with
`board_field_slot_xy()`, which has no callers; the live field is `table_slot_rect()` (81 px step vs
the figure's 96). Use `tables/base.png` (`./tables/build.sh base <name>` for a real backdrop). It is drawn from
`table_slot_rect()`, `assets/back.png` at 56 × 91 step 62, 176 × 286 atlas cells in the sticky slots,
88 × 143 step 96 for the hand, **and the game's own panel fills** — black @ 178, info strips
112 idle / 205 active, nothing behind the field.

Then assert, per unit:

- interior of the frame == target rect, ±2 px
- no two frames or rails overlap, and every rail is drawn after the casing it sits on
- one preview on the real backdrop (`assets/<name>.jpg`) **and** one on a flat dark canvas. If it
  looks right on one and wrong on the other, the base is suspect before the frame is.

### 8. Verify
Art and docs only → `./build.sh test`, `git diff --check`, and **`tables/CHECKLIST.md` all ticked**.
`python3 tools/table_art.py build ...` must end with `0 mismatch(es)`. If step 3 chose runtime restyle, it is
a code change and the full gate applies, including `./tools/action_guard.sh --report` — the Action
Card mode is hash-frozen and shares both draw functions.

---

## The traps, in the order they were hit

1. **Mockup pixels as layout.** Sized parts from the mockup's image; nothing fitted.
2. **`board_layout.h` is dead code.** `board_field_slot_of()` has no callers, `board_field_slot_xy()`
   is only reached through `board_get_field_card_rect()` which has none. The live table is
   `table_slot_rect()` `src/ui_board.c:560-573`.
3. **The README's own table figure is drawn with the dead layout.** `tools/make_board_figure.py`
   uses `board_field_slot_xy()`, so `table-map.png` shows cards 96 px apart where the game puts them
   81 px apart. Three rounds were lost to compositing onto it. See `anchors.md`.
4. **Soft alpha instead of a cut-out.** Alpha from luminance made the metal itself translucent — a
   ghost over any background. Step 6.
5. **Multi-compartment panels from a generator.** The right column needs 66 / 218 / 218; it came back
   44 / 120 / 260. One scale cannot reconcile that, and band-surgery to stretch it broke every joint.
6. **Per-lane frames are geometrically impossible.** 37.4 px lane, 4 px gaps, 4 px label inset.
   Frame the group.
7. **Frames drawn inside the rect.** The game's own text ended up on the metal. Frame goes outside,
   interior equals the rect.
8. **Fitting by measured holes.** Only valid when the art's hole count equals the code's rect count;
   otherwise it silently shears.
9. **Stamping every rect individually.** Ten rects in a 267 × 700 left column with border against
   border has nowhere to go. Group them, and use rails for the internal seams.
10. **Uniform border thickness.** The right column has 8 px free on one side and 80 on the other; a
    uniform border is dragged down to 8 and the panel reads thin next to its neighbours. Measure all
    four sides and use asymmetric borders.
11. **Decor placed in the middle of the left side.** A vertical spine between the blocks was rejected
    twice. A connector belongs on the **canvas edge**, where it can be flush and where it bridges
    the two blocks at the point they actually meet the border.
12. **Rails drawn before the casing.** The seam rails were covered by the block frame until the draw
    order was flipped.
13. **The Action Card mode is hash-locked.** `ui_action.c`/`.h` are hash-guarded and 12 influence
    lines outside them are pinned verbatim, four inside `board_draw_table()`. Its stock anchor
    `(330, 262)` and meter bar `(414, 170)` cannot move. Run `./tools/action_guard.sh --report`
    before touching a shared draw function.
14. **No bezel art ships.** `assets/` has no frame, border or panel texture, so a metal look is
    either procedural in the border function or a new 9-slice asset.

---

## Open decisions

1. **Layer** (step 3): runtime restyle, stamped art, or both — and if runtime, the teal
   `ui_draw_panel_border` line inside every metal frame has to be dealt with or the result reads as
   a box inside a box.
2. **Grouping:** which rects get frames (score strip + lane block as one casing with rails, the three
   right-column boxes as one column, buttons as one unit).
3. **Lane metrics:** keep 37.4 / 4 px and group the frames, or change them in code and allow
   per-lane frames.
4. **Palette:** a bone/neutral `TerminalTheme` entry (today `theme_set()` cycles 5 while the Options
   list shows 6 names) or a separate `table_design` field in `GameSettings`.
5. **Nine tables:** one layout, nine palettes and one casing profile each — or each backdrop gets
   bespoke ornament as well.
