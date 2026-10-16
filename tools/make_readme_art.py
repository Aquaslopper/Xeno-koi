#!/usr/bin/env python3
"""
Paint the README's masthead band and its link buttons out of the game itself.

Nothing in here draws anything new. The band is the shipped start screen, painted
the way `screens_draw_start()` paints it and post-processed with the numbers in
`assets/postprocess.fs` at the game's default settings, then cropped. The buttons
copy `draw_btn()`. Every constant below names the line it came from:

  canvas 1280 x 720            src/ui_theme.h:37-38
  background                   src/ui_board.c:98-100, bg_names[0] = assets/harvest.jpg,
                               the default index from src/storage.c:36. The blit is
                               DrawTexturePro 0,0,1280,720 -> the same rect
                               (src/ui_screens.c:68-71), and the file is already
                               1280 x 720, so it is a 1:1 copy at 1:1 scale.
  dim over the background      black at alpha 140 on the start screen
                               (src/ui_screens.c:101 -> draw_bg_if_loaded)
  title "[XENO / KOI]"         font_title 84px, tracking 2.0, centred on x=640,
                               y=78 (src/ui_screens.c:102, draw_centered_text :17-20)
  subtitle                     font_body "BIOMECHANICAL EDITION", 20px, tracking 2.0,
                               phosphor dim, y=190 (src/ui_screens.c:103)
  phosphor colours             TEAL theme, base (100,240,255) dim (45,175,195)
                               (src/ui_theme.c:3-11); g_theme is initialised to it
  panel fill + border          (0,0,0,178) fill, 1px stroke in the dim phosphor, plus a
                               1px (255,255,255,18) highlight fading out from the centre
                               of the top and bottom edges (src/ui_screens.c:27-34 and
                               ui_draw_panel_border() in src/ui_theme.c)
  button geometry              300 x 40 rect, font_title at 18px, tracking 1.2, text
                               centred, y = r.y + (r.height - fsz) * 0.5
                               (src/ui_screens.c:104 and :27-34)
  bloom off, CRT 0.65, 1.40    src/storage.c:42-44 -> the mapping to uniform values is
                               src/ui_main.c:168-170; the maths is
                               assets/postprocess.fs:44-53

The panel's crawling flare (step 3 of ui_draw_panel_border) is time- and rand()-based,
so it is left out: a README image that changed on every run would be worse than no flare.

Output: docs/assets/readme/hero.jpg and three button PNGs.

Usage:
  python3 tools/make_readme_art.py              # write them
  python3 tools/make_readme_art.py --check      # exit 1 if any is missing
  python3 tools/make_readme_art.py --frame F    # F in harvest|hole|arctic|castle|city|
                                                #   deus|face|realm, for trying a backdrop

Requires Pillow, and the game's own fonts in assets/ (OFL, licence files alongside them).
"""
import math
import pathlib
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "assets" / "readme"

# --- the numbers, from the lines named in the header -------------------------------
CANVAS_W, CANVAS_H = 1280, 720            # ui_theme.h:37-38
PHOSPHOR = (100, 240, 255)                # ui_theme.c:5  g_theme.base
PHOSPHOR_DIM = (45, 175, 195)             # ui_theme.c:6  g_theme.dim
PANEL_BG_A = 178                          # ui_theme.h:32 COLOR_PANEL_BG alpha
HIGHLIGHT_A = 18                          # ui_theme.c, ui_draw_panel_border step 2
BG_DIM_A = 140                            # ui_screens.c:101, the start screen
BLOOM = 0.0                               # storage.c:42  settings.bloom == 0
CRT = 0.65                                # storage.c:43  crt_filter == 2 -> 0.65, ui_main.c:169
GAMMA = 1.40                              # storage.c:44  gamma, applied as pow(c, 1/g)
SCAN_FREQ = 2261.94671                    # postprocess.fs:45
SCAN_POW = 0.35                           # postprocess.fs:46
SCAN_MIX = CRT * 0.7                      # postprocess.fs:47  mix(1, beam, crt * 0.7)

# The eight backgrounds, in the order the game loads them (ui_board.c:98-100).
BACKDROPS = {
    "harvest": "assets/harvest.jpg",
    "hole": "assets/hole.jpg",
    "arctic": "assets/arctic.jpg",
    "castle": "assets/castle.jpg",
    "city": "assets/city.jpg",
    "deus": "assets/deus.png",
    "face": "assets/face.jpg",
    "realm": "assets/realm.jpg",
}
# The README band is painted over assets/hole.jpg, background index 1. Not the game's own
# default frame -- that is harvest.jpg, index 0 (storage.c:36) -- because hole.jpg is the one
# of the eight whose middle falls away into dark architecture, so the masthead has something to
# sit on. --frame takes any of them; the default here is a presentation choice, and that is the
# only number in this file that is not copied from the game.
DEFAULT_BACKDROP = "hole"

