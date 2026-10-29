#!/usr/bin/env python3
"""Generators for tables/ - everything in that folder that is derived, is derived from here.

    python3 tools/table_map.py figure     -> tables/table-to-scale.png   (1:1 map + legend)
    python3 tools/table_map.py svg        -> tables/table-to-scale.svg   (same, vector)
    python3 tools/table_map.py base       -> tables/base.png             (composite base)
    python3 tools/table_map.py base --backdrop city -> tables/base-city.png
    python3 tools/table_map.py palettes   -> tables/palettes.{png,json}
    python3 tools/table_map.py check      -> assert anchors.json still matches src/
    python3 tools/table_map.py all

Geometry comes from tables/anchors.json only. Nothing here is measured from a screenshot, a
mockup or docs/assets/readme/table-map.png (that figure is drawn with dead code - see anchors.md).
"""
import json
import os
import re
import sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TAB = os.path.join(ROOT, "tables")
ANCH = json.load(open(os.path.join(TAB, "anchors.json")))

W, H = ANCH["canvas"]["w"], ANCH["canvas"]["h"]
BG = (10, 10, 12)
INK = (232, 208, 150)          # new art
INK_DIM = (150, 134, 100)
FILL = (24, 26, 30)
FILL_EDGE = (58, 62, 70)
CARD = (46, 50, 58)
CARD_EDGE = (96, 104, 116)
FREE = (90, 160, 150)          # free space
FROZEN = (200, 96, 96)

F_TITLE = ImageFont.truetype(os.path.join(ROOT, "assets", "audiowide.ttf"), 15)
F_BODY = ImageFont.truetype(os.path.join(ROOT, "assets", "play.ttf"), 12)
F_SMALL = ImageFont.truetype(os.path.join(ROOT, "assets", "play.ttf"), 10)
F_TINY = ImageFont.truetype(os.path.join(ROOT, "assets", "small_font.ttf"), 8)


# ---------------------------------------------------------------- geometry helpers
def field_slot(col, row, overflow=False):
    x = 434 + col * 81
    y = 170 if row == 0 else 310
    if overflow:
        x += 12
        y += 14
    return (x, y, 76, 124)


def hand_rect(i, n):
    step, w = 96.0, 88.0
    total = (n - 1) * step + w
    return (710.0 - total / 2.0 + i * step, 566.0, w, 143.0)


def cpu_rect(i, n):
    step, w = 62.0, 56.0
    total = (n - 1) * step + w
    return (710.0 - total / 2.0 + i * step, 12.0, w, 91.0)


def lane_rect(side, t):
    return (12.0, (46.0 if side == "cpu" else 508.0) + t * 41.4, 255.0, 37.4)


def mini_rect(side, t, c, count):
    y0 = (46.0 if side == "cpu" else 508.0) + t * 41.4
    ch = 30.652
    card_y = y0 + (37.4 - ch) / 2.0
    step = min(24.0, (255.0 - 54.0 - 17.324) / (count - 1)) if count > 1 else 0.0
    return (54.0 + c * step, card_y, 17.324, ch)


# ---------------------------------------------------------------- drawing helpers
def chip(d, xy, text, font=F_SMALL, fg=INK, bg=(8, 8, 10, 225), pad=2):
    x, y = xy
    tw = d.textlength(text, font=font)
    d.rectangle([x, y, x + tw + pad * 2, y + font.size + pad * 2 - 1], fill=bg)
    d.text((x + pad, y + pad - 1), text, font=font, fill=fg)


def label_rect(d, r, name, dims=True, colour=INK, side="tl", font=F_SMALL):
    x, y, w, h = r
    d.rectangle([x, y, x + w, y + h], outline=colour, width=1)
    text = f"{name}  {w:g}×{h:g}" if dims else name
    tw = d.textlength(text, font=font)
    th = font.size + 4
    if side == "tl":
        p = (x + 3, y + 3)
    elif side == "tr":
        p = (x + w - tw - 3, y + 3)
    elif side == "bl":
        p = (x + 3, y + h - th)
    else:
        p = (x + w - tw - 3, y + h - th)
    if w < tw + 8 and h < th + 6:          # too small to label inside -> label above
        p = (x, y - th)
    chip(d, p, text, font=font, fg=colour)


