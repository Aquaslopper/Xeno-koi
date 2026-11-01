#!/usr/bin/env python3
"""Bake the CRT casing pair for a forged table.

Reads assets/maps/<name>/overlay.png and bezel.png, writes crt_overlay.png and
crt_bezel.png beside them. Then the in-game CRT CASING switch just picks the other
texture - no shader, so the result is identical on desktop and on the web build.

Why not a shader: a hue shift keeps the painted metal. The casing has to stop being
painted volume and become glowing stroke, and that is an edge extraction, not a colour
operation. Deriving it from the map's own art means the silhouette, the ornaments and
the card cavity are the map's, exactly as the artist drew them.

Pipeline, per pixel:
  1. luminance, then a 3x3 Sobel magnitude  -> where the art has an edge
  2. normalise, cut the soft floor, gamma    -> strokes only, no haze
  3. three separable box blurs               -> the glow around each stroke
  4. a phosphor ramp                         -> teal-green CRT tone
  5. alpha: strokes solid, interiors hollow with a dark floor, source alpha respected

The floor matters: the game draws live text and card art on top of the casing, so a
fully hollow interior would put phosphor on top of the background image and make the
score strings unreadable. A ~30% dark floor keeps them legible while still letting the
background show through, which is what makes it read as a CRT rather than a painted box.

Geometry is never resampled, so the measured card cavity in k_bezel_cav[] (ui_board.c)
stays correct for the CRT bezel too.

The baked pairs are hand-touchable: they are ordinary PNGs and the owner has already
painted over them once. So the tool NEVER overwrites an existing pair unless --force is
passed. A plain run fills in gaps and leaves finished art alone.

Usage:
  python3 tools/crt_casing.py             # fill in missing pairs, keep existing art
  python3 tools/crt_casing.py castle      # one or more maps by folder name
  python3 tools/crt_casing.py --force     # regenerate, destroying hand work
  python3 tools/crt_casing.py --check     # exit 1 if a pair is missing
"""
import math
import os
import struct
import sys
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAPS = os.path.join(ROOT, "assets", "maps")

# Phosphor ramp: (position, r, g, b) in 0..1, sampled between neighbours.
RAMP = [
    (0.00, 0.020, 0.100, 0.100),
    (0.28, 0.050, 0.420, 0.420),
    (0.60, 0.180, 0.780, 0.750),
    (0.85, 0.500, 0.960, 0.930),
    (1.00, 0.880, 1.000, 0.990),
]
EDGE_FLOOR = 0.055      # Sobel values below this are noise, not casing
EDGE_GAMMA = 0.72       # >0.5 lifts mid-strength edges so thin rails survive
GLOW = (0.85, 0.50, 0.28)          # weight per octave
GLOW_RADII = (3, 10, 26)
HOLLOW_FLOOR = 78.0     # 0..255 dark floor under live text, scaled by source alpha


def read_png(path):
    data = open(path, "rb").read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit("%s is not a PNG" % path)
    pos, idat, w, h, depth, ctype = 8, b"", 0, 0, 0, 0
    while pos < len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        kind = data[pos + 4:pos + 8]
        chunk = data[pos + 8:pos + 8 + length]
        if kind == b"IHDR":
            w, h, depth, ctype = struct.unpack(">IIBB", chunk[:10])
        elif kind == b"IDAT":
            idat += chunk
        pos += 12 + length
    if depth != 8 or ctype not in (2, 6):
        raise SystemExit("%s: need 8-bit RGB or RGBA" % path)
    bpp = 4 if ctype == 6 else 3
    stride = w * bpp
    raw = zlib.decompress(idat)
    out = bytearray(h * stride)
    prev = bytearray(stride)
    src = 0
    for y in range(h):
        ft = raw[src]
        src += 1
        line = bytearray(raw[src:src + stride])
        src += stride
        if ft == 1:
            for x in range(bpp, stride):
                line[x] = (line[x] + line[x - bpp]) & 255
        elif ft == 2:
            for x in range(stride):
                line[x] = (line[x] + prev[x]) & 255
        elif ft == 3:
            for x in range(stride):
                a = line[x - bpp] if x >= bpp else 0
                line[x] = (line[x] + ((a + prev[x]) >> 1)) & 255
        elif ft == 4:
            for x in range(stride):
                a = line[x - bpp] if x >= bpp else 0
                b = prev[x]
                c = prev[x - bpp] if x >= bpp else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[x] = (line[x] + pr) & 255
        out[y * stride:(y + 1) * stride] = line
        prev = line
    return w, h, bpp, out


def write_png(path, w, h, buf):
    def chunk(kind, payload):
        return (struct.pack(">I", len(payload)) + kind + payload +
                struct.pack(">I", zlib.crc32(kind + payload) & 0xffffffff))
    raw = bytearray()
    stride = w * 4
    for y in range(h):
        raw.append(0)
        raw += buf[y * stride:(y + 1) * stride]
    blob = (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(bytes(raw), 9)) +
            chunk(b"IEND", b""))
    with open(path, "wb") as fh:
        fh.write(blob)


