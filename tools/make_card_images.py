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
used at the top of the wiki). tools/card_images.sha256 records the source and
output fingerprints. The owner's art is only cropped, resized and compressed,
never recoloured or redrawn.

Usage:
  python3 tools/make_card_images.py           # write images + manifest
  python3 tools/make_card_images.py --check   # exit 1 if an image is missing or stale

Requires Pillow:  pip install --break-system-packages pillow
"""
import hashlib
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
MANIFEST = ROOT / "tools" / "card_images.sha256"
SOURCE_PATHS = (ATLAS, ATLAS_DEUS, BACK, DEUS, pathlib.Path(__file__).resolve())

COLS, ROWS = 12, 4
CELL_W, CELL_H = 176, 286
# Thumbnail size: half the cell. Big enough to read on screen, small in bytes.
THUMB_W, THUMB_H = 88, 143
QUALITY = 82


def card_box(card_id: int):
    col, row = card_id >> 2, card_id & 3
    return (col * CELL_W, row * CELL_H, (col + 1) * CELL_W, (row + 1) * CELL_H)


def file_sha256(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_sha256() -> str:
    """Fingerprint the art and the crop/compression recipe, not the generated files."""
    digest = hashlib.sha256()
    for path in SOURCE_PATHS:
        digest.update(path.relative_to(ROOT).as_posix().encode("utf-8"))
        digest.update(b"\0")
        digest.update(path.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()


def manifest_text(images) -> str:
    lines = [f"source {source_sha256()}"]
    lines.extend(f"{file_sha256(path)}  {path.relative_to(ROOT).as_posix()}" for path in images)
    return "\n".join(lines) + "\n"


def main() -> int:
    check_only = "--check" in sys.argv[1:]
    cards = [OUT / f"card-{i:02d}.webp" for i in range(48)]
    expected = cards + [OUT_DEUS / f"card-{i:02d}.webp" for i in range(48)]
    expected += [OUT / "back.webp", DEUS_OUT, DEUS_ATLAS_OUT]

    if check_only:
        missing = [str(p.relative_to(ROOT)) for p in expected + [MANIFEST] if not p.exists()]
        if missing:
            print(f"missing card images or manifest: {', '.join(missing)}")
            print("Run: python3 tools/make_card_images.py")
            return 1
        lines = MANIFEST.read_text(encoding="utf-8").splitlines()
        stale = []
        if not lines or lines[0] != f"source {source_sha256()}":
            stale.append("source art or generator changed")
        recorded = {}
        for line in lines[1:]:
            try:
                checksum, name = line.split("  ", 1)
                recorded[name] = checksum
            except ValueError:
                stale.append("invalid manifest entry")
        for path in expected:
            name = path.relative_to(ROOT).as_posix()
            if recorded.get(name) != file_sha256(path):
                stale.append(name)
        if stale:
            print(f"stale card images: {', '.join(stale[:12])}")
            if len(stale) > 12:
                print(f"… and {len(stale) - 12} more")
            print("Run: python3 tools/make_card_images.py")
            return 1
        print(f"card images match source art and generator ({len(expected)} files).")
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

    # The mono sheet, for the study page: it must stay 2112 x 1144 and it must be the art.
    # Lossless, deliberately. Lossy WebP round-trips through YUV, and on this sheet that moves
    # every pixel - measured against the source PNG at mean error 10.6/255 at quality 95 and
    # 10.6 at quality 100, max 55, i.e. the grey of the etching is shifted, not softened. That
    # is acceptable on a 88px thumb and not acceptable on the one image a player studies at 1:1.
    # 4.26 MiB against the PNG's 5.49 MiB, and 6.5x smaller than re-encoding it as PNG would be
    # if the atlas had alpha. The file is a build product: delete it and re-run this tool.
    Image.open(ATLAS_DEUS).convert("RGB").save(DEUS_ATLAS_OUT, "WEBP", lossless=True, method=6)

    MANIFEST.write_text(manifest_text(expected), encoding="utf-8")
    total = sum(p.stat().st_size for p in expected)
    print(f"wrote {len(expected)} images ({total / 1024:.0f} KiB total) and {MANIFEST.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
