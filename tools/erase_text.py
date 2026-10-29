#!/usr/bin/env python3
"""Erase the mockup's game-drawn text from a table backdrop.

Per text run we mask only the glyph pixels, inpaint them (Telea) and put
panel grain back on top, sampled from a measured clean patch of the same
material, so the fill keeps the grain instead of turning into a flat
patch.  Frames, bubbles, medallions and ornaments are left untouched.

Glyphs are found two ways, depending on how the skin paints them:
  cyan   - the label colour (b and g well above r)            -> exact
  bright - white labels on a dark face, inside a tight window -> exact

Usage:  python3 tools/erase_text.py [table]        (default: hole)
        reads  tables/assets_<table>/<table>-original.png
        writes tables/assets_<table>/<table>.png
"""
import os
import sys

import cv2
import numpy as np
from PIL import Image
from PIL import ImageColor

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# region: (name, window, mode, param, pool)
#   mode 'cyan'  -> param = (min b-r, min g-r, min luma)
#   mode 'bright'-> param = luma threshold
# pool: name of a measured clean patch of the same material

CYAN = (25, 25, 45)

HOLE_POOLS = {
    'left':   (86, 88, 292, 112),
    'right':  (1726, 62, 1855, 78),
    'status': (700, 762, 1400, 773),
    'deck':   (440, 512, 492, 586),
}

HOLE_REGIONS = [
    ('cpu_score',   (26, 28, 205, 66),      'bright', 52, 'left'),
    ('you_score',   (24, 1008, 300, 1042),  'bright', 52, 'left'),
    ('lane1',       (26, 84, 74, 114),      'bright', 50, 'left'),
    ('lane2',       (26, 146, 74, 178),     'bright', 50, 'left'),
    ('lane3',       (26, 208, 74, 240),     'bright', 50, 'left'),
    ('lane4',       (26, 270, 74, 302),     'bright', 50, 'left'),
    ('lane5',       (26, 742, 74, 772),     'bright', 50, 'left'),
    ('lane6',       (26, 806, 74, 838),     'bright', 50, 'left'),
    ('lane7',       (26, 870, 74, 902),     'bright', 50, 'left'),
    ('lane8',       (26, 936, 74, 968),     'bright', 50, 'left'),
    ('deck_vert',   (496, 506, 532, 592),   'bright', 55, 'deck'),
    ('deck_count',  (470, 598, 514, 658),   'bright', 55, 'deck'),
    ('status',      (690, 767, 1420, 794),  'bright', 58, 'status'),
    ('rnd',         (1724, 31, 1878, 68),   'bright', 55, 'right'),
    ('kk',          (1724, 72, 1878, 106),  'bright', 55, 'right'),
    ('cpu_combos',  (1724, 130, 1878, 300), 'bright', 55, 'right'),
    ('cpu_click',   (1728, 410, 1878, 458), 'bright', 55, 'right'),
    ('your_combos', (1724, 460, 1878, 652), 'bright', 55, 'right'),
    ('you_click',   (1728, 754, 1878, 804), 'bright', 55, 'right'),
    ('pause',       (1760, 856, 1878, 896), 'bright', 55, 'right'),
    ('restart',     (1748, 940, 1878, 982), 'bright', 55, 'right'),
]

ARCTIC_POOLS = {
    'left':   (120, 86, 400, 112),
    'card':   (700, 30, 760, 88),
    'right':  (1740, 290, 1860, 380),
    'deck':   (490, 400, 560, 560),
    'status': (620, 796, 780, 836),
}

