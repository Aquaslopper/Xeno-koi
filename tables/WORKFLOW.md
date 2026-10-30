# WORKFLOW — how a table gets built

Data: `anchors.json` (machine) and `anchors.md` (human, with the source line). Visual truth:
`table-to-scale.png`. Base: `base.png`. **One table = one folder**: `tables/table_<name>/` holds its
spec, manifest, raw, final, preview and examples, and nothing about it is written anywhere else. Start every session with `./tables/build.sh`; if it does not
print `anchors in sync`, stop — every number below it is suspect.

## An example is not evidence (paid for twice)

Pictures in `refs/` are **examples**: a direction, never a number and never a statement about the
build. "The game doesn't look like this example" is not a finding — that is what an example is for.
Facts live in `src/` and `anchors.json`.

| word | what it is | what may be inferred |
|---|---|---|
| **example / mockup** | a picture of a direction | a feeling: metal, capsules, connected panels. No numbers, no positions |
| **reference** | real art for texture and palette | shapes; palette sampled, not eyeballed |
| **the game** | what `src/` does at these anchors | everything measured, verified by `./tables/build.sh` |
| **the design** | what we choose to build | a spec in `designs/`, with numbers and its own checklist run |

## The forge loop

A table is not finished when the preview looks right — it is finished when it has survived a real
round. The loop, and where each stage writes:

```
   forge  ─────────────►  beta port  ─────────►  live test  ─────────►  back to the forge
 tables/table_<name>/     src/, selectable      played, written into      fixes its README
 spec, raw, final,        in the Pancratium     live-test.md, not memory   and live-test.md
 preview, table.json      (src/campaign.c:121)
```

- **The folder is created when the table starts**, never scaffolded ahead: `cp -r tables/_template
  tables/table_<name>`. An empty folder is not a plan.
- **A beta port is allowed to be unfinished** — that is what beta means. It exists so the table can be
  picked in-game and played, because some faults only appear in a real round: a frame under the
  game's own text, a bezel that is right at rest and wrong mid-flight, a panel erased by a glow.
- **The Pancratium is the door.** `src/campaign.c:121` draws the picker (`APEX PANCRATIUM CIRCUIT`);
  the opponent slots are `155 + i·82, 96, 68 × 110`, START MATCH `460, 570, 240 × 48`, BACK
  `720, 570, 180 × 48`, clearance presets `560 | 730 | 890, 364, ~150 × 26`. A table selector belongs
  on that screen — the free area inside the detail panel is where it was sketched.
- **Every live test is written into `live-test.md` in the table's folder**: build, what was tested,
  what broke (symptom → where → expected), verdict, next action. If it is not in that file, it did
  not happen, and the next pass will rebuild the same fault.
- **Porting is the only stage that touches `src/`**, and it goes through the chokepoints, the guard
  manifest and `./build.sh test`.

## The three rules

1. **An example shows a direction; the layout is what `src/` already draws.** Anchors come from code.
2. **Measure all four sides** before choosing a border thickness. Compare neighbours, not one panel.
3. **Assert, don't eyeball.** Interior == target rect ±2 px, checked in code; previews on live geometry.

## Steps

1. **Gate.** `./build.sh test` stays locked until `LEARN_THE_CODE.md`'s six questions are answered
   into `/tmp/ans.txt` and `./build.sh --verify /tmp/ans.txt` passes.
2. **Folder, now.** `cp -r tables/_template tables/table_<name>`, then fill in its `README.md`.
   Created at the moment the table starts, never ahead of time. Open
   `table-to-scale.png`, work from `anchors.json`. A number that is not there gets
   found in `src/` and added — then `./tables/build.sh` proves it still matches. `palettes.png` has
   the colours for all eight backdrops already sampled.
3. **Layer, before any art.** *Runtime restyle* — `ui_draw_panel_border()` (`ui_theme.c:583`, 76 call
   sites) and/or `board_draw_card()` (`ui_board.c:715`): every panel and card gets the look at its
   exact rect, no overlap, no scaling, nothing to mis-place. A plain rectangular frame is always
   runtime. *Stamped art* — PNGs at anchors, only for what a rect cannot express: edge brackets,
   ornaments, a casing with a profile. Default: runtime for frames, art for decoration.
4. **Budget.** `anchors.md` → free space. Short version: no per-lane frame ever; the right column has
   8 px on one side and 80 on the other; the left blocks 12 and 163. **Uniform borders come out thin**
   — use asymmetric ones with the weight on the inner side.
