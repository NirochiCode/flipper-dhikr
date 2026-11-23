#!/usr/bin/env python3
"""Generate the 10x10 1-bit app icon (crescent with a five-pointed star).

Usage: python3 scripts/generate_icon.py [--check] [--preview PATH]

'#' = black (drawn on the Flipper display), '.' = white. Requires Pillow.
--check validates the existing file without writing it.
--preview writes an enlarged nearest-neighbour copy to PATH (never the icon).
"""
import argparse
import sys
from pathlib import Path

from PIL import Image

ICON = Path(__file__).resolve().parent.parent / "assets" / "flipper_dhikr_10px.png"

MATRIX = [
    "....###...",
    "..####....",
    ".###...#..",
    ".##...#.#.",
    "###..#####",
    "###...###.",
    ".##...#.#.",
    ".###......",
    "..####....",
    "....###...",
]


def build() -> Image.Image:
    img = Image.new("1", (10, 10), 1)  # 1 = white
    for y, row in enumerate(MATRIX):
        for x, ch in enumerate(row):
            img.putpixel((x, y), 0 if ch == "#" else 1)
    return img


def validate(path: Path) -> list[str]:
    errors = []
    with Image.open(path) as img:
        img.load()
        if img.format != "PNG":
            errors.append(f"format {img.format}, expected PNG")
        if img.size != (10, 10):
            errors.append(f"size {img.size}, expected (10, 10)")
        if img.mode != "1":
            errors.append(f"mode {img.mode}, expected 1 (1-bit)")
        if "A" in img.getbands() or "transparency" in img.info:
            errors.append("has alpha/transparency")
        if not errors:
            for y, row in enumerate(MATRIX):
                for x, ch in enumerate(row):
                    black = img.getpixel((x, y)) == 0
                    if black != (ch == "#"):
                        errors.append(f"pixel ({x},{y}) mismatch")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--preview", type=Path)
    parser.add_argument("--scale", type=int, default=32)
    args = parser.parse_args()

    assert len(MATRIX) == 10 and all(len(r) == 10 and set(r) <= {"#", "."} for r in MATRIX)

    if not args.check:
        build().save(ICON, format="PNG", optimize=True)

    errors = validate(ICON)
    if errors:
        print("INVALID:", *errors, sep="\n  ")
        return 1
    print(f"OK: {ICON.name} is 10x10, 1-bit, no alpha, matches the matrix")

    if args.preview:
        if args.preview.resolve() == ICON:
            print("refusing to overwrite the icon with the preview")
            return 1
        with Image.open(ICON) as img:
            img.resize((10 * args.scale, 10 * args.scale), Image.NEAREST).save(args.preview)
        print(f"preview: {args.preview}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
