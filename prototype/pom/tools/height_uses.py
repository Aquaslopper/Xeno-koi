#!/usr/bin/env python3
"""What the height maps are good for, without any POM.

Three versions of the *shipped* look, in order:

  1. AS SHIPPED   - assets/depth_fog.fs exactly as it is today.
  2. + HEIGHT FOG - the same fog, but weighted by height: it settles in the low
                    ground and leaves the ridges clear, instead of keying off
                    brightness alone.
  3. + BEAM SHADE - the drifting light is occluded by whatever stands between it
                    and the pixel, so the beam only lands where it can reach.

Neither change moves a single pixel of the painting. They only decide how much
fog and light land on it, which is why they cannot smear the skyline the way
parallax occlusion mapping did.

    python3 tools/height_uses.py --bg harvest
    python3 tools/height_uses.py --all
"""
import argparse
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import render_stills as R  # noqa: E402  (the numpy port of depth_fog.fs)

OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "stills")
LABEL_BG = (8, 8, 8)
LABEL_FG = (196, 148, 78)


def beam_occlusion(uv, height, t, taps=12, reach=0.22, elev=0.28):
    """1 = lit, 0 = fully behind a ridge. Marches from the pixel toward the light."""
    light_uv = np.array([0.08 + 0.84 * (0.5 - 0.5 * np.cos(t * 0.1954768)), 0.50], np.float32)
    d = light_uv - uv
    d = d / np.maximum(np.linalg.norm(d, axis=-1, keepdims=True), 1e-4)
    h0 = R.bilinear(height, uv)
    occ = np.zeros(uv.shape[:2], np.float32)
    for i in range(1, taps + 1):
        f = i / taps
        p = np.clip(uv + d * (reach * f), 0.0, 1.0)
        ray = h0 + elev * reach * f
        stand = R.bilinear(height, p)
        # nearer blockers count most; soft knee so a flat ridge does not hard-clip
        grain = bilinear_lod(stand, f)
        occ = np.maximum(occ, np.clip((grain - ray) * 5.0, 0.0, 1.0) * (1.0 - 0.75 * f))
    return 1.0 - 0.55 * occ


def bilinear_lod(stand, f):
    """De-weight single-texel spikes: a blocker has to be a mass, not noise."""
    return stand * (1.0 - 0.35 * f)


def render_variants(name, t=6.0):
    rgb, nrm, hgt = R.load(name)
    uv = R.screen_uv()
    base, norm = R.bilinear(rgb, uv), R.bilinear(nrm, uv)

    shipped = R.fog_and_vignette(R.bg_moving_lights(base, norm, uv, uv, t), uv, t)

    # fog that settles low: the same density, redistributed by height
    fogged = R.fog_and_vignette(
        R.bg_moving_lights(base, norm, uv, uv, t), uv, t,
        height=hgt, height_fog=1.0)

    shade = beam_occlusion(uv, hgt, t)
    lit = R.fog_and_vignette(
        R.bg_moving_lights(base, norm, uv, uv, t, beam_shade=shade), uv, t,
        height=hgt, height_fog=1.0)

    return shipped, fogged, lit, shade


def label(img, text, height=26):
    """Amber caption strip above a panel."""
    w = img.width
    strip = Image.new("RGB", (w, height), LABEL_BG)
    d = ImageDraw.Draw(strip)
    try:
        font = ImageFont.load_default(size=15)
    except TypeError:                     # Pillow < 10.1
        font = ImageFont.load_default()
    d.text((10, height // 2), text, fill=LABEL_FG, font=font, anchor="lm")
    out = Image.new("RGB", (w, height + img.height), (0, 0, 0))
    out.paste(strip, (0, 0))
    out.paste(img, (0, height))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bg", default="harvest")
    ap.add_argument("--time", type=float, default=6.0)
    ap.add_argument("--out", default=OUT)
    ap.add_argument("--all", action="store_true")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)

    for name in (["harvest", "castle"] if args.all else [args.bg]):
        shipped, fogged, lit, shade = render_variants(name, args.time)
        panels = [
            label(_img(shipped), "1  AS SHIPPED"),
            label(_img(fogged), "2  + HEIGHT FOG (fog settles in the low ground)"),
            label(_img(lit), "3  + BEAM SHADE (light blocked by ridges)"),
        ]
        gap = 6
        w = panels[0].width
        h = panels[0].height
        sheet = Image.new("RGB", (w * 3 + gap * 2, h), (0, 0, 0))
        for i, p in enumerate(panels):
            sheet.paste(p, (i * (w + gap), 0))
        path = os.path.join(args.out, f"{name}-height-uses.jpg")
        sheet.save(path, quality=90, optimize=True, subsampling=0)
        lum = [float((v @ R.LUMA).mean()) for v in (shipped, fogged, lit)]
        print(f"{name}: mean luma shipped {lum[0]:.4f} fog {lum[1]:.4f} beam {lum[2]:.4f} | "
              f"beam blocked on {(shade < 0.9).mean() * 100:.0f}% of the frame -> {path}")


def _img(arr):
    return Image.fromarray((np.clip(arr, 0, 1) * 255).astype(np.uint8))


if __name__ == "__main__":
    main()
