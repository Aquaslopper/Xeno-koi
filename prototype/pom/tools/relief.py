#!/usr/bin/env python3
"""Relief lighting from the height map — additive, not subtractive.

Every mode on the ambient page so far only *darkens* (AO, fog pooling). These backgrounds
are already ~90% black (mean luma 0.10-0.16, median 0.04), so darkening has nothing to work
with: it turns a black valley into a blacker valley and reads as "a few black spots".

This does the opposite. The height map's gradient becomes a surface normal; the game's own
drifting light shades it; and the result is *added* to the painting. On near-black art an
added highlight is visible where a subtracted one is not.

    python3 tools/relief.py --all
"""
import argparse
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import render_stills as R  # noqa: E402
import bake_ao as B  # noqa: E402

LUMA = R.LUMA
STILLS = B.STILLS


def height_normal(hgt, relief=6.0, smooth=2):
    """Tangent-space normal from the height field, at painting scale rather than texel scale."""
    from PIL import ImageFilter
    im = Image.fromarray((np.clip(hgt, 0, 1) * 255).astype(np.uint8))
    h = np.asarray(im.filter(ImageFilter.GaussianBlur(smooth))).astype(np.float32) / 255.0
    gy, gx = np.gradient(h)
    n = np.stack([-gx * relief, -gy * relief, np.ones_like(h)], -1)
    return n / np.maximum(np.linalg.norm(n, axis=-1, keepdims=True), 1e-6)


def light_dirs(uv, t):
    light_uv = np.array([0.08 + 0.84 * (0.5 - 0.5 * np.cos(t * 0.1954768)), 0.50], np.float32)
    to_l = (light_uv - uv) * R.ASPECT
    ld = np.concatenate([to_l, np.full(to_l.shape[:2] + (1,), 0.42, np.float32)], -1)
    ld /= np.maximum(np.linalg.norm(ld, axis=-1, keepdims=True), 1e-6)
    return ld


def relief_add(rgb, hgt, uv, t=6.0, strength=1.0, elev=6.0):
    n = height_normal(hgt, relief=elev)
    ld = light_dirs(uv, t)
    hv = ld + np.array([0, 0, 1], np.float32)
    hv /= np.maximum(np.linalg.norm(hv, axis=-1, keepdims=True), 1e-6)

    lam = np.clip((n * ld).sum(-1), 0.0, 1.0)
    spec = np.clip((n * hv).sum(-1), 0.0, 1.0) ** 24.0
    # only where the painting has *something* to catch light, and stronger up high
    detail = np.clip(R.bilinear(hgt, uv), 0, 1) ** 0.7

    warm = np.array([1.0, 0.90, 0.74], np.float32)
    graze = np.clip(lam * 0.65 + spec * 0.9, 0.0, 1.6) * detail * strength
    out = rgb + warm * (graze * 0.22)[..., None] * (1.0 - rgb) ** 0.6
    return np.clip(out, 0.0, 1.0)


def label(img, text):
    strip = Image.new("RGB", (img.width, 26), (8, 8, 8))
    d = ImageDraw.Draw(strip)
    try:
        font = ImageFont.load_default(size=15)
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
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--strength", type=float, default=1.0)
    ap.add_argument("--crop", default="380,300,940,600")
    args = ap.parse_args()
    box = tuple(int(v) for v in args.crop.split(","))
    uv = R.screen_uv()

    for name in (["castle", "harvest"] if args.all else [args.bg]):
        rgb, nrm, hgt = R.load(name)
        light = R.bilinear(nrm, uv)
        base = R.bilinear(rgb, uv)

        shipped = R.fog_and_vignette(
            R.bg_moving_lights(base, light, uv, uv, 6.0), uv, 6.0)
        lit_rgb = relief_add(base, hgt, uv, 6.0, args.strength)
        relief = R.fog_and_vignette(
            R.bg_moving_lights(lit_rgb, light, uv, uv, 6.0), uv, 6.0)

        d = np.abs((relief @ LUMA) - (shipped @ LUMA))
        print(f"{name:8s} additive relief  >2%: {(d > 0.02).mean() * 100:5.1f}%  "
              f">5%: {(d > 0.05).mean() * 100:5.1f}%  mean {d.mean():.4f}  max {d.max():.3f}  "
              f"mean luma {float((shipped @ LUMA).mean()):.4f} -> {float((relief @ LUMA).mean()):.4f}")

        panels = []
        for tag, img in (("AS SHIPPED", shipped), ("ADDITIVE RELIEF (new idea)", relief)):
            c = Image.fromarray((np.clip(img, 0, 1) * 255).astype(np.uint8)).crop(box)
            c = c.resize((c.width * 2, c.height * 2), Image.LANCZOS)
            panels.append(label(c, tag))
        w, h = panels[0].size
        sheet = Image.new("RGB", (w, h * 2 + 4), (0, 0, 0))
        for i, p in enumerate(panels):
            sheet.paste(p, (0, i * (h + 4)))
        sheet.save(os.path.join(STILLS, f"{name}-relief-2x.jpg"),
                   quality=91, optimize=True, subsampling=0)
        print("         ->", os.path.join(STILLS, f"{name}-relief-2x.jpg"))


if __name__ == "__main__":
    main()