5. **Unit art**, only if step 3 said art. Ask for **one empty frame**, state the interior's aspect
   ratio, pure black background, no scenery. A wrong compartment count cannot be scaled into place.
   Output goes to `tables/table_<name>/raw/`, never edited in place.
6. **Cut out** — solid silhouette, not soft alpha. `python3 tools/table_art.py cut|build` implements
   this whole step; do not rewrite it in a scratch script. `./tables/build.sh table_<name>` builds the
   table's `table.json` and writes `final/` + `preview/` into that table's folder. The recipe:
   seed `max>26` → 13 px close → fill enclosed blobs <90 px → 0.7 px feather → grade to the sampled
   palette → **clear the interior to alpha 0** (a render keeps a wash that darkens the game's text).
   Sanity: 40–55 % fully opaque.
7. **Compose on live geometry.** `./tables/build.sh base <name>` → `tables/table_<name>/base.png`.
   Never composite on `docs/assets/readme/table-map.png` (dead 96 px layout). `table_art.py build` prints
   unit / final / interior / target / status and must end `0 mismatch(es)`.
8. **Verify.** One preview on the real backdrop **and** one on a flat canvas; both looked at, at the
   real pitch. `./build.sh test`, `git diff --check`, `CHECKLIST.md` ticked. If step 3 chose runtime,
   it is a code change: `./tools/action_guard.sh --report` too, because the Action Card mode is
   hash-frozen.
9. **Beta port, then live test.** A port is three things: the forged assets copied from
   `table_<name>/final/` into `assets/`, the drawing wired through the chokepoints of step 3, and a
   way to select this table in-game (open decision 6). `final/` stays the source of truth; `assets/`
   only ever holds a copy. Then **the owner plays** — the forge's job is to make that test possible
   and to write down what it found, verbatim, in the table's `live-test.md`: build, what was played,
   what broke as symptom → where → expected, verdict, next action. Anything that broke goes back to
   step 3 as the new work list. A table is forged when a live round found nothing left to fix.
   Gate the whole skin behind one predicate so the unselected path stays byte-identical, and
   keep that predicate free of the `settings` global (refresh a file-static per table frame) —
   the headless UI tests link sidebar/card paths without stubbing `settings`, and they will
   find the reference for you.

## Delivered art (owner-provided pieces)

When the owner delivers finished pieces (`tables/assets_<name>/`), there is no generation step
and `stamp()` is forbidden — the method is **cut-and-place**, implemented as
`build_cut_and_place` in `tools/table_art.py` and driven by the table's `table.json` record
(`"method": "cut-and-place"`, one entry per compartment: crop, measured cavity, anchor,
alignment, scale, placed cavity/outer). Castle is the worked example;
`tables/table_castle/README.md` carries its numbers.

- Pieces arrive as positioned layers at a multiple of the canvas (castle: 1920×1080 = 1.5×).
  Each compartment is cropped at native resolution, given ONE uniform downscale to canvas
  space (×⅔), plus only a recorded per-compartment uniform scale where a measured cavity
  demands it — minimal, owner's rule — then **translated** so its measured cavity lands on the
  anchor. No 9-slice, no non-uniform resizing, ever: chunky frames keep their proportions and
  are never flattened into the old UI's 1 px lines.
- **Measure compartments; never place pieces whole.** A delivery's internal rhythm is the
  generator's, not the game's (castle: stack pitch drifts up to 30 px from the game's 41.4;
  hand strips carry 7 frames for 8 cards). Enumerate compartments piece by piece —
  band-averaged luminance for the dark cavities, alpha extents for the outers — and assert
  every cavity against `anchors.json`, never against the delivery's own clean plate (trap 16).
- Delivered panel interiors stay **opaque** (the art's own leather). Step 6's "clear the
  interior to alpha 0 / 40–55 % opaque" is the generated cut-out recipe, not this path.
  Static panels composite into one 1280×720 overlay in `final/`; at runtime it draws above
  the dimmed backdrop/ash and below every live element, and only the CRT fills/hairlines it
  replaces are gated off — text, counts, flash fills, glows, tooltips stay.
- **One card bezel per table, provided transparent by the owner** (trap 17): inside empty,
  only the border kept, corner ornaments whole. The fitter measures its transparent cavity
  from the file's alpha into `final/_manifest.json` and passes the file through; the game
  mirrors the constants and draws it per card — under and over (sandwich), scaled by card
  height — never as a placed strip, so every live hand count works. There needs to be one
  card bezel for every table built this way; without it the cards sit naked in a framed UI.
- A contiguous stack of panels is aligned as **one object**: pick the edge the eye tracks and
  flush that edge for the whole stack; the far edges stagger where ornaments hide it
  (trap 18). Which edge that is gets settled by measuring the owner's screenshot, not by
  arguing alignments in the abstract.

## The traps, in the order they were hit

1. **Mockup pixels as layout.** Nothing fitted.
2. **`board_layout.h` is dead code.** `board_field_slot_of()` has no callers; `board_field_slot_xy()`
   is only reached via `board_get_field_card_rect()`, which has none. Live is `table_slot_rect()`.
3. **The README's table figure uses the dead layout**, so it puts cards 96 px apart instead of 81.
   Three rounds lost to compositing on it.
4. **Soft alpha instead of a cut-out.** Luminance-derived alpha makes the metal translucent.
5. **Multi-compartment panels from a generator.** Wanted 66/218/218, got 44/120/260; one scale cannot
   reconcile it, and band-surgery broke every joint.
6. **Per-lane frames are geometrically impossible.** 37.4 tall, 4 px gaps, 4 px inset.
7. **Frames drawn inside the rect.** The game's text ended up on the metal.
8. **Fitting by measured holes.** Only valid when the art's hole count equals the code's rect count.
9. **Stamping every rect individually.** Ten rects in a 267 × 700 column has nowhere to go — group
   them, and use rails for internal seams.
10. **Uniform border thickness.** Dragged down to the tightest free side; reads thin next to its
    neighbours.
11. **Decor in the middle of the left side.** A spine between the blocks was rejected twice. A
    connector belongs on the **canvas edge**.
12. **Rails drawn before the casing.** Covered until the draw order flipped.
13. **The Action Card mode is hash-locked.** `ui_action.c/.h` are hash-guarded and 12 influence lines
    outside them are pinned, 4 inside `board_draw_table()`; its stock `330, 262` and meter
    `414, 170` cannot move. Run `./tools/action_guard.sh --report`.
14. **No bezel art ships.** `assets/` has no frame, border or panel texture — a metal look is
    procedural in the border function or a new 9-slice asset.
15. **An example treated as evidence — twice.** Once as a layout to copy, once as a claim about the
    current build. Both wrong the same way: the example's layout is the layout of whatever was fed to
    the generator. Read it for the idea.
16. **Trusting the clean plate** (castle, 30 Oct 2026). The delivered pieces stack into a flawless
    composite — mean |RGB diff| 1.36 against their own clean plate — and still miss the anchors by
    up to 30 px. A plate agreeing with itself is the generator agreeing with itself; the game was
    never in the room. Measure compartments against `anchors.json`, always.
17. **Reporting the asset you meant to make** (castle bezels, 30 Oct 2026). A card bezel only
    works over a card if its inside is transparent and nothing but the border remains — every
    delivered-art table has at least its card bezel(s) to make transparent this way: flood-clear
    the dark interior from the centre so the corner ornaments that sweep inward survive, and
    zero the RGB under the cleared alpha. The first pass cleared a rectangle, chopped the
    ornaments to a hairline, blurred what was left — and was reported as done anyway, round
    after round, until the owner made a correct one by hand in five minutes. Rule: never state
    a property of an asset — transparent, fitted, on-canvas — without opening the produced
    file in the same pass (checkerboard composite, alpha numbers). Do not lie about the asset;
    check it. The settlement: a transparent card bezel is **owner-provided** art
    (`assets_castle/castle_card_bezel.png`), not something the agent manufactures — the fitter
    measures its cavity, the game mirrors the constants.
18. **Aligning a stack by the wrong edge** (castle right tower, 30 Oct 2026). The RND box read
    "offset right" under outer-right placement (its left rail indented 10 px against the combo
    panels), and still read offset with cavity centres aligned (5 px indent) — in one
    contiguous column the eye tracks the **left rail line**, so the tower shares one outer-left
    edge and the right edges stagger into the screen-edge ornaments. Two alignment theories
    were argued from the record; the owner's screenshot, measured, settled which edge was off.
19. **The casing is not the only box** (castle, 30 Oct 2026). Gating the board's own fills is
    not enough: `ui_main.c` redraws a hitbox-sized fill+border on PAUSE/RESTART hover, which
    inside the wider bronze cavity read as a button offset right — gate the box under the
    skin, keep the hover label. And not every `board_draw_card()` call is a table card: the
    capture-info hover preview is a presentation draw and wore a bezel over the CRT panel
    chrome until given a mute switch (`board_set_castle_bezel_muted`). Audit every draw path
    that touches a skinned element, not just the table pass.

## Open decisions

1. **Layer** (step 3) — and if runtime, what happens to the teal `ui_draw_panel_border` line inside
   every frame: as long as it stays, the result reads as a box inside a box.
2. **Grouping** — score strip + lane block as one casing with rails; the three right-column boxes as
   one column; buttons as one unit.
3. **Lane metrics** — keep 37.4 / 4 px and group the frames, or change them in code.
4. **Palette** — a `TerminalTheme` entry (today `theme_set()` cycles 5 while Options lists 6 names) or
   a separate `table_design` field in `GameSettings`.
5. **Nine tables** — one layout, nine palettes and casing profiles, or bespoke ornament per backdrop.
6. ~~**The port contract**~~ — **decided 29 Oct 2026: option B, done.** `GameSettings.table_design`
   (`storage.h`, persisted as `table_design=` in `settings.txt`), `TABLE_DESIGN_COUNT` for the range,
   and a picker row in the Pancratium detail panel: `CARD TABLE:  <  CRT  >  DEFAULT`, plus the
   placeholder entry `CITY (BETA)` / `PLACEHOLDER - NOT FORGED YET`. It sits at y 500..526 inside the
   panel and cycles with a click, saving immediately. **Nothing renders a skin differently yet** —
   index 0 is the CRT table, index 1 is the door the first forged table walks through.
set.
7. **Frames drawn inside the rect.** The game's text ended up on the metal.
8. **Fitting by measured holes.** Only valid when the art's hole count equals the code's rect count.
9. **Stamping every rect individually.** Ten rects in a 267 × 700 column has nowhere to go — group
   them, and use rails for internal seams.
10. **Uniform border thickness.** Dragged down to the tightest free side; reads thin next to its
    neighbours.
11. **Decor in the middle of the left side.** A spine between the blocks was rejected twice. A
    connector belongs on the **canvas edge**.
12. **Rails drawn before the casing.** Covered until the draw order flipped.
13. **The Action Card mode is hash-locked.** `ui_action.c/.h` are hash-guarded and 12 influence lines
    outside them are pinned, 4 inside `board_draw_table()`; its stock `330, 262` and meter
    `414, 170` cannot move. Run `./tools/action_guard.sh --report`.
14. **No bezel art ships.** `assets/` has no frame, border or panel texture — a metal look is
    procedural in the border function or a new 9-slice asset.
15. **An example treated as evidence — twice.** Once as a layout to copy, once as a claim about the
    current build. Both wrong the same way: the example's layout is the layout of whatever was fed to
    the generator. Read it for the idea.

## Open decisions

1. **Layer** (step 3) — and if runtime, what happens to the teal `ui_draw_panel_border` line inside
   every frame: as long as it stays, the result reads as a box inside a box.
2. **Grouping** — score strip + lane block as one casing with rails; the three right-column boxes as
   one column; buttons as one unit.
3. **Lane metrics** — keep 37.4 / 4 px and group the frames, or change them in code.
4. **Palette** — a `TerminalTheme` entry (today `theme_set()` cycles 5 while Options lists 6 names) or
   a separate `table_design` field in `GameSettings`.
5. **Nine tables** — one layout, nine palettes and casing profiles, or bespoke ornament per backdrop.
6. ~~**The port contract**~~ — **decided 29 Oct 2026: option B, done.** `GameSettings.table_design`
   (`storage.h`, persisted as `table_design=` in `settings.txt`), `TABLE_DESIGN_COUNT` for the range,
   and a picker row in the Pancratium detail panel: `CARD TABLE:  <  CRT  >  DEFAULT`, plus the
   placeholder entry `CITY (BETA)` / `PLACEHOLDER - NOT FORGED YET`. It sits at y 500..526 inside the
   panel and cycles with a click, saving immediately. **Nothing renders a skin differently yet** —
   index 0 is the CRT table, index 1 is the door the first forged table walks through.