# The band of the start screen the README uses: the masthead, nothing else.
BAND = (0, 0, 1280, 300)

TITLE = "[XENO / KOI]"                    # ui_screens.c:102
TITLE_SIZE, TITLE_TRACK = 84.0, 2.0
TITLE_Y = 78.0
SUBTITLE = "BIOMECHANICAL EDITION"         # ui_screens.c:103
SUB_SIZE, SUB_TRACK, SUB_Y = 20.0, 2.0, 190.0

BTN_W, BTN_H, BTN_FONT, BTN_TRACK = 300.0, 40.0, 18.0, 1.2   # ui_screens.c:104, :27-34
BTN_SCALE = 2                             # supersample, so retina README text stays crisp

TITLE_TTF = ROOT / "assets" / "audiowide.ttf"
BODY_TTF = ROOT / "assets" / "play.ttf"

# The three links the player actually came for. Labels are measured, not guessed: draw_btn()
# centres text in a fixed 300 x 40 rect and never wraps it, so a long label would simply run
# past the border. paint_button() refuses to draw one.
BUTTONS = {
    "btn-play.png": "PLAY IN THE BROWSER",
    "btn-rulebook.png": "READ THE RULEBOOK",
    "btn-release.png": "WINDOWS BUILD",
}
BTN_TEXT_PAD = 16                         # per side, at 2x, inside the rect


def measure(font, text, tracking):
    """MeasureTextEx(): raylib adds `spacing` between glyphs, not after the last one."""
    widths = [font.getlength(ch) for ch in text]
    return round(sum(widths) + tracking * max(0, len(text) - 1))


def draw_tracked(img, font, text, xy, tracking, colour):
    """Draw `text` at xy (top-left, floored) with `tracking` between glyphs."""
    draw = ImageDraw.Draw(img)
    widths = [font.getlength(ch) for ch in text]
    x = float(xy[0])
    for ch, w in zip(text, widths):
        draw.text((math.floor(x), math.floor(xy[1])), ch, font=font, fill=colour)
        x += w + tracking


def panel_border(draw, rect, colour, highlight, width):
    """ui_draw_panel_border(): 1px stroke plus a centre highlight on top and bottom.

    `width` scales the whole thing, because the buttons are drawn supersampled.
    """
    x0, y0, x1, y1 = rect
    draw.rectangle([x0, y0, x1, y1], outline=colour, width=width)
    half = (x1 - x0) // 2
    if half <= 0:
        return
    c_mid = (255, 255, 255, highlight)
    c_edge = (255, 255, 255, 0)
    for y in (y0, y1):
        _gradient_h(draw, x0, y, half, c_edge, c_mid)
        _gradient_h(draw, x0 + half, y, (x1 - x0) - half, c_mid, c_edge)


def _gradient_h(draw, x, y, w, c_from, c_to):
    if w <= 0:
        return
    for i in range(w):
        t = i / max(1, w - 1)
        col = tuple(round(c_from[k] + (c_to[k] - c_from[k]) * t) for k in range(4))
        draw.point((x + i, y), fill=col)


def scanline_and_gamma(im):
    """postprocess.fs:44-53, at the default settings, in order: bloom, CRT, gamma.

    The bloom term is skipped because the game's default is bloom off (storage.c:42).
    The scanline depends only on uv.y, so it is one scalar per row of the 720-high frame.
    """
    if BLOOM:
        raise SystemExit("this painter implements the game's defaults; bloom is off there")
    rows = []
    for y in range(im.height):
        v = (y + 0.5) / im.height
        s = max(0.0, 0.5 + 0.5 * math.sin(v * SCAN_FREQ))
        beam = s ** SCAN_POW
        rows.append(1.0 + (beam - 1.0) * SCAN_MIX)
    # One LUT per row, then two C loops: the scanline multiply and the gamma curve folded
    # together. A 768-entry table is Pillow's 256-entry table repeated for R, G and B.
    luts = []
    for m in rows:
        curve = [
            min(255, max(0, round(255 * ((i * m) / 255.0) ** (1.0 / GAMMA))))
            for i in range(256)
        ]
        luts.append(curve * 3)
    out = Image.new("RGB", im.size)
    for y, lut in enumerate(luts):
        out.paste(im.crop((0, y, im.width, y + 1)).point(lut), (0, y))
    return out


