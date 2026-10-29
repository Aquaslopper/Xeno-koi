#!/usr/bin/env python3
"""Cut a table backdrop into its pieces, at native pixels.

Every piece is a straight crop of the source canvas: nothing is resized,
resampled, retinted or refitted, so the art keeps the exact pixels it was
drawn with.  Cut rects are measured on the art itself: each one ends in a
row/column that holds no art, so no ornament is sliced in two.  The only
border allowed to be non-empty is the canvas edge.

Used for the Hole art, whose cut rects are the cells of the grid measured
off the user's cut-out markup, and for the Arctic art, whose rects are the
measured empty gaps between its frames.

Usage:  python3 tables/cut_art.py [table ...]        (default: hole arctic)
        reads  tables/assets_<table>/<table>.png
        writes tables/assets_<table>/cut/
"""
import json
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))

# name -> rect (x0, y0, x1, y1), left/top inclusive, right/bottom exclusive
PIECES = {
    'hole': [
        ('cpu_panel',     (0, 0, 501, 345)),       # CPU SCORE plaque + B/A/R/P rows
        ('card_row_cpu',  (501, 0, 1675, 206)),    # CPU hand, 8 card slots
        ('right_column',  (1675, 0, 1920, 1080)),  # RND/KK plate, combos, buttons
        ('deck',          (357, 345, 614, 692)),   # deck card + count bubble
        ('player_panel',  (0, 692, 476, 1080)),    # B/A/R/P rows + YOU SCORE plaque
        ('status_bar',    (614, 692, 1675, 824)),  # turn banner
        ('card_row_you',  (476, 824, 1675, 1080)),  # your hand, 8 card slots
    ],
    'arctic': [
        ('left_column',   (0, 0, 458, 1080)),      # CPU plaque, B/A/R/P, player plaque
        ('card_row_cpu',  (459, 0, 1712, 180)),    # CPU hand, 8 card slots
        ('deck',          (459, 181, 700, 789)),   # deck card + count bubble
        ('hand_and_status', (459, 790, 1712, 1080)),  # turn banner + your 8 slots
        ('right_column',  (1712, 0, 1920, 1080)),  # RND/KK, combos, pause, restart
    ],
}

# cells of the hole grid that carry no art: recorded so the decomposition
# of that canvas stays complete and verifiable
HOLE_EMPTY = [
    ('empty_top_strip',   (501, 206, 1675, 345)),
    ('empty_left_mid',    (0, 345, 357, 692)),
    ('empty_center',      (614, 345, 1675, 692)),
    ('empty_status_left', (476, 692, 614, 824)),
]


def lum(im):
    a = np.asarray(im.convert('RGBA')).astype(np.float32)
    return a[..., :3].mean(2) * (a[..., 3] / 255.0)


def main():
    tables = [a for a in sys.argv[1:] if not a.startswith('-')] or ['hole', 'arctic']
    for table in tables:
        src = os.path.join(HERE, f'assets_{table}/{table}.png')
        out = os.path.join(HERE, f'assets_{table}/cut')
        os.makedirs(out, exist_ok=True)
        im = Image.open(src).convert('RGBA')
        L = lum(im)
        W, H = im.size
        manifest = {'source': f'tables/assets_{table}/{table}.png',
                    'canvas': [W, H], 'pieces': []}
        print(f'{table}: {W}x{H}')
        for name, (x0, y0, x1, y1) in PIECES[table]:
            x0, y0, x1, y1 = max(0, x0), max(0, y0), min(W, x1), min(H, y1)
            edges = {
                'top':    L[y0, x0:x1].max() if y0 > 0 else 0.0,
                'bottom': L[y1 - 1, x0:x1].max() if y1 < H else 0.0,
                'left':   L[y0:y1, x0].max() if x0 > 0 else 0.0,
                'right':  L[y0:y1, x1 - 1].max() if x1 < W else 0.0,
            }
            bad = {k: float(v) for k, v in edges.items() if v > 8}
            if bad:
                raise SystemExit(f'  {name}: cut would slice art at {bad}')
            piece = im.crop((x0, y0, x1, y1))
            piece.save(os.path.join(out, name + '.png'))
            manifest['pieces'].append({'name': name, 'file': name + '.png',
                                       'rect': [x0, y0, x1, y1],
                                       'size': [x1 - x0, y1 - y0]})
            print(f'  {name:16s} {x1 - x0:4d}x{y1 - y0:<4d} at ({x0},{y0})')

        if table == 'hole':
            manifest['empty_cells'] = [
                {'name': n, 'rect': list(r), 'size': [r[2] - r[0], r[3] - r[1]]}
                for n, r in HOLE_EMPTY]
            for n, r in HOLE_EMPTY:
                assert L[r[1]:r[3], r[0]:r[2]].max() < 8, n
            print(f'  (+{len(HOLE_EMPTY)} empty grid cells, no art, not written)')

        # prove the pieces reproduce the canvas
        rebuilt = Image.new('RGBA', (W, H), (0, 0, 0, 255))
        for p in manifest['pieces']:
            piece = Image.open(os.path.join(out, p['file']))
            assert piece.size == tuple(p['size']), p['name']
            rebuilt.paste(piece, (p['rect'][0], p['rect'][1]))
        a = np.asarray(im).astype(np.int16)
        b = np.asarray(rebuilt).astype(np.int16)
        diff = np.abs(a - b).max(2) > 0
        inside = np.zeros_like(diff)
        for p in manifest['pieces']:
            x0, y0, x1, y1 = p['rect']
            inside[y0:y1, x0:x1] = True
        assert not (diff & inside).any(), 'pieces differ from the source'

        with open(os.path.join(out, 'pieces.json'), 'w') as fh:
            json.dump(manifest, fh, indent=2)
        print(f'  -> {len(manifest["pieces"])} pieces + pieces.json, '
              f'bit-exact, in tables/assets_{table}/cut/')


if __name__ == '__main__':
    main()
