#!/usr/bin/env python3
"""Table art: cut units out, size their borders to the game's rects, compose, assert.

This is WORKFLOW.md steps 6-7 as code. Manifest driven: one table = one folder under `tables/`,
and everything built for it stays in that folder.

    tables/table_city/
        README.md      the spec
        table.json     the manifest  <- this tool reads it
        raw/           generator output
        final/         cut out, borders sized, interior == the game's rect   (written)
        preview/       composites + the assertion table                     (written)
        refs/          examples of the direction

    python3 tools/table_art.py build tables/table_city/table.json
    python3 tools/table_art.py build table_city        # same, folder name is enough
    python3 tools/table_art.py cut raw.png out.png --rect 255x196 --border 12,12,44,12

A manifest (schema and a filled example in `tables/_template/table.json`):

    {
      "design": "table_city",                  // optional, defaults to the folder name
      "backdrop": "assets/city.jpg",
      "units": [ {"file": "city-frame-block", "rect": [255,196],
                  "border": [12,12,44,12], "place": [[12,12],[12,508]]} ],
      "rails": [ {"from": "city-frame-block", "size": [255,8], "place": [[12,40],[12,670]]} ],
      "cards": [ {"file": "city-card-frame", "size": [76,124], "border": 5, "kind": "field"} ],
      "decor": [ {"file": "city-edge-bracket", "box": [0,176,78,372]} ]
    }

A `"method": "cut-and-place"` manifest (owner-delivered art) builds differently -
crop + one uniform downscale + translation, never stamp(). See
tables/WORKFLOW.md -> Delivered art.

`raw`, `final` and `preview` default to subfolders of the manifest's own folder; override with a
`"raw"` key if a table ever needs to point elsewhere.

The rules it enforces, because each of them was violated once:
  * a frame's open interior equals the game's rect, +-2 px, or the unit is reported MISMATCH
  * borders are asymmetric where the free space is, rather than crushed to the tightest side
  * the interior is cleared to alpha 0 (a render keeps a wash that darkens the game's text)
  * previews are composited on tables/base.png / a base drawn from live geometry, never on
    docs/assets/readme/table-map.png (dead 96 px layout - see tables/anchors.md)
"""
import json
import os
import sys
from collections import deque

import numpy as np
from PIL import Image, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)

import table_map  # noqa: E402  (one definition of live geometry, shared with the map)

ANCH = table_map.ANCH

# ---------------------------------------------------------------- cut-out recipe (WORKFLOW step 6)
SEED, CLOSE_PX, FEATHER, FILL_MAX = 26.0, 13, 0.7, 90
MUL, SAT, TINT, LIFT = 0.86, 0.78, (1.00, 0.96, 0.86), 10.0


def cutout(im):
    """Solid silhouette: opaque metal, transparent background and openings."""
    a = np.asarray(im.convert("RGB"), dtype=np.float32)
    seed = (a.max(axis=2) > SEED).astype(np.uint8) * 255
    m = Image.fromarray(seed, "L")
    m = m.filter(ImageFilter.MaxFilter(CLOSE_PX)).filter(ImageFilter.MinFilter(CLOSE_PX))
    m = m.filter(ImageFilter.GaussianBlur(FEATHER))
    alpha = np.asarray(m, dtype=np.uint8).copy()
    alpha = np.where(alpha > 250, 255, np.where(alpha < 5, 0, alpha)).astype(np.uint8)

    free = alpha < 128
    h, w = free.shape
    seen = np.zeros_like(free)
    for y in range(h):
        for x in range(w):
            if not free[y, x] or seen[y, x]:
                continue
            q, pts, edge = deque([(y, x)]), [], False
            seen[y, x] = True
            while q:
                cy, cx = q.popleft()
                pts.append((cy, cx))
                if cy in (0, h - 1) or cx in (0, w - 1):
                    edge = True
                for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    ny, nx = cy + dy, cx + dx
                    if 0 <= ny < h and 0 <= nx < w and free[ny, nx] and not seen[ny, nx]:
                        seen[ny, nx] = True
                        q.append((ny, nx))
            if not edge and len(pts) < FILL_MAX:
                for cy, cx in pts:
                    alpha[cy, cx] = 255
    return Image.fromarray(np.dstack([a.astype(np.uint8), alpha]), "RGBA")


