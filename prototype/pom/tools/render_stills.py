#!/usr/bin/env python3
"""Offline preview of Parallax Occlusion Mapping on Xeno Koi's painted backgrounds.

`assets/depth_fog.fs` is ported here 1:1 (moving normal-map light -> luminance fog ->
vignette), so the "flat" still is what the game draws today. The "pom" still runs the
same light on top of a ray-marched height field, which is what the shader would do.

The only difference allowed between the two stills is the POM itself:
  * sampling point of the colour/normal maps moves (uv -> ray hit)
  * the marching ray's own travel gives a cheap contact-AO term
  * a short march toward the screen-space light gives contact shadow

    python3 tools/render_stills.py --bg harvest --depth 34
"""
import argparse
import os
import sys

import numpy as np
from PIL import Image
from scipy.ndimage import map_coordinates

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))   # prototype/pom
REPO = os.path.dirname(os.path.dirname(ROOT))
GAME = os.path.join(REPO, "assets")
HEIGHT = os.path.join(ROOT, "height")
OUT = os.path.join(ROOT, "stills")
W, H = 1280, 720
LUMA = np.array([0.299, 0.587, 0.114], np.float32)
ASPECT = np.array([1.7777778, 1.0], np.float32)


# ------------------------------------------------------------------ maths
def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def bilinear(img, uv):
    h, w = img.shape[:2]
    xs = uv[..., 0] * (w - 1)
    ys = uv[..., 1] * (h - 1)
    if img.ndim == 3:
        return np.stack([map_coordinates(img[..., c], [ys, xs], order=1, mode="nearest")
                         for c in range(img.shape[2])], -1)
    return map_coordinates(img, [ys, xs], order=1, mode="nearest")


def load(name):
    ext = "png" if name == "deus" else "jpg"
    rgb = Image.open(os.path.join(GAME, f"{name}.{ext}")).convert("RGB").resize((W, H), Image.LANCZOS)
    nrm = Image.open(os.path.join(GAME, f"{name}_normal.png")).convert("RGB").resize((W, H), Image.LANCZOS)
    hgt = Image.open(os.path.join(HEIGHT, f"{name}_height.png")).convert("L").resize((W, H), Image.BILINEAR)
    return (np.asarray(rgb, np.float32) / 255.0,
            np.asarray(nrm, np.float32) / 255.0,
            np.asarray(hgt, np.float32) / 255.0)


def screen_uv():
    xs = (np.arange(W) + 0.5) / W
    ys = (np.arange(H) + 0.5) / H
    return np.stack(np.meshgrid(xs, ys), -1).astype(np.float32)


# ------------------------------------------------------------------ POM
def pom(uv, height, depth_px, steps=28, refine=4):
    """Steep parallax + binary refine, on a flat 'window' camera.

    depth_px is the on-screen offset, in pixels, at the edge of the frame; the
    offset falls off linearly toward the centre, which is what a viewer standing in
    front of a flat wall would see.
    """
    p = uv * 2.0 - 1.0
    p[..., 1] *= -1.0                                   # +v is down, +y is up
    slope = p * (depth_px / (W * 0.5))                  # uv units of offset per unit depth

    layer = 1.0 / steps
    hit = np.zeros(uv.shape[:2], bool)
    hit_t = np.ones(uv.shape[:2], np.float32)
    prev_t = np.zeros(uv.shape[:2], np.float32)
    prev_uv = uv.copy()
    prev_h = bilinear(height, uv)

    for i in range(1, steps + 1):
        t = np.float32(i * layer)
        c = np.clip(uv - slope * t, 0.0, 1.0)
        h = bilinear(height, c)
        cross = (~hit) & (h < t)
        if cross.any():
            # where in this layer the ray first went under the surface (linear fit)
            den = (prev_h - prev_t) - (h - t)
            frac = np.clip((prev_h - prev_t) / np.where(np.abs(den) < 1e-6, 1e-6, den), 0.0, 1.0)
            hit_t = np.where(cross, prev_t + frac * layer, hit_t)
            hit |= cross
        prev_t = np.where(hit, prev_t, t)
        prev_uv = np.where(hit[..., None], prev_uv, c)
        prev_h = np.where(hit, prev_h, h)

    # one binary refine pass inside the layer the hit landed in
    lo, hi = np.maximum(hit_t - layer, 0.0), hit_t
    for _ in range(refine):
        mid = 0.5 * (lo + hi)
        hh = bilinear(height, np.clip(uv - slope * mid[..., None], 0.0, 1.0))
        under = hh < mid
        hi = np.where(hit & under, mid, hi)
        lo = np.where(hit & under, lo, mid)
    hit_t = np.where(hit, 0.5 * (lo + hi), hit_t)
    hit_uv = np.clip(uv - slope * hit_t[..., None], 0.0, 1.0)
    return hit_uv, hit_t, slope