def box_blur(src, w, h, radius):
    """Separable box blur with running sums - O(1) per pixel."""
    tmp = [0.0] * (w * h)
    out = [0.0] * (w * h)
    for y in range(h):
        row = y * w
        acc, count = 0.0, 0
        for x in range(w):
            if x == 0:
                for k in range(0, min(radius + 1, w)):
                    acc += src[row + k]
                count = min(radius + 1, w)
            else:
                add, rem = x + radius, x - radius - 1
                if add < w:
                    acc += src[row + add]
                    count += 1
                if rem >= 0:
                    acc -= src[row + rem]
                    count -= 1
            tmp[row + x] = acc / count if count else 0.0
    for x in range(w):
        acc, count = 0.0, 0
        for y in range(h):
            if y == 0:
                for k in range(0, min(radius + 1, h)):
                    acc += tmp[k * w + x]
                count = min(radius + 1, h)
            else:
                add, rem = y + radius, y - radius - 1
                if add < h:
                    acc += tmp[add * w + x]
                    count += 1
                if rem >= 0:
                    acc -= tmp[rem * w + x]
                    count -= 1
            out[y * w + x] = acc / count if count else 0.0
    return out


def ramp(value):
    if value <= 0.0:
        return 0.0, 0.0, 0.0
    value = min(value, 1.0)
    for i in range(len(RAMP) - 1):
        lo, hi = RAMP[i], RAMP[i + 1]
        if lo[0] <= value <= hi[0]:
            span = hi[0] - lo[0]
            k = (value - lo[0]) / span if span > 0 else 0.0
            return (lo[1] + (hi[1] - lo[1]) * k,
                    lo[2] + (hi[2] - lo[2]) * k,
                    lo[3] + (hi[3] - lo[3]) * k)
    return RAMP[-1][1:]


def bake(src_path, dst_path):
    w, h, bpp, px = read_png(src_path)
    n = w * h
    lum = [0.0] * n
    alpha = [0] * n
    for i in range(n):
        o = i * bpp
        lum[i] = (px[o] * 0.299 + px[o + 1] * 0.587 + px[o + 2] * 0.114) / 255.0
        alpha[i] = px[o + 3] if bpp == 4 else 255

    edge = [0.0] * n
    for y in range(1, h - 1):
        up, mid, dn = (y - 1) * w, y * w, (y + 1) * w
        for x in range(1, w - 1):
            gx = ((lum[up + x + 1] + 2 * lum[mid + x + 1] + lum[dn + x + 1]) -
                  (lum[up + x - 1] + 2 * lum[mid + x - 1] + lum[dn + x - 1]))
            gy = ((lum[dn + x - 1] + 2 * lum[dn + x] + lum[dn + x + 1]) -
                  (lum[up + x - 1] + 2 * lum[up + x] + lum[up + x + 1]))
            edge[mid + x] = math.sqrt(gx * gx + gy * gy)

    peak = max(edge) or 1.0
    for i in range(n):
        v = edge[i] / peak
        edge[i] = 0.0 if v < EDGE_FLOOR else ((v - EDGE_FLOOR) / (1.0 - EDGE_FLOOR)) ** EDGE_GAMMA

    octaves = [box_blur(edge, w, h, r) for r in GLOW_RADII]

    out = bytearray(n * 4)
    for i in range(n):
        intensity = edge[i]
        for octave, weight in zip(octaves, GLOW):
            intensity += octave[i] * weight
        r, g, b = ramp(intensity)
        stroke = min(1.0, intensity * 2.0)
        hollow = min(alpha[i] * 0.30, HOLLOW_FLOOR)
        a = max(stroke * 255.0, hollow)
        o = i * 4
        out[o] = int(min(255.0, r * 255.0))
        out[o + 1] = int(min(255.0, g * 255.0))
        out[o + 2] = int(min(255.0, b * 255.0))
        out[o + 3] = int(min(255.0, a))
    write_png(dst_path, w, h, out)
    return w, h


def map_dirs(argv):
    names = [a for a in argv if not a.startswith("-")]
    if names:
        return names
    return sorted(d for d in os.listdir(MAPS)
                  if os.path.isfile(os.path.join(MAPS, d, "overlay.png")))


def main(argv):
    check = "--check" in argv
    force = "--force" in argv
    missing = []
    kept = 0
    for name in map_dirs([a for a in argv if not a.startswith("-")]):
        folder = os.path.join(MAPS, name)
        for src_name, dst_name in (("overlay.png", "crt_overlay.png"),
                                   ("bezel.png", "crt_bezel.png")):
            src = os.path.join(folder, src_name)
            dst = os.path.join(folder, dst_name)
            if not os.path.isfile(src):
                continue
            exists = os.path.isfile(dst)
            if check:
                if not exists:
                    missing.append(os.path.relpath(dst, ROOT))
                continue
            if exists and not force:
                kept += 1        # hand-painted or already baked: not ours to overwrite
                continue
            w, h = bake(src, dst)
            print("%-8s %-14s %4dx%-4d -> %s" % (name, src_name, w, h,
                                                 os.path.relpath(dst, ROOT)))
    if check:
        if missing:
            print("missing CRT casing pairs (run tools/crt_casing.py to bake them):")
            for p in missing:
                print("  " + p)
            return 1
        print("Every map has its CRT casing pair.")
        return 0
    if kept:
        print("%d pair(s) already exist and were left alone (--force to regenerate)." % kept)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