def grade(im):
    a = np.asarray(im, dtype=np.float32)
    rgb, al = a[..., :3], a[..., 3:]
    g = rgb.mean(axis=2, keepdims=True)
    rgb = np.clip((g + (rgb - g) * SAT) * MUL * np.array(TINT, dtype=np.float32) + LIFT, 0, 255)
    return Image.fromarray(np.dstack([rgb, al]).astype("uint8"), "RGBA")


def trim(im, thr=9):
    g = np.asarray(im.convert("L"), dtype=np.float32)
    ys, xs = np.where(g > thr)
    return im.crop((int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1))


def prepare(path):
    return trim(grade(cutout(Image.open(path))))


# ---------------------------------------------------------------- measurement
def interior_hole(im, min_area=400):
    """Largest fully transparent region that does not touch the image edge -> (x, y, w, h)."""
    a = np.asarray(im)[..., 3]
    free = (a < 12).astype(np.uint8)
    h, w = free.shape
    seen = np.zeros_like(free)
    best = None
    for y in range(h):
        for x in range(w):
            if not free[y, x] or seen[y, x]:
                continue
            q, pts, edge = deque([(y, x)]), [], False
            seen[y, x] = 1
            while q:
                cy, cx = q.popleft()
                pts.append((cy, cx))
                if cy in (0, h - 1) or cx in (0, w - 1):
                    edge = True
                for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    ny, nx = cy + dy, cx + dx
                    if 0 <= ny < h and 0 <= nx < w and free[ny, nx] and not seen[ny, nx]:
                        seen[ny, nx] = 1
                        q.append((ny, nx))
            if edge or len(pts) < min_area:
                continue
            ys = [p[0] for p in pts]
            xs = [p[1] for p in pts]
            cand = (min(xs), min(ys), max(xs) - min(xs) + 1, max(ys) - min(ys) + 1)
            if best is None or cand[2] * cand[3] > best[2] * best[3]:
                best = cand
    return best


def border_thickness(im, hole):
    """(L, T, R, B) border thickness around the interior hole."""
    hx, hy, hw, hh = hole
    return hx, hy, im.width - (hx + hw), im.height - (hy + hh)


def fit_border(im, target_t):
    hole = interior_hole(im)
    if not hole:
        return im, None
    L, T, R, B = border_thickness(im, hole)
    cur = max(1.0, min(L, T, R, B))
    k = target_t / cur
    return im.resize((max(8, int(round(im.width * k))), max(8, int(round(im.height * k)))),
                     Image.LANCZOS), k


def stamp(im, rect, border_t):
    """Build a unit whose interior is exactly `rect` and whose border is outside it.

    `border_t` is one number (uniform) or (L, T, R, B). Asymmetric matters: the right column
    has 80 px of free space on its left and 8 px on its right, so a uniform border is forced
    down to 8 and reads thin beside the other panels.
    """
    targets = (border_t,) * 4 if not isinstance(border_t, (list, tuple)) else tuple(border_t)
    im, _ = fit_border(im, min(targets))
    hole = interior_hole(im)
    if not hole:
        raise SystemExit("unit has no open interior - regenerate it as ONE empty frame")
    w, h = rect
    L, T, R, B = targets
    hx, hy, hw, hh = hole
    out = Image.new("RGBA", (w + L + R, h + T + B), (0, 0, 0, 0))

    out.alpha_composite(im.crop((0, 0, hx, hy)).resize((L, T), Image.LANCZOS), (0, 0))
    out.alpha_composite(im.crop((hx + hw, 0, im.width, hy)).resize((R, T), Image.LANCZOS), (w + L, 0))
    out.alpha_composite(im.crop((0, hy + hh, hx, im.height)).resize((L, B), Image.LANCZOS), (0, h + T))
    out.alpha_composite(im.crop((hx + hw, hy + hh, im.width, im.height)).resize((R, B), Image.LANCZOS),
                        (w + L, h + T))
    out.alpha_composite(im.crop((hx, 0, hx + hw, hy)).resize((w, T), Image.LANCZOS), (L, 0))
    out.alpha_composite(im.crop((hx, hy + hh, hx + hw, im.height)).resize((w, B), Image.LANCZOS), (L, h + T))
    out.alpha_composite(im.crop((0, hy, hx, hy + hh)).resize((L, h), Image.LANCZOS), (0, T))
    out.alpha_composite(im.crop((hx + hw, hy, im.width, hy + hh)).resize((R, h), Image.LANCZOS),
                        (w + L, T))

    # The opening must be EMPTY: a render keeps a faint wash inside its own frame, and that reads
    # as a dark overlay on the game's text. First pass hit exactly this.
    arr = np.asarray(out).copy()
    arr[T:T + h, L:L + w, 3] = 0
    return Image.fromarray(arr, "RGBA")