def light_dirs(t, tilt=(0.0, 0.0)):
    """Screen-space light position (same drift as the shader) + the direction to it."""
    drift = t * 0.1954768
    light_uv = np.array([0.08 + 0.84 * (0.5 - 0.5 * np.cos(drift)), 0.50], np.float32)
    return light_uv


def self_shadow(hit, height, uv, t, depth_px, elev_tan=0.9, taps=8, reach_px=16.0):
    """March from the surface toward the light over the height field.

    Height is in 0..1; depth_px pixels of horizontal parallax == 1.0 height, so the
    light ray rises (tan(elev) / depth_px) per screen pixel of side-step. Taps are
    weighted toward the surface, so a crevice away from the light darkens before
    the whole hill does (soft contact shadow, no hard pencil line).
    """
    light_uv = light_dirs(t)
    d = light_uv - uv
    n = np.linalg.norm(d, axis=-1, keepdims=True)
    d = d / np.maximum(n, 1e-4)
    h0 = bilinear(height, hit)
    low = np.clip(1.0 - h0 * 1.35, 0.0, 1.0)      # only crevices and undersides get shadows
    occ = np.zeros(hit.shape[:2], np.float32)
    wsum = 0.0
    for i in range(1, taps + 1):
        f = i / taps
        # closer taps matter most: 1 - f^2 falloff
        w = 1.0 - f * f
        step_px = reach_px * f
        p = hit + d * np.array([step_px / W, step_px / H], np.float32)
        ray = h0 + (step_px * elev_tan) / max(depth_px, 1e-3)
        stand = bilinear(height, np.clip(p, 0.0, 1.0))
        occ += w * np.clip((stand - ray) * 5.0, 0.0, 1.0)
        wsum += w
    return np.clip(occ / max(wsum, 1e-4), 0.0, 1.0) * low


# ------------------------------------------------------------------ the shipped shader
def light_layer(d, width):
    half = 0.5 * width
    feather = max(0.24, half * 0.65)
    return 1.0 - smoothstep(half - feather, half + 0.08, d)


