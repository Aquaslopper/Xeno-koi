# 01 — RELIQUARY (metal bezel design)

Status: **spec only.** No art is kept, nothing is implemented, `src/` is untouched.

Design intent, read off the owner's mockup (29 October 2026):

- **Metal over phosphor.** Panels framed in tarnished bronze / gunmetal bevels instead of 1 px
  teal strokes; card edges get a warm bezel instead of a dark hairline.
- **Warm neutral UI text** sampled from the mockup: bone `(112,112,96)` against today's teal
  `(80,144,144)`.
- **Near-opaque panel fill** (~220 alpha) instead of `COLOR_PANEL_BG`'s black at 178.
- **One connective spine** joining the two left-hand panel groups, running vertically through the
  free band at `0, 208, 267, 300` (see `anchors.md`).
- **Same layout.** Field, hands, stock, lanes, right column and buttons stay where they are. The
  mockup was measured against a real screenshot at the same scale and the layout is identical
  within ±8 px, so this is a skin, not a relayout.

## What it needs

Following `WORKFLOW.md`:

- **Step 3 (frame budget) is the binding constraint.** Lanes give 4 px of padding and are 4 px
  apart, so **no per-lane frame** — the eight lanes must be framed as **two groups** (one block
  around the CPU's score box plus four lanes, one around the player's), or the lane metrics in
  `src/ui_board.c` have to change first.
- **Step 4 (layer): runtime.** Every framed thing here is a plain rectangle and the game already
  frames all 76 panels at their exact rects through `ui_draw_panel_border()` (`src/ui_theme.c:583`).
  So the design is a change to that one function plus `board_draw_card()` (`src/ui_board.c:715`) —
  not a set of stamped PNGs.
- **Art is needed for exactly one element:** the spine in the free band. One unit, no
  compartments, decorative only.
- **Palette:** bone/neutral is either a 6th `TerminalTheme` (today `theme_set()` cycles 5, and the
  Options cycle is `% 5` while the name list has 6 entries) or a separate `table_design` field in
  `GameSettings`. One decision, not a rewrite.

## Open decisions

1. Frames around **groups** (no code change beyond the draw functions) or **per panel**
   (lane metrics change)?
2. Spine: procedural from the border function, or one cut-out PNG in the free band?
3. Action Card shares `ui_draw_panel_border()` and `board_draw_card()`. Restyling them changes
   how that frozen mode *looks* without editing its file. Per `AGENTS.md` that is allowed but
   must be reported — say yes or no explicitly before it happens.
4. Palette: 6th theme, or its own setting?