def dashed(d, r, colour=INK_DIM, dash=3, gap=3, width=1):
    x0, y0, w, h = r
    x1, y1 = x0 + w, y0 + h

    def seg(a, b, horiz, fixed):
        p = a
        while p < b:
            q = min(p + dash, b)
            if horiz:
                d.line([p, fixed, q, fixed], fill=colour, width=width)
            else:
                d.line([fixed, p, fixed, q], fill=colour, width=width)
            p = q + gap

    seg(x0, x1, True, y0)
    seg(x0, x1, True, y1)
    seg(y0, y1, False, x0)
    seg(y0, y1, False, x1)


def grid(d):
    for x in range(0, W, 40):
        d.line([x, 0, x, H], fill=(20, 21, 24))
    for y in range(0, H, 40):
        d.line([0, y, W, y], fill=(20, 21, 24))


# ---------------------------------------------------------------- the figure
def draw_map(d, base=None):
    """Everything the game draws, at 1:1. base = optional Image already on the canvas."""
    grid(d)

    # 1. panel fills: exactly what the game paints at rest
    for r in ([12, 12, 255, 30], [12, 678, 255, 30]):
        d.rectangle([r[0], r[1], r[0] + r[2], r[1] + r[3]], fill=FILL, outline=FILL_EDGE)
    for side in ("cpu", "player"):
        for t in range(4):
            r = lane_rect(side, t)
            d.rectangle([r[0], r[1], r[0] + r[2], r[1] + r[3]], fill=FILL, outline=FILL_EDGE)
    for r in ([1160, 12, 112, 66], [1160, 88, 112, 218], [1160, 316, 112, 218],
              [1160, 560, 112, 40], [1160, 610, 112, 40]):
        d.rectangle([r[0], r[1], r[0] + r[2], r[1] + r[3]], fill=FILL, outline=FILL_EDGE)

    # 2. cards
    for row in range(2):
        for col in range(8):
            r = field_slot(col, row)
            dashed(d, r, INK_DIM)
            chip(d, (r[0] + 2, r[1] + 2), f"{col + row * 8}", font=F_TINY, fg=INK_DIM,
                 bg=(8, 8, 10, 200))
    for i in range(4):                        # overflow slots of a 20-24 card field
        r = field_slot(2 + i, 0, overflow=True)
        dashed(d, r, (120, 84, 60))
    for i in range(4):
        r = field_slot(2 + i, 1, overflow=True)
        dashed(d, r, (120, 84, 60))
    for i in range(8):
        r = hand_rect(i, 8)
        d.rectangle([r[0], r[1], r[0] + r[2], r[1] + r[3]], fill=CARD, outline=CARD_EDGE)
    for i in range(8):
        r = cpu_rect(i, 8)
        d.rectangle([r[0], r[1], r[0] + r[2], r[1] + r[3]], fill=CARD, outline=CARD_EDGE)
    r = ANCH["rects"]["stock"]["rect"]
    d.rectangle([r[0], r[1], r[0] + r[2], r[1] + r[3]], fill=CARD, outline=CARD_EDGE)
    for side in ("cpu", "player"):
        for t in range(4):
            for c in range(6):
                r = mini_rect(side, t, c, 6)
                d.rectangle([r[0], r[1], r[0] + r[2], r[1] + r[3]], fill=(54, 58, 66),
                            outline=(84, 90, 100))

    # 3. named rects
    label_rect(d, ANCH["rects"]["cpu_score_box"]["rect"], "cpu score", side="tl")
    label_rect(d, ANCH["rects"]["you_score_box"]["rect"], "you score", side="bl")
    label_rect(d, ANCH["rects"]["stock"]["rect"], "stock", dims=False, side="bl")
    label_rect(d, ANCH["rects"]["rnd_panel"]["rect"], "rnd", dims=False, side="tl")
    label_rect(d, ANCH["rects"]["cpu_combos"]["rect"], "cpu combos", side="tl")
    label_rect(d, ANCH["rects"]["your_combos"]["rect"], "your combos", side="bl")
    label_rect(d, ANCH["rects"]["pause"]["rect"], "pause", dims=False, side="tl")
    label_rect(d, ANCH["rects"]["restart"]["rect"], "restart", dims=False, side="bl")
    r = ANCH["rects"]["field_box"]["rect"]
    dashed(d, r, (110, 96, 70))
    chip(d, (r[0] + 4, r[1] + r[3] - 14), "field bbox 650×400 — nothing is painted here",
         font=F_TINY, fg=(150, 134, 100), bg=(8, 8, 10, 205))

    # 4. free space, the only place new art can go
    place = {"between_status_and_hand": (710 - 60, 546)}
    for key, spec in ANCH["free_space"].items():
        r = spec["rect"]
        dashed(d, r, FREE)
        pos = place.get(key, (r[0] + 4, r[1] + 4))
        chip(d, pos, key.replace("_", " "), font=F_TINY, fg=FREE, bg=(8, 8, 10, 205))

    # 5. frozen anchors
    for key, spec in ANCH["frozen"].items():
        if isinstance(spec, dict) and "rect" in spec:
            x, y = spec["rect"][0], spec["rect"][1]
            d.line([x - 4, y, x + 4, y], fill=FROZEN)
            d.line([x, y - 4, x, y + 4], fill=FROZEN)
            chip(d, (x + 5, y - 12), key.replace("_", " "), font=F_TINY, fg=FROZEN,
                 bg=(8, 8, 10, 210))

    # 6. pitch callouts
    def dim_h(x0, x1, y, text):
        d.line([x0, y, x1, y], fill=INK)
        d.line([x0, y - 4, x0, y + 4], fill=INK)
        d.line([x1, y - 4, x1, y + 4], fill=INK)
        tw = d.textlength(text, font=F_TINY)
        d.rectangle([(x0 + x1) / 2 - tw / 2 - 2, y - 9, (x0 + x1) / 2 + tw / 2 + 2, y + 1],
                    fill=(8, 8, 10, 225))
        d.text(((x0 + x1) / 2 - tw / 2, y - 9), text, font=F_TINY, fill=INK)

    chip(d, (438, 448), "81 px pitch · 76 px card · 5 px gap  (live table_slot_rect)",
         font=F_TINY, fg=INK, bg=(8, 8, 10, 215))