def card_frame(im, w, h, t):
    """Bezel drawn INSIDE the card rect so it can never overlap a neighbour."""
    im = prepare(im) if isinstance(im, str) else im
    hole = interior_hole(im)
    if not hole:
        raise SystemExit("card unit has no open interior")
    L, T, R, B = border_thickness(im, hole)
    k = t / max(1.0, min(L, T, R, B))
    im = im.resize((max(8, int(round(im.width * k))), max(8, int(round(im.height * k)))), Image.LANCZOS)
    hole = interior_hole(im)
    L, T, R, B = border_thickness(im, hole)
    hx, hy, hw, hh = hole
    out = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    out.alpha_composite(im.crop((0, 0, hx, hy)), (0, 0))
    out.alpha_composite(im.crop((hx + hw, 0, im.width, hy)), (w - R, 0))
    out.alpha_composite(im.crop((0, hy + hh, hx, im.height)), (0, h - B))
    out.alpha_composite(im.crop((hx + hw, hy + hh, im.width, im.height)), (w - R, h - B))
    out.alpha_composite(im.crop((hx, 0, hx + hw, hy)).resize((w - L - R, T), Image.LANCZOS), (L, 0))
    out.alpha_composite(im.crop((hx, hy + hh, hx + hw, im.height)).resize((w - L - R, B), Image.LANCZOS),
                        (L, h - B))
    out.alpha_composite(im.crop((0, hy, hx, hy + hh)).resize((L, h - T - B), Image.LANCZOS), (0, T))
    out.alpha_composite(im.crop((hx + hw, hy, im.width, hy + hh)).resize((R, h - T - B), Image.LANCZOS),
                        (w - R, T))
    arr = np.asarray(out).copy()
    arr[T:h - B, L:w - R, 3] = 0
    return Image.fromarray(arr, "RGBA")


def card_rects(kind):
    """Card rectangles, from the rules in tables/anchors.json - not from a picture."""
    if kind == "field":                       # eight cards land in columns 2..5, both rows
        return [(434 + c * 81, y, 76, 124) for y in (170, 310) for c in (2, 3, 4, 5)]
    if kind == "hand":
        return [(330 + i * 96, 566, 88, 143) for i in range(8)]
    if kind == "cpu":
        return [(465 + i * 62, 12, 56, 91) for i in range(8)]
    if kind == "lane_mini":                   # 17x31 at x 54 - too small for a bezel, listed only
        return [(54 + i * 24, 0, 17.324, 30.652) for i in range(6)]
    raise SystemExit(f"unknown card kind {kind!r} - expected field / hand / cpu / lane_mini")


