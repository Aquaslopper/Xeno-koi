#!/usr/bin/env python3
"""Clean a table artwork's baked-in text and cut it into one piece per area.

The `tables/assets_*/` and `tables/assetes_castle/` folders hold the full-table artwork at 1.5x the
1280x720 canvas. Two things are wrong with a raw render, and this tool fixes exactly those:

  1. **Text.** The render has the game's own labels baked in ("CPU SCORE: 0", "PAUSE", the status
     line, ...). The game draws those itself, so a baked copy lands underneath it and doubles.
     `cleantext()` removes glyphs and leaves the metal untouched.
  2. **One image is not placeable.** A panel has to be cut out to be put down at its own rect, and the
     cut has to keep breathing room around the art - cropping to the tight bbox shaves the outer
     ornament and the piece then reads as chopped when it is put down. `pieces()` cuts on the game's
     own area edges and pads each piece outward while the strip around it is still empty, so no
     piece takes a pixel that belongs to its neighbour.

    python3 tools/table_slice.py tables/assets_city            # clean + cut, report
    python3 tools/table_slice.py tables/assets_city --pad 20   # wider breathing room
    python3 tools/table_slice.py --all                         # every asset folder

Writes, inside the folder it was given - nothing anywhere else:

    <name>_clean.png        the whole artwork, text gone, same size as the source
    pieces/NN-<area>.png    one piece per area, padded, transparent
    pieces.json             every piece's box in the source, its canvas box, the game rect it belongs to
    preview/pieces.png      contact sheet of the pieces (the picture to look at)
    preview/placed.png      the pieces put back at their own positions on the real backdrop

A piece's box is never tighter than its art and never wider than the empty gap to the next piece.
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont
from scipy import ndimage as ndi

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

# The artwork is rendered at 1.5x the 1280x720 canvas (1920x1080).
SCALE = 1.5

# The areas a table is made of, in canvas px, taken from tables/anchors.json - never from a picture.
# A piece may cover more than one of these (city's left block is score + four lanes in one casing);
# what a piece belongs to is decided by how much of each area its box covers, below.
AREAS = {
    "cpu-block": (12, 12, 255, 196),          # score strip + four CPU lanes
    "cpu-score": (12, 12, 255, 30),           # only used when the artwork splits them
    "cpu-lanes": (12, 46, 255, 162),
    "player-block": (12, 508, 255, 200),      # four player lanes + you score strip
    "field": (430, 132, 650, 400),            # no art in the renders: cards sit on the backdrop
    "stock": (330, 262, 76, 148),
    "right-column": (1160, 12, 112, 522),     # round + ruleset, CPU combos, your combos
    "round-panel": (1160, 12, 112, 66),
    "cpu-combos": (1160, 88, 112, 218),
    "your-combos": (1160, 316, 112, 218),
    "buttons": (1160, 560, 112, 90),          # PAUSE / RESTART
    "status": (480, 518, 460, 34),            # the turn line's banner
    "cpu-hand": (465, 12, 490, 91),
    "player-hand": (330, 566, 760, 143),
}

# Text removal. Every number here was chosen against the six renders in the repo.
BG_R, BRIGHT, OVER_BG = 21, 52, 15      # background median radius, minimum glyph brightness, contrast
MAX_AREA, MAX_SPAN = 2600, 74           # a glyph is small; a rivet cluster or a cable is not
DARK_BG = 105                           # only erase where the thing behind it is dark metal
FILL_RADIUS = 10                        # px: the median window an emptied glyph is filled from
ERASE_SLACK = 3                         # px: a glyph's anti-aliased rim and its dark outline


def lum_of(rgb):
    return rgb[..., :3].astype(np.float32).mean(axis=2)


def fill_from_around(im, erase):
    """Fill the erased pixels with the metal around them, and nothing else.

    A blur pulls the glyph back into its own hole; a median does not, because the ink is a small
    minority of any window wider than a stroke. The window is 21 px - wider than a glyph is thick,
    narrow enough that the frame next to it is still outvoted.
    """
    a = np.asarray(im).astype(np.float32)
    rgb, alpha = a[..., :3], a[..., 3]
    k = 2 * FILL_RADIUS + 1
    fill_rgb = np.dstack([
        np.asarray(Image.fromarray(np.clip(rgb[..., c], 0, 255).astype(np.uint8))
                   .filter(ImageFilter.MedianFilter(k)), dtype=np.float32) for c in range(3)])
    fill_a = np.asarray(Image.fromarray(np.clip(alpha, 0, 255).astype(np.uint8))
                        .filter(ImageFilter.MedianFilter(k)), dtype=np.float32)
    out = np.where(erase[..., None], fill_rgb, rgb)
    al = np.where(erase, fill_a, alpha)
    return Image.fromarray(np.dstack([np.clip(out, 0, 255), np.clip(al, 0, 255)]).astype(np.uint8), "RGBA")


def cleantext(im, verbose=False):
    """Erase baked-in glyphs, keep every pixel of metal.

    A glyph is a small, bright mark on dark metal: brighter than a median of its own 21 px window -
    wide enough to ignore a 4 px stroke - and only a few tens of pixels big. Anything larger (a
    frame, a cable, a rivet cluster) fails the size test and is kept, which is how the ornaments
    survive a pass meant to delete text.
    """
    a = np.asarray(im).astype(np.float32)
    rgb, alpha = a[..., :3], a[..., 3]
    lum = lum_of(rgb)
    bg = np.asarray(Image.fromarray(np.clip(lum, 0, 255).astype(np.uint8))
                   .filter(ImageFilter.MedianFilter(BG_R)), dtype=np.float32)

    cand = (lum > bg + OVER_BG) & (lum > BRIGHT) & (alpha > 40) & (bg < DARK_BG)
    lab, n = ndi.label(cand, structure=np.ones((3, 3)))
    if n == 0:
        return im, 0
    idx = np.arange(1, n + 1)
    area = ndi.sum(cand, lab, idx)
    keep = np.zeros(n + 1, bool)
    for i, sl in enumerate(ndi.find_objects(lab)):
        if sl is None:
            continue
        h = sl[0].stop - sl[0].start
        w = sl[1].stop - sl[1].start
        if area[i] <= MAX_AREA and h <= MAX_SPAN and w <= MAX_SPAN:
            keep[i + 1] = True
    erase = keep[lab]
    if verbose:
        print(f"    text: {int(keep.sum())} marks, {int(erase.sum())} px")
    if not erase.any():
        return im, 0

    # A glyph's own holes (the counter of an O, a gap in a stroke) go with it, and so does the rim:
    # the anti-aliased edge and the dark outline sit up to three pixels outside the bright core.
    erase = ndi.binary_fill_holes(erase)
    r = ERASE_SLACK
    yy, xx = np.mgrid[-r:r + 1, -r:r + 1]
    erase = ndi.binary_dilation(erase, (yy * yy + xx * xx) <= r * r)
    out = fill_from_around(im, erase)
    return out, int(erase.sum())


# ---------------------------------------------------------------- pieces
def area_edges():
    """Every x and y where a game area starts or ends, in artwork px.

    These are the only places a cut is allowed to run: the game draws a panel at each of these
    rects, so a seam there can be put back together exactly. The gap between two cards in a row is
    not an edge - cutting there would leave a piece with no rect to belong to.
    """
    xs, ys = set(), set()
    for (x, y, w, h) in AREAS.values():
        for v in (x, x + w):
            xs.add(round(v * SCALE))
        for v in (y, y + h):
            ys.add(round(v * SCALE))
    return xs, ys


def on_edge(a, b, edges, tol=6):
    return any(a - tol <= e <= b + tol for e in edges)


def runs(flags, run=4):
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
    """Split a block on the empty bands that lie on a game area edge.

    A band counts as a gap when at most `frac` of its length has any ink, for at least `run` rows
    (or columns), and its middle sits on an edge from `area_edges()`. Where two panels touch there
    is no gap and they stay one piece - cutting there would slice the casing the art is made of.
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


