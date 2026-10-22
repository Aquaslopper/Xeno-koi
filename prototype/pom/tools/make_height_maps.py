#!/usr/bin/env python3
"""Build the height maps Parallax Occlusion Mapping needs, out of the art we already ship.

The game ships eight painted backgrounds (`assets/*.jpg`, `assets/deus.png`) and eight
normal maps (`assets/*_normal.png`). A normal map gives you a slope per texel but no
absolute height, so POM cannot use it directly. This tool derives a height field from
what is already in the repository:

  broad masses   gaussian-blurred luminance, contrast-stretched   (raised tubes, sunken sky)
+  cavity detail  the normal map's alpha channel                  (the seams between tubes)
+  breathing room a little of the normal map's own X/Y slope integrated locally

Output: 640x360 8-bit grayscale PNGs (half-res -- height fields do not need the
detail of the colour map and half-res is ~4x cheaper to sample).

    python3 tools/make_height_maps.py                 # write prototype/pom/height/
    python3 tools/make_height_maps.py --variant b     # alternate recipe
    python3 tools/make_height_maps.py --check         # verify they are current
"""
import argparse
import hashlib
import os
import sys

import numpy as np
from PIL import Image, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))   # prototype/pom
REPO = os.path.dirname(os.path.dirname(ROOT))                      # repo root
GAME_ASSETS = os.path.join(REPO, "assets")
OUT = os.path.join(ROOT, "height")

BACKGROUNDS = [
    ("harvest", "jpg"), ("hole", "jpg"), ("arctic", "jpg"), ("castle", "jpg"),
    ("city", "jpg"), ("deus", "png"), ("face", "jpg"), ("realm", "jpg"),
]

W, H = 640, 360
LUMA = np.array([0.299, 0.587, 0.114], dtype=np.float32)


def to_linear(x):
    return np.where(x <= 0.04045, x / 12.92, ((x + 0.055) / 1.055) ** 2.4)


def blur(img, radius):
    im = Image.fromarray((np.clip(img, 0, 1) * 255).astype(np.uint8))
    return np.asarray(im.filter(ImageFilter.GaussianBlur(radius))).astype(np.float32) / 255.0


def stretch(x, lo_pct=1.0, hi_pct=99.0):
    lo, hi = np.percentile(x, [lo_pct, hi_pct])
    return np.clip((x - lo) / max(float(hi - lo), 1e-5), 0.0, 1.0)


def build(name, ext, variant="a"):
    """Height for one background.

    'a' (default, the one that reads as sculpted tubes):
        detail = |luminance - luminance blurred by 3px|, itself blurred by 10px.
        This finds the *edges* of the painted forms -- the rims of every rib and
        tube -- and turns them into stubs, which is what a POM marching ray
        actually needs to catch and occlude. Pure brightness makes cauliflower.
    """
    colour = Image.open(os.path.join(GAME_ASSETS, f"{name}.{ext}")).convert("RGB")
    rgb = np.asarray(colour.resize((W, H), Image.LANCZOS)).astype(np.float32) / 255.0

    nrm = Image.open(os.path.join(GAME_ASSETS, f"{name}_normal.png"))
    cav_src = nrm.getchannel("A") if nrm.mode == "RGBA" else nrm.convert("L")
    cavity = np.asarray(cav_src.resize((W, H), Image.LANCZOS)).astype(np.float32) / 255.0

    lum = to_linear(rgb) @ LUMA

    if variant == "a":
        edge = np.abs(blur(lum, 1.0) - blur(lum, 3.0))
        relief = stretch(blur(edge, 10.0), 2.0, 99.0)
        mass = stretch(blur(lum, 14.0), 2.0, 98.0)
        h = 0.75 * relief + 0.25 * mass - 0.10 * stretch(blur(cavity, 2.0), 5.0, 95.0)
    else:  # 'b' -- pure mass, softer and more diffuse (for comparison)
        h = 0.55 * stretch(blur(lum, 14.0), 2.0, 98.0) + 0.45 * stretch(blur(lum, 4.0), 2.0, 98.0)

    h = stretch(h, 0.5, 99.5)
    h = blur(h, 1.0)          # one last smoothing pass: no single-texel spikes
    return (np.clip(h, 0.0, 1.0) * 255).astype(np.uint8)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--variant", default="a", choices=["a", "b"])
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--out", default=OUT)
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)

    stale = 0
    for name, ext in BACKGROUNDS:
        want = build(name, ext, args.variant)
        path = os.path.join(args.out, f"{name}_height.png")
        if args.check:
            if not os.path.exists(path):
                print(f"MISSING {path}")
                stale += 1
                continue
            have = np.asarray(Image.open(path).convert("L"))
            if have.shape != want.shape or not np.array_equal(have, want):
                print(f"STALE   {name}_height.png")
                stale += 1
        else:
            Image.fromarray(want, mode="L").save(path, optimize=True)
            print(f"wrote {path}")

    if args.check:
        if stale:
            return 1
        print("height maps are up to date with the art.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
