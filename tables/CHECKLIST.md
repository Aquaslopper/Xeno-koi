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
- [ ] delivered art: every compartment measured against its anchor (profiles, not the clean plate);
      translations and any uniform scale listed with the cavity numbers; nothing 9-sliced
- [ ] borders as thick as the space allows; neighbours comparable
- [ ] no per-lane frame; rails drawn **after** the casing
- [ ] connective art on the **canvas edge**, never a spine mid-left
- [ ] metal opaque, cavities alpha 0, interior cleared hard; 40–55 % opaque (generated path;
      delivered art keeps its opaque leather cavities as delivered)
- [ ] every moving card size framed: 76×124, 88×143, 56×91 (not the 17×31 minis)
- [ ] exactly **one card bezel**, provided transparent by the owner — inside empty, only the
      border, corner ornaments whole. Cavity measured from the file's alpha (never drawn by
      me), constants mirrored in game code; worn under and over each card, front and back;
      verified on a checkerboard composite at every live card size
- [ ] a contiguous stack of panels aligned as one object: the edge the eye tracks is flush,
      far edges stagger where ornaments hide it — and the flush is proven in px, not eyeballed

**Verifying**
- [ ] `table_art.py build` ends `0 mismatch(es)`
- [ ] previews on `base.png` (never `table-map.png`), at the real pitch (81 / 96 / 62)
      — or, for a runtime-only look, the beta port is the preview and this line is marked N/A
- [ ] one preview on the real backdrop and one on flat canvas, both looked at
- [ ] I can quote the measured fact behind every claim ("opaque x 21..54", not "it's on the edge")
- [ ] anything assumed is labelled assumed

**Porting and testing (only stage that touches `src/`)**
- [ ] the port contract is decided, not improvised: how this table is selected in-game
      (`WORKFLOW.md` open decision 6)
- [ ] ported as beta so it can actually be picked and played
- [ ] every draw path over a skinned element audited, not just the table pass: hover/pressed
      boxes the game redraws itself (e.g. `ui_main.c` PAUSE/RESTART) gated under the skin with
      labels and feedback kept; presentation draws (inventory/detail card previews) checked —
      the bezel is muted where the card is not a table card
- [ ] full hands and every smaller count framed (a delivered strip may hold fewer frames than
      the 8 cards the game deals — the bezel is drawn per card, never as the strip)
- [ ] the owner played a real round — deal, capture, call, drag, pause, restart — or it is stated
      that nobody has
- [ ] the owner's screenshot measured, not eyeballed: panel edges/centres in px against the
      record; anything off is a number before it is an opinion
- [ ] everything that broke is in `live-test.md` — symptom, where, expected — not in my head

**Presenting**
- [ ] previews are standalone files in the repo, and the main one is presented
- [ ] the owner's points are answered one by one, in their words; what is still wrong comes first
- [ ] `git diff --check` clean; `src/` untouched unless runtime was chosen
- [ ] if `src/` changed: `./build.sh test` green, `./tools/action_guard.sh --report` run

**The question that catches the most:** can I show, with a number, that this frame is where I say it
is? If the answer is a picture, it is not verified.
