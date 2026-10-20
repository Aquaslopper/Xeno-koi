#!/usr/bin/env python3
"""
Generate the bump/normal maps the background lighting shader samples.

For each non-Deus table background (assets/<name>.jpg) this writes
assets/<name>_normal.png, 1280x720 RGBA:

  RGB  tangent-space normal of a height field derived from the art.
       Convention matches assets/depth_fog*.fs on the fullscreen quad:
       x right, y DOWN (v grows with image rows), z toward the viewer,
       encoded as rgb * 0.5 + 0.5. No green-channel flip.
  A    the smoothed height field itself (0..1), kept for inspection. Nothing
       samples it: the shaders light the RGB normal with plain tangent-space
       N.L, so no relief mask is baked any more.

The height field is the two-scale gaussian of the art's luminance, so the
bright biomechanical ridges become raised forms. Gradient strength is
auto-normalized per image so every background gets equally strong relief
(the old hand-baked normal texture was nearly flat -- see PR #58).

Deus (assets/deus.png) is deliberately excluded: it keeps its own look.

Usage:
  python3 tools/make_bg_normal_maps.py               # write normal maps
  python3 tools/make_bg_normal_maps.py --check       # exit 1 if stale/missing
  python3 tools/make_bg_normal_maps.py --preview DIR # light-sweep simulation
                                                     # frames for visual review

--check also asserts the moving-light blocks of assets/depth_fog.fs and
assets/depth_fog_web.fs stay in sync (texture/texture2D spelling aside).

The preview replicates the shader math with the constants below; if you
tune the shader, tune these to match. Requires Pillow and numpy:
  pip install --break-system-packages pillow numpy
"""
import math
import pathlib
import sys

import numpy as np
from PIL import Image, ImageFilter

ROOT = pathlib.Path(__file__).resolve().parent.parent
ASSETS = ROOT / "assets"

# The seven non-Deus backgrounds; index 5 (deus.png) is intentionally absent.
BACKGROUNDS = ["harvest", "hole", "arctic", "castle", "city", "face", "realm"]

WIDTH, HEIGHT = 1280, 720
# The height field comes from luminance, so its gradient is only non-zero
# where the art actually changes -- half the frame is flat and the relief
# sits in bands along the art's contours. Lighting that read as edging
# rather than bumps, so the blur is now heavy: only the large forms (dome,
# spires, ridges) carry relief, and the art's own texture stops being
# amplified into visible contour banding.
BLUR_DETAIL = 13.0     # px sigma for the fine height scale
BLUR_BROAD = 26.0      # px sigma for the broad forms
DETAIL_WEIGHT = 0.35   # weight of the fine scale vs the broad one
TARGET_SLOPE = 2.6     # p98 of |grad height| is scaled to this slope, which
                       # sets how strong the relief is. 1.0 left the map nearly
                       # flat (mean tilt 10 degrees), so N.L barely varied and
                       # the light read as a wash; 2.6 gives mean tilt 25
                       # degrees, p90 56 -- bumps that actually answer a light.
SLOPE_MAX = 3.0        # clamp on the scaled slope, so the steepest art edges
                       # cannot push the normal past ~72 degrees and strobe.

# ---- moving-light constants; keep in sync with assets/depth_fog*.fs ----
# Standard tangent-space normal mapping: an ambient term plus two directional
# lights (a warm key whose direction drifts, a cool fixed fill) and a
# Blinn-Phong glint. No pools, no masks, no slope-only tricks.
AMBIENT = 0.55       # floor, so shadowed slopes stay out of pitch black
KEY_GAIN = 1.25
KEY_COLOR = (1.00, 0.93, 0.82)
FILL_GAIN = 0.30
FILL_COLOR = (0.55, 0.70, 1.00)
SPEC_POWER = 48.0
SPEC_GAIN = 0.30
FLASH_GAIN = 1.00    # thunder-spike lift; the decay envelope is C-side


