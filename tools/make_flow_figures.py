#!/usr/bin/env python3
"""
Two figures for the pages that explain the turn and the score: a turn as the engine runs it,
and the gates an award passes through.

Like tools/make_board_figure.py these are not sketches: every box is a branch of this build's
C, and the line it cites is painted inside the box so a reader can check it. Nothing here links
the engine -- the figures carry no numbers, only shape -- because the numbers already live in
generated tables on the same pages (docs/scoring/yaku.md, docs/scoring/multipliers.md) and a
figure that repeats a total is a figure that goes stale.

  Stage 1 and its four cases   engine_play_hand(), src/engine.c:224-310
      0 matches -> the card stays on the field                      :248-253
      1 match   -> both cards are captured                          :255-266
      2 matches -> PHASE_HAND_CHOICE, and it can be cancelled      :268-287, :311
      3 matches -> the played card and all three are taken          :289-300
  Stage 2                      engine_reveal_stock(), src/engine.c:399-462
      same four cases against the field as it now stands; the drawn card never enters the hand
      its 2-match choice cannot be cancelled                        :455-461, no cancel path
  The verdict                  finish_stock_turn() + combo_triggered(), src/engine.c:378-397
      KK-ADD-10 asks whether a combination is new or has grown      :380
      the other four compare base points with your last call        :381
      empty hand -> Shobu without asking                            :386-388
      Nintendo / Tournament / Yakuza after one call -> Shobu forced :390-392
      otherwise -> PHASE_DECIDE, the prompt                         :394
  The award                    calculate_award(), src/rules.c:315-322
      base <= 0 pays 0, before anything is multiplied               :316
      base >= 7 doubles                                             :318
      a Koi-Koi the OPPONENT called doubles                         :319
      each call you made adds +1 to your own multiplier               :320, rules.h:66-68
      capped at KOI_CALLER_MULT_MAX = 6 (src/rules.h)

Sizes: the design box is 760 px wide, which is the width Material gives an article (.md-grid is
61rem and the table of contents takes 12.1rem of it). So the figure is drawn at the size it will
be read at and saved at 2x for crisp text, and the stylesheet's zoom shows the 2x file. The board
map is 1:1 with the game's canvas instead, because that figure is a place, not a diagram.

Usage:
  python3 tools/make_flow_figures.py            # write both
  python3 tools/make_flow_figures.py --check    # exit 1 if either is missing
"""
import pathlib
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT_DIR = ROOT / "docs" / "assets" / "figures"
TURN_FLOW = OUT_DIR / "turn-flow.png"
AWARD_PIPELINE = OUT_DIR / "award-pipeline.png"
TITLE_TTF = ROOT / "assets" / "audiowide.ttf"
BODY_TTF = ROOT / "assets" / "play.ttf"

W = 760                      # the column the figure will be read in
SCALE = 2
PHOSPHOR = (100, 240, 255)
PHOSPHOR_DIM = (45, 175, 195)
TEXT_INFO = (242, 253, 255)
PANEL_A = 178

M = 16                       # outer margin
PAD = 10                     # inside a box
GAP = 8