MIN_PIECE = 1200                       # px of own ink below which a cut piece is a stray, not an area

GROUPS = [
    ("cpu-block", (12, 12, 255, 196)),
    ("cpu-hand", (465, 12, 490, 91)),
    ("stock", (330, 262, 76, 148)),
    ("player-block", (12, 508, 255, 200)),
    ("player-hand", (330, 566, 760, 143)),
    ("status", (480, 518, 460, 34)),
    ("right-column", (1160, 12, 112, 522)),
    ("buttons", (1160, 560, 112, 90)),
]


def group_index(shape):
    """Every pixel -> the nearest game area group, as a label image (1..len(GROUPS)).

    Used for art that is one connected sheet (realm's panels are joined by cables): such a sheet
    cannot be placed at any rect until it is divided, and dividing by nearest area is the cut that
    loses the least - each piece still holds exactly the art that belongs to its own panel.
    """
    h, w = shape
    gy, gx = np.mgrid[0:h, 0:w]
    best = np.full(shape, np.inf, dtype=np.float64)
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


def blocks_of(mask, min_area=60):
    """The artwork's own blocks, each with the pixels it actually owns.

    Returns (box, own) pairs, where `own` is the full-size mask of that one block - needed because
    a block's rectangle can enclose a neighbour (realm's sheet spans the whole canvas).
    """
    xs, ys = area_edges()
    m = ndi.binary_closing(mask, structure=np.ones((5, 5))) | mask
    lab, n = ndi.label(m, structure=np.ones((3, 3)))
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


