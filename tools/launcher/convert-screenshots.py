#!/usr/bin/env python3
"""Convert launcher preview BMP captures to portable PNG files."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

from PIL import Image


def convert(directory: Path) -> list[Path]:
    if not directory.is_dir():
        raise ValueError(f"screenshot directory does not exist: {directory}")
    sources = sorted(directory.glob("launcher-*.bmp"))
    if not sources:
        raise ValueError(f"no launcher BMP screenshots found in {directory}")
    outputs: list[Path] = []
    for source in sources:
        destination = source.with_suffix(".png")
        with Image.open(source) as image:
            image.convert("RGBA").save(destination, format="PNG", optimize=True)
        outputs.append(destination)
        print(f"screenshot: {destination}")
    return outputs


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path, help="directory containing launcher-*.bmp captures")
    args = parser.parse_args()
    try:
        convert(args.directory)
    except (OSError, ValueError) as exc:
        print(f"screenshot conversion failed: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
