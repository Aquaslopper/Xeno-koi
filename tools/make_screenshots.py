#!/usr/bin/env python3
"""
The wiki's real screenshots: the frames captured from the running game, plus the close-ups
cropped out of them.

Everything else under docs/assets/ is drawn from source. These are not: a picture of the shipped
board is the one thing no tool here can produce, because this repo has no GPU and no headless
browser. So the six frames are captured by hand and committed as they came in, and this tool does
the two parts that can be generated:

  1. import  --from <dir> re-encodes a capture to WebP at its captured size. Run once per frame.
  2. crops   always derived, from the committed frame, so a crop can be redone by anyone with
             Pillow and no screenshot folder. The boxes are the game's own rectangles from src/
             (see CROPS below) times SCALE, not numbers copied off the image.

Geometry: the captures are 1920 x 1080 of a 1280 x 720 canvas, so one game pixel is 1.5 device
pixels and the canvas origin is the image origin - checked against the panel borders themselves
(CAPTURE_PANEL_W 255 at x=12, src/ui_theme.h:55 and src/ui_board.c:211, land on px 18 and 400).
SCALE is derived from the file on import, so a capture at another size keeps working as long as it
shows the whole canvas.

Encoding: lossless WebP measured 1451 / 1394 / 2444 / 632 / 1545 / 1371 KiB for the six frames -
8.8 MB of wiki for pixels nobody will inspect at 1:1. q95 is 344 / 326 / 596 / 118 / 213 / 218 KiB
(1.8 MB) at a mean absolute error of 0.4 to 1.5 per channel out of 255, worst sampled pixel 26 -
good enough that the frames are q95 while the card thumbnails next to them stay lossless, because a
23 px thumbnail has no scanlines to hide behind.
The captured frames are already dimmed and scanlined by assets/postprocess.fs, so a slightly
lossier encode lands on top of noise the game put there on purpose.

Usage:
  python3 tools/make_screenshots.py --from ~/shots   # import the six captures
  python3 tools/make_screenshots.py                  # re-crop from what is committed
  python3 tools/make_screenshots.py --check          # exit 1 if any image is missing

To capture a new frame: run the game (or the browser build), get to the state, screenshot the
whole window, and save it as PNG. Do not crop it by hand and do not export a JPEG - the crop is
this tool's job, because it is the part that has to line up with the layout constants.
"""
import pathlib
import sys

from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "assets" / "screenshots"
MISC_CAP = 2268              # the reference sheet keeps its own width; the click-to-zoom is the point
# A scan of printed cards is not UI pixels: at native width it measured 2054 KiB lossless, 669 at
# q95, 500 at q90 and 398 at q85, and shrinking to 1600 wide bought less than the detail it threw
# away. q90 is where the halftone stops costing bytes.
MISC_QUALITY = 90
CANVAS_W, CANVAS_H = 1280, 720        # the game's fixed canvas, src/ui_main.c
QUALITY = 95
WEBP = dict(quality=QUALITY, method=6)
# every border in this game is drawn on its rectangle's own edge (ui_draw_panel_border), so a crop
# the exact size of a rect clips the line in half. Four game pixels of air on each side.
PAD = 4

# out-name, the file name it came in as, and what the frame has to show for the caption to hold
FRAMES = [
    ("board-mid-round", "board mid round.png",
     "four turns into a round: half the field gone, both capture columns filling"),
    ("card-tooltip", "card tool tip.png",
     "a field card held under the cursor, with its tooltip beside it"),
    ("koi-prompt", "koi koi prompt.png",
     "the YAKU DETECTED overlay open over the board, two buttons at the bottom"),
    ("deus-taunt", "deus taunt.png",
     "the mono deck, the round-start banner and DEUS's transmission panel"),
    ("match-result", "match result.png", "the match-over screen with its round ledger"),
    ("options", "option.png", "the whole options screen, every setting at its default"),
    ("campaign", "camp.png", "the sector grid with one opponent's dossier open"),
    ("stats", "stats.png", "the stats screen, the last ten matches and the replay controls"),
]