# ---------------------------------------------------------------- build
def build_cut_and_place(spec, folder):
    """Fit owner-delivered art (WORKFLOW.md -> Delivered art): crop each recorded
    compartment at native resolution, ONE uniform downscale to canvas space (plus
    the compartment's recorded uniform scale), translate its measured cavity onto
    the anchor. No 9-slice, no non-uniform resizing. Panel units composite into
    one static <design>_overlay.png (they never move); bezels stay separate at
    native resolution (they travel with their card) and their cavity-in-texture
    constants land in _manifest.json for the game code to mirror."""
    design = spec.get("design") or os.path.basename(folder)
    raw = os.path.join(ROOT, spec["raw"]) if spec.get("raw") else os.path.join(folder, "raw")
    final = os.path.join(folder, "final")
    os.makedirs(final, exist_ok=True)
    W, H = ANCH["canvas"]["w"], ANCH["canvas"]["h"]
    overlay = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    report, out_manifest, pieces = [], {}, {}

    def piece(fname):
        if fname not in pieces:
            pieces[fname] = Image.open(os.path.join(raw, fname)).convert("RGBA")
        return pieces[fname]

    for c in spec.get("compartments", []):
        im = piece(c["file"])
        ns = im.width / W                       # native delivery scale (1920/1280 = 1.5)
        if abs(im.height / H - ns) > 0.01:
            raise SystemExit(f"{c['file']}: not a uniform multiple of the canvas ({im.size})")
        us = float(c.get("uniform_scale", 1.0))
        crop = [round(v * ns) for v in c["crop"]]          # the record is canvas-space
        if crop[0] < 0 or crop[1] < 0 or crop[2] > im.width or crop[3] > im.height:
            raise SystemExit(f"{c['name']}: crop {crop} outside {c['file']} {im.size}")
        unit = im.crop(tuple(crop))
        cav, corp = c["cavity_measured"], c["crop"]
        if c.get("group") == "bezel":
            k = us                                          # native res; the game scales
            if k != 1.0:
                unit = unit.resize((round(unit.width * k), round(unit.height * k)), Image.LANCZOS)
            cav_px = [(cav[0] - corp[0]) * ns * k, (cav[1] - corp[1]) * ns * k,
                      cav[2] * ns * k, cav[3] * ns * k]
            # Hollow frame: flood-clear the dark leather INTERIOR from the cavity
            # centre so only the border survives - corner ornaments that sweep
            # inside the cavity rect included. (First pass cleared the rectangle
            # and chopped those ornaments to a hairline; a blur on top ate the
            # rails. WORKFLOW trap 17.) Cleared pixels become (0,0,0,0): no
            # leather RGB left under the transparency to bleed through mipmaps.
            arr = np.array(unit)
            val = arr[..., :3].max(axis=2)
            fillable = (arr[..., 3] > 0) & (val < 48)
            cx = min(max(round(cav_px[0] + cav_px[2] / 2), 0), unit.width - 1)
            cy = min(max(round(cav_px[1] + cav_px[3] / 2), 0), unit.height - 1)
            if not fillable[cy, cx]:
                raise SystemExit(f"{c['name']}: cavity centre ({cx},{cy}) is not dark "
                                 f"interior (value {val[cy, cx]}) - cannot flood-clear")

            def flood(seeds, passable):
                seen = np.zeros(val.shape, bool)
                dq = deque()
                for sx, sy in seeds:
                    if passable[sy, sx] and not seen[sy, sx]:
                        seen[sy, sx] = True
                        dq.append((sx, sy))
                while dq:
                    x, y = dq.popleft()
                    for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                        if 0 <= nx < unit.width and 0 <= ny < unit.height \
                                and not seen[ny, nx] and passable[ny, nx]:
                            seen[ny, nx] = True
                            dq.append((nx, ny))
                return seen

            inside = flood([(cx, cy)], fillable)
            # Bright sheen specks enclosed by the cleared interior clear too:
            # anything not reachable from the image border without crossing it.
            border_seeds = [(x, 0) for x in range(unit.width)] + \
                           [(x, unit.height - 1) for x in range(unit.width)] + \
                           [(0, y) for y in range(unit.height)] + \
                           [(unit.width - 1, y) for y in range(unit.height)]
            outside = flood(border_seeds, ~inside)
            clear = inside | (~inside & ~outside)
            # Never clear beyond the measured cavity (+6 px slop): a leak through
            # a dark rail gap must not eat the border's outer shadow.
            x0 = max(round(cav_px[0]) - 6, 0)
            y0 = max(round(cav_px[1]) - 6, 0)
            x1 = min(round(cav_px[0] + cav_px[2]) + 6, unit.width)
            y1 = min(round(cav_px[1] + cav_px[3]) + 6, unit.height)
            box = np.zeros(val.shape, bool)
            box[y0:y1, x0:x1] = True
            arr[clear & box] = (0, 0, 0, 0)
            unit = Image.fromarray(arr, "RGBA")
            out_manifest[c["name"]] = {"size": [unit.width, unit.height],
                                       "cavity_px": [round(v, 2) for v in cav_px],
                                       "cavity_cleared": True,
                                       "card_size": c.get("card_size")}
            status, target = "ok", f"card {c['card_size'][0]}x{c['card_size'][1]}"
            inside = f"cav {cav_px[2]:.0f}x{cav_px[3]:.0f}px"
        else:
            k = us / ns                                     # one uniform downscale to canvas
            unit = unit.resize((round(unit.width * k), round(unit.height * k)), Image.LANCZOS)
            px = round(c["cavity_placed"][0] - (cav[0] - corp[0]) * us)
            py = round(c["cavity_placed"][1] - (cav[1] - corp[1]) * us)
            overlay.alpha_composite(unit, (px, py))
            outer = c["outer_placed"]
            on = outer[0] >= 0 and outer[1] >= 0 and outer[0] + outer[2] <= W and outer[1] + outer[3] <= H
            anc = c["anchor"]
            sd = (anc[2] - cav[2] * us, anc[3] - cav[3] * us)
            out_manifest[c["name"]] = {"size": [unit.width, unit.height], "place": [px, py],
                                       "cavity_placed": c["cavity_placed"], "anchor": anc}
            status = "ok" if on else "OFF-CANVAS"
            target = f"{anc[0]:.0f},{anc[1]:.0f} {anc[2]:.0f}x{anc[3]:.0f}"
            inside = f"sd ({sd[0]:+.1f},{sd[1]:+.1f})"
        unit.save(os.path.join(final, c["name"] + ".png"))
        report.append((c["name"], f"{unit.width}x{unit.height}", inside, target, status))

    overlay.save(os.path.join(final, design + "_overlay.png"))
    json.dump(out_manifest, open(os.path.join(final, "_manifest.json"), "w"), indent=1)
    return report, final


