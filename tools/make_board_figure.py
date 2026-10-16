#!/usr/bin/env python3
"""
Draw the table as the engine places it, so a player can see where everything is.

Not a mock-up and not a screenshot: this is the geometry in `src/` painted at 1:1. The file is
1280 x 720 because the game's virtual canvas is 1280 x 720 (`CANVAS_W`/`CANVAS_H`,
src/ui_theme.h:37-38), and the real card art sits in the exact rectangles the painter uses. The
numbered badges are the game's own paint-order numbers, from the comments in `board_draw()` in
src/ui_board.c, so a legend can cite the line each region comes from and a reader can grep it.

The state is the opening of a round, because that is the only state whose every count is fixed
by the rules and nothing has to be invented: 8 cards dealt to each side, 8 face-up on the field,
24 face-down in the stock; both scores 0, so all eight capture lanes read "B 0 / A 0 / R 0 / P 0";
"RND 1/2" and "KK-ADD-10" are the defaults in storage_load_settings(), src/storage.c:31-35.

Rectangles, each with the line it was copied from:

  2. left column   score box (12, 12, 255, 30) and (12, 678, 255, 30), text at x+6 vertically
                   centred at 16px, dim phosphor at alpha 165 for a player not on turn, panel
                   alpha 112 (src/ui_board.c:300,302 and board_draw_info_box in
                   src/ui_board_status.c:22-68); four lanes per side from y=46 and y=508, 255 x
                   37.4 with 4px gaps, "B/A/R/P" label at 16, y+8.5, 20px (draw_capture_lanes)
  3. CPU hand      56 x 91 backs (src/ui_theme.h:58-59), step 62, centred on x=710, y=12
                   (src/ui_board.c:306-308)
  4. stock         76 x 124 at (330, 262) (board_get_stock_rect, src/ui_board.c:158), stacked
                   (stock_count >> 2) = 6 times, each up-left by 1px, count at (356, 392) at
                   18px spacing 1.2 (src/ui_board.c:320-323)
  5. field         every slot from board_field_slot_xy(): rows at y=170 and y=310, step 96
                   shrinking to fit, centred inside FIELD_X/W = 430/650 (src/board_layout.c:16-27);
                   the field box (430, 132, 650, 400) from src/ui_theme.h:65-68 is dashed, because
                   it is measured against rather than drawn
  7. your hand     88 x 143 (src/ui_theme.h:44-45), step 96, centred on x=710, y=566 and 552 while
                   hovered (board_get_hand_card_rect, src/ui_board.c:169-172)
  10. status line  text only, centred on x=710 at y=529 in 18px (board_draw_status_message,
                   src/ui_board_status.c:73-80), and the Master timer on x=640 at y=505 in 22px
                   (src/ui_board.c:~640-660); dashed guide, since it has no box
  11. right column (1160, 12, 112, 54) round + ruleset, combo panels (1160, 74/132, 112, 48) --
                   height is base_h 48 with no rows to draw (draw_combo_panel) -- and PAUSE /
                   RESTART at (1160, 560) and (1160, 610), 112 x 40

Regions 1, 6, 8 and 9 are in the paint order but have nothing to place: the background is the
whole canvas, and the pending stock card, the dragged card and the animated cards only exist
mid-motion. The legend says so rather than the figure pretending they are absent.

Ground is black with no art behind it on purpose: that is what the game paints when no background
texture is loaded (`ClearBackground(BLACK)`, src/ui_board.c:281), and a map for reading gets no
scanline pass either. Card art is cropped from the game's atlas by the same mapping
make_card_images.py uses (column id >> 2, row id & 3, cell 176 x 286) and the back is
assets/back.png. The eight field cards are one card from each of eight different months, because a
field holding all four of a month -- or four pairs -- is a misdeal the engine would reject.

Usage:
  python3 tools/make_board_figure.py            # write it
  python3 tools/make_board_figure.py --check    # exit 1 if the file is missing
  python3 tools/make_board_figure.py --scale 2  # 2560 x 1440 for a zoom or a print
"""
import pathlib
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "assets" / "readme" / "table-map.png"
ATLAS = ROOT / "assets" / "atlas.png"
BACK = ROOT / "assets" / "back.png"
TITLE_TTF = ROOT / "assets" / "audiowide.ttf"
BODY_TTF = ROOT / "assets" / "play.ttf"