def pieces(im, pad=16, min_area=60, verbose=False):
    """Cut the artwork into one piece per game area.

    1. the artwork's own blocks, split only on the game's area edges;
    2. a block still covering half the canvas (realm: its panels are joined by cables) is divided
       between the areas by nearest-area, because no gap exists to cut on and a sheet that covers
       everything cannot be placed at any rect;
    3. blocks with the same area are joined back together - a row of cards drawn as separate
       objects is still one row, and the empty gaps between them are not an area edge;
    4. a speck too small to be an area is given to the nearest piece, so no art is dropped;
    5. every piece is clipped to its own ink and padded into empty space.
    """
    mask = np.asarray(im.split()[3]) > 24
    lab = group_index(mask.shape)
    whole = float(im.width * im.height)

    # -- 1 and 2: blocks, big sheets divided by area
    blocks = []
    for (x0, y0, x1, y1), own in blocks_of(mask, min_area):
        if (x1 - x0) * (y1 - y0) < 0.45 * whole:
            blocks.append(([x0, y0, x1, y1], own))
            continue
        sliced = lab[y0:y1, x0:x1]
        for i in range(1, len(GROUPS) + 1):
            sel = own[y0:y1, x0:x1] & (sliced == i)
            if sel.sum() < min_area:
                continue
            ys_i, xs_i = np.where(sel)
            box = [x0 + int(xs_i.min()), y0 + int(ys_i.min()),
                   x0 + int(xs_i.max()) + 1, y0 + int(ys_i.max()) + 1]
            piece = np.zeros_like(mask)
            piece[y0:y1, x0:x1] = sel
            blocks.append((box, piece))

    # -- 3: join blocks that belong to the same area
    primary = []
    for box, own in blocks:
        hist = np.bincount(lab[own].ravel(), minlength=len(GROUPS) + 1)[1:]
        primary.append(int(np.argmax(hist)) + 1)
    merged, used = [], [False] * len(blocks)
    for i in range(len(blocks)):
        if used[i]:
            continue
        box, ownmask = list(blocks[i][0]), blocks[i][1].copy()
        used[i] = True
        changed = True
        while changed:
            changed = False
            for j in range(len(blocks)):
                if used[j] or primary[j] != primary[i]:
                    continue
                u = [min(box[0], blocks[j][0][0]), min(box[1], blocks[j][0][1]),
                     max(box[2], blocks[j][0][2]), max(box[3], blocks[j][0][3])]
                # joining is only allowed when the joined box holds no other area's ink
                foreign = mask[u[1]:u[3], u[0]:u[2]] & ~(ownmask | blocks[j][1])[u[1]:u[3], u[0]:u[2]]
                if foreign.any():
                    continue
                box, ownmask = u, ownmask | blocks[j][1]
                used[j] = True
                changed = True
        merged.append([box, ownmask])

    # -- 4: a speck is not an area - give it to the nearest piece that is
    big = [k for k, (b, o) in enumerate(merged) if o.sum() >= MIN_PIECE]
    drop = set()
    for k, (box, ownmask) in enumerate(merged):
        if k in big or not big:
            continue
        j = min(big, key=lambda j: _box_gap(merged[j][0], box))
        b, o = merged[j]
        merged[j][0] = [min(b[0], box[0]), min(b[1], box[1]), max(b[2], box[2]), max(b[3], box[3])]
        merged[j][1] = o | ownmask
        drop.add(k)
    merged = [m for k, m in enumerate(merged) if k not in drop]

    # -- 3b: no ink may be left over. Anything the split could not keep (a 2 px strip on the canvas
    # edge) joins the piece that owns its game area, so every pixel lands in exactly one piece.
    owned = np.zeros_like(mask)
    for _, o in merged:
        owned |= o
    leftover = mask & ~owned
    if leftover.any():
        per_group = {}
        for j, (_, o) in enumerate(merged):
            hist = np.bincount(lab[o].ravel(), minlength=len(GROUPS) + 1)[1:]
            per_group.setdefault(int(np.argmax(hist)) + 1, j)
        lab_l, _ = ndi.label(leftover, structure=np.ones((3, 3)))
        for i, sl in enumerate(ndi.find_objects(lab_l), start=1):
            if sl is None:
                continue
            comp = np.zeros_like(mask)
            comp[sl] = (lab_l[sl] == i)
            own_g = int(np.argmax(np.bincount(lab[comp].ravel(), minlength=len(GROUPS) + 1)[1:])) + 1
            j = per_group.get(own_g)
            if j is None:
                ys_i, xs_i = np.where(comp)
                cb = [int(xs_i.min()), int(ys_i.min()), int(xs_i.max()) + 1, int(ys_i.max()) + 1]
                j = _nearest_piece(merged, cb)
            b, o = merged[j]
            merged[j][1] = o | comp

    # -- 5: clip to own ink (a box may enclose a neighbour's art), then pad into empty space
    clipped = []
    for box, ownmask in merged:
        x0, y0, x1, y1 = box
        while x1 - x0 > 2 and not ownmask[y0:y1, x0].any():
            x0 += 1
        while x1 - x0 > 2 and not ownmask[y0:y1, x1 - 1].any():
            x1 -= 1
        while y1 - y0 > 2 and not ownmask[y0, x0:x1].any():
            y0 += 1
        while y1 - y0 > 2 and not ownmask[y1 - 1, x0:x1].any():
            y1 -= 1
        clipped.append(([x0, y0, x1, y1], ownmask))

    taken = mask.copy()
    out = []
    for box, ownmask in sorted(clipped, key=lambda b: (round(b[0][1] / 150), b[0][0])):
        box = pad_box(list(box), taken, pad, im.width, im.height)
        taken[box[1]:box[3], box[0]:box[2]] = True
        hist = np.bincount(lab[ownmask].ravel(), minlength=len(GROUPS) + 1)[1:]
        share = hist / max(1, hist.sum())
        names = [GROUPS[k][0] for k in np.argsort(-share) if share[k] >= 0.12]
        if not names:
            names = [GROUPS[int(np.argmax(hist))][0]]
        out.append({"box": box, "own": ownmask, "groups": names})
    return out