def figure(out_png, out_svg=None):
    legend_h = 176
    im = Image.new("RGBA", (W, H + legend_h), (0, 0, 0, 255))
    d = ImageDraw.Draw(im, "RGBA")
    d.rectangle([0, 0, W, H], fill=BG)
    draw_map(d)

    # legend band
    y0 = H
    d.rectangle([0, y0, W, H + legend_h], fill=(16, 16, 19))
    d.line([0, y0, W, y0], fill=(70, 70, 70))
    d.text((16, y0 + 10), "TABLE, TO SCALE — 1280 × 720, 1 px = 1 px", font=F_TITLE, fill=INK)
    d.text((16, y0 + 32), "Live geometry: table_slot_rect() + src/ui_board.c painters.",
           font=F_SMALL, fill=INK_DIM)
    d.text((16, y0 + 45), "Never docs/assets/readme/table-map.png (dead 96 px layout).",
           font=F_SMALL, fill=(150, 110, 96))

    key = [((232, 208, 150), "rect the game draws at this exact place"),
           ((150, 134, 100), "field slot / card rect"),
           ((120, 84, 60), "overflow slot (21st..24th card)"),
           ((90, 160, 150), "free space: the only place new art can go"),
           ((200, 96, 96), "frozen anchor: action-card guard")]

    def swatch(x, y, c):
        d.rectangle([x, y + 2, x + 10, y + 12], fill=c + (60,), outline=c)

    col = 16
    for i, (c, t) in enumerate(key):
        yy = y0 + 66 + i * 15
        swatch(col, yy, c)
        d.text((col + 16, yy), t, font=F_SMALL, fill=(210, 210, 210))

    facts = [
        ("field", "8 × 2 slots, 434 + col·81, rows 170 / 310, card 76 × 124"),
        ("deal", "8 cards land in columns 2..5; 14 → 0..6; 16 → all"),
        ("hand", "8 × 88 × 143, 96 px pitch, centred on x 710, y 566 (hover 552)"),
        ("cpu hand", "8 × 56 × 91, 62 px pitch, centred on x 710, y 12"),
        ("lanes", "4 × 255 × 37.4 per side, 4 px gaps, mini 17 × 31 from x 54"),
        ("fills", "black @ alpha 178 (info strips 112 idle / 205 active)"),
        ("frozen", "action card: stock 330,262 · meter 414,170 14 × 264"),
        ("scale bar", "100 px"),
    ]
    col2 = 430
    for i, (k, v) in enumerate(facts):
        yy = y0 + 14 + i * 17
        d.text((col2, yy), k, font=F_SMALL, fill=INK)
        d.text((col2 + 86, yy), v, font=F_SMALL, fill=(205, 205, 205))
    d.line([1010, y0 + 140, 1110, y0 + 140], fill=INK)
    d.line([1010, y0 + 136, 1010, y0 + 144], fill=INK)
    d.line([1110, y0 + 136, 1110, y0 + 144], fill=INK)
    d.text((1032, y0 + 145), "100 px ≈ 5 % of height", font=F_TINY, fill=INK_DIM)

    im.convert("RGB").save(out_png)
    print(f"wrote {os.path.relpath(out_png, ROOT)}  {im.width}×{im.height}")

    if out_svg:
        write_svg(out_svg)