CANVAS_W, CANVAS_H = 1280, 720
PHOSPHOR = (100, 240, 255)
PHOSPHOR_DIM = (45, 175, 195)
TEXT_INFO = (242, 253, 255)
PANEL_A, PANEL_A_DIM, LABEL_INACTIVE = 178, 112, 165

HAND_W, HAND_H = 88, 143
FIELD_W_CARD, FIELD_H_CARD = 76, 124
CPU_W, CPU_H = 56, 91
FIELD_X, FIELD_Y, FIELD_W, FIELD_H = 430, 132, 650, 400
PANEL_W, PANEL_H = 255, 37.4
STOCK_XY = (330.0, 262.0)
ROW_Y = (170.0, 310.0)
STEP = 96.0
CPU_STEP = 62.0
AXIS_X = 710.0
CELL_W, CELL_H = 176, 286

FIELD_IDS = [0, 5, 10, 13, 17, 20, 24, 28]
HAND_IDS = [33, 36, 37, 40, 41, 42, 44, 45]
STOCK_COUNT, HAND_COUNT = 24, 8

# Each region number, at the top-left of a free spot beside the thing it labels. They are
# deliberately not centred on the regions: a badge sitting on a card teaches nothing, and these are
# the gaps the layout leaves -- x<267 and x>1080 belong to the columns, and the rows between them.
BADGES = [
    (2, 270, 17),      # left column: the score box, whose right edge is at 267
    (3, 437, 15),      # CPU hand: its first card starts at 465 (710 - ((8-1)*62+56)/2)
    (4, 302, 239),     # stock at (330, 262)
    (5, 402, 109),     # the field box corner at (430, 132)
    (7, 302, 543),     # your hand: first card at 330, 566
    (10, 532, 523),    # the status line, centred on x=710 at y=529
    (11, 1132, 15),    # right column, which starts at 1160
]
BADGE_W, BADGE_H = 24, 20


def card_box(card_id):
    col, row = card_id >> 2, card_id & 3
    return (col * CELL_W, row * CELL_H, (col + 1) * CELL_W, (row + 1) * CELL_H)


def field_slots(total):
    """board_field_slot_xy() for each index, in paint order."""
    top = (total + 1) // 2
    for index in range(total):
        row = 0 if index < top else 1
        count = top if row == 0 else total - top
        col = index if row == 0 else index - top
        step, row_w = STEP, (count - 1) * STEP + FIELD_W_CARD
        if row_w > FIELD_W - 20.0 and count > 1:
            step = (FIELD_W - 20.0 - FIELD_W_CARD) / (count - 1)
            row_w = (count - 1) * step + FIELD_W_CARD
        yield (FIELD_X + (FIELD_W - row_w) / 2.0 + col * step, ROW_Y[row],
               FIELD_W_CARD, FIELD_H_CARD)


def hand_slots(count, w, h, step, y):
    """The two hands: same centring rule, AXIS_X is the table's axis."""
    total_w = (count - 1) * step + w
    x0 = AXIS_X - total_w / 2.0
    for i in range(count):
        yield x0 + i * step, y, w, h