ARCTIC_REGIONS = [
    ('cpu_score',   (24, 18, 230, 64),      'cyan', CYAN, 'left'),
    ('you_score',   (40, 1006, 350, 1056),  'cyan', CYAN, 'left'),
    ('lane1',       (30, 80, 105, 124),     'bright', 120, 'left'),
    ('lane2',       (30, 143, 105, 187),    'bright', 120, 'left'),
    ('lane3',       (30, 206, 105, 250),    'bright', 120, 'left'),
    ('lane4',       (30, 262, 105, 316),    'bright', 120, 'left'),
    ('lane5',       (30, 752, 105, 796),    'bright', 120, 'left'),
    ('lane6',       (30, 815, 105, 859),    'bright', 120, 'left'),
    ('lane7',       (30, 880, 105, 924),    'bright', 120, 'left'),
    ('lane8',       (30, 946, 105, 990),    'bright', 120, 'left'),
    ('card_labels', (700, 80, 1440, 175),   'cyan', CYAN, 'card'),
    ('deck_vert',   (560, 496, 615, 576),   'cyan', CYAN, 'deck'),
    ('deck_count',  (520, 596, 590, 630),   'cyan', CYAN, 'deck'),
    ('status',      (780, 798, 1340, 838),  'cyan', CYAN, 'status'),
    ('rnd',         (1734, 26, 1840, 58),   'cyan', CYAN, 'right'),
    ('kk',          (1734, 68, 1866, 96),   'bright', 120, 'right'),
    ('cpu_combos',  (1734, 142, 1868, 168), 'cyan', CYAN, 'right'),
    ('cpu_base',    (1734, 178, 1818, 202), 'bright', 120, 'right'),
    ('cpu_koi',     (1734, 206, 1814, 232), 'bright', 120, 'right'),
    ('cpu_nocomb',  (1734, 250, 1868, 276), 'bright', 120, 'right'),
    ('cpu_click',   (1734, 400, 1872, 426), 'cyan', CYAN, 'right'),
    ('your_combos', (1734, 488, 1872, 514), 'cyan', CYAN, 'right'),
    ('your_base',   (1734, 522, 1818, 548), 'bright', 120, 'right'),
    ('your_koi',    (1734, 552, 1814, 578), 'bright', 120, 'right'),
    ('your_nocomb', (1734, 596, 1868, 622), 'bright', 120, 'right'),
    ('you_click',   (1734, 768, 1872, 796), 'cyan', CYAN, 'right'),
    ('pause',       (1760, 862, 1870, 892), 'cyan', CYAN, 'right'),
    ('restart',     (1748, 946, 1888, 976), 'cyan', CYAN, 'right'),
]

CASTLE_POOLS = {
    'left':   (140, 90, 400, 114),      # clean face inside the B row
    'right':  (1740, 300, 1850, 380),   # empty middle of the combos panel
    'deck':   (500, 400, 570, 560),     # deck card face, left of DECK
}

# warm gold capitals on near-black leather: a plain luma threshold separates
# them cleanly (faces stay under ~50)
CASTLE_REGIONS = [
    ('cpu_score',    (40, 24, 400, 70),      'bright', 95, 'left'),
    ('you_score',    (40, 995, 380, 1050),   'bright', 95, 'left'),
    ('lane1',        (38, 90, 120, 130),     'bright', 95, 'left'),
    ('lane2',        (38, 160, 120, 200),    'bright', 95, 'left'),
    ('lane3',        (38, 232, 120, 272),    'bright', 95, 'left'),
    ('lane4',        (38, 305, 120, 345),    'bright', 95, 'left'),
    ('lane5',        (38, 730, 120, 770),    'bright', 95, 'left'),
    ('lane6',        (38, 800, 120, 840),    'bright', 95, 'left'),
    ('lane7',        (38, 868, 120, 908),    'bright', 95, 'left'),
    ('lane8',        (38, 936, 120, 976),    'bright', 95, 'left'),
    ('deck_vert',    (598, 460, 630, 582),   'bright', 95, 'deck'),
    ('deck_count',   (525, 605, 600, 646),   'bright', 95, 'deck'),
    ('status',       (735, 755, 1430, 782),  'bright', 95, 'left'),
    ('rnd',          (1740, 24, 1870, 66),   'bright', 95, 'right'),
    ('kk',           (1738, 72, 1876, 110),  'bright', 95, 'right'),
    ('cpu_combos',   (1732, 148, 1876, 176), 'bright', 95, 'right'),
    ('cpu_base',     (1730, 192, 1820, 220), 'bright', 95, 'right'),
    ('cpu_koi',      (1730, 224, 1815, 252), 'bright', 95, 'right'),
    ('cpu_nocomb',   (1730, 276, 1870, 306), 'bright', 95, 'right'),
    ('cpu_click',    (1735, 415, 1895, 448), 'bright', 95, 'right'),
    ('your_combos',  (1730, 486, 1885, 518), 'bright', 95, 'right'),
    ('your_base',    (1730, 526, 1820, 556), 'bright', 95, 'right'),
    ('your_koi',     (1730, 560, 1815, 588), 'bright', 95, 'right'),
    ('your_nocomb',  (1730, 610, 1872, 640), 'bright', 95, 'right'),
    ('you_click',    (1730, 750, 1885, 784), 'bright', 95, 'right'),
    ('pause',        (1745, 852, 1885, 892), 'bright', 95, 'right'),
    ('restart',      (1728, 944, 1880, 978), 'bright', 95, 'right'),
]

