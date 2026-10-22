#!/usr/bin/env python3
"""Bake occlusion into the backgrounds, using the height maps. No shader, no runtime cost.

This is option 3 from the README: the height field is used *offline* to darken the
crevices between the tubes and ribs, and the result is an ordinary JPEG that the game
loads today. Nothing in `src/` changes; the art just has more shape in it.

The occlusion term is the classic multi-scale "blurred height minus height": ground that
sits below its surroundings goes dark, ridges and plate faces keep their brightness. Three
radii, so a seam between two tubes and a whole sunken valley both get caught.

    python3 tools/bake_ao.py --bg castle --strength 0.55
    python3 tools/bake_ao.py --all --strength 0.55 --preview   # stills only
    python3 tools/bake_ao.py --all --write                     # write trial JPEGs
"""
import argparse
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import render_stills as R  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HEIGHT = os.path.join(ROOT, "height")
STILLS = os.path.join(ROOT, "stills")
TRIAL = os.path.join(ROOT, "baked")
LUMA = R.LUMA


def _blur(img, radius):
    from PIL import ImageFilter
    im = Image.fromarray((np.clip(img, 0, 1) * 255).astype(np.uint8))
    return np.asarray(im.filter(ImageFilter.GaussianBlur(radius))).astype(np.float32) / 255.0


def occlusion(height, radii=(2.0, 6.0, 16.0), gains=(0.55, 0.30, 0.15)):
    """1 = unoccluded, 0 = deep in a crevice.

    Each scale contributes where the surface sits *below* its neighbourhood. Ridges and
    flat faces sit at or above their own blur, so they stay at 1.0 and keep their light.
    """
    ao = np.ones_like(height)
    for r, g in zip(radii, gains):
        cavity = np.clip(_blur(height, r) - height, 0.0, 1.0)
        ao *= 1.0 - np.clip(cavity * g * 6.0, 0.0, 1.0)
    return np.clip(ao, 0.0, 1.0)


def bake(rgb, ao, strength, tint=(0.72, 0.62, 0.52)):
    """Multiply toward a warm shadow rather than toward black, so it stays painted."""
    amount = (1.0 - ao) * strength
    shadow = np.array(tint, np.float32) * rgb
    out = rgb * (1.0 - amount[..., None]) + shadow * amount[..., None]
    # a hair of contrast back, because occlusion flattens the mid-tones
    out = np.clip((out - 0.5) * 1.04 + 0.5, 0.0, 1.0)
    return out


def load_pair(name):
    ext = "png" if name == "deus" else "jpg"
    rgb = np.asarray(Image.open(os.path.join(R.GAME, f"{name}.{ext}")).convert("RGB")
                     .resize((R.W, R.H), Image.LANCZOS), np.float32) / 255.0
    hgt = np.asarray(Image.open(os.path.join(HEIGHT, f"{name}_height.png")).convert("L")
                     .resize((R.W, R.H), Image.BILINEAR), np.float32) / 255.0
    return rgb, hgt


def label(img, text, size=15):
    strip = Image.new("RGB", (img.width, 26), (8, 8, 8))
    d = ImageDraw.Draw(strip)
    try:
        font = ImageFont.load_default(size=size)
    except TypeError:
        font = ImageFont.load_default()
    d.text((10, 13), text, fill=(200, 152, 80), font=font, anchor="lm")
    out = Image.new("RGB", (img.width, img.height + 26), (0, 0, 0))
    out.paste(strip, (0, 0))
    out.paste(img, (0, 26))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bg", default="castle")
    ap.add_argument("--strength", type=float, default=0.55)
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--write", action="store_true", help="write trial JPEGs into baked/")
    ap.add_argument("--crop", default="380,300,940,600", help="x0,y0,x1,y1 for the 2x sheet")
    args = ap.parse_args()
    os.makedirs(STILLS, exist_ok=True)
    os.makedirs(TRIAL, exist_ok=True)

    names = ["harvest", "hole", "arctic", "castle", "city", "deus", "face", "realm"] \
        if args.all else [args.bg]
    box = tuple(int(v) for v in args.crop.split(","))

    for name in names:
        rgb, hgt = load_pair(name)
        ao = occlusion(hgt)
        out = bake(rgb, ao, args.strength)

        before, after = rgb, out
        # 2x crop, shipped above, baked below
        b = Image.fromarray((before * 255).astype(np.uint8)).crop(box)
        a = Image.fromarray((after * 255).astype(np.uint8)).crop(box)
        b = b.resize((b.width * 2, b.height * 2), Image.LANCZOS)
        a = a.resize((a.width * 2, a.height * 2), Image.LANCZOS)
        sheet = Image.new("RGB", (a.width, b.height + a.height + 56), (0, 0, 0))
        sheet.paste(label(b, "AS SHIPPED", 15), (0, 0))
        sheet.paste(label(a, f"BAKED AO · strength {args.strength:.2f}", 15), (0, b.height + 30))
        sheet.save(os.path.join(STILLS, f"{name}-baked-ao-2x.jpg"),
                   quality=91, optimize=True, subsampling=0)

        if args.write:
            Image.fromarray((out * 255).astype(np.uint8)).save(
                os.path.join(TRIAL, f"{name}.jpg"), quality=94, optimize=True, subsampling=0)

        lb, la = before @ LUMA, after @ LUMA
        print(f"{name:8s} mean {lb.mean():.4f} -> {la.mean():.4f} | "
              f"p10 {np.percentile(lb, 10):.4f} -> {np.percentile(la, 10):.4f} | "
              f"p95 {np.percentile(lb, 95):.4f} -> {np.percentile(la, 95):.4f} | "
              f"occluded {(ao < 0.95).mean() * 100:4.1f}% of frame")


if __name__ == "__main__":
    main()
