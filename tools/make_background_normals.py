#!/usr/bin/env python3
"""Generate animated-light normals from the original, pre-bake background art.

Requires Pillow and NumPy. Source art remains available in Git history;
never derive normals from the already relit/gamma-adjusted shipped JPGs.
"""
import io
import subprocess
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parent.parent
SOURCE_REF = 'c188a05f72caf13b7a91c71e52ccf2dac5950b10'
STRENGTH = 6.0
NAMES = ('harvest', 'hole', 'arctic', 'castle', 'city', 'face', 'realm')


def main():
    for name in NAMES:
        raw = subprocess.check_output(
            ['git', 'show', f'{SOURCE_REF}:assets/{name}.jpg'], cwd=ROOT)
        with Image.open(io.BytesIO(raw)) as im:
            rgb = np.asarray(im.convert('RGB'), dtype=np.float32) / 255.0
        lum = rgb @ np.array([0.299, 0.587, 0.114], dtype=np.float32)
        blurred = Image.fromarray((lum * 255).astype('uint8')).filter(
            ImageFilter.GaussianBlur(1.2))
        height = np.asarray(blurred, dtype=np.float32) / 255.0
        height = np.clip((height - 0.15) / 0.75, 0, 1) ** 0.9
        p = np.pad(height, 1, mode='edge')
        dx = (p[:-2, 2:] + 2*p[1:-1, 2:] + p[2:, 2:]
              - p[:-2, :-2] - 2*p[1:-1, :-2] - p[2:, :-2]) / 8
        dy = (p[2:, :-2] + 2*p[2:, 1:-1] + p[2:, 2:]
              - p[:-2, :-2] - 2*p[:-2, 1:-1] - p[:-2, 2:]) / 8
        normal = np.stack((-dx*STRENGTH, -dy*STRENGTH, np.ones_like(dx)), axis=-1)
        normal /= np.linalg.norm(normal, axis=-1, keepdims=True)
        Image.fromarray(((normal*0.5 + 0.5)*255).astype('uint8')).save(
            ROOT / 'assets' / f'{name}_normal.png')
        print(f'{name}: normal strength {STRENGTH}')


if __name__ == '__main__':
    main()