def build(manifest_path, backdrop=None):
    if not os.path.isabs(manifest_path):
        tries = [os.path.join(ROOT, manifest_path),
                 os.path.join(ROOT, "tables", manifest_path, "table.json"),
                 os.path.join(ROOT, "tables", manifest_path + ".json")]
        manifest_path = next((c for c in tries if os.path.exists(c)), tries[0])
    if not os.path.exists(manifest_path):
        raise SystemExit(f"no manifest at {manifest_path} (a table folder is tables/<name>/table.json)")
    spec = json.load(open(manifest_path))
    if spec.get("method") == "cut-and-place":
        return build_cut_and_place(spec, os.path.dirname(os.path.abspath(manifest_path)))
    if not any(spec.get(k) for k in ("units", "decor", "rails", "cards")):
        raise SystemExit("this manifest builds nothing (no units/decor/rails/cards); "
                         "refusing to report an empty build as 0 mismatches")
    folder = os.path.dirname(os.path.abspath(manifest_path))
    design = spec.get("design") or os.path.basename(folder)
    raw = os.path.join(ROOT, spec["raw"]) if spec.get("raw") else os.path.join(folder, "raw")
    final, preview = os.path.join(folder, "final"), os.path.join(folder, "preview")
    os.makedirs(final, exist_ok=True)
    os.makedirs(preview, exist_ok=True)
    bd = backdrop or spec.get("backdrop")

    def base(with_cards):
        src = os.path.join(preview, "_tmp_base.png")
        table_map.base(src, os.path.basename(bd) if bd else None, cards=with_cards)
        im = Image.open(src).convert("RGBA")
        os.remove(src)
        return im

    geo = base(False)      # empty rects: the clean read on placement
    art = base(True)       # real cards: the honest read on how it looks
    report, out_manifest = [], {}

    for d in spec.get("decor", []):
        im = prepare(os.path.join(raw, d["file"] + ".png"))
        box = d["box"]
        im = im.resize((box[2], box[3]), Image.LANCZOS)
        im.save(os.path.join(final, d["file"] + ".png"))
        for canvas in (geo, art):
            canvas.alpha_composite(im, (box[0], box[1]))
        out_manifest[d["file"]] = {"decor": box}
        report.append((d["file"], f"{box[2]}x{box[3]}", "decor", f"{box[0]},{box[1]}", "ok"))

    for u in spec.get("units", []):
        unit = stamp(prepare(os.path.join(raw, u["file"] + ".png")), u["rect"], u["border"])
        unit.save(os.path.join(final, u["file"] + ".png"))
        hole = interior_hole(unit)
        dx = abs(hole[2] - u["rect"][0]) if hole else 999
        dy = abs(hole[3] - u["rect"][1]) if hole else 999
        ok = dx <= 2 and dy <= 2
        for (px, py) in u["place"]:
            for canvas in (geo, art):
                canvas.alpha_composite(unit, (int(px - hole[0]), int(py - hole[1])))
        out_manifest[u["file"]] = {"size": [unit.width, unit.height], "interior": list(hole),
                                   "rect": u["rect"], "border": u["border"],
                                   "place": u["place"]}
        report.append((u["file"], f"{unit.width}x{unit.height}", f"{hole[2]}x{hole[3]}",
                       f"target {u['rect'][0]}x{u['rect'][1]}",
                       "ok" if ok else "MISMATCH"))

    for r in spec.get("rails", []):
        src = prepare(os.path.join(raw, r["from"] + ".png"))
        hole = interior_hole(src)
        hx, hy, hw, hh = hole
        rail = src.crop((0, hy + hh - 2, src.width, src.height))
        rw, rh = r["size"]
        rail = rail.resize((rw, rh), Image.LANCZOS)
        name = f"{r.get('name', r['from'].replace('frame', 'rail'))}-{rw}x{rh}"
        rail.save(os.path.join(final, name + ".png"))
        for (px, py) in r["place"]:
            for canvas in (geo, art):          # rails last: the casing must not cover the seam
                canvas.alpha_composite(rail, (px, py))
        out_manifest[name] = {"size": [rw, rh], "place": r["place"]}
        report.append((name, f"{rw}x{rh}", "divider", str(r["place"]), "ok"))

    for c in spec.get("cards", []):
        cw, ch = c["size"]
        cf = card_frame(prepare(os.path.join(raw, c["file"] + ".png")), cw, ch, c["border"])
        name = f"{c['file']}-{cw}x{ch}"
        cf.save(os.path.join(final, name + ".png"))
        rects = card_rects(c["kind"])
        for rr in rects:
            for canvas in (geo, art):
                canvas.alpha_composite(cf, (int(round(rr[0])), int(round(rr[1]))))
        out_manifest[name] = {"size": [cw, ch], "border": c["border"], "kind": c["kind"],
                              "rects": len(rects)}
        report.append((name, f"{cw}x{ch}", f"inner {cw - 2 * c['border']}x{ch - 2 * c['border']}",
                       c["kind"], "ok"))

    geo.convert("RGB").save(os.path.join(preview, design + "-geometry.png"))
    art.convert("RGB").save(os.path.join(preview, design + "-table.png"))
    json.dump(out_manifest, open(os.path.join(final, "_manifest.json"), "w"), indent=1)
    return report, preview


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    cmd = sys.argv[1]
    if cmd == "cut":
        src, dst = sys.argv[2], sys.argv[3]
        rect = [int(v) for v in sys.argv[sys.argv.index("--rect") + 1].split("x")]
        bt = sys.argv[sys.argv.index("--border") + 1] if "--border" in sys.argv else "8"
        border = [int(v) for v in bt.split(",")]
        border = border if len(border) == 4 else border[0]
        out = stamp(prepare(src), rect, border)
        out.save(dst)
        print(f"wrote {dst}  {out.width}x{out.height}  interior {rect[0]}x{rect[1]}")
        return 0
    if cmd != "build":
        raise SystemExit(__doc__)

    target = sys.argv[2]
    bd = sys.argv[sys.argv.index("--backdrop") + 1] if "--backdrop" in sys.argv else None
    report, prev = build(target, bd)
    w = max(len(r[0]) for r in report) if report else 4
    print(f"{'unit'.ljust(w)}  {'final':>10}  {'interior':>10}  {'target':>16}  status")
    for row in report:
        print(f"{row[0].ljust(w)}  {row[1]:>10}  {row[2]:>10}  {row[3]:>16}  {row[4]}")
    bad = [r for r in report if r[4] != "ok"]
    print(f"\n{len(bad)} mismatch(es)\n{os.path.relpath(prev, ROOT)}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
