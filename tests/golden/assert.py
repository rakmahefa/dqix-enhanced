#!/usr/bin/env python3
"""Assert the captured golden corpus against tests/golden/corpus.json.

Checks:
  - every case has a non-empty CRC file (crc_nonempty)
  - every case has at least one captured frame (frame_exists)
  - every interaction-matrix cell (crc_equal / crc_differs)

Exits 0 on success, 1 on any failure, 2 if the corpus is not captured yet
(so CI can skip gracefully).
"""
from __future__ import annotations

import json
import os
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
MANIFEST = HERE / "corpus.json"
CORPUS = HERE / "corpus"


def main():
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if not CORPUS.is_dir() or not any(CORPUS.iterdir()):
        print("golden corpus: not captured yet (run tests/golden/run.sh); skipping")
        return 2

    failures = []
    # per-case basic checks
    for case in manifest["cases"]:
        cid = case["id"]
        cdir = CORPUS / cid
        crc = cdir / "crc.txt"
        frames = cdir / "frames"
        if not crc.is_file() or crc.stat().st_size == 0:
            failures.append(f"{cid}: missing or empty crc.txt")
            continue
        if not frames.is_dir() or not any(frames.iterdir()):
            failures.append(f"{cid}: no captured frames")
    # matrix cells
    for cell in manifest["matrix"]:
        cid_a = cell["case_a"]
        cid_b = cell["case_b"]
        pa = CORPUS / cid_a / "crc.txt"
        pb = CORPUS / cid_b / "crc.txt"
        if not (pa.is_file() and pb.is_file()):
            failures.append(f"{cell['combo']}: missing crc for {cid_a} or {cid_b}")
            continue
        a = pa.read_bytes()
        b = pb.read_bytes()
        assertion = cell["assertion"]
        if assertion == "crc_equal":
            if a != b:
                failures.append(f"{cell['combo']}: expected identical CRCs but they differ")
        elif assertion == "crc_differs":
            if a == b:
                failures.append(f"{cell['combo']}: expected differing CRCs but they are identical")
        else:
            failures.append(f"{cell['combo']}: unknown assertion {assertion}")

    if failures:
        print("golden corpus assertions FAILED:")
        for f in failures:
            print("  - " + f)
        return 1
    print(f"golden corpus: all {len(manifest['cases'])} cases and {len(manifest['matrix'])} matrix cells OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