TABLES = {
    'hole':    dict(pools=HOLE_POOLS,   regions=HOLE_REGIONS,   dilate=3, ring=8,
                    method='inpaint'),
    'arctic':  dict(pools=ARCTIC_POOLS, regions=ARCTIC_REGIONS, dilate=2, ring=6,
                    method='inpaint'),
    'castle':  dict(pools=CASTLE_POOLS, regions=CASTLE_REGIONS, dilate=3, ring=8,
                    method='inpaint'),
}

AREA_MIN, AREA_MAX = 6, 2500
H_MIN, H_MAX = 2, 48
GRAIN_MAX = 30.0
SEED = 20261029


def hp(a, r=1):
    return a - cv2.blur(a, (2 * r + 1, 2 * r + 1), borderType=cv2.BORDER_REFLECT)


def glyph_mask(sub_rgb, sub_lum, mode, param):
    hh, ww = sub_lum.shape
    if mode == 'cyan':
        r, g, b = sub_rgb[..., 0], sub_rgb[..., 1], sub_rgb[..., 2]
        m = (((b - r) > param[0]) & ((g - r) > param[1]) & (sub_lum > param[2])).astype(np.uint8)
    else:
        m = (sub_lum > param).astype(np.uint8)
    # hysteresis: grow the seed pixels a few px through anything clearly
    # brighter than the plate face, so anti-aliased glyph edges go too
    grow_lo = param[2] if mode == 'cyan' else max(38, param * 0.45)
    for _ in range(4):
        nxt = cv2.dilate(m, np.ones((3, 3), np.uint8))
        m = np.maximum(m, (nxt & (sub_lum > grow_lo)).astype(np.uint8))
    n, lab, st, _ = cv2.connectedComponentsWithStats(m, 8)
    keep = np.zeros_like(m)
    for i in range(1, n):
        x, y, w, h, ar = st[i]
        edge = x <= 0 or y <= 0 or x + w >= ww or y + h >= hh
        if AREA_MIN <= ar <= AREA_MAX and H_MIN <= h <= H_MAX and not edge:
            keep |= (lab == i).astype(np.uint8)
    return keep


def row_fill(sub, mask):
    """Continue each masked run straight across, from the real pixels either
    side of it.  The metal's texture runs along the rows, so every row keeps
    its own tone and the striations stay continuous.
    """
    out = sub.copy()
    h, w = mask.shape[:2]
    for y in range(h):
        row = mask[y]
        if not row.any():
            continue
        xs = np.where(row)[0]
        breaks = np.where(np.diff(xs) > 1)[0]
        starts = np.concatenate(([0], breaks + 1))
        ends = np.concatenate((breaks, [xs.size - 1]))
        for a, b in zip(xs[starts], xs[ends]):
            n = b - a + 1
            # step a few pixels clear of the run: the pixels right next to a
            # glyph still carry its anti-aliasing halo
            left = a - 3
            while left >= 0 and row[left]:
                left -= 1
            if left < 0:
                left = None
            right = b + 3
            while right < w and row[right]:
                right += 1
            if right >= w:
                right = None
            if left is None and right is None:
                continue
            if left is None:
                out[y, a:b + 1] = sub[y, right]
            elif right is None:
                out[y, a:b + 1] = sub[y, left]
            else:
                t = (np.arange(1, n + 1) / (n + 1))[:, None]
                out[y, a:b + 1] = sub[y, left] * (1 - t) + sub[y, right] * t
    return out


