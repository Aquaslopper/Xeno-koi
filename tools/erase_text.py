#!/usr/bin/env python3
"""Erase the mockup's game-drawn text from a table backdrop.

Each text run gets a search window.  Inside the window, bright connected
components that do not touch the window edge are the glyph pixels.  Those
pixels are inpainted (Telea) and re-grained with texture sampled from a
measured clean patch of the same panel, so the fill keeps the metal grain
instead of turning into a flat patch.  Frames, bubbles, medallions and
ornaments are left untouched.

Reads  tables/assets_hole/hole-original.png
Writes tables/assets_hole/hole.png
"""
import numpy as np
import cv2
from PIL import Image
import os

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(HERE, 'tables/assets_hole/hole-original.png')
DST = os.path.join(HERE, 'tables/assets_hole/hole.png')

# Left column plates are one material, the right column another, the status
# banner another, the deck card another.  Each pool is a measured clean patch.
POOLS = {
    'left':   (86, 88, 292, 112),
    'right':  (1726, 62, 1855, 78),
    'status': (700, 762, 1400, 773),
    'deck':   (440, 512, 492, 586),
}

# name, glyph window (x0, y0, x1, y1), threshold, pool
REGIONS = [
    ('cpu_score',   (26, 28, 205, 66),      52, 'left'),
    ('you_score',   (24, 1008, 300, 1042),  52, 'left'),
    ('lane1',       (26, 84, 74, 114),      50, 'left'),
    ('lane2',       (26, 146, 74, 178),     50, 'left'),
    ('lane3',       (26, 208, 74, 240),     50, 'left'),
    ('lane4',       (26, 270, 74, 302),     50, 'left'),
    ('lane5',       (26, 742, 74, 772),     50, 'left'),
    ('lane6',       (26, 806, 74, 838),     50, 'left'),
    ('lane7',       (26, 870, 74, 902),     50, 'left'),
    ('lane8',       (26, 936, 74, 968),     50, 'left'),
    ('deck_vert',   (496, 506, 532, 592),   55, 'deck'),
    ('deck_count',  (470, 598, 514, 658),   55, 'deck'),
    ('status',      (690, 767, 1420, 794),  58, 'status'),
    ('rnd',         (1724, 31, 1878, 68),   55, 'right'),
    ('kk',          (1724, 72, 1878, 106),  55, 'right'),
    ('cpu_combos',  (1724, 130, 1878, 300), 55, 'right'),
    ('cpu_click',   (1728, 410, 1878, 458), 55, 'right'),
    ('your_combos', (1724, 460, 1878, 652), 55, 'right'),
    ('you_click',   (1728, 754, 1878, 804), 55, 'right'),
    ('pause',       (1760, 856, 1878, 896), 55, 'right'),
    ('restart',     (1748, 940, 1878, 982), 55, 'right'),
]

AREA_MIN, AREA_MAX = 6, 2500
H_MIN, H_MAX = 2, 48
DILATE = 3
RING = 8
GRAIN_MAX = 14.0          # cap on the sampled texture amplitude
SEED = 20261029


def hp(a, r=2):
    """High-pass residual: local grain, no low-frequency shading."""
    return a - cv2.blur(a, (2 * r + 1, 2 * r + 1), borderType=cv2.BORDER_REFLECT)


def glyph_mask(sub_lum, thr):
    wh, hh = sub_lum.shape[1], sub_lum.shape[0]
    m = (sub_lum > thr).astype(np.uint8)
    n, lab, st, _ = cv2.connectedComponentsWithStats(m, 8)
    keep = np.zeros_like(m)
    for i in range(1, n):
        x, y, w, h, ar = st[i]
        edge = x <= 0 or y <= 0 or x + w >= wh or y + h >= hh
        if AREA_MIN <= ar <= AREA_MAX and H_MIN <= h <= H_MAX and not edge:
            keep |= (lab == i).astype(np.uint8)
    return keep


def grain_from(pool_res, shape, rng, block=12):
    """Stitch random blocks of the pool's grain: no visible repeat, no seams."""
    h, w = shape
    ph, pw = pool_res.shape[:2]
    out = np.zeros((h, w, pool_res.shape[2]), np.float32)
    for by in range(0, h, block):
        for bx in range(0, w, block):
            oy = int(rng.integers(0, max(1, ph - block)))
            ox = int(rng.integers(0, max(1, pw - block)))
            bh, bw = min(block, h - by, ph - oy), min(block, w - bx, pw - ox)
            bh, bw = min(bh, ph), min(bw, pw)
            out[by:by + bh, bx:bx + bw] = pool_res[oy:oy + bh, ox:ox + bw]
    return out


def robust_std(values):
    med = np.median(values)
    return float(1.4826 * np.median(np.abs(values - med)))


def main():
    img = np.asarray(Image.open(SRC).convert('RGBA'))
    rgb = img[..., :3].astype(np.float32)
    lum = rgb.mean(2)
    rng = np.random.default_rng(SEED)

    pool_res = {}
    for key, (x0, y0, x1, y1) in POOLS.items():
        pool_res[key] = hp(rgb[y0:y1, x0:x1])

    for name, (x0, y0, x1, y1), thr, pool in REGIONS:
        sub = rgb[y0:y1, x0:x1]
        sub_lum = lum[y0:y1, x0:x1]
        pr = pool_res[pool]
        keep = glyph_mask(sub_lum, thr)
        if keep.sum() < 4:
            print(f'  {name:12s} no glyph pixels found')
            continue
        mask = cv2.dilate(keep, np.ones((2 * DILATE + 1, 2 * DILATE + 1), np.uint8))
        base = cv2.inpaint(np.clip(sub, 0, 255).astype(np.uint8), mask,
                           4, cv2.INPAINT_TELEA).astype(np.float32)

        ring = cv2.dilate(keep, np.ones((2 * (DILATE + RING) + 1, 2 * (DILATE + RING) + 1), np.uint8))
        ring = (ring > 0) & (mask == 0)
        if ring.sum() < 100:
            ring = mask == 0
        amp = min(robust_std(hp(sub)[ring]), GRAIN_MAX)
        g = grain_from(pr, sub.shape[:2], rng) * (amp / max(pr.std(), 1e-6))

        alpha = np.clip(cv2.GaussianBlur(mask.astype(np.float32), (0, 0), 1.4), 0, 1)[..., None]
        rgb[y0:y1, x0:x1] = sub * (1 - alpha) + (base + g) * alpha
        print(f'  {name:12s} glyph px {int(keep.sum()):5d}  grain amp {amp:5.2f}')

    out = np.dstack([np.clip(rgb, 0, 255), img[..., 3:4]]).astype(np.uint8)
    Image.fromarray(out, 'RGBA').save(DST)
    print(f'wrote {os.path.relpath(DST, HERE)}  ({out.shape[1]}x{out.shape[0]})')


if __name__ == '__main__':
    main()