class Painter:
    """Two layers: `art` for pixels that go under the chrome, `ui` for panels and guides."""

    def __init__(self, scale):
        self.s = scale
        self.art = Image.new("RGB", (CANVAS_W * scale, CANVAS_H * scale), (0, 0, 0))
        self.ui = Image.new("RGBA", self.art.size, (0, 0, 0, 0))
        self.d = ImageDraw.Draw(self.ui)

    def rect(self, x, y, w, h):
        s = self.s
        return [x * s, y * s, (x + w) * s - 1, (y + h) * s - 1]

    def paste(self, im, x, y):
        self.art.paste(im, (int(x * self.s), int(y * self.s)))

    def panel(self, x, y, w, h, alpha):
        s = self.s
        fill = Image.new("RGBA", (int(w * s), int(h * s)), (0, 0, 0, alpha))
        flat = Image.new("RGBA", fill.size, (0, 0, 0, 0))
        self.art.paste(Image.alpha_composite(flat, fill),
                       (int(x * s), int(y * s)), fill)

    def border(self, x, y, w, h, colour=PHOSPHOR_DIM, highlight=True):
        x0, y0, x1, y1 = [int(round(v)) for v in self.rect(x, y, w, h)]
        self.d.rectangle([x0, y0, x1, y1], outline=colour + (255,), width=self.s)
        if not highlight:
            return
        half = (x1 - x0) // 2                      # the 1px centre highlight, ui_theme.c
        for yy in (y0, y1):
            for i in range(half):
                a = (i / max(1, half - 1)) * 18
                b = 18 - a
                self.d.point((x0 + i, yy), fill=(255, 255, 255, round(a)))
                self.d.point((x0 + half + i, yy), fill=(255, 255, 255, round(b)))

    def dash(self, x0, y0, x1, y1, colour=PHOSPHOR_DIM, alpha=140, dash=6):
        """Dashed guide: the diagram's own language, for rectangles the game does not paint."""
        s = self.s
        for a, b, c, e in ((x0, y0, x1, y0), (x1, y0, x1, y1),
                           (x1, y1, x0, y1), (x0, y1, x0, y0)):
            ax, ay, bx, by = a * s, b * s, c * s, e * s
            span = ((bx - ax) if abs(bx - ax) > abs(by - ay) else (by - ay))
            horiz = abs(bx - ax) > abs(by - ay)
            pos = 0
            while pos < abs(span):
                n = min(dash * s, abs(span) - pos)
                p0 = (ax + (pos if horiz else 0), ay + (0 if horiz else pos))
                p1 = (ax + (pos + n if horiz else 0), ay + (0 if horiz else pos + n))
                self.d.line([p0, p1], fill=colour + (alpha,), width=s)
                pos += 2 * dash * s

    def text(self, font, s_, xy, colour, tracking=0.0):
        x = float(xy[0]) * self.s
        for ch in s_:
            self.d.text((int(x), int(xy[1] * self.s)), ch, font=font, fill=colour)
            x += font.getlength(ch) + tracking * self.s