def write_svg(path):
    """Same geometry, vector, for zooming and annotating."""
    s = ['<svg xmlns="http://www.w3.org/2000/svg" width="1280" height="720" '
         'viewBox="0 0 1280 720" font-family="monospace">',
         '<rect width="1280" height="720" fill="#0a0a0c"/>']
    for x in range(0, 1281, 40):
        s.append(f'<line x1="{x}" y1="0" x2="{x}" y2="720" stroke="#141518"/>')
    for y in range(0, 721, 40):
        s.append(f'<line x1="0" y1="{y}" x2="1280" y2="{y}" stroke="#141518"/>')

    def box(r, stroke, w=1, dash=None, fill="none", op=1):
        x, y, ww, hh = r
        da = f' stroke-dasharray="{dash}"' if dash else ""
        s.append(f'<rect x="{x}" y="{y}" width="{ww}" height="{hh}" fill="{fill}" '
                 f'fill-opacity="{op}" stroke="{stroke}" stroke-width="{w}"{da}/>')

    def txt(x, y, t, fill="#e8d096", size=10, anchor="start"):
        s.append(f'<text x="{x}" y="{y}" fill="{fill}" font-size="{size}" '
                 f'text-anchor="{anchor}">{t}</text>')

    for r in ([12, 12, 255, 30], [12, 678, 255, 30]):
        box(r, "#3a3e46", fill="#181a1e", op=0.8)
    for side in ("cpu", "player"):
        for t in range(4):
            box(lane_rect(side, t), "#3a3e46", fill="#181a1e", op=0.8)
    for r in ([1160, 12, 112, 66], [1160, 88, 112, 218], [1160, 316, 112, 218],
              [1160, 560, 112, 40], [1160, 610, 112, 40]):
        box(r, "#3a3e46", fill="#181a1e", op=0.8)
    for row in range(2):
        for col in range(8):
            box(field_slot(col, row), "#968664", dash="3 3")
    for i in range(8):
        box(hand_rect(i, 8), "#606874", fill="#2e323a", op=0.6)
        box(cpu_rect(i, 8), "#606874", fill="#2e323a", op=0.6)
    r = ANCH["rects"]["stock"]["rect"]
    box(r, "#606874", fill="#2e323a", op=0.6)
    for side in ("cpu", "player"):
        for t in range(4):
            for c in range(6):
                box(mini_rect(side, t, c, 6), "#545a64", fill="#363a42", op=0.7)

    named = [("rects", "cpu_score_box", "cpu score"), ("rects", "you_score_box", "you score"),
             ("rects", "stock", "stock"), ("rects", "rnd_panel", "rnd"),
             ("rects", "cpu_combos", "cpu combos"), ("rects", "your_combos", "your combos"),
             ("rects", "pause", "pause"), ("rects", "restart", "restart")]
    for _, k, name in named:
        r = ANCH["rects"][k]["rect"]
        box(r, "#e8d096")
        txt(r[0] + 3, r[1] + 11, f"{name} {r[2]:g}x{r[3]:g}", "#e8d096", 9)
    r = ANCH["rects"]["field_box"]["rect"]
    box(r, "#6e6046", dash="2 4")
    txt(r[0] + 4, r[1] + 12, "field bbox 650x400 (nothing painted)", "#96865f", 9)
    for k, spec in ANCH["free_space"].items():
        r = spec["rect"]
        box(r, "#5aa096", dash="4 3")
        txt(r[0] + 4, r[1] + 12, k.replace("_", " "), "#5aa096", 9)
    for k, spec in ANCH["frozen"].items():
        if isinstance(spec, dict) and "rect" in spec:
            x, y = spec["rect"][0], spec["rect"][1]
            s.append(f'<line x1="{x - 4}" y1="{y}" x2="{x + 4}" y2="{y}" stroke="#c86060"/>')
            s.append(f'<line x1="{x}" y1="{y - 4}" x2="{x}" y2="{y + 4}" stroke="#c86060"/>')
            txt(x + 5, y - 5, k.replace("_", " "), "#c86060", 9)
    s.append("</svg>")
    open(path, "w").write("\n".join(s) + "\n")
    print(f"wrote {os.path.relpath(path, ROOT)}")