def bg_moving_lights(base, normal, uv_screen, uv_sample, t, flash=0.0, beam_shade=None):
    """assets/depth_fog.fs bgMovingLights(). Light geometry in screen space,
    texture fetches at uv_sample."""
    n = normal * 2.0 - 1.0
    n /= np.maximum(np.linalg.norm(n, axis=-1, keepdims=True), 1e-6)

    drift, spin = t * 0.1954768, t * 0.2150244
    light_uv = np.array([0.08 + 0.84 * (0.5 - 0.5 * np.cos(drift)), 0.50], np.float32)
    axis = np.array([np.cos(spin), np.sin(spin)], np.float32)
    xd = np.abs((uv_screen - light_uv) @ axis)

    layers = np.maximum.reduce([0.10 * light_layer(xd, 1.568),
                                0.133333 * light_layer(xd, 1.176),
                                0.166667 * light_layer(xd, 0.784),
                                0.20 * light_layer(xd, 0.392)])
    layers = 0.65 * layers + 0.35 * (0.20 * np.exp(-(xd ** 2) / (0.52 ** 2)))

    to_l = (light_uv - uv_screen) * ASPECT
    reach = np.exp(-(to_l ** 2).sum(-1) / (0.90 ** 2))
    ld = np.concatenate([to_l, np.full(to_l.shape[:2] + (1,), 0.42, np.float32)], -1)
    ld /= np.maximum(np.linalg.norm(ld, axis=-1, keepdims=True), 1e-6)
    hv = ld + np.array([0, 0, 1], np.float32)
    hv /= np.maximum(np.linalg.norm(hv, axis=-1, keepdims=True), 1e-6)

    relief = smoothstep(0.015, 0.30, np.linalg.norm(n[..., :2], axis=-1))
    dirbias = 0.55 + 0.45 * smoothstep(-0.30, 0.35, (n[..., :2] * ld[..., :2]).sum(-1))
    slope = relief * dirbias
    spec = np.maximum((n * hv).sum(-1), 0.0) ** 14.0
    surf = smoothstep(0.014, 0.065, base @ LUMA)

    r1 = np.array([7.0 / W, 7.0 / H], np.float32)
    r2 = np.array([13.0 / W, 13.0 / H], np.float32)
    bloom = np.zeros(base.shape[:2], np.float32)
    for o in [(-r1[0], 0), (r1[0], 0), (0, -r1[1]), (0, r1[1]),
              (-r2[0], -r2[1]), (r2[0], -r2[1]), (-r2[0], r2[1]), (r2[0], r2[1])]:
        s = np.clip(uv_sample + np.array(o, np.float32), 0.001, 0.999)
        sn = bilinear(normal, s) * 2.0 - 1.0
        sn /= np.maximum(np.linalg.norm(sn, axis=-1, keepdims=True), 1e-6)
        sb = bilinear(base, s)
        bloom += (smoothstep(0.015, 0.30, np.linalg.norm(sn[..., :2], axis=-1))
                  * (0.55 + 0.45 * smoothstep(-0.30, 0.35, (sn[..., :2] * ld[..., :2]).sum(-1)))
                  * smoothstep(0.014, 0.065, sb @ LUMA))
    bloom *= 0.125

    beam = layers * reach * (1.0 + 0.95 * flash) + flash * 0.065 * (0.35 + 0.65 * reach)
    if beam_shade is not None:          # 1 = unobstructed, 0 = behind a ridge
        beam = beam * beam_shade
    warm = np.array([1.0, 0.93, 0.80], np.float32)
    diff = beam * (slope * 11.0 + bloom * 6.5)
    lblr = beam * (surf * slope * (0.32 + 1.05 * spec) * 0.95 + bloom * 1.15)
    add = base * warm * diff[..., None] + warm * lblr[..., None]
    return np.clip(base + (1.0 - base) * (1.0 - np.exp(-add)), 0.0, 1.0)


def fog_and_vignette(img, uv, t, height=None, height_fog=0.0):
    y = uv[..., 1]
    y_mask = smoothstep(0.38, 0.88, y)
    lum = img @ LUMA
    depth_mask = smoothstep(0.78, 0.18, lum)
    w1 = np.sin(uv[..., 0] * 6.283 + t * 0.32) * 0.5 + 0.5
    w2 = np.sin(uv[..., 0] * 14.137 + uv[..., 1] * 3.1415 - t * 0.22) * 0.5 + 0.5
    w3 = np.sin(uv[..., 0] * 4.712 + uv[..., 1] * 7.854 + t * 0.15) * 0.5 + 0.5
    patch = smoothstep(0.15, 0.75, w1 * 0.50 + w2 * 0.30 + w3 * 0.20)
    a = np.clip(depth_mask * y_mask * patch * 0.80, 0.0, 0.55)
    if height is not None and height_fog > 0.0:
        # same amount of fog, but it pools in the low ground and leaves ridges clear
        low = 1.0 - np.clip(bilinear(height, uv), 0.0, 1.0)
        a = a * (1.0 - height_fog + height_fog * low) * 1.25
    a = np.where(y > 0.38, a, 0.0)[..., None]
    out = img * (1 - a) + np.array([0.0196, 0.0196, 0.0196], np.float32) * a
    vig = 1.0 - smoothstep(0.25, 0.75, np.linalg.norm(uv - 0.5, axis=-1))
    return out * vig[..., None]