def key_direction(t: float) -> np.ndarray:
    """Drifting key-light direction (unit, z toward the viewer, y DOWN)."""
    az = 0.90 * math.sin(t * 0.043) + 0.35 * math.sin(t * 0.017 + 1.3)
    el = 0.55 + 0.18 * math.sin(t * 0.029)
    v = np.array([math.sin(az) * math.cos(el), -math.sin(el), math.cos(az) * math.cos(el)])
    return v / np.linalg.norm(v)


FILL_DIRECTION = np.array([-0.55, 0.35, 0.75])
FILL_DIRECTION = FILL_DIRECTION / np.linalg.norm(FILL_DIRECTION)
# -----------------------------------------------------------------------


def height_field(img: Image.Image) -> np.ndarray:
    lum = np.asarray(img.convert("L"), dtype=np.float64) / 255.0
    broad = np.asarray(
        img.convert("L").filter(ImageFilter.GaussianBlur(BLUR_BROAD)),
        dtype=np.float64) / 255.0
    detail = np.asarray(
        img.convert("L").filter(ImageFilter.GaussianBlur(BLUR_DETAIL)),
        dtype=np.float64) / 255.0
    h = (1.0 - DETAIL_WEIGHT) * broad + DETAIL_WEIGHT * detail
    lo, hi = np.percentile(h, 1.0), np.percentile(h, 99.5)
    return np.clip((h - lo) / max(hi - lo, 1e-6), 0.0, 1.0)


def normal_map(img: Image.Image) -> np.ndarray:
    h = height_field(img)
    gy, gx = np.gradient(h)  # rows = v (down), cols = u (right)
    mag = np.sqrt(gx * gx + gy * gy)
    p = float(np.percentile(mag, 98.0))
    s = TARGET_SLOPE / max(p, 1e-6)
    slope = np.minimum(mag * s, SLOPE_MAX)
    ang = np.arctan2(gy, gx)
    nx, ny, nz = -slope * np.cos(ang), -slope * np.sin(ang), np.ones_like(h)
    inv = 1.0 / np.sqrt(nx * nx + ny * ny + nz * nz)
    out = np.zeros(h.shape + (4,), dtype=np.uint8)
    out[..., 0] = np.clip((nx * inv * 0.5 + 0.5) * 255.0, 0, 255)
    out[..., 1] = np.clip((ny * inv * 0.5 + 0.5) * 255.0, 0, 255)
    out[..., 2] = np.clip((nz * inv * 0.5 + 0.5) * 255.0, 0, 255)
    out[..., 3] = np.clip(h * 255.0, 0, 255)
    return out


def render_lights(normal_rgba: np.ndarray, base: np.ndarray, t: float,
                  flash: float = 0.0) -> np.ndarray:
    """Replicate the depth_fog moving-light block on an RGB float image.

    `flash` is the capture / Koi-Koi spike amount (0 at rest, 1 at the peak).
    Its decay envelope lives in src/ui_board.c, so pass the amount you want
    to preview; this only mirrors the shader's FLASH_GAIN conversion.
    """
    nx = normal_rgba[..., 0] / 255.0 * 2.0 - 1.0
    ny = normal_rgba[..., 1] / 255.0 * 2.0 - 1.0
    nz = normal_rgba[..., 2] / 255.0 * 2.0 - 1.0
    inv = 1.0 / np.sqrt(nx * nx + ny * ny + nz * nz)
    nx, ny, nz = nx * inv, ny * inv, nz * inv

    key = key_direction(t)
    fill = FILL_DIRECTION
    kd = np.clip(nx * key[0] + ny * key[1] + nz * key[2], 0.0, None)
    fd = np.clip(nx * fill[0] + ny * fill[1] + nz * fill[2], 0.0, None)
    # half-vector against the viewer (+z)
    h = key + np.array([0.0, 0.0, 1.0])
    h = h / np.linalg.norm(h)
    spec = np.clip(nx * h[0] + ny * h[1] + nz * h[2], 0.0, None) ** SPEC_POWER

    flash_gain = 1.0 + FLASH_GAIN * flash
    lit = np.full(nx.shape + (3,), AMBIENT, dtype=np.float64) * flash_gain
    for c in range(3):
        lit[..., c] += flash_gain * (KEY_COLOR[c] * (kd * KEY_GAIN + spec * SPEC_GAIN)
                                     + FILL_COLOR[c] * fd * FILL_GAIN)
    return np.clip(base * lit, 0.0, 1.0)