class Figure:
    """Boxes, wrapped text, a flow rail. One layer for panels, one for chrome."""

    def __init__(self, height):
        self.h = height                      # the canvas is built tall and cropped to content
        self.bottom = 0.0
        self.art = Image.new("RGBA", (W * SCALE, height * SCALE), (0, 0, 0, 255))
        self.d = ImageDraw.Draw(self.art)
        self.title = lambda px: ImageFont.truetype(str(TITLE_TTF), px * SCALE)
        self.body = lambda px: ImageFont.truetype(str(BODY_TTF), px * SCALE)

    # --- primitives -------------------------------------------------------------
    def box(self, x, y, w, h, colour=PHOSPHOR_DIM, fill=PANEL_A):
        self.bottom = max(self.bottom, y + h)
        self.d.rectangle([x * SCALE, y * SCALE, (x + w) * SCALE - 1, (y + h) * SCALE - 1],
                         fill=(0, 0, 0, fill), outline=colour + (200,), width=SCALE)

    def text(self, font, s, x, y, colour, tracking=0.0, alpha=255):
        # the bottom of the ink is tracked from real font metrics: a caption's descenders are the
        # last thing on the page and save() crops from this number
        self.bottom = max(self.bottom, y + sum(font.getmetrics()) / SCALE)
        cx = x * SCALE
        for ch in s:
            self.d.text((int(cx), int(y * SCALE)), ch, font=font,
                        fill=(colour + (alpha,))[:4])
            cx += font.getlength(ch) + tracking * SCALE
        return cx / SCALE

    def wrap(self, font, s, width):
        """Greedy wrap, measured with the same font that will draw it. A \\n in the text is a
        hard line break, which is how the pipeline boxes stack their words."""
        lines, cur = [], ""
        for word in s.split():
            trial = (cur + " " + word).strip()
            if font.getlength(trial) / SCALE > width:
                lines.append(cur)
                cur = word
            else:
                cur = trial
        if cur:
            lines.append(cur)
        return lines

    def _lines(self, font, s, width):
        out = []
        for hard in s.split("\\n"):
            out.extend(self.wrap(font, hard, width) if " " in hard or len(hard) else [hard])
        return out

    def para(self, font, s, x, y, width, colour, leading, alpha=255):
        lines = self._lines(font, s, width)
        for i, line in enumerate(lines):
            self.text(font, line, x, y + i * leading, colour, alpha=alpha)
        return y + len(lines) * leading

    def rail(self, x, y0, y1, colour=PHOSPHOR_DIM):
        self.d.line([x * SCALE, y0 * SCALE, x * SCALE, y1 * SCALE], fill=colour + (150,),
                    width=SCALE)

    def tick(self, x0, y, x1, colour=PHOSPHOR_DIM):
        self.d.line([x0 * SCALE, y * SCALE, x1 * SCALE, y * SCALE], fill=colour + (150,),
                    width=SCALE)
        for dx, dy in ((-5, -3.5), (-5, 3.5)):
            self.d.line([x1 * SCALE, y * SCALE, (x1 + dx) * SCALE, (y + dy) * SCALE],
                        fill=colour + (200,), width=SCALE)

    def save(self, path):
        path.parent.mkdir(parents=True, exist_ok=True)
        bottom = min(self.h, self.bottom + M) * SCALE      # self.bottom is in design units
        self.art.crop((0, 0, W * SCALE, int(bottom))).convert("RGB").save(path, "PNG",
                                                                           optimize=True)
        self.h = int(bottom / SCALE)


def header(fig, title, sub):
    """Title across the top. It has to fit, so it is measured: a caption that runs off the canvas
    is cropped by save() into a half-word, and nobody notices until the page is published."""
    head, small = fig.title(19), fig.body(11)
    room = W - 2 * M
    for font, text in ((head, title), (small, sub)):
        wide = sum(font.getlength(c) + 1.2 * SCALE for c in text) / SCALE
        if wide > room:
            raise SystemExit(f"header does not fit in {room:.0f} px: {text!r} measures {wide:.0f}")
    fig.text(head, title, M, 10, PHOSPHOR, 1.2)
    fig.text(small, sub, M, 30, PHOSPHOR_DIM + (200,), 0.4)


def labelled_box(fig, x, y, w, name, body, cite, size_head=13, size_body=11.5):
    """A box with a display-face label, wrapped prose and a source line under it.

    The height is measured, not guessed: a box sized by eye today is a box the first reworded
    sentence overflows, and the overflow lands on the source line, which is the one part a reader
    needs most.
    """
    head, body_font = fig.title(size_head), fig.body(size_body)
    lines = fig._lines(body_font, body, w - 2 * PAD)
    leading = size_body + 3.5
    h = 8 + size_head + 6 + len(lines) * leading + (14 if cite else 0) + 5
    fig.box(x, y, w, h)
    fig.text(head, name, x + PAD, y + 8, PHOSPHOR, 0.6)
    fig.para(body_font, body, x + PAD, y + 8 + size_head + 6, w - 2 * PAD, TEXT_INFO + (215,),
             leading)
    if cite:
        fig.text(fig.body(9.5), cite, x + PAD, y + h - 17, PHOSPHOR_DIM + (190,), 0.3)
    return y + h