def main() -> int:
    if "--check" in sys.argv[1:]:
        print("table map present." if OUT.exists()
              else f"missing {OUT.relative_to(ROOT)} -- run: python3 tools/make_board_figure.py")
        return 0 if OUT.exists() else 1

    scale = int(sys.argv[sys.argv.index("--scale") + 1]) if "--scale" in sys.argv else 1
    atlas = Image.open(ATLAS).convert("RGB")
    back = Image.open(BACK).convert("RGB")

    def card(cid, w, h):
        return atlas.crop(card_box(cid)).resize((w * scale, h * scale), Image.LANCZOS)

    def back_card(w, h):
        return back.resize((w * scale, h * scale), Image.LANCZOS)

    p = Painter(scale)
    body = lambda px: ImageFont.truetype(str(BODY_TTF), px * scale)      # noqa: E731
    title = lambda px: ImageFont.truetype(str(TITLE_TTF), px * scale)    # noqa: E731

    # 5. field: the dashed measuring box, then the eight slots in paint order
    p.dash(FIELD_X, FIELD_Y, FIELD_X + FIELD_W, FIELD_Y + FIELD_H)
    for (x, y, w, h), cid in zip(field_slots(len(FIELD_IDS)), FIELD_IDS):
        p.paste(card(cid, w, h), x, y)

    # 3. CPU hand: face-down, step 62, y=12      |      7. your hand: face-up, y=566
    for (x, y, w, h) in hand_slots(HAND_COUNT, CPU_W, CPU_H, CPU_STEP, 12.0):
        p.paste(back_card(w, h), x, y)
    for (x, y, w, h), cid in zip(hand_slots(HAND_COUNT, HAND_W, HAND_H, STEP, 566.0), HAND_IDS):
        p.paste(card(cid, w, h), x, y)

    # 4. stock: a stack of backs, and the count the painter prints under it
    for i in range(STOCK_COUNT >> 2, -1, -1):
        p.paste(back_card(FIELD_W_CARD, FIELD_H_CARD), STOCK_XY[0] - i, STOCK_XY[1] - i)
    p.text(body(18), str(STOCK_COUNT), (356, 392), PHOSPHOR + (255,), 1.2)

    # 2. left column: two score boxes, four lanes per side
    for by, label in ((12, "CPU   SCORE: 0"), (678, "YOU   SCORE: 0")):
        p.panel(12, by, PANEL_W, 30, PANEL_A_DIM)
        p.border(12, by, PANEL_W, 30)
        p.text(body(16), label, (18, by + 7), PHOSPHOR_DIM + (LABEL_INACTIVE,))
    for start in (46.0, 508.0):
        for t, letter in enumerate("BARP"):
            y = start + t * (PANEL_H + 4.0)
            p.panel(12, y, PANEL_W, PANEL_H, PANEL_A)
            p.border(12, y, PANEL_W, PANEL_H)
            p.text(body(20), f"{letter} 0", (16, y + 8.5), TEXT_INFO + (255,), 1.4)

    # 11. right column: round + ruleset, the two combo panels, PAUSE / RESTART
    p.panel(1160, 12, 112, 54, PANEL_A)
    p.border(1160, 12, 112, 54)
    p.text(body(14), "RND 1/2", (1168, 20), PHOSPHOR + (255,))
    p.text(body(11), "KK-ADD-10", (1168, 38), PHOSPHOR_DIM + (255,), 0.7)
    for y, t in ((74.0, "CPU COMBOS"), (132.0, "YOU COMBOS")):
        p.panel(1160, y, 112, 48, PANEL_A)
        p.border(1160, y, 112, 48)
        p.text(title(11), t, (1168, y + 8), PHOSPHOR + (255,), 0.8)
        p.text(body(15), "Base: 0", (1168, y + 25), TEXT_INFO + (200,))
    for y, t in ((560.0, "PAUSE"), (610.0, "RESTART")):
        p.panel(1160, y, 112, 40, PANEL_A)
        p.border(1160, y, 112, 40, PHOSPHOR)
        p.text(title(16), t, (1160 + (112 - title(16).getlength(t) / scale) / 2.0, y + 12),
               PHOSPHOR + (255,))

    # 10. where the two text lines sit, and nothing else about them
    p.dash(560, 519, 860, 547, alpha=95)

    # the region numbers, in the game's own button idiom
    num = body(13)
    for n, bx, by in BADGES:
        p.panel(bx, by, BADGE_W, BADGE_H, PANEL_A)
        p.border(bx, by, BADGE_W, BADGE_H, PHOSPHOR, highlight=False)
        label = str(n)
        p.text(num, label, (bx + (BADGE_W - num.getlength(label) / scale) / 2.0, by + 5),
               PHOSPHOR + (255,), 0.8)

    OUT.parent.mkdir(parents=True, exist_ok=True)
    Image.alpha_composite(p.art.convert("RGBA"), p.ui).convert("RGB").save(OUT, "PNG", optimize=True, compress_level=9)
    print(f"wrote {OUT.relative_to(ROOT)} at {p.art.size[0]}x{p.art.size[1]}, "
          f"{OUT.stat().st_size / 1024:.0f} KiB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