# ---------------------------------------------------------------- composite base
def base(out, backdrop=None, cards=True):
    """A truthful 1280x720 base: the game's own fills and cards, no art of ours."""
    if backdrop:
        im = Image.open(os.path.join(ROOT, "assets", backdrop)).convert("RGBA").resize((W, H))
    else:
        im = Image.new("RGBA", (W, H), (10, 10, 12, 255))
        d0 = ImageDraw.Draw(im, "RGBA")
        for y in range(0, H, 40):
            d0.line([0, y, W, y], fill=(18, 19, 22, 255))
        for x in range(0, W, 40):
            d0.line([x, 0, x, H], fill=(18, 19, 22, 255))

    dark = Image.new("RGBA", im.size, (0, 0, 0, 0))
    dp = ImageDraw.Draw(dark, "RGBA")
    for r in ([12, 12, 255, 30], [12, 678, 255, 30]):
        dp.rectangle([r[0], r[1], r[0] + r[2] - 1, r[1] + r[3] - 1], fill=(0, 0, 0, 112))
    for side in ("cpu", "player"):
        for t in range(4):
            r = lane_rect(side, t)
            dp.rectangle([r[0], r[1], r[0] + r[2] - 1, r[1] + r[3] - 1], fill=(0, 0, 0, 178))
    for r in ([1160, 12, 112, 66], [1160, 88, 112, 218], [1160, 316, 112, 218],
              [1160, 560, 112, 40], [1160, 610, 112, 40]):
        dp.rectangle([r[0], r[1], r[0] + r[2] - 1, r[1] + r[3] - 1], fill=(0, 0, 0, 178))
    im = Image.alpha_composite(im, dark)

    if not cards:
        im.convert("RGB").save(out)
        print(f"wrote {os.path.relpath(out, ROOT)}  (base, no cards)")
        return

    atlas = Image.open(os.path.join(ROOT, "assets", "atlas.png")).convert("RGBA")
    back = Image.open(os.path.join(ROOT, "assets", "back.png")).convert("RGBA")

    def cell(cid):
        cx, cy = (cid >> 2) * 176, (cid & 3) * 286
        return atlas.crop((cx, cy, cx + 176, cy + 286))

    def put(img, r):
        x, y, w, h = r
        im.alpha_composite(img.resize((int(round(w)), int(round(h))), Image.LANCZOS),
                           (int(round(x)), int(round(y))))

    # an 8-card opening field: ids of the same month family, centred in columns 2..5
    ids = [0, 4, 8, 12, 16, 20, 24, 28]
    for row in range(2):
        for i in range(4):
            put(cell(ids[row * 4 + i]), field_slot(2 + i, row))
    for i in range(8):
        put(back, cpu_rect(i, 8))
    for i in range(8):
        put(cell(32 + i), hand_rect(i, 8))
    r = ANCH["rects"]["stock"]["rect"]
    put(back, r)
    for side in ("cpu", "player"):
        for t in range(4):
            for c in range(6):
                put(cell(t * 4 + c + 4), mini_rect(side, t, c, 6))

    im.convert("RGB").save(out)
    print(f"wrote {os.path.relpath(out, ROOT)}  (base, live geometry, "
          f"{'backdrop ' + backdrop if backdrop else 'flat canvas'})")


