# `04` — table_`city`

**One table = one folder.** Everything for this table lives here — spec, manifest record, previews. The source art stays where the owner delivered it: `tables/assets_city/`, untouched.

Status: **spec / pass 1 (forge, uniform-resize only).** `src/` touched: **no** (pass 1 is tables/ only — no beta port, no `src/` modification). CRT (index 0) unchanged. `table_city` is the placeholder at index 1 (`CITY (BETA)` / `PLACEHOLDER - NOT FORGED YET`); this pass forges it but does not port it.

Backdrop `assets/city.jpg`, index **4** of `{0 Harvest, 1 Hole, 2 Arctic, 3 Castle, 4 City, 5 Deus, 6 Face, 7 Realm}` (`src/ui_screens.c:326`).

Intent, one line: dark city-night casing — brass/gold ornate frames over black leather interiors — wrapping every panel group, with the field left bare exactly as the game paints it, and one owner-provided transparent card bezel worn under and over each card (sandwich) at port time.

## Source art (owner-delivered, never edited in place)

`tables/assets_city/`: seven 1920×1080 RGBA positioned layers, one UI group each, plus `city.png` (the example, with baked text) and `city_clean.png` (the clean stack), plus the owner-provided card bezel `bezzel_card_city.png` (136×232).

Pieces: `city_upper_left` (CPU block), `city_lower_left` (player block), `city_right_column` (right column), `city_status` (status plaque), `city_stock` (stock frame), `city_upper_hand` (8 CPU card frames), `city_lower_hand` (8 player card frames).

`city.png` is an **example**: the text in it sits in the generator's own layout, which is not the game's (trap 15). Direction only, never evidence.

## Method this pass: uniform resize only (per owner instruction)

Every piece resized by ONE uniform downscale ×0.6667 (1920×1080 → 1280×720). **No crop, no per-compartment scale, no non-uniform transform, no cut.** Pieces are positioned layers, so ×⅔ places each UI group at its canvas coordinates; they composite at canvas origin into `final/table_city_overlay.png`.

Verified: compositing the seven pieces at ×⅔ reproduces `city_clean.png` with mean|RGB diff| ≈ 3.9 (castle: 1.36) — the pieces are consistent positioned layers.

## Measurements — honest deltas vs anchors (pure resize, NOT forced to zero)

All numbers canvas px (1280×720). `delta` = cavity − anchor (x, y, w, h). Under pure resize these deltas stand; nothing was forced to fit.

| unit | cavity measured | anchor | delta (x,y,w,h) | on-canvas |
|---|---|---|---|---|
| cpu_score | 278×34 @ (20,11) | 255×30 @ (12,12) | (+8,−1,+23,+4) | yes |
| cpu_lane_0..3 | 274–276×40 | 255×37.4 | x −4..−7, y +6..+15, w +19..+21, h +3 | yes |
| you_lane_0..3 | 275–276×40–51 | 255×37.4 | x −6, y −25..−11, w +20..+21, h +3..+14 | yes |
| you_score | 300×57 @ (6,656) | 255×30 @ (12,678) | (−6,−22,+45,+27) | yes |
| rnd_panel | 137×68 @ (1134,13) | 112×66 @ (1160,12) | (−26,+1,+25,+2) | yes |
| cpu_combos | 138×181 @ (1135,93) | 112×218 @ (1160,88) | (−25,+5,+26,−37) | yes |
| your_combos | 128×182 @ (1135,314) | 112×218 @ (1160,316) | (−25,−2,+16,−36) | yes |
| pause | 123×44 @ (1139,366) | 112×40 @ (1160,560) | (−21,−194,+11,+4) | yes |
| restart | 123×47 @ (1139,402) | 112×40 @ (1160,610) | (−21,−208,+11,+7) | yes |
| status | 664×93 @ (351,539) | 670×88 @ (375,494) | (−24,+45,−6,+5) | yes |
| stock_frame | 114×182 @ (312,232) | 76×124 @ (330,262) | (−18,−30,+38,+58) | yes |

