#!/usr/bin/env python3
"""Validate the minimum Nintendo DS ROM header required by the DQIX build."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys


def inspect_rom(path: Path) -> str:
    try:
        with path.open("rb") as stream:
            header = stream.read(16)
    except OSError as exc:
        raise ValueError(f"cannot read ROM: {exc}") from exc

    if len(header) < 16:
        raise ValueError("file is too small to contain a Nintendo DS header")

    code = header[12:16].decode("ascii", errors="replace")
    if not code.strip() or not code.isalnum():
        raise ValueError("the Nintendo DS game-code field is empty or invalid")

    if code == "YDQE":
        print("   Dragon Quest IX (USA)")
    elif code == "YDQP":
        print("   Dragon Quest IX (Europe)")
    elif code == "YDQJ":
        print("   Dragon Quest IX (Japan): unsupported region; attempting the build may fail")
    else:
        print(f"   Unknown game code {code!r}; attempting the build as the legacy script did")
    return code


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path)
    args = parser.parse_args()
    try:
        inspect_rom(args.rom)
    except ValueError as exc:
        print(f"ROM check failed: {exc}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