def write_normal(name: str) -> pathlib.Path:
    img = Image.open(ASSETS / f"{name}.jpg").convert("RGB")
    if img.size != (WIDTH, HEIGHT):
        raise SystemExit(f"{name}.jpg is {img.size}, expected {WIDTH}x{HEIGHT}")
    out = ASSETS / f"{name}_normal.png"
    Image.fromarray(normal_map(img), "RGBA").save(out, optimize=True)
    return out


def preview(outdir: pathlib.Path) -> None:
    outdir.mkdir(parents=True, exist_ok=True)
    for name in BACKGROUNDS:
        base_img = Image.open(ASSETS / f"{name}.jpg").convert("RGB")
        base = np.asarray(base_img, dtype=np.float64) / 255.0
        nm = np.asarray(Image.open(ASSETS / f"{name}_normal.png"))
        frames = [(0.0, "unlit"), (8.0, "t08"), (25.0, "t25"), (47.0, "t47")]
        cells = []
        for t, tag in frames:
            if tag == "unlit":
                cells.append(base)
            else:
                cells.append(render_lights(nm, base, t))
            Image.fromarray((cells[-1] * 255.0).astype(np.uint8)).save(
                outdir / f"{name}_{tag}.jpg", quality=88)
        top = np.concatenate([cells[0], cells[1]], axis=1)
        bottom = np.concatenate([cells[2], cells[3]], axis=1)
        grid = np.concatenate([top, bottom], axis=0)
        Image.fromarray((grid * 255.0).astype(np.uint8)).resize(
            (WIDTH, HEIGHT)).save(outdir / f"{name}_grid.jpg", quality=88)
        lit_means = [float(np.mean(c)) * 255.0 for c in cells]
        print(f"{name}: mean luma unlit={lit_means[0]:.1f} "
              f"t08={lit_means[1]:.1f} t25={lit_means[2]:.1f} t47={lit_means[3]:.1f}")


def lighting_block(path: pathlib.Path) -> str:
    text = path.read_text()
    begin, end = "// moving-lights begin", "// moving-lights end"
    i, j = text.index(begin), text.index(end)
    block = text[i:j]
    return block.replace("texture2D(", "texture(")


def check() -> int:
    ok = True
    for name in BACKGROUNDS:
        out = ASSETS / f"{name}_normal.png"
        src = ASSETS / f"{name}.jpg"
        if not out.exists():
            print(f"MISSING: {out.relative_to(ROOT)}")
            ok = False
            continue
        arr = np.asarray(Image.open(out))
        same = normal_map(Image.open(src).convert("RGB"))
        if arr.shape != (HEIGHT, WIDTH, 4) or not np.array_equal(arr, same):
            print(f"STALE: {out.relative_to(ROOT)} (regenerate with "
                  f"python3 tools/make_bg_normal_maps.py)")
            ok = False
    try:
        a = lighting_block(ASSETS / "depth_fog.fs")
        b = lighting_block(ASSETS / "depth_fog_web.fs")
        if a != b:
            print("STALE: moving-light block differs between depth_fog.fs "
                  "and depth_fog_web.fs")
            ok = False
    except ValueError as exc:
        print(f"STALE: lighting block markers missing ({exc})")
        ok = False
    return 0 if ok else 1


def main() -> int:
    args = sys.argv[1:]
    if "--check" in args:
        return check()
    if "--preview" in args:
        i = args.index("--preview")
        if i + 1 >= len(args):
            raise SystemExit("--preview needs a directory")
        preview(pathlib.Path(args[i + 1]))
        return 0
    for name in BACKGROUNDS:
        out = write_normal(name)
        print(f"wrote {out.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