# out-name, which frame it is cropped from, and the box in GAME pixels. The boxes are the
# rectangles the code draws, quoted; tooltip-card is the one exception and says so.
CROPS = [
    # draw_capture_lanes(ba, g, 0, 508.0f), src/ui_board.c:302 - four rows of CAPTURE_PANEL_H
    # (37.4f, src/ui_theme.h:56) stepped by +4, then the YOU SCORE box at y=678 (ui_board.c:303).
    ("capture-lanes", "board-mid-round", (12, 502, 255, 206),
     "your four lanes and your score line"),
    # Rectangle panel = { 200, 45, 880, 630 }, src/ui_screens.c:466 - the whole decide overlay,
    # buttons included (they sit at y=520, :503-504).
    ("koi-panel", "koi-prompt", (200, 45, 880, 630),
     "the decide overlay without the darkened board around it"),
    # Rectangle box = { 12, 260, 255, 95 }, src/ui_taunt.c:43 and :120 - same rect for both
    # callers, which is why the panel never moves mid-round.
    ("taunt-panel", "deus-taunt", (12, 260, 255, 95),
     "the transmission panel on its own"),
    # Measured off the capture, not from src: the tooltip follows the cursor (panel = {floor(max(8,
    # x)), floor(max(8, y)), ceil(width), height}, src/ui_card_tooltip.c:111) so it has no fixed
    # rect to crop against. The box is the panel plus the card it describes.
    ("tooltip-card", "card-tooltip", (660, 300, 292, 136),
     "the tooltip next to the card it is describing"),
    # Rectangle slot = { 155 + i * 82, 96, 68, 110 } for i in 0..11, src/campaign.c:120, plus the
    # M-nn captions drawn under the tiles.
    ("opponent-grid", "campaign", (150, 88, 983, 132),
     "twelve tiles, nine of them open"),
    # PRIORITY YAKU at (385, 324), SIGNATURE TARGET at (385, 344), CLEARANCE PROTOCOL at (385, 368)
    # and the three preset buttons spanning y 364..390 - src/campaign.c:162-180.
    ("dossier-row", "campaign", (378, 316, 668, 78),
     "one opponent's dossier, the three rows that matter"),
    # #define ROW_X 650 / ROW_Y 155 / ROW_W 400 / ROW_STEP 28, src/ui_replay.c:16-20, ten rows, with
    # the "LAST 10 MATCHES" heading at (650, 120) from src/ui_screens.c:329. The width is not the
    # rect: the line it draws runs to game x 1116 in this capture, 66 px past the 400-wide highlight,
    # because draw_text_snapped is not clipped to ROW_W - so the crop follows the text.
    ("stats-log", "stats", (646, 112, 478, 328),
     "the match log, ten raw history lines"),
]

# Not a frame of the canvas: a reference picture the wiki keeps beside the game's own art. It is not
# the 1280 x 720 canvas at any scale, so it skips the geometry rules above - import only, no crops.
MISC = [
    ("deck-sheet", "test.jpg", 2268, 1190,
     "the 48 cards of a traditional hanafuda sheet, twelve month columns of four"),
]


def crop_box(x: float, y: float, w: float, h: float, scale: float) -> tuple:
    """A game rectangle -> device pixels, padding included. Both the cropper and --check go through
    this, because rounding the two edges is not the same as rounding the width."""
    return tuple(int(round(v * scale)) for v in (x - PAD, y - PAD, x + w + PAD, y + h + PAD))


def webp(im: Image.Image, path: pathlib.Path) -> int:
    im.convert("RGB").save(path, "WEBP", **WEBP)
    return path.stat().st_size


def frame_size(im: Image.Image) -> float:
    """Device pixels per game pixel, read off the file. 1920 x 1080 gives 1.5; the canvas only has
    to be in frame at its own aspect ratio, whole-number scale is not required."""
    w, h = im.size
    if w * CANVAS_H != h * CANVAS_W:
        raise SystemExit(f"capture is {w}x{h}, which is not the {CANVAS_W}x{CANVAS_H} canvas - "
                         "screenshot the whole window, do not crop it by hand")
    return w / CANVAS_W


def import_from(src: pathlib.Path) -> float:
    scale = None
    for out, fname, why in FRAMES:
        p = src / fname
        if not p.exists():
            print(f"  skip {out}: {fname!r} not in {src}  ({why})")
            continue
        im = Image.open(p)
        s = frame_size(im)
        if scale is None:
            scale = s
        elif s != scale:
            raise SystemExit(f"{fname}: scale {s} does not match the other captures ({scale})")
        if im.mode == "RGBA":
            flat = Image.new("RGB", im.size, (0, 0, 0))
            flat.paste(im, mask=im.getchannel("A"))   # the game's own black is the matte
            im = flat
        OUT.mkdir(parents=True, exist_ok=True)
        n = webp(im, OUT / f"{out}.webp")
        print(f"  {out}.webp  {im.size[0]}x{im.size[1]}  {n / 1024:.0f} KiB")
    return scale or 1.5


