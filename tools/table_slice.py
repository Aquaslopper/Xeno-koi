#!/usr/bin/env python3
"""Cut a table artwork into one piece per area, so each area can be placed on its own.

The `tables/assets_*/` and `tables/assetes_castle/` folders hold the full-table artwork at 1920x1080
- the 1280x720 canvas at 1.5x. One image cannot be placed: a panel has to stand alone to be put
down at its own rect. This tool does exactly one thing - it crops. **It never changes a pixel.**
Every piece is a plain rectangle of the source, so the art that comes out is the art that went in.

What decides a cut:

  * only the game's own area edges (`tables/anchors.json`) - the x and y where a panel starts or
    ends. The gap between two cards in a row is not an edge: cutting there would leave a piece
    with no rect to belong to, so a row of cards stays one piece.
  * never tight around the art. Each piece is padded outward into the empty space around it
    (default 16 px, `--pad`), and the padding never crosses into a neighbouring piece's ink. The
    outer ornament therefore stays inside the piece instead of being shaved off at the boundary.
  * a piece is only ever as big as it has to be: a block that spans several areas stays one piece
    if those areas are drawn as one casing (city's left block), and is split when they are not.

    python3 tools/table_slice.py tables/assets_city      # cut one folder
    python3 tools/table_slice.py --all                   # every asset folder
    python3 tools/table_slice.py --all --pad 24          # wider breathing room

Writes, inside the folder it was given and nowhere else:

    pieces/NN-<area>.png   one piece per area - a crop, nothing else
    pieces.json            each piece's box in the source, where it goes, the rect it belongs to
    preview/pieces.png     contact sheet of the pieces
    preview/placed.png     the pieces put back at 1:1.5 on the real backdrop

It ends with a check, not an opinion: pasting every piece back at its own box must reproduce the
source image. `pieces.json` -> `recompose_max_diff` must be 0.
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont
from scipy import ndimage as ndi

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

# The artwork is rendered at 1.5x the 1280x720 canvas.
SCALE = 1.5

# The areas a table is made of, in canvas px, from tables/anchors.json - never from a picture.
GROUPS = [
    ("cpu-block", (12, 12, 255, 196)),        # score strip + four CPU lanes
    ("cpu-hand", (465, 12, 490, 91)),
    ("stock", (330, 262, 76, 148)),
    ("player-block", (12, 508, 255, 200)),    # four player lanes + you score strip
    ("player-hand", (330, 566, 760, 143)),
    ("status", (480, 518, 460, 34)),          # the turn line's banner
    ("right-column", (1160, 12, 112, 522)),   # round + CPU combos + your combos
    ("buttons", (1160, 560, 112, 90)),        # PAUSE / RESTART
]
GROUP_RECT = dict(GROUPS)

PAD = 16              # px of empty space kept around the art, in source px
MIN_INK = 24          # alpha above which a pixel counts as art
MIN_PIECE = 1200      # px of ink below which a block is a speck, not an area
SHEET = 0.45          # a block covering this much of the canvas is a joined sheet, not a panel


# ---------------------------------------------------------------- reading the art
def ink_of(im):
    return np.asarray(im)[..., 3] > MIN_INK


def area_edges():
    """Every x and y where a game area starts or ends, in artwork px."""
    xs, ys = set(), set()
    for (x, y, w, h) in GROUP_RECT.values():
        xs.update(round(v * SCALE) for v in (x, x + w))
        ys.update(round(v * SCALE) for v in (y, y + h))
    return xs, ys


def on_edge(a, b, edges, tol=6):
    return any(a - tol <= e <= b + tol for e in edges)


def runs(flags, run=4):
    """Runs of at least `run` True values -> [(start, stop)]."""
    out, start = [], None
    for i, f in enumerate(flags):
        if f and start is None:
            start = i
        elif not f and start is not None:
            if i - start >= run:
                out.append((start, i))
            start = None
    if start is not None and len(flags) - start >= run:
        out.append((start, len(flags)))
    return out


def split_gaps(sub, xs, ys, frac=0.02, run=4, origin=(0, 0)):
    """Split a block on the empty bands that sit on a game area edge.

    A band counts only when almost none of it has ink (`frac`) and its middle is on an edge from
    `area_edges()`. Where two panels touch there is no band and they stay one piece - cutting
    there would slice the casing the art is made of.
    """
    h, w = sub.shape
    ox, oy = origin
    boxes = [(0, h, 0, w)]
    for a, b in runs(sub.sum(1) <= max(1, int(w * frac)), run):
        if a == 0 or b == h or not on_edge(oy + a, oy + b, ys):
            continue
        nxt = []
        for (y0, y1, x0, x1) in boxes:
            nxt += [(y0, a, x0, x1), (b, y1, x0, x1)] if a > y0 and b < y1 else [(y0, y1, x0, x1)]
        boxes = nxt
    for a, b in runs(sub.sum(0) <= max(1, int(h * frac)), run):
        if a == 0 or b == w or not on_edge(ox + a, ox + b, xs):
            continue
        nxt = []
        for (y0, y1, x0, x1) in boxes:
            nxt += [(y0, y1, x0, a), (y0, y1, b, x1)] if a > x0 and b < x1 else [(y0, y1, x0, x1)]
        boxes = nxt
    return boxes


def group_index(shape):
    """Every pixel -> the nearest game area, as a label image (1..len(GROUPS)).

    Needed only for art that is one connected sheet (realm: its panels are joined by cables). Such
    a sheet cannot be placed at any rect until it is divided, and dividing it by nearest area is
    the cut that keeps each piece holding the art of its own panel.
    """
    h, w = shape
    gy, gx = np.mgrid[0:h, 0:w]
    best = np.full(shape, np.inf)
    lab = np.zeros(shape, np.uint8)
    for i, (_, (x, y, rw, rh)) in enumerate(GROUPS, start=1):
        x0, y0, x1, y1 = x * SCALE, y * SCALE, (x + rw) * SCALE, (y + rh) * SCALE
        dx = np.maximum(np.maximum(x0 - gx, gx - x1), 0)
        dy = np.maximum(np.maximum(y0 - gy, gy - y1), 0)
        d = dx * dx + dy * dy
        take = d < best
        best[take] = d[take]
        lab[take] = i
    return lab


def blocks_of(mask, min_area=MIN_INK):
    """The artwork's own blocks as (box, own-mask), split on the game's area edges."""
    xs, ys = area_edges()
    m = ndi.binary_closing(mask, structure=np.ones((5, 5))) | mask
    lab, _ = ndi.label(m, structure=np.ones((3, 3)))
    out = []
    for i, sl in enumerate(ndi.find_objects(lab), start=1):
        if sl is None:
            continue
        own = np.zeros_like(mask)
        own[sl] = (lab[sl] == i)
        if own.sum() < min_area:
            continue
        y0, x0 = sl[0].start, sl[1].start
        for (sy0, sy1, sx0, sx1) in split_gaps(own[sl], xs, ys, origin=(x0, y0)):
            box = (x0 + sx0, y0 + sy0, x0 + sx1, y0 + sy1)
            piece = np.zeros_like(mask)
            piece[box[1]:box[3], box[0]:box[2]] = own[box[1]:box[3], box[0]:box[2]]
            out.append((box, piece))
    return out


def box_gap(a, b):
    dx = max(0, max(a[0], b[0]) - min(a[2], b[2]))
    dy = max(0, max(a[1], b[1]) - min(a[3], b[3]))
    return dx * dx + dy * dy


def pad_box(box, ink, pad, w, h):
    """Grow a box outward into empty space - the breathing room the art needs.

    Stops at ink, so a piece never swallows a neighbour's ornament and no art is drawn twice.
    """
    x0, y0, x1, y1 = box

    def free(a, b, c, d):
        return not ink[a:b, c:d].any()

    while y0 > 0 and box[1] - y0 < pad and free(y0 - 1, y0, x0, x1):
        y0 -= 1
    while y1 < h and y1 - box[3] < pad and free(y1, y1 + 1, x0, x1):
        y1 += 1
    while x0 > 0 and box[0] - x0 < pad and free(y0, y1, x0 - 1, x0):
        x0 -= 1
    while x1 < w and x1 - box[2] < pad and free(y0, y1, x1, x1 + 1):
        x1 += 1
    return [x0, y0, x1, y1]


def target_rect(names):
    """The canvas rect a piece belongs to: its own areas, joined if it holds more than one."""
    rs = [GROUP_RECT[n] for n in names]
    x0, y0 = min(r[0] for r in rs), min(r[1] for r in rs)
    x1 = max(r[0] + r[2] for r in rs)
    y1 = max(r[1] + r[3] for r in rs)
    return [x0, y0, x1 - x0, y1 - y0]


# ---------------------------------------------------------------- cutting
def pieces(im, pad=PAD):
    """One piece per area. Crops only - the pixels are never touched.

    The cut is lossless by construction: every pixel that carries any alpha ends up inside exactly
    one piece's rectangle, and each piece is written by cropping the source. The last step in
    `run()` proves it by pasting them all back and comparing with the original.
    """
    ink = ink_of(im)                      # what a panel is drawn from: alpha > 24
    every = np.asarray(im)[..., 3] > 0    # everything that carries any art at all
    lab = group_index(ink.shape)
    whole = float(im.width * im.height)

    # 1. the blocks the artwork is drawn as, and any joined sheet divided by nearest area
    blocks = []
    for (x0, y0, x1, y1), own in blocks_of(ink):
        if (x1 - x0) * (y1 - y0) < SHEET * whole:
            blocks.append(([x0, y0, x1, y1], own))
            continue
        sub = lab[y0:y1, x0:x1]
        for i in range(1, len(GROUPS) + 1):
            sel = own[y0:y1, x0:x1] & (sub == i)
            if sel.sum() < MIN_PIECE:
                continue
            ys_i, xs_i = np.where(sel)
            piece = np.zeros_like(ink)
            piece[y0:y1, x0:x1] = sel
            blocks.append(([x0 + int(xs_i.min()), y0 + int(ys_i.min()),
                            x0 + int(xs_i.max()) + 1, y0 + int(ys_i.max()) + 1], piece))

    # 2. join blocks that are the same area, unless the joined box would hold another area's art
    primary = [int(np.argmax(np.bincount(lab[o].ravel(), minlength=len(GROUPS) + 1)[1:])) + 1
               for _, o in blocks]
    merged, used = [], [False] * len(blocks)
    for i in range(len(blocks)):
        if used[i]:
            continue
        box, own = list(blocks[i][0]), blocks[i][1].copy()
        used[i] = True
        changed = True
        while changed:
            changed = False
            for j in range(len(blocks)):
                if used[j] or primary[j] != primary[i]:
                    continue
                u = [min(box[0], blocks[j][0][0]), min(box[1], blocks[j][0][1]),
                     max(box[2], blocks[j][0][2]), max(box[3], blocks[j][0][3])]
                foreign = ink[u[1]:u[3], u[0]:u[2]] & ~(own | blocks[j][1])[u[1]:u[3], u[0]:u[2]]
                if foreign.any():
                    continue
                box, own = u, own | blocks[j][1]
                used[j] = True
                changed = True
        merged.append([box, own])

    # 3. a speck is not an area: it belongs to the piece beside it
    big = [k for k, (_, o) in enumerate(merged) if o.sum() >= MIN_PIECE]
    drop = set()
    for k, (box, own) in enumerate(merged):
        if k in big or not big:
            continue
        j = min(big, key=lambda j: box_gap(merged[j][0], box))
        b, o = merged[j]
        merged[j][0] = [min(b[0], box[0]), min(b[1], box[1]), max(b[2], box[2]), max(b[3], box[3])]
        merged[j][1] = o | own
        drop.add(k)
    merged = [m for k, m in enumerate(merged) if k not in drop]

    # 4. clip a box to the ink it actually holds (a box may enclose a neighbour's art), keeping the
    #    box name of the area its ink belongs to
    clipped = []
    for box, own in merged:
        x0, y0, x1, y1 = box
        while x1 - x0 > 2 and not own[y0:y1, x0].any():
            x0 += 1
        while x1 - x0 > 2 and not own[y0:y1, x1 - 1].any():
            x1 -= 1
        while y1 - y0 > 2 and not own[y0, x0:x1].any():
            y0 += 1
        while y1 - y0 > 2 and not own[y1 - 1, x0:x1].any():
            y1 -= 1
        hist = np.bincount(lab[own].ravel(), minlength=len(GROUPS) + 1)[1:]
        share = hist / max(1, hist.sum())
        names = [GROUPS[k][0] for k in np.argsort(-share) if share[k] >= 0.12]
        if not names:
            names = [GROUPS[int(np.argmax(hist))][0]]
        clipped.append({"box": [x0, y0, x1, y1], "names": names})

    # 5. a box is clipped to alpha > 24, but a panel's soft fringe (alpha 1..24) sits outside that
    #    and would be shaved off. Every such pixel goes to the nearest box, so the pieces together
    #    hold every pixel that carries colour: nothing lost, nothing shaved at a boundary.
    cover = np.zeros_like(ink)
    for p in clipped:
        b = p["box"]
        cover[b[1]:b[3], b[0]:b[2]] = True
    leftover = every & ~cover
    if leftover.any():
        lab_l, _ = ndi.label(leftover, structure=np.ones((3, 3)))
        for i, sl in enumerate(ndi.find_objects(lab_l), start=1):
            if sl is None:
                continue
            ys_i, xs_i = np.where(lab_l[sl] == i)
            cb = [sl[1].start + int(xs_i.min()), sl[0].start + int(ys_i.min()),
                  sl[1].start + int(xs_i.max()) + 1, sl[0].start + int(ys_i.max()) + 1]
            j = min(range(len(clipped)), key=lambda j: box_gap(clipped[j]["box"], cb))
            b = clipped[j]["box"]
            clipped[j]["box"] = [min(b[0], cb[0]), min(b[1], cb[1]),
                                 max(b[2], cb[2]), max(b[3], cb[3])]

    # 6. the breathing room: grow each box into empty space, stopping at any other art
    out = []
    for p in clipped:
        out.append({"box": pad_box(list(p["box"]), every, pad, im.width, im.height),
                    "groups": p["names"]})
    out.sort(key=lambda p: (round(p["box"][1] / 150), p["box"][0]))
    return out


# ---------------------------------------------------------------- previews and the check
def contact_sheet(records, path, cell=(360, 240)):
    cols = 4
    rows = (len(records) + cols - 1) // cols
    cw, ch = cell
    sheet = Image.new("RGB", (cols * cw, rows * ch), (22, 22, 24))
    d = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.truetype(os.path.join(ROOT, "assets", "saira_condensed.ttf"), 15)
    except OSError:
        font = ImageFont.load_default()
    for i, r in enumerate(records):
        im = Image.open(r["_path"]).convert("RGBA")
        k = min((cw - 16) / im.width, (ch - 40) / im.height, 1.0)
        im = im.resize((max(1, int(im.width * k)), max(1, int(im.height * k))), Image.LANCZOS)
        tile = Image.new("RGB", (cw - 16, ch - 40), (255, 0, 255))
        tile.paste(im, ((cw - 16 - im.width) // 2, (ch - 40 - im.height) // 2), im)
        x, y = (i % cols) * cw + 8, (i // cols) * ch + 8
        sheet.paste(tile, (x, y))
        d.rectangle([x, y, x + cw - 17, y + ch - 41], outline=(70, 70, 80))
        d.text((x, y + ch - 30), r["file"], fill=(230, 230, 235), font=font)
        d.text((x, y + ch - 17), f"{r['box'][2]-r['box'][0]}x{r['box'][3]-r['box'][1]}"
                                 f"  -> {r['target_name']}", fill=(140, 200, 160), font=font)
    sheet.save(path)


def compose(records, backdrop, path):
    """Put every piece back at its own position, 1:1.5, on the real backdrop."""
    canvas = Image.new("RGBA", (1280, 720), (8, 8, 10, 255))
    if backdrop and os.path.exists(backdrop):
        canvas.paste(Image.open(backdrop).convert("RGB").resize((1280, 720), Image.LANCZOS), (0, 0))
    for r in records:
        im = Image.open(r["_path"]).convert("RGBA")
        im = im.resize((max(1, round(im.width / SCALE)), max(1, round(im.height / SCALE))),
                       Image.LANCZOS)
        canvas.alpha_composite(im, (round(r["box"][0] / SCALE), round(r["box"][1] / SCALE)))
    canvas.convert("RGB").save(path)


def verify(source, records, cover):
    """Prove the cut is lossless, two ways - neither of them an opinion.

    1. **coverage**: every pixel of the source that carries any alpha lies inside at least one
       piece's box, so no part of the art is left behind or shaved off at a boundary;
    2. **verbatim**: each piece is compared with the same rectangle of the source, pixel for pixel,
       so a piece is a crop and nothing else - no filtering, no re-colouring, no inpainting.
    Returns (coverage px lost, worst channel difference inside a piece).
    """
    every = np.asarray(source)[..., 3] > 0
    lost = int((every & ~cover).sum())
    worst = 0
    for r in records:
        piece = np.asarray(Image.open(r["_path"]).convert("RGBA")).astype(np.int16)
        ref = np.asarray(source.crop(tuple(r["box"]))).astype(np.int16)
        worst = max(worst, int(np.abs(piece - ref).max()))
    return lost, worst


# ---------------------------------------------------------------- per folder
def run(folder, pad=PAD):
    folder = os.path.abspath(folder)
    name = os.path.basename(folder).replace("assetes_", "").replace("assets_", "")
    # cut the text-cleaned artwork where one exists - it is the same artwork, same size
    clean = os.path.join(folder, name + "_clean.png")
    src = clean if os.path.exists(clean) else os.path.join(folder, name + ".png")
    if not os.path.exists(src):
        raise SystemExit(f"no {os.path.join(folder, name + '.png')}")
    im = Image.open(src).convert("RGBA")
    if abs(im.width / im.height - 16 / 9) > 0.01:
        raise SystemExit(f"{src} is not 16:9 - the artwork is the canvas at 1.5x")

    pdir = os.path.join(folder, "pieces")
    os.makedirs(pdir, exist_ok=True)
    for f in os.listdir(pdir):
        if f.endswith(".png"):
            os.remove(os.path.join(pdir, f))

    cut = pieces(im, pad=pad)
    cover = np.zeros(im.size[::-1], bool)
    for piece in cut:
        b = piece["box"]
        cover[b[1]:b[3], b[0]:b[2]] = True

    records, used = [], {}
    for i, piece in enumerate(cut):
        box, names = piece["box"], piece["groups"]
        label = "+".join(names)
        used[label] = used.get(label, 0) + 1
        fname = f"{i + 1:02d}-{label}" + (f"-{used[label]}" if used[label] > 1 else "") + ".png"
        im.crop(tuple(box)).save(os.path.join(pdir, fname))       # a crop, nothing else
        records.append({"_path": os.path.join(pdir, fname), "file": fname, "box": box,
                        "canvas_box": [round(v / SCALE, 1) for v in box],
                        "place": [round(box[0] / SCALE), round(box[1] / SCALE)],
                        "scale": 1 / SCALE, "target_name": label,
                        "target": target_rect(names), "areas": names})

    pv = os.path.join(folder, "preview")
    os.makedirs(pv, exist_ok=True)
    contact_sheet(records, os.path.join(pv, "pieces.png"))
    compose(records, os.path.join(ROOT, "assets", name + ".jpg"), os.path.join(pv, "placed.png"))
    lost, worst = verify(im, records, cover)

    meta = {
        "_about": "Pieces cut from " + os.path.basename(src) + " by tools/table_slice.py. A piece is a plain crop "
                  "of the source - no pixel is changed. box = px in the artwork (1920x1080, 1.5x "
                  "the 1280x720 canvas); place = where it goes on the canvas; target = the game "
                  "rect it belongs to, from tables/anchors.json.",
        "source": os.path.basename(src),
        "scale": SCALE,
        "pieces": [{k: v for k, v in r.items() if not k.startswith("_")} for r in records],
        "lossless": {"px_outside_pieces": lost, "piece_vs_source_max_diff": worst},
    }
    json.dump(meta, open(os.path.join(folder, "pieces.json"), "w"), indent=1)
    return records, lost, worst


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    args = sys.argv[1:]
    pad = int(args[args.index("--pad") + 1]) if "--pad" in args else PAD
    folders = arguments = [a for a in args if not a.startswith("--") and not a.isdigit()]
    if "--all" in args:
        folders = sorted(os.path.join(ROOT, "tables", d) for d in os.listdir(os.path.join(ROOT, "tables"))
                         if d.startswith(("assets_", "assetes_")))
    total, bad = 0, 0
    for folder in folders:
        recs, lost, worst = run(folder, pad)
        total += len(recs)
        ok = lost == 0 and worst == 0
        bad += not ok
        print(f"{os.path.relpath(folder, ROOT)}: {len(recs)} piece(s), pad {pad}px, "
              f"lossless {'yes' if ok else f'NO ({lost} px outside pieces, max diff {worst})'}")
        for r in recs:
            print(f"   {r['file']:40s} {r['box'][2]-r['box'][0]:4d}x{r['box'][3]-r['box'][1]:<4d}"
                  f" canvas {r['canvas_box']}  -> {r['target_name']}")
    print(f"{total} piece(s), {bad} folder(s) not lossless")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