def grain_from(pool_res, shape, rng, block=12):
    h, w = shape
    ph, pw = pool_res.shape[:2]
    out = np.zeros((h, w, pool_res.shape[2]), np.float32)
    for by in range(0, h, block):
        for bx in range(0, w, block):
            oy = int(rng.integers(0, max(1, ph - block)))
            ox = int(rng.integers(0, max(1, pw - block)))
            bh, bw = min(block, h - by, ph - oy), min(block, w - bx, pw - ox)
            out[by:by + bh, bx:bx + bw] = pool_res[oy:oy + bh, ox:ox + bw]
    return out


def robust_std(v):
    return float(1.4826 * np.median(np.abs(v - np.median(v))))


def erase(table, preview=False):
    cfg = TABLES[table]
    src = os.path.join(HERE, f'tables/assets_{table}/{table}-original.png')
    dst = os.path.join(HERE, f'tables/assets_{table}/{table}.png')
    img = np.asarray(Image.open(src).convert('RGBA'))
    rgb = img[..., :3].astype(np.float32)
    lum = rgb.mean(2)
    rng = np.random.default_rng(SEED)
    pool_res = {k: hp(rgb[y0:y1, x0:x1]) for k, (x0, y0, x1, y1) in cfg['pools'].items()}

    total = np.zeros(lum.shape, np.uint8)
    for name, (x0, y0, x1, y1), mode, param, pool in cfg['regions']:
        sub = rgb[y0:y1, x0:x1]
        sub_lum = lum[y0:y1, x0:x1]
        keep = glyph_mask(sub, sub_lum, mode, param)
        if keep.sum() < 4:
            print(f'  {name:12s} no glyph pixels found')
            continue
        mask = cv2.dilate(keep, np.ones((2 * cfg['dilate'] + 1, 2 * cfg['dilate'] + 1), np.uint8))
        if cfg['method'] == 'row':
            base = row_fill(sub, mask > 0)
        else:
            base = cv2.inpaint(np.clip(sub, 0, 255).astype(np.uint8), mask,
                               4, cv2.INPAINT_TELEA).astype(np.float32)
        # keep the fill at the face's own level: a fill a shade off the plate
        # reads as a smudge even when it is perfectly empty
        pad = 2 * (cfg['dilate'] + 2) + 1
        near = cv2.dilate(mask, np.ones((pad, pad), np.uint8)) > 0
        ref = near & (mask == 0)
        if ref.sum() > 50:
            base = base + (np.median(sub[ref], axis=0) - np.median(base[mask > 0], axis=0))
        rr = cfg['dilate'] + cfg['ring']
        ring = cv2.dilate(keep, np.ones((2 * rr + 1, 2 * rr + 1), np.uint8))
        ring = (ring > 0) & (mask == 0)
        if ring.sum() < 100:
            ring = mask == 0
        amp = min(robust_std(hp(sub)[ring]), GRAIN_MAX)
        g = grain_from(pool_res[pool], sub.shape[:2], rng) * (amp / max(pool_res[pool].std(), 1e-6))
        g -= g.mean(axis=(0, 1), keepdims=True)
        alpha = np.clip(cv2.GaussianBlur(mask.astype(np.float32), (0, 0), 1.0), 0, 1)[..., None]
        rgb[y0:y1, x0:x1] = sub * (1 - alpha) + (base + g) * alpha
        total[y0:y1, x0:x1] |= mask
        print(f'  {name:12s} glyph px {int(keep.sum()):5d}  grain amp {amp:5.2f}')

    out = np.dstack([np.clip(rgb, 0, 255), img[..., 3:4]]).astype(np.uint8)
    Image.fromarray(out, 'RGBA').save(dst)
    if preview:
        vis = np.clip(rgb, 0, 255).astype(np.uint8)
        mm = cv2.dilate(total, np.ones((3, 3), np.uint8)) > 0
        vis[mm] = vis[mm] * 0.3 + np.array([0, 255, 0]) * 0.7
        Image.fromarray(vis).save(os.path.join(HERE, f'tables/assets_{table}/mask_preview.png'))
    print(f'wrote {os.path.relpath(dst, HERE)}  ({out.shape[1]}x{out.shape[0]})')


if __name__ == '__main__':
    table = sys.argv[1] if len(sys.argv) > 1 else 'hole'
    erase(table, preview='--preview' in sys.argv)