def import_misc(src: pathlib.Path) -> None:
    """The reference pictures: no canvas geometry, just a width cap and one encode."""
    for out, fname, _w, _h, _why in MISC:
        q = src / fname
        if not q.exists():
            print(f"  skip {out}: {fname!r} not in {src}")
            continue
        im = Image.open(q).convert("RGB")
        if im.size[0] > MISC_CAP:
            im = im.resize((MISC_CAP, round(im.size[1] * MISC_CAP / im.size[0])), Image.LANCZOS)
        im.save(OUT / f"{out}.webp", "WEBP", quality=MISC_QUALITY, method=6)
        n = (OUT / f"{out}.webp").stat().st_size
        print(f"  {out}.webp  {im.size[0]}x{im.size[1]}  {n / 1024:.0f} KiB   (imported)")


def crop_all(scale: float) -> None:
    for out, from_frame, (x, y, w, h), why in CROPS:
        src = OUT / f"{from_frame}.webp"
        if not src.exists():
            raise SystemExit(f"{out}: no {src.relative_to(ROOT)} to crop from")
        im = Image.open(src)
        box = crop_box(x, y, w, h, scale)
        if box[0] < 0 or box[1] < 0:
            raise SystemExit(f"{out}: the padded box runs off the top-left of the frame")
        if box[2] > im.size[0] or box[3] > im.size[1]:
            raise SystemExit(f"{out}: box {box} runs past the {im.size} frame - the game rects in "
                             "src/ moved, or the capture is cropped")
        n = webp(im.crop(box), OUT / f"{out}.webp")
        print(f"  {out}.webp  game ({x},{y},{w},{h}) + {PAD} -> {box[2]-box[0]}x{box[3]-box[1]} px "
              f" {n / 1024:.0f} KiB   ({why})")


def scale_of_committed() -> float:
    """The scale the frames were committed at, so the crops are checked against them and not
    against a number written down in this file."""
    for out, _f, _w in FRAMES:
        p = OUT / f"{out}.webp"
        if p.exists():
            return Image.open(p).size[0] / CANVAS_W
    return 1.5


def check() -> int:
    scale = scale_of_committed()
    missing, bad, refs = [], [], []
    for out, _fname, _why in FRAMES:
        p = OUT / f"{out}.webp"
        if not p.exists():
            missing.append(p.name)
        elif Image.open(p).size[0] / CANVAS_W != scale:
            bad.append(f"{p.name} is not at the {scale}x scale its neighbours are")
    for out, _frame, (x, y, w, h), _why in CROPS:
        p = OUT / f"{out}.webp"
        b = crop_box(x, y, w, h, scale)
        want = (b[2] - b[0], b[3] - b[1])
        if not p.exists():
            missing.append(p.name)
        elif Image.open(p).size != want:
            bad.append(f"{p.name} is {Image.open(p).size}, the crop box in src/ plus {PAD} px says "
                       f"{want}")
    if missing:
        print(f"missing screenshots: {', '.join(missing)} - run tools/make_screenshots.py")
    for out, _fname, _w, _h, _why in MISC:
        if not (OUT / f"{out}.webp").exists():
            refs.append(f"{out}.webp is missing")
    for b in bad:
        print("wrong size:", b)
    for r in refs:
        print("reference picture:", r)
    if missing or bad or refs:
        return 1
    print(f"screenshots present ({len(FRAMES)} frames, {len(CROPS)} crops, "
          f"{len(MISC)} reference picture).")
    return 0


def main() -> int:
    if "--check" in sys.argv:
        return check()
    src = None
    if "--from" in sys.argv:
        src = pathlib.Path(sys.argv[sys.argv.index("--from") + 1]).expanduser()
        if not src.is_dir():
            raise SystemExit(f"--from {src} is not a folder of captures")
    scale = import_from(src) if src else 1.5
    if src:
        import_misc(src)
    crop_all(scale)
    if src:
        print(f"imported from {src}; crops re-derived at scale {scale}")
    else:
        print("no --from: frames untouched, crops re-derived from what is committed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