def paint_start_screen(backdrop):
    """The start screen's background stack: art, black at alpha 140, masthead, subtitle."""
    art = Image.open(ROOT / BACKDROPS[backdrop]).convert("RGB")
    if art.size != (CANVAS_W, CANVAS_H):
        # The game would stretch it to the canvas; do the same so the scale matches.
        art = art.resize((CANVAS_W, CANVAS_H), Image.LANCZOS)
    veil = Image.new("RGBA", (CANVAS_W, CANVAS_H), (0, 0, 0, BG_DIM_A))
    frame = Image.alpha_composite(art.convert("RGBA"), veil)

    title_font = ImageFont.truetype(str(TITLE_TTF), int(round(TITLE_SIZE)))
    body_font = ImageFont.truetype(str(BODY_TTF), int(round(SUB_SIZE)))

    # draw_centered_text(): cx - MeasureTextEx().x / 2, floored by draw_text_ex_snapped.
    # The game bakes its font at 48px (ui_main.c:716) and scales it to 84, so its caps are a
    # touch softer than a native 84px render. Not worth reproducing in a README banner.
    tw = measure(title_font, TITLE, TITLE_TRACK)
    draw_tracked(frame, title_font, TITLE, (640.0 - tw / 2.0, TITLE_Y), TITLE_TRACK,
                 PHOSPHOR + (255,))
    sw = measure(body_font, SUBTITLE, SUB_TRACK)
    draw_tracked(frame, body_font, SUBTITLE, (640.0 - sw / 2.0, SUB_Y), SUB_TRACK,
                 PHOSPHOR_DIM + (255,))

    return scanline_and_gamma(frame.convert("RGB"))


def paint_button(label):
    """draw_btn() at rest: panel fill, dim border with its centre highlight, label centred."""
    w, h = int(BTN_W * BTN_SCALE), int(BTN_H * BTN_SCALE)
    # Fill and border go on a layer and are composited, so the alpha-18 highlight blends
    # over the panel the way raylib's alpha blend does, instead of replacing its pixels.
    layer = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    ImageDraw.Draw(layer).rectangle([0, 0, w - 1, h - 1], fill=(0, 0, 0, PANEL_BG_A))
    panel_border(ImageDraw.Draw(layer), (0, 0, w - 1, h - 1), PHOSPHOR_DIM + (255,),
                 HIGHLIGHT_A, BTN_SCALE)
    img = Image.alpha_composite(Image.new("RGBA", (w, h), (0, 0, 0, 0)), layer)

    fsz = BTN_FONT * BTN_SCALE
    track = BTN_TRACK * BTN_SCALE
    font = ImageFont.truetype(str(TITLE_TTF), int(round(fsz)))
    bw = measure(font, label, track)
    if bw > w - 2 * BTN_TEXT_PAD:
        raise SystemExit(
            f"button label {label!r} is {bw}px wide in a {w}px rect "
            f"(limit {w - 2 * BTN_TEXT_PAD}px). Shorten it or widen BTN_W; "
            "draw_btn() would let it run past the border, and so will I."
        )
    y = (h - fsz) * 0.5                      # draw_btn: r.y + (r.height - fsz) * 0.5
    draw_tracked(img, font, label, ((w - bw) / 2.0, y), track, PHOSPHOR + (255,))
    return img

def main() -> int:
    check_only = "--check" in sys.argv[1:]
    expected = [OUT / "hero.jpg"] + [OUT / name for name in BUTTONS]
    if check_only:
        missing = [p.name for p in expected if not p.exists()]
        if missing:
            print(f"missing README art: {', '.join(missing)}")
            print("Run: python3 tools/make_readme_art.py")
            return 1
        print(f"README art present ({len(expected)} files).")
        return 0

    backdrop = DEFAULT_BACKDROP
    if "--frame" in sys.argv:
        backdrop = sys.argv[sys.argv.index("--frame") + 1]
        if backdrop not in BACKDROPS:
            raise SystemExit(f"unknown frame {backdrop!r}; pick one of {', '.join(BACKDROPS)}")

    OUT.mkdir(parents=True, exist_ok=True)
    if "--full" in sys.argv:
        frame = paint_start_screen(backdrop)
        frame.save(OUT / "_frame.png")
        print(f"wrote {OUT / '_frame.png'} ({backdrop})")
        return 0

    frame = paint_start_screen(backdrop)
    hero = frame.crop(BAND)
    # JPEG, not WebP: this is decoration, and the README has to render everywhere.
    # q92 with no chroma subsampling measured against the lossless band at mean error 0.67/255
    # (max 28, one pixel) for 144 KiB. WebP at q90 is 84 KiB but 0.92 mean and max 68, and lossless
    # WebP is 474 KiB -- this is the one image in the repo allowed to be lossy, because unlike the
    # atlas it is not a fidelity claim about the game's data, it is a banner.
    hero.save(OUT / "hero.jpg", "JPEG", quality=92, subsampling=0, optimize=True)
    for name, label in BUTTONS.items():
        paint_button(label).save(OUT / name, "PNG", optimize=True)

    total = sum(p.stat().st_size for p in expected)
    print(f"wrote {len(expected)} files ({total / 1024:.0f} KiB) from {BACKDROPS[backdrop]}")
    print(f"  hero {hero.size[0]}x{hero.size[1]}, buttons "
          f"{int(BTN_W)}x{int(BTN_H)} css at {BTN_SCALE}x")
    return 0


if __name__ == "__main__":
    sys.exit(main())
