#!/usr/bin/env python3
"""Cut the table art along the marked cut lines, at native pixels.

The cut rects are the cells of the line grid measured off the user's
"EXAMPLE ON HOW TO CUT OUT THE ART" markup (straight 7 px strokes; each
line's centre is the cut).  Nothing is resized, resampled or refitted:
every piece is a straight crop of the source canvas, so the art keeps
every pixel it was drawn with.  Cells that contain no art are skipped.

Usage:  python3 tables/cut_art.py [source.png] [outdir]
        default source: tables/assets_hole/hole.png
        default outdir: tables/assets_hole/cut
"""
import json
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_SRC = os.path.join(HERE, 'assets_hole/hole.png')
DEFAULT_OUT = os.path.join(HERE, 'assets_hole/cut')

# Cells of the measured line grid: x=357,476,501,614,1675 / y=206,345,692,824.
PIECES = [
    ('cpu_panel',       (0, 0, 501, 345)),      # CPU SCORE plaque + B/A/R/P rows
    ('card_row_cpu',    (501, 0, 1675, 206)),   # CPU hand, 8 card slots
    ('right_column',    (1675, 0, 1920, 1080)),  # RND/KK plate, combos, buttons
    ('deck',            (357, 345, 614, 692)),  # deck card + count bubble
    ('player_panel',    (0, 692, 476, 1080)),   # B/A/R/P rows + YOU SCORE plaque
    ('status_bar',      (614, 692, 1675, 824)),  # turn banner
    ('card_row_you',    (476, 824, 1675, 1080)),  # your hand, 8 card slots
]

# Cells that the drawn grid leaves empty on purpose: cut anyway so the
# decomposition of the canvas stays complete and verifiable.
EMPTY = [
    ('empty_top_strip',   (501, 206, 1675, 345)),
    ('empty_left_mid',    (0, 345, 357, 692)),
    ('empty_center',      (614, 345, 1675, 692)),
    ('empty_status_left', (476, 692, 614, 824)),
]


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SRC
    out = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_OUT
    os.makedirs(out, exist_ok=True)

    im = Image.open(src).convert('RGBA')
    W, H = im.size
    manifest = {'source': os.path.relpath(src, HERE), 'canvas': [W, H],
                'pieces': [], 'empty_cells': []}

    for name, (x0, y0, x1, y1) in PIECES + EMPTY:
        x0, y0, x1, y1 = max(0, x0), max(0, y0), min(W, x1), min(H, y1)
        piece = im.crop((x0, y0, x1, y1))          # native crop, no scaling
        path = os.path.join(out, name + '.png')
        empty = name.startswith('empty_')
        if not empty:
            piece.save(path)
        rec = {'name': name, 'file': None if empty else os.path.basename(path),
               'rect': [x0, y0, x1, y1], 'size': [x1 - x0, y1 - y0]}
        (manifest['empty_cells'] if empty else manifest['pieces']).append(rec)
        print(f'  {name:18s} {x1 - x0:4d}x{y1 - y0:<4d}  at ({x0},{y0})'
              f'{"  [empty cell, not written]" if empty else ""}')

    with open(os.path.join(out, 'pieces.json'), 'w') as fh:
        json.dump(manifest, fh, indent=2)
    print(f'wrote {len(manifest["pieces"])} pieces + pieces.json to '
          f'{os.path.relpath(out, HERE)}')


if __name__ == '__main__':
    main()