# ------------------------------------------------------------------ frames
def render(name, depth_px, t=6.0, ao_strength=0.30, shadow_strength=0.75, elev_tan=0.9):
    rgb, nrm, hgt = load(name)
    uv = screen_uv()

    flat = bg_moving_lights(bilinear(rgb, uv), bilinear(nrm, uv), uv, uv, t)
    flat = fog_and_vignette(flat, uv, t)

    hit, travel, slope = pom(uv, hgt, depth_px)
    cbase = bilinear(rgb, hit)
    cnrm = bilinear(nrm, hit)
    pomc = bg_moving_lights(cbase, cnrm, uv, hit, t)

    if ao_strength > 0.0:
        # Free contact occlusion: how deep the marching ray had to travel before it
        # found the surface. Crevices and undersides come out darker for no extra cost.
        pomc = pomc * (1.0 - ao_strength * np.clip(travel, 0, 1) ** 1.5)[..., None]

    if shadow_strength > 0.0:
        sh = self_shadow(hit, hgt, uv, t, depth_px)
        pomc = pomc * (1.0 - shadow_strength * sh)[..., None]

    pomc = fog_and_vignette(pomc, hit, t)
    return flat, pomc, hit, travel


def pair(a, b, gap=3):
    """flat on the left, POM on the right, an amber hairline between them."""
    h, w = a.shape[:2]
    out = np.zeros((h, w * 2 + gap, 3), np.float32)
    out[:, :w] = a
    out[:, w + gap:] = b
    out[:, w:w + gap] = np.array([0.85, 0.55, 0.2], np.float32)
    return out


def wipe(a, b, cut=0.5):
    """One frame, flat on the left of the divider and POM on the right."""
    out = b.copy()
    x = int(cut * a.shape[1])
    out[:, :x] = a[:, :x]
    out[:, max(x - 1, 0):x + 1] = np.array([0.95, 0.62, 0.24], np.float32)
    return out


# Half-res pairs for the whole frame; 2x crops for the two frames where shape
# change is easiest to see. Boxes are in 1280x720 pixels, (x0, y0, x1, y1).
CROPS = {
    "harvest": (560, 300, 1000, 575),
    "castle": (700, 120, 1140, 395),
    "face": (700, 60, 1140, 335),
}


def save(arr, path, quality=88):
    Image.fromarray((np.clip(arr, 0, 1) * 255).astype(np.uint8)).save(
        path, quality=quality, optimize=True, subsampling=1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bg", default="harvest")
    ap.add_argument("--depth", type=float, default=18.0, help="pixel offset at the frame edge")
    ap.add_argument("--crop-depth", type=float, default=26.0, help="depth for the 2x crop sheets")
    ap.add_argument("--time", type=float, default=6.0)
    ap.add_argument("--out", default=OUT)
    ap.add_argument("--all", action="store_true")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)

    names = list(CROPS) if args.all else [args.bg]
    for n in names:
        flat, pomc, hit, travel = render(n, args.depth, args.time)
        save(pair(flat, pomc)[::2, ::2], os.path.join(args.out, f"{n}-flat-vs-pom.jpg"))
        save(wipe(flat, pomc), os.path.join(args.out, f"{n}-wipe.jpg"))
        print(f"{n:8s} luma flat {(flat @ LUMA).mean():.4f} pom {(pomc @ LUMA).mean():.4f}")

    if args.all:
        for n, box in CROPS.items():
            flat, pomc, _, _ = render(n, args.crop_depth, args.time)
            f = Image.fromarray((flat * 255).astype(np.uint8)).crop(box)
            p = Image.fromarray((pomc * 255).astype(np.uint8)).crop(box)
            f = f.resize((f.width * 2, f.height * 2), Image.LANCZOS)
            p = p.resize((p.width * 2, p.height * 2), Image.LANCZOS)
            sheet = Image.new("RGB", (f.width * 2 + 4, f.height), (90, 60, 25))
            sheet.paste(f, (0, 0))
            sheet.paste(p, (f.width + 4, 0))
            sheet.save(os.path.join(args.out, f"{n}-crop-2x.jpg"), quality=90, optimize=True,
                       subsampling=0)
            print(f"         crop {n} {sheet.size}")


if __name__ == "__main__":
    main()