All 17 units on-canvas; none off-canvas.

## Alignment problem (owner decision needed)

Pure resize leaves **pause, restart, status, stock_frame** and the right column's internal vertical spacing **misaligned vs game anchors** (e.g. pause cavity at y366 vs the game's pause button at y560 — dy −194). Under pure resize a live round would draw frames offset from the clickable hitboxes. This cannot be fixed by resize alone.

Owner decision: accept the offsets as-is, or permit **per-compartment placement (translation only — no crop, no squash)** to align cavities to anchors. This is the one point where pass 1 stops for an owner call.

## Card bezel (owner-provided)

`bezzel_card_city.png` 136×232 native: opaque frame 132×228; **interior cavity 99×190, centre already alpha 0** (verified hollow — only the border kept, corner ornaments whole). Owner-provided art, never manufactured (trap 17). Passed through pixel-identical at port; cavity constants mirrored into game code. Drawn per card under+over (sandwich), scaled by card height. Cavity height 190 vs hand card 143 → ratio ≈ 0.75 (castle used ≈0.94) — port-time owner decision. Minis (17×31) un-bezeled; capture-info preview mutes it. **Not rendered in pass 1** (no `src/` touch). Not a placed strip.

## Palette (`tables/palettes.json` → `city.jpg`, sampled — not eyeballed)

Pending values from `tables/palettes.json` (regenerate with `./tables/build.sh`). Direction: black leather interiors, brass/gold ornate frames, dark city-night backdrop.

## Layer

- [ ] runtime restyle
- [x] stamped art — uniform-resize delivered layers (`table.json` is the record; no `stamp()`, no cut-and-place per compartment)
- nothing is painted behind the field (`anchors.md`: `nothing at all behind the field`).

## Previews

- `preview/city-fit-flat.png` — overlay on flat dark canvas
- `preview/city-fit-table.png` — overlay on `base.png` (real `assets/city.jpg` backdrop)
- `base.png` — `./tables/build.sh base city`

## Port notes (NOT done in this pass — pass 1 is tables/ only)

- Picker already exists: `GameSettings.table_design` + the Pancratium row (`CARD TABLE:`). `CITY (BETA)` is the placeholder at index 1 (`src/campaign.c`); city takes the next free index when ported.
- Final runtime copies: the seven resized pieces composited into `assets/table_city_overlay.png` + the bezel texture passed through from `bezzel_card_city.png`; `_manifest.json` records cavity constants.
- Runtime: one predicate, true only when city selected; predicate reads no `settings` global (headless UI tests link sidebar/card paths without it). Overlay draws after the backdrop dim, below every live element; CRT fills/hairlines gated off on skinned groups while city selected; byte-identical CRT path otherwise.
- Hitboxes stay the code rects; visual frames may extend inboard of clickable rects (decoration, not button).
- Frozen neighbours: action-card stock anchor `330,262` and meter `414,170` must not move; run `./tools/action_guard.sh --report` at port.

## Open questions for the owner

1. **Alignment fix.** Accept the pure-resize offsets (frames off hitboxes live), or permit per-compartment translation-only placement to align cavities to anchors?
2. **Status plaque vs timer.** The plaque (664×93) is wider than the score strip and sits at y539 vs the status line y529/turn-timer y505. Overlap acceptable, or shrink/move?
3. **Bezel scale ratio.** Cavity 190 vs hand card 143 → ≈0.75, shallower than castle's 0.94. Confirm intended grip.

## Checks run (this pass)

- `./build.sh --verify /tmp/ans.txt` → SUCCESS (build unlocked)
- `./tables/build.sh` → `anchors in sync with src/`
- `./tables/build.sh base city` → `tables/table_city/base.png`
- Uniform-resize build → 17 pieces + overlay, **0 off-canvas units** (honest deltas reported above, not forced)
- `python3 tools/table_map.py check` → `anchors in sync`
- Composite vs `city_clean.png` → mean|diff| ≈ 3.9 (pieces are consistent positioned layers)
