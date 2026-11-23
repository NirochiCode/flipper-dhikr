#!/usr/bin/env python3
"""Check qFlipper screenshots for the Flipper Apps Catalog rules.

Usage: python3 scripts/check_screenshots.py [DIR_OR_PNG ...]
Default: screenshots/catalog and screenshots/qflipper.

Each image must be a PNG, exactly 512x256 (landscape) and use exactly the two
qFlipper colours: black (0,0,0) and orange (254,138,44). Read-only: the files
are never modified. Requires Pillow.
"""
import hashlib
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
PALETTE = {(0, 0, 0), (254, 138, 44)}


def check(path: Path) -> list[str]:
    errors = []
    with Image.open(path) as img:
        img.load()
        if img.format != "PNG":
            errors.append(f"format {img.format}")
        if img.size != (512, 256):
            errors.append(f"size {img.size[0]}x{img.size[1]}")
        rgba = img.convert("RGBA")
        alphas = {px[3] for px in rgba.getdata()}
        if alphas != {255}:
            errors.append("transparent pixels")
        colours = {px[:3] for px in rgba.getdata()}
        if colours != PALETTE:
            extra = sorted(colours - PALETTE)[:4]
            errors.append(f"colours {sorted(colours)[:4]} (unexpected: {extra})")
    return errors


def main() -> int:
    args = [Path(a) for a in sys.argv[1:]] or [ROOT / "screenshots/catalog", ROOT / "screenshots/qflipper"]
    files = []
    for a in args:
        files += sorted(a.glob("*.png")) if a.is_dir() else [a]
    if not files:
        print("no PNG files found")
        return 1
    failed = 0
    for f in files:
        errors = check(f)
        digest = hashlib.sha256(f.read_bytes()).hexdigest()
        name = f.relative_to(ROOT) if f.is_relative_to(ROOT) else f
        print(f"{'OK  ' if not errors else 'FAIL'} {name} sha256={digest}", *errors)
        failed += bool(errors)
    print(f"{len(files) - failed}/{len(files)} valid")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