def turn_flow() -> Figure:
    fig = Figure(1200)          # built tall; Figure.save() crops to the content it measured
    header(fig, "ONE TURN, AS THE ENGINE RUNS IT",
           "src/engine.c - two stages, always both, then one question")

    bx, bw = M + 20, W - M - 20 - M
    y = labelled_box(
        fig, bx, 46, bw,
        "STAGE 1   YOUR HAND",
        "You drag one card from your hand onto the field. It is the only decision you make twice a turn.",
        "engine_play_hand(), src/engine.c:224")

    y += GAP + 4
    fig.text(fig.body(11.5), "How many cards on the field share its month?", bx, y,
             PHOSPHOR + (220,), 0.4)
    y += 20

    # the four cases, 2x2
    cw = (bw - GAP) / 2
    cases = [
        ("0", "Nothing is captured. The card you played stays face-up on the table.",
         "engine.c:248-253"),
        ("1", "You take both: the card you played and the one it matched.",
         "engine.c:255-266"),
        ("2", "The game stops and asks which one. Backing out and taking the other is allowed.",
         "engine.c:268-287, cancel at :311"),
        ("3", "You take the played card and all three matches. No question asked.",
         "engine.c:289-300"),
    ]
    # every case box is as tall as the tallest of them, so the grid stays a grid
    chh = max(len(fig._lines(fig.body(11), b, cw - 2 * PAD))
              for _n, b, _c in cases) * 14.5 + 47

    grid_y = y
    for i, (n, body, cite) in enumerate(cases):
        x = bx + (i % 2) * (cw + GAP)
        y = grid_y + (i // 2) * (chh + GAP)
        fig.box(x, y, cw, chh)
        fig.text(fig.title(17), n + (" match" if n == "1" else " matches"), x + PAD, y + 8,
                 PHOSPHOR, 0.6)
        fig.para(fig.body(11), body, x + PAD, y + 32, cw - 2 * PAD, TEXT_INFO + (215,), 14.5)
        fig.text(fig.body(8.5), cite, x + PAD, y + chh - 11, PHOSPHOR_DIM + (185,), 0.2)

    y = grid_y + 2 * (chh + GAP) + 4
    y = labelled_box(
        fig, bx, y, bw,
        "STAGE 2   THE STOCK",
        "The top card of the draw pile flips and answers the same question, against the field as "
        "it now stands - including the card you just put down. It never enters your hand, and its "
        "two-match choice cannot be backed out.",
        "engine_reveal_stock(), src/engine.c:399-462")

    y = labelled_box(
        fig, bx, y, bw,
        "THE VERDICT   DID THE TURN CHANGE WHAT YOU SCORE?",
        "KK-ADD-10 asks whether a combination is new or has grown in points. The other four rulesets "
        "compare your base points with what they were at your last call.",
        "combo_triggered(), src/engine.c:378-382")

    y += GAP + 4
    fig.text(fig.body(11.5), "Then one of three things happens:", bx, y, PHOSPHOR + (220,), 0.4)
    y += 22

    outs = [
        ("SHOBU, NOT ASKED", "Your hand is empty, so there is nothing to push into. The round "
                             "closes on what you have.", "engine.c:386-388"),
        ("SHOBU, FORCED", "Nintendo, Tournament or Yakuza, and you already called Koi-Koi once: "
                          "this round will not let you call it again.", "engine.c:390-392"),
        ("THE PROMPT", "Otherwise the game stops and offers the two buttons - SHOBU to bank the "
                       "round, KOI-KOI to keep playing for more.", "engine.c:394, PHASE_DECIDE"),
    ]
    label_w, cite_w = 150, 132
    for name, body, cite in outs:
        lines = fig._lines(fig.body(11), body, bw - 2 * PAD - label_w - cite_w)
        h_row = max(12 + 12 * 2 + 8, 10 + len(lines) * 14.5 + 6)
        fig.box(bx, y, bw, h_row)
        fig.text(fig.title(12), name, bx + PAD, y + 10, PHOSPHOR, 0.6)
        fig.para(fig.body(11), body, bx + PAD + label_w, y + 10, bw - 2 * PAD - label_w - cite_w,
                 TEXT_INFO + (215,), 14.5)
        fig.text(fig.body(8.5), cite, bx + bw - cite_w + 4, y + 10, PHOSPHOR_DIM + (185,), 0.2)
        y += h_row + GAP

    fig.text(fig.body(10), "A capture and a point are not the same thing: a turn that captured "
                          "two cards can still score nothing, and then it simply passes to him.",
             bx, y + 4, PHOSPHOR_DIM + (210,), 0.3)
    fig.rail(M + 3, 50, fig.bottom)
    return fig


def award_pipeline() -> Figure:
    fig = Figure(1200)          # built tall; Figure.save() crops to the content it measured
    header(fig, "WHAT TURNS BASE POINTS INTO AN AWARD",
           "calculate_award(), src/rules.c:315-322 - in this order")

    steps = [
        ("YOUR PILE", "Everything you captured this round, in either stage.", "engine.c"),
        ("BASE POINTS", "The yaku your pile forms, summed by the ruleset you picked.",
         "score_captures(), rules.c"),
        ("IF BASE >= 7", "Doubles. A big round is worth more than twice a small one.",
         "rules.c:318"),
        ("IF HE CALLED KOI-KOI", "Doubles again. He pushed his luck against you and lost.",
         "rules.c:319"),
        ("IF YOU CALLED KOI-KOI", "x(1 + calls), so one call x2, two x3 - capped at x6.",
         "rules.c:320, rules.h:66-68"),
        ("AWARD", "base x every multiplier above. If base is 0, nothing here runs at all.",
         "rules.c:316"),
    ]
    n = len(steps)
    bw = (W - 2 * M - 20 - (n - 1) * GAP) / n
    y, bh = 64, 150
    x = M + 20
    body = fig.body(10.5)
    for i, (name, text, cite) in enumerate(steps):
        fig.box(x, y, bw, bh, PHOSPHOR if i == n - 1 else PHOSPHOR_DIM)
        fig.para(fig.title(11), name.replace(" ", "\n"), x + 6, y + 8, bw - 12, PHOSPHOR, 13.5)
        fig.para(body, text, x + 6, y + 62, bw - 12, TEXT_INFO + (215,), 13.5)
        fig.para(fig.body(8), cite, x + 6, y + bh - 24, bw - 12, PHOSPHOR_DIM, 10, alpha=185)
        if i:
            fig.tick(x - GAP, y + bh / 2, x - 1)
        x += bw + GAP
    fig.text(fig.body(10), "No numbers on this figure on purpose: the totals are generated on "
                           "Multipliers & Awards, from this same function.",
             M + 20, y + bh + 12, PHOSPHOR_DIM + (210,), 0.3)
    fig.rail(M + 3, 50, fig.bottom)
    return fig


def main() -> int:
    if "--check" in sys.argv[1:]:
        missing = [p.name for p in (TURN_FLOW, AWARD_PIPELINE) if not p.exists()]
        if missing:
            print(f"missing flow figures: {', '.join(missing)} - run tools/make_flow_figures.py")
            return 1
        print("flow figures present (2).")
        return 0

    for fig, path, name in ((turn_flow(), TURN_FLOW, "turn-flow"),
                            (award_pipeline(), AWARD_PIPELINE, "award-pipeline")):
        fig.save(path)
        print(f"wrote {path.relative_to(ROOT)} at {path.stat().st_size / 1024:.0f} KiB "
              f"({W}x{fig.h} design, {SCALE}x)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
