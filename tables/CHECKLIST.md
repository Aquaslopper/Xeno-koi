# Checklist — before showing anyone a table

Every line is a mistake already made once. Order matters: a wrong "yes" near the top invalidates
everything under it.

**Before drawing**
- [ ] the table has its own folder `tables/table_<name>/`; anything I build is written inside it
- [ ] `./tables/build.sh` prints `anchors in sync` (if it drifts, re-derive the number — don't work around it)
- [ ] every number came from `anchors.json`; none from an example, a mockup or a screenshot
- [ ] nothing I am about to assert came from `refs/` — an example shows a direction, not a position
- [ ] I know the free space **on all four sides** of the panel, and the layer (runtime / stamped art)
- [ ] I know what the game paints there at rest (`anchors.md` → fills): nothing behind the field

**While drawing**
- [ ] frames drawn **outside** their rect; interior == the game's rect ±2 px
- [ ] borders as thick as the space allows; neighbours comparable
- [ ] no per-lane frame; rails drawn **after** the casing
- [ ] connective art on the **canvas edge**, never a spine mid-left
- [ ] metal opaque, cavities alpha 0, interior cleared hard; 40–55 % opaque
- [ ] every moving card size framed: 76×124, 88×143, 56×91 (not the 17×31 minis)

**Verifying**
- [ ] `table_art.py build` ends `0 mismatch(es)`
- [ ] previews on `base.png` (never `table-map.png`), at the real pitch (81 / 96 / 62)
- [ ] one preview on the real backdrop and one on flat canvas, both looked at
- [ ] I can quote the measured fact behind every claim ("opaque x 21..54", not "it's on the edge")
- [ ] anything assumed is labelled assumed

**Presenting**
- [ ] previews are standalone files in the repo, and the main one is presented
- [ ] the owner's points are answered one by one, in their words; what is still wrong comes first
- [ ] `git diff --check` clean; `src/` untouched unless runtime was chosen
- [ ] if `src/` changed: `./build.sh test` green, `./tools/action_guard.sh --report` run

**The question that catches the most:** can I show, with a number, that this frame is where I say it
is? If the answer is a picture, it is not verified.
