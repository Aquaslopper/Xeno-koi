#!/usr/bin/env python3
"""
Cut per-card thumbnails out of the atlases for the wiki.

Each atlas holds 48 cards in a 12 x 4 grid, cell 176 x 286 px.
Card id maps to column id >> 2 (month - 1) and row id & 3 (slot) --
the same mapping board_draw_card() uses in src/ui_board.c.

assets/atlas_deus.png is the same grid with the mono art, because that
is what the game draws when the difficulty is DEUS (see the atlas choice
in board_draw_card). So the same card id is cut from both, and the pages
about Entity 13 use the deus/ set instead of the coloured one.

Output: docs/assets/cards/card-00.webp ... card-47.webp, the same 48 under
docs/assets/cards/deus/, the whole mono sheet as
docs/assets/deus-atlas.webp, plus back.webp (from assets/back.png) and
docs/assets/deus-bg.webp (from assets/deus.png, the DEUS SUPREMUS background
used at the top of the wiki). The owner's art is only cropped, resized and
compressed, never recoloured or redrawn.

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
ATLAS_DEUS = ROOT / "assets" / "atlas_deus.png"
BACK = ROOT / "assets" / "back.png"
DEUS = ROOT / "assets" / "deus.png"
OUT = ROOT / "docs" / "assets" / "cards"
OUT_DEUS = OUT / "deus"
DEUS_OUT = ROOT / "docs" / "assets" / "deus-bg.webp"
# The whole mono sheet, for the study page: one file, so the player can zoom into the
# map itself instead of clicking through 48 thumbs.
DEUS_ATLAS_OUT = ROOT / "docs" / "assets" / "deus-atlas.webp"

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
    cards = [OUT / f"card-{i:02d}.webp" for i in range(48)]
    expected = cards + [OUT_DEUS / f"card-{i:02d}.webp" for i in range(48)]
    expected += [OUT / "back.webp", DEUS_OUT, DEUS_ATLAS_OUT]

    if check_only:
        missing = [p.name for p in expected if not p.exists()]
        if missing:
            print(f"missing card images: {', '.join(missing)}")
            print("Run: python3 tools/make_card_images.py")
            return 1
        print(f"card images present ({len(expected)} files, both atlases + back + hero).")
        return 0

    for atlas_path, out_dir in ((ATLAS, OUT), (ATLAS_DEUS, OUT_DEUS)):
        atlas = Image.open(atlas_path).convert("RGB")
        if atlas.size != (COLS * CELL_W, ROWS * CELL_H):
            raise SystemExit(
                f"{atlas_path.name} size {atlas.size} != expected {(COLS * CELL_W, ROWS * CELL_H)}"
            )
        out_dir.mkdir(parents=True, exist_ok=True)
        for card_id in range(48):
            thumb = atlas.crop(card_box(card_id)).resize((THUMB_W, THUMB_H), Image.LANCZOS)
            thumb.save(out_dir / f"card-{card_id:02d}.webp", "WEBP", quality=QUALITY)

    back = Image.open(BACK).convert("RGB")
    back.resize((THUMB_W, THUMB_H), Image.LANCZOS).save(
        OUT / "back.webp", "WEBP", quality=QUALITY
    )

    # Wiki top background: kept at its native 1280 x 720, WebP-compressed.
    Image.open(DEUS).convert("RGB").save(DEUS_OUT, "WEBP", quality=78)

    # The mono sheet at native resolution - the study page zooms into this one image,
    # so it must stay 2112 x 1144 and never be resampled down.
    Image.open(ATLAS_DEUS).convert("RGB").save(DEUS_ATLAS_OUT, "WEBP", quality=82)

    total = sum(p.stat().st_size for p in expected)
    print(f"wrote {len(expected)} images ({total / 1024:.0f} KiB total)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
