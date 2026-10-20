#!/usr/bin/env python3
"""
Generate the bump/normal maps the background lighting shader samples.

For each non-Deus table background (assets/<name>.jpg) this writes
assets/<name>_normal.png, 1280x720 RGBA:

  RGB  tangent-space normal of a height field derived from the art.
       Convention matches assets/depth_fog*.fs on the fullscreen quad:
       x right, y DOWN (v grows with image rows), z toward the viewer,
       encoded as rgb * 0.5 + 0.5. No green-channel flip.
  A    the relief mask the shaders multiply the lights by (0..1):
       smoothstep(RELIEF_MIN, RELIEF_MAX) over a *blurred* slope. Baking it
       matters: the height field pins to 0/1 where the art is dark, which
       leaves perfectly flat plateaus, and the slope collapses to ~0 in a
       single pixel at their border. Computing the mask from the raw slope in
       the shader flips it 0<->1 across that one pixel -- a hard edge that
       traces the art's contours (visible on the flanks of the Harvest
       face). Blurring the slope first turns the cliff into a ramp.

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
BLUR_DETAIL = 1.6      # px sigma for the fine height scale
BLUR_BROAD = 3.6       # px sigma for the broad forms
DETAIL_WEIGHT = 0.35   # weight of the fine scale vs the broad one
TARGET_SLOPE = 1.0     # p98 of |grad height| is scaled to this slope

# Relief mask, baked into the normal maps' alpha channel (see normal_map()).
# The slope is blurred first: where the height field pins to 0/1, or where the
# art itself has a hard edge, the slope collapses in a single pixel, and a mask
# taken from the raw slope flips 0<->1 there -- a hard edge that traces the
# art's contours. RELIEF_BLUR turns those cliffs into ramps.
RELIEF_MIN = 0.07    # slope below this is flat: stays at ambient
RELIEF_MAX = 0.58    # slope above this is lit at full strength
RELIEF_BLUR = 5.0    # px sigma the slope is blurred by before the mask

# ---- moving-light constants; keep in sync with assets/depth_fog*.fs ----
AMBIENT = 1.10       # what flat art is multiplied by
LIGHT_HEIGHT = 0.35  # grazing light height, shared by all three lights
FLASH_GAIN = 1.00    # thunder-spike lift; the decay envelope is C-side
SPEC_POWER = 28.0
SPEC_GAIN = 0.40
ASPECT = 16.0 / 9.0
# (path(t) -> (u, v) centre, radius, gain, tint)
LIGHTS = [
    (lambda t: (0.50 + 0.42 * math.sin(t * 0.100),
                0.30 + 0.18 * math.sin(t * 0.067 + 1.7)),
     0.62, 3.20, (1.00, 0.97, 0.90)),
    (lambda t: (0.22 + 0.26 * math.cos(t * 0.073 + 0.6),
                0.52 + 0.24 * math.sin(t * 0.073)),
     0.55, 2.80, (0.90, 0.96, 1.00)),
    (lambda t: (0.80 + 0.22 * math.sin(t * 0.055 + 2.9),
                0.42 + 0.28 * math.cos(t * 0.049 + 1.1)),
     0.58, 2.60, (1.00, 0.93, 0.88)),
]
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
    nx, ny, nz = -gx * s, -gy * s, np.ones_like(h)
    inv = 1.0 / np.sqrt(nx * nx + ny * ny + nz * nz)
    # Alpha: the relief mask, computed from a blurred slope so that art edges
    # -- which become one-pixel cliffs where the height field pins to 0/1 --
    # ramp instead of flipping the mask in a single pixel.
    relief = relief_mask(gaussian_blur(mag * s, RELIEF_BLUR))
    out = np.zeros(h.shape + (4,), dtype=np.uint8)
    out[..., 0] = np.clip((nx * inv * 0.5 + 0.5) * 255.0, 0, 255)
    out[..., 1] = np.clip((ny * inv * 0.5 + 0.5) * 255.0, 0, 255)
    out[..., 2] = np.clip((nz * inv * 0.5 + 0.5) * 255.0, 0, 255)
    out[..., 3] = np.clip(relief * 255.0, 0, 255)
    return out


def gaussian_blur(a: np.ndarray, sigma: float) -> np.ndarray:
    """Separable gaussian blur with edge padding (numpy only: PIL's
    GaussianBlur cannot handle float arrays)."""
    if sigma <= 0.0:
        return a
    radius = max(1, int(round(sigma * 3.0)))
    x = np.arange(-radius, radius + 1, dtype=np.float64)
    kernel = np.exp(-0.5 * (x / sigma) ** 2)
    kernel /= kernel.sum()
    out = a
    for axis in (0, 1):
        pad = [(0, 0)] * out.ndim
        pad[axis] = (radius, radius)
        padded = np.pad(out, pad, mode="edge")
        acc = np.zeros_like(out)
        for i in range(kernel.size):
            src = [slice(None)] * out.ndim
            src[axis] = slice(i, i + out.shape[axis])
            acc += kernel[i] * padded[tuple(src)]
        out = acc
    return out


def relief_mask(slope: np.ndarray) -> np.ndarray:
    """Smoothstep mask from RELIEF_MIN to RELIEF_MAX over a slope field."""
    ramp = np.clip((slope - RELIEF_MIN) / max(RELIEF_MAX - RELIEF_MIN, 1e-6), 0.0, 1.0)
    return ramp * ramp * (3.0 - 2.0 * ramp)


def render_lights(normal_rgba: np.ndarray, base: np.ndarray, t: float,
                  flash: float = 0.0) -> np.ndarray:
    """Replicate the depth_fog moving-light block on an RGB float image.

    `flash` is the capture / Koi-Koi spike amount (0 at rest, 1 at the peak).
    Its decay envelope lives in src/ui_board.c, so pass the amount you want
    to preview; this only mirrors the shader's FLASH_GAIN conversion.
    """
    u = (np.arange(WIDTH, dtype=np.float64) + 0.5) / WIDTH
    v = (np.arange(HEIGHT, dtype=np.float64) + 0.5) / HEIGHT
    uu, vv = np.meshgrid(u, v)
    nx = normal_rgba[..., 0] / 255.0 * 2.0 - 1.0
    ny = normal_rgba[..., 1] / 255.0 * 2.0 - 1.0
    nz = normal_rgba[..., 2] / 255.0 * 2.0 - 1.0
    inv = 1.0 / np.sqrt(nx * nx + ny * ny + nz * nz)
    nx, ny, nz = nx * inv, ny * inv, nz * inv
    relief = normal_rgba[..., 3] / 255.0  # baked mask, as the shader samples it
    flash_gain = 1.0 + FLASH_GAIN * flash
    lit = np.full((HEIGHT, WIDTH, 3), AMBIENT * flash_gain, dtype=np.float64)
    for path, radius, gain, tint in LIGHTS:
        lu, lv = path(t)
        dx = (lu - uu) * ASPECT
        dy = lv - vv
        d2 = dx * dx + dy * dy
        inv_len = 1.0 / np.sqrt(d2 + LIGHT_HEIGHT * LIGHT_HEIGHT)
        lx, ly, lz = dx * inv_len, dy * inv_len, LIGHT_HEIGHT * inv_len
        # Relief response only: a flat normal contributes exactly zero, so
        # the pools land on the bumps instead of washing the whole frame.
        diff = np.clip(nx * lx + ny * ly, 0.0, None)
        # half-vector with view = +z
        hx, hy, hz = lx, ly, lz + 1.0
        inv_h = 1.0 / np.sqrt(hx * hx + hy * hy + hz * hz)
        spec = np.clip(nx * hx * inv_h + ny * hy * inv_h + nz * hz * inv_h,
                       0.0, None) ** SPEC_POWER
        atten = 1.0 / (1.0 + d2 / (radius * radius))
        contrib = relief * flash_gain * (diff + SPEC_GAIN * spec) * atten * gain
        for c in range(3):
            lit[..., c] += contrib * tint[c]
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
