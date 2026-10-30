# AGENT START — read this first

You are forging a card-table skin for Xeno-koi (raylib C game, canvas 1280×720). This file is
your starting prompt and map. It does not replace the canon — it routes you into it and lists
the rules that were paid for. When anything here disagrees with `WORKFLOW.md`, `WORKFLOW.md`
wins and this file is stale: say so in your reply.

## Read, in this order

1. `tables/README.md` — what the forge is, the folder law, current status.
2. `tables/WORKFLOW.md` — the loop, the steps, **Delivered art**, the traps (read all of them;
   each one cost a round), the open decisions.
3. `tables/CHECKLIST.md` — tick it as you go; every line is a mistake already made once.
4. `tables/anchors.md` + `tables/anchors.json` — every layout number you may use, each with its
   `src/` source line. `./tables/build.sh` must print `anchors in sync` before you trust them.
5. The table's own folder `tables/table_<name>/` — its `README.md` (spec + measured record),
   `table.json` (machine record), `live-test.md` (what broke live; treat as your work list).
6. `LEARN_THE_CODE.md` gate before `./build.sh test` unlocks (answer into `/tmp/ans.txt`,
   `./build.sh --verify /tmp/ans.txt`).

`tables/table_castle/` is the worked example of a finished loop. Read its README and
live-test.md before starting a new table — most of what follows was learned there.

## The loop

```
forge (tables/)  ->  beta port (src/, selectable)  ->  owner plays  ->  back to the forge
```

- **One table at a time, one folder per table.** Everything for a table lives in
  `tables/table_<name>/`; owner-delivered art stays untouched in `tables/assets_<name>/`.
- `src/` is touched **only** at beta port. The port must make the table selectable in the
  Pancratium — an unselectable table cannot be tested, and an untested table is unfinished.
- The owner's live round is the visual gate. The build sandbox has no display/GL: you cannot
  screenshot the game, so never claim a visual result you have not seen. Previews and
  measurements are your evidence; the verdict is the owner's.
- Every live test goes into the table's `live-test.md` — build, what was played, what broke
  (symptom → where → expected), verdict, next action. If it is not in that file, it did not
  happen and the next pass rebuilds the fault.

## Hard rules (paid for — do not re-buy)

1. **Geometry only from `anchors.json` / `src/`.** Examples and mockups show direction, never
   numbers. Never composite on `docs/assets/readme/table-map.png` (dead layout).
2. **Never squish art.** No non-uniform scaling, no 9-slicing delivered art into the old UI's
   1 px lines. Delivered art: crop at native res, ONE uniform downscale to canvas (×⅔ from
   1920×1080), a further uniform scale only where a measured cavity demands it — minimal and
   recorded — then translate. Chunky frames keep their proportions.
3. **Live text stays live.** Scores, counts, status, timers, combos, tooltips and hover panels
   keep drawing where the game draws them; the skin replaces casings, never information.
4. **One card bezel per table, provided by the owner, transparent inside** — only the border,
   corner ornaments whole. Do not manufacture it (WORKFLOW trap 17). You measure its
   transparent cavity from the file's alpha, record it, and mirror the constants in game code.
   It is drawn per card — under and over (sandwich) — never as a placed strip, so an 8-card
   hand works even when a delivered strip holds 7 frames.
5. **CRT (index 0) stays pixel-identical** when the skin is off. One predicate gates the skin;
   keep it free of the `settings` global (headless tests link UI paths without it).
6. **The Action Card mode is hash-frozen.** Run `./tools/action_guard.sh --report` before and
   after touching `src/`; the 12 pinned lines and both file hashes must survive. Same for the
   tests lock: never edit `tests/`.
7. **Verify on the produced file, never on intention.** Any claim about an asset (transparent,
   fitted, on-canvas) needs the file opened in the same pass: checkerboard composite, alpha
   numbers, bounding boxes. Do not lie about the asset — check it. The owner's screenshot is
   ground truth for alignment questions: measure its pixels.
8. **Existing picker indices never move.** New tables append (`TABLE_DESIGN_*` in
   `src/storage.h`, names in `src/campaign.c`); old saves keep their meaning.

## The two art paths

- **Generated art** → `tools/table_art.py` stamp path (`units`/`decor`/`rails`/`cards` in
  `table.json`): cut-out recipe, interiors cleared to alpha 0, 40–55 % opaque sanity.
- **Delivered art** → `"method": "cut-and-place"`: `build_cut_and_place` crops each recorded
  compartment and translates its measured cavity onto the anchor. Static panels composite
  into one overlay PNG; bezels stay separate (cards move). Full method: `WORKFLOW.md` →
  *Delivered art*.

## Commands

```sh
./tables/build.sh                  # anchors-vs-src check (must say: anchors in sync)
./tables/build.sh table_<name>     # build that table's final/ from its table.json
./build.sh test                    # full test suite incl. Action Card lockdown
./tools/action_guard.sh --report   # frozen-file hashes + pinned influence lines
```

Python forge tools run from the repo venv: `PATH=.venv/bin:$PATH`.

## How to report

Answer the owner's points one by one, in their words, worst first. State measured facts with
numbers ("cavity x 1141.9..1259.9, centre 1200.9"), label assumptions as assumed, and say
plainly what you could not verify (no display in the sandbox). If the owner corrects you,
the correction goes into the table's `live-test.md` and, if it generalises, into
`WORKFLOW.md`'s traps — before the next claim.
