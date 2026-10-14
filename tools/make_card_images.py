#!/usr/bin/env python3
"""
Cut per-card thumbnails out of assets/atlas.png for the wiki.

The atlas holds 48 cards in a 12 x 4 grid, each cell 176 x 286 px.
Card id maps to column id >> 2 (month - 1) and row id & 3 (slot) --
the same mapping board_draw_card() uses in src/ui_board.c.

Output: docs/assets/cards/card-00.webp ... card-47.webp, plus back.webp
(from assets/back.png) and docs/assets/deus-bg.webp (from assets/deus.png,
the Deus background used at the top of the wiki). The owner's art is only
cropped, resized and compressed, never recoloured or redrawn.

Usage:
  python3 tools/make_card_images.py           # write images
  python3 tools/make_card_images.py --check   # exit 1 if any image is missing

Requires Pillow:  pip install --break-system-packages pillow
"""
import pathlib
import sys

from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parent.parent
ATLAS = ROOT / "assets" / "atlas.png"
BACK = ROOT / "assets" / "back.png"
DEUS = ROOT / "assets" / "deus.png"
OUT = ROOT / "docs" / "assets" / "cards"
DEUS_OUT = ROOT / "docs" / "assets" / "deus-bg.webp"

COLS, ROWS = 12, 4
CELL_W, CELL_H = 176, 286
# Thumbnail size: half the cell. Big enough to read on screen, small in bytes.
THUMB_W, THUMB_H = 88, 143
QUALITY = 82


def card_box(card_id: int):
    col, row = card_id >> 2, card_id & 3
    return (col * CELL_W, row * CELL_H, (col + 1) * CELL_W, (row + 1) * CELL_H)


def main() -> int:
    check_only = "--check" in sys.argv[1:]
    expected = [OUT / f"card-{i:02d}.webp" for i in range(48)] + [OUT / "back.webp", DEUS_OUT]

    if check_only:
        missing = [p.name for p in expected if not p.exists()]
        if missing:
            print(f"missing card images: {', '.join(missing)}")
            print("Run: python3 tools/make_card_images.py")
            return 1
        print("card images present (48 cards + back).")
        return 0

    atlas = Image.open(ATLAS).convert("RGB")
    if atlas.size != (COLS * CELL_W, ROWS * CELL_H):
        raise SystemExit(f"atlas size {atlas.size} != expected {(COLS * CELL_W, ROWS * CELL_H)}")

    OUT.mkdir(parents=True, exist_ok=True)
    for card_id in range(48):
        thumb = atlas.crop(card_box(card_id)).resize((THUMB_W, THUMB_H), Image.LANCZOS)
        thumb.save(OUT / f"card-{card_id:02d}.webp", "WEBP", quality=QUALITY)

    back = Image.open(BACK).convert("RGB")
    back.resize((THUMB_W, THUMB_H), Image.LANCZOS).save(
        OUT / "back.webp", "WEBP", quality=QUALITY
    )

    # Wiki top background: kept at its native 1280 x 720, WebP-compressed.
    Image.open(DEUS).convert("RGB").save(DEUS_OUT, "WEBP", quality=78)

    total = sum(p.stat().st_size for p in expected)
    print(f"wrote {len(expected)} images ({total / 1024:.0f} KiB total)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