def _nearest_piece(merged, box):
    """Which piece a leftover belongs to: the nearest one, and on a tie the one it runs alongside.

    A 2 px strip on the canvas edge can be the same distance from two panels; taking the first
    would drag a whole column out of shape, so the panel it actually continues wins.
    """
    def overlap(a, b):
        ox = min(a[2], b[2]) - max(a[0], b[0])
        oy = min(a[3], b[3]) - max(a[1], b[1])
        return max(0, ox) + max(0, oy)

    return min(range(len(merged)),
               key=lambda j: (_box_gap(merged[j][0], box), -overlap(merged[j][0], box)))


def _box_gap(a, b):
    """Distance between two boxes (0 if they touch) - used to attach a stray to its neighbour."""
    dx = max(0, max(a[0], b[0]) - min(a[2], b[2]))
    dy = max(0, max(a[1], b[1]) - min(a[3], b[3]))
    return dx * dx + dy * dy


def pad_box(box, taken, pad, canvas_w, canvas_h):
    """Grow a box outward while it is still empty space - the breathing room the art needs.

    `taken` is every ink pixel plus every box already claimed: a piece never grows into a
    neighbour, so nothing is drawn twice and no panel loses a pixel to the piece next door.
    """
    x0, y0, x1, y1 = box

    def free(a, b, c, d):
        return not taken[a:b, c:d].any()

    while y0 > 0 and box[1] - y0 < pad and free(y0 - 1, y0, x0, x1):
        y0 -= 1
    while y1 < canvas_h and y1 - box[3] < pad and free(y1, y1 + 1, x0, x1):
        y1 += 1
    while x0 > 0 and box[0] - x0 < pad and free(y0, y1, x0 - 1, x0):
        x0 -= 1
    while x1 < canvas_w and x1 - box[2] < pad and free(y0, y1, x1, x1 + 1):
        x1 += 1
    return [x0, y0, x1, y1]