# ---------------------------------------------------------------- palettes
def palettes():
    files = ["harvest.jpg", "hole.jpg", "arctic.jpg", "castle.jpg",
             "city.jpg", "deus.png", "face.jpg", "realm.jpg"]
    out = {}
    for f in files:
        im = Image.open(os.path.join(ROOT, "assets", f)).convert("RGB")
        a = np.asarray(im).reshape(-1, 3).astype(float)
        lum = a @ [0.2126, 0.7152, 0.0722]
        hi = a[np.argsort(lum)[-max(1, len(lum) // 100):]]
        out[f] = {
            "mean": [int(v) for v in a.mean(0)],
            "highlight_p99": [int(v) for v in hi.mean(0)],
            "max": [int(v) for v in a.max(0)],
            "bright_pixels_pct": round(float((lum > 96).mean()) * 100, 2),
            "swatches": [("#%02x%02x%02x" % tuple(int(v) for v in c))
                         for c in (a.mean(0), hi.mean(0),
                                   np.percentile(a, 92, axis=0), np.percentile(a, 60, axis=0))],
        }
    json.dump(out, open(os.path.join(TAB, "palettes.json"), "w"), indent=1)

    cw, ch = 300, 96
    sheet = Image.new("RGBA", (cw * 3 + 40, 24 + ch * 3 + 20), (14, 14, 17, 255))
    d = ImageDraw.Draw(sheet)
    d.text((16, 8), "BACKDROP PALETTES — sampled, not guessed", font=F_TITLE, fill=INK)
    for i, f in enumerate(files):
        col, row = i % 3, i // 3
        x, y = 10 + col * (cw + 10), 32 + row * (ch + 10)
        thumb = Image.open(os.path.join(ROOT, "assets", f)).convert("RGB").resize((150, 84))
        sheet.paste(thumb, (x, y))
        d.rectangle([x, y, x + 150, y + 84], outline=(70, 70, 70))
        d.text((x + 158, y + 2), f, font=F_BODY, fill=INK)
        d.text((x + 158, y + 18), "mean %s" % ("#%02x%02x%02x" % tuple(out[f]["mean"])),
               font=F_SMALL, fill=(200, 200, 200))
        d.text((x + 158, y + 32), "hi   %s" % ("#%02x%02x%02x" % tuple(out[f]["highlight_p99"])),
               font=F_SMALL, fill=(200, 200, 200))
        d.text((x + 158, y + 46), "bright %.1f %%" % out[f]["bright_pixels_pct"],
               font=F_SMALL, fill=(180, 180, 180))
        for j, sw in enumerate(out[f]["swatches"]):
            sx = x + 158 + j * 22
            d.rectangle([sx, y + 62, sx + 18, y + 80], fill=sw, outline=(90, 90, 90))
    sheet.convert("RGB").save(os.path.join(TAB, "palettes.png"))
    print(f"wrote tables/palettes.json and tables/palettes.png ({len(files)} backdrops)")


# ---------------------------------------------------------------- drift check
def num(text, pat):
    m = re.search(pat, text)
    return float(m.group(1)) if m else None


def check():
    def read(p):
        return open(os.path.join(ROOT, p)).read()

    theme = read("src/ui_theme.h")
    board = read("src/ui_board.c")
    combos = read("src/ui_combos.c")
    fails = []

    def eq(name, got, want):
        if got is None or abs(float(got) - float(want)) > 0.01:
            fails.append(f"{name}: src={got} anchors={want}")

    eq("HAND_CARD_W", num(theme, r"#define HAND_CARD_W\s+([\d.]+)"), ANCH["card_sizes"]["hand"]["w"])
    eq("HAND_CARD_H", num(theme, r"#define HAND_CARD_H\s+([\d.]+)"), ANCH["card_sizes"]["hand"]["h"])
    eq("FIELD_CARD_W", num(theme, r"#define FIELD_CARD_W\s+([\d.]+)"), ANCH["card_sizes"]["field"]["w"])
    eq("FIELD_CARD_H", num(theme, r"#define FIELD_CARD_H\s+([\d.]+)"), ANCH["card_sizes"]["field"]["h"])
    eq("CPU_CARD_W", num(theme, r"#define CPU_CARD_W\s+([\d.]+)"), ANCH["card_sizes"]["cpu"]["w"])
    eq("CPU_CARD_H", num(theme, r"#define CPU_CARD_H\s+([\d.]+)"), ANCH["card_sizes"]["cpu"]["h"])
    eq("CAPTURE_PANEL_W", num(theme, r"#define CAPTURE_PANEL_W\s+([\d.]+)"), 255)
    eq("CAPTURE_PANEL_H", num(theme, r"#define CAPTURE_PANEL_H\s+([\d.]+)"), 37.4)
    eq("FIELD_X", num(theme, r"#define FIELD_X\s+([\d.]+)"), ANCH["rects"]["field_box"]["rect"][0])
    eq("FIELD_Y", num(theme, r"#define FIELD_Y\s+([\d.]+)"), ANCH["rects"]["field_box"]["rect"][1])
    eq("FIELD_W", num(theme, r"#define FIELD_W\s+([\d.]+)"), ANCH["rects"]["field_box"]["rect"][2])
    eq("FIELD_H", num(theme, r"#define FIELD_H\s+([\d.]+)"), ANCH["rects"]["field_box"]["rect"][3])
    eq("ATLAS_CELL_W", num(theme, r"#define ATLAS_CELL_W\s+([\d.]+)"), 176)
    eq("ATLAS_CELL_H", num(theme, r"#define ATLAS_CELL_H\s+([\d.]+)"), 286)

    slot = re.search(r"static Rectangle table_slot_rect.*?\n}", board, re.S).group(0)
    eq("slot x origin", num(slot, r"([\d.]+)f \+ \(float\)col \* 81"), 434)
    eq("slot pitch", num(slot, r"\* ([\d.]+)f, \(row == 0\)"), 81)
    eq("slot row0 y", num(slot, r"\(row == 0\) \? ([\d.]+)f"), 170)
    eq("slot row1 y", num(slot, r": ([\d.]+)f,"), 310)
    eq("overflow y0", num(slot, r"\+ 12\.0f, \(row == 0\) \? ([\d.]+)f"), 184)

    eq("hand centre_x", num(board, r"([\d.]+)f - total_w / 2\.0f \+ index \* step"), 710)
    eq("hand step", num(board, r"float step = ([\d.]+)f, total_w"), 96)
    eq("hand y", num(board, r"hovered \? 552\.0f : ([\d.]+)f, HAND_CARD_W"), 566)
    eq("hand y hover", num(board, r"hovered \? ([\d.]+)f : 566\.0f"), 552)
    eq("cpu pitch", num(board, r"float cpu_step = ([\d.]+)f"), 62)
    eq("lane x", num(board, r"Rectangle lr = \{ ([\d.]+)f, y, CAPTURE_PANEL_W"), 12)
    eq("lane pitch", num(board, r"CAPTURE_PANEL_H \+ ([\d.]+)f\)"), 4)


    for k in ("rnd_panel", "pause", "restart"):
        want = ANCH["rects"][k]["rect"]
        pat = rf"1160(?:\.0f)?,\s*{want[1]}(?:\.0f)?,\s*{want[2]}(?:\.0f)?,\s*{want[3]}(?:\.0f)?"
        if not re.search(pat, board + combos):
            fails.append(f"{k}: no 1160,{want[1]},{want[2]},{want[3]} in src")

    # the combo panels come out of a helper: { 1160, player == 1 ? 88 : 316, 112, 218 }
    m = re.search(r"combos_panel_rect.*?\{ 1160, player == 1 \? (\d+) : (\d+), (\d+), (\d+) \}",
                  combos, re.S)
    if not m:
        fails.append("combos_panel_rect() no longer parses - re-derive cpu_combos / your_combos")
    else:
        eq("cpu_combos y", m.group(1), ANCH["rects"]["cpu_combos"]["rect"][1])
        eq("your_combos y", m.group(2), ANCH["rects"]["your_combos"]["rect"][1])
        eq("combos w", m.group(3), ANCH["rects"]["cpu_combos"]["rect"][2])
        eq("combos h", m.group(4), ANCH["rects"]["cpu_combos"]["rect"][3])

    if fails:
        print("ANCHOR DRIFT - anchors.json no longer matches src/:")
        for f in fails:
            print("  x", f)
        return 1
    print(f"anchors in sync with src/ ({len(ANCH['rects'])} rects, "
          f"{len(ANCH['card_sizes'])} card sizes, 13 defines)")
    return 0


# ---------------------------------------------------------------- main
if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else "all"
    bd = None
    if "--backdrop" in sys.argv:
        bd = sys.argv[sys.argv.index("--backdrop") + 1]

    if mode in ("figure", "all"):
        figure(os.path.join(TAB, "table-to-scale.png"), os.path.join(TAB, "table-to-scale.svg"))
    if mode == "svg":
        write_svg(os.path.join(TAB, "table-to-scale.svg"))
    if mode == "base":
        name = f"base-{os.path.splitext(bd)[0]}.png" if bd else "base.png"
        base(os.path.join(TAB, name), bd)
    if mode in ("palettes", "all"):
        palettes()
    if mode == "check":
        sys.exit(check())
    if mode == "all":
        sys.exit(check())