def target_rect(names):
    """The canvas rect a piece is placed on: its own areas, joined if it holds more than one."""
    rs = [dict(GROUPS)[n] for n in names]
    x0 = min(r[0] for r in rs)
    y0 = min(r[1] for r in rs)
    x1 = max(r[0] + r[2] for r in rs)
    y1 = max(r[1] + r[3] for r in rs)
    return [x0, y0, x1 - x0, y1 - y0]


# ---------------------------------------------------------------- reporting
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
    """Put every piece back where it came from, at 1/1.5 scale, on the real backdrop.

    This is the check that a cut is usable: if the pieces do not rebuild the artwork, they cannot
    be placed on the table either. Placement is exact - the piece's own box, scaled once - not a
    stretch to fit a rect, because stretching is what makes a panel read wrong.
    """
    canvas = Image.new("RGBA", (1280, 720), (8, 8, 10, 255))
    if backdrop and os.path.exists(backdrop):
        bd = Image.open(backdrop).convert("RGB").resize((1280, 720), Image.LANCZOS)
        canvas.paste(bd, (0, 0))
    for r in records:
        im = Image.open(r["_path"]).convert("RGBA")
        im = im.resize((max(1, round(im.width / SCALE)), max(1, round(im.height / SCALE))),
                       Image.LANCZOS)
        canvas.alpha_composite(im, (round(r["box"][0] / SCALE), round(r["box"][1] / SCALE)))
    canvas.convert("RGB").save(path)
    return canvas


def verify(clean, records):
    """Rebuild the artwork from its pieces, at source size, and compare with the cleaned original.

    Done at 1:1 rather than on the canvas so the result is about the cut and not about the
    resampler: a piece that drops a pixel, takes a neighbour's, or is cropped appears here as a
    non-zero difference. Returns (worst channel difference, pixels off by more than 2).
    """
    canvas = Image.new("RGBA", clean.size, (0, 0, 0, 0))
    for r in records:
        canvas.alpha_composite(Image.open(r["_path"]).convert("RGBA"), (r["box"][0], r["box"][1]))
    a = np.asarray(canvas).astype(np.int16)
    b = np.asarray(clean).astype(np.int16)
    diff = np.abs(a - b).max(axis=2)
    return int(diff.max()), int((diff > 2).sum())


# ---------------------------------------------------------------- per folder
def run(folder, pad=16):
    folder = os.path.abspath(folder)
    name = os.path.basename(folder).replace("assetes_", "").replace("assets_", "")
    src = os.path.join(folder, name + ".png")
    if not os.path.exists(src):
        raise SystemExit(f"no {src}")
    im = Image.open(src).convert("RGBA")
    if abs(im.width / im.height - 16 / 9) > 0.01:
        raise SystemExit(f"{src} is not 16:9 - the artwork is the canvas at 1.5x")

    clean, n_erased = cleantext(im, verbose=True)
    clean_path = os.path.join(folder, name + "_clean.png")
    clean.save(clean_path)

    pdir = os.path.join(folder, "pieces")
    os.makedirs(pdir, exist_ok=True)
    for f in os.listdir(pdir):
        if f.endswith(".png"):
            os.remove(os.path.join(pdir, f))

    found = pieces(clean, pad=pad)
    ink = np.asarray(clean.split()[3]) > 24
    owned = np.zeros_like(ink)
    for piece in found:
        owned |= piece["own"]
    lost = int((ink & ~owned).sum())
    records, used = [], {}
    for i, rec in enumerate(found):
        box, names = rec["box"], rec["groups"]
        label = "+".join(names)
        used[label] = used.get(label, 0) + 1
        fname = f"{i + 1:02d}-{label}" + (f"-{used[label]}" if used[label] > 1 else "") + ".png"
        img = clean.crop(tuple(box))
        keep = rec["own"][box[1]:box[3], box[0]:box[2]] | ~ink[box[1]:box[3], box[0]:box[2]]
        if not keep.all():                       # a neighbour's art inside this box: not ours to draw
            arr = np.asarray(img).copy()
            arr[..., 3] = np.where(keep, arr[..., 3], 0)
            img = Image.fromarray(arr, "RGBA")
        img.save(os.path.join(pdir, fname))
        records.append({"_path": os.path.join(pdir, fname), "file": fname, "box": box,
                        "canvas_box": [round(v / SCALE, 1) for v in box],
                        "place": [round(box[0] / SCALE), round(box[1] / SCALE)], "scale": 1 / SCALE,
                        "target_name": label, "target": target_rect(names), "areas": names})

    pv = os.path.join(folder, "preview")
    os.makedirs(pv, exist_ok=True)
    contact_sheet(records, os.path.join(pv, "pieces.png"))
    bd = os.path.join(ROOT, "assets", name + ".jpg")
    compose(records, bd, os.path.join(pv, "placed.png"))
    max_diff, off = verify(clean, records)

    meta = {
        "_about": "Pieces cut from " + name + ".png by tools/table_slice.py. box = px in the source "
                  "artwork (1920x1080, 1.5x the 1280x720 canvas); canvas_box = the same in canvas px; "
                  "target = the game rect the piece belongs to, from tables/anchors.json.",
        "source": name + ".png",
        "clean": name + "_clean.png",
        "scale": SCALE,
        "pieces": [{k: v for k, v in r.items() if not k.startswith("_")} for r in records],
    }
    meta["coverage"] = {"ink_px": int(ink.sum()), "owned_px": int(owned.sum()), "lost_px": lost,
                        "recompose_max_diff": max_diff, "recompose_px_off": off}
    json.dump(meta, open(os.path.join(folder, "pieces.json"), "w"), indent=1)
    if lost:
        print(f"    WARNING: {lost} ink px belong to no piece", file=sys.stderr)
    for r in records:
        cw, ch = r["box"][2] - r["box"][0], r["box"][3] - r["box"][1]
        tw, th = r["target"][2] * SCALE, r["target"][3] * SCALE
        if cw > tw * 1.35 or ch > th * 1.35:
            print(f"    note: {r['file']} is {cw}x{ch} for a {int(tw)}x{int(th)} area",
                  file=sys.stderr)
    return records, clean_path


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    args = sys.argv[1:]
    pad = int(args[args.index("--pad") + 1]) if "--pad" in args else 16
    folders = (sorted(os.path.join(ROOT, "tables", d) for d in os.listdir(os.path.join(ROOT, "tables"))
                      if d.startswith(("assets_", "assetes_")))) if "--all" in args else \
        [a for a in args if not a.startswith("--") and not a.isdigit()]
    total = 0
    for folder in folders:
        recs, _ = run(folder, pad)
        total += len(recs)
        print(f"{os.path.relpath(folder, ROOT)}: {len(recs)} piece(s), pad {pad}px")
        for r in recs:
            print(f"   {r['file']:34s} {r['box'][2]-r['box'][0]:4d}x{r['box'][3]-r['box'][1]:<4d}"
                  f" canvas {r['canvas_box']}  -> {r['target_name']}")
    print(f"{total} piece(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
