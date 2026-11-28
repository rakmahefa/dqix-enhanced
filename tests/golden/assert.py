#!/usr/bin/env python3
"""Assert the captured golden corpus against tests/golden/corpus.json.

Checks:
  - every case has a non-empty CRC file (crc_nonempty)
  - every case has at least one captured frame (frame_exists)
  - every interaction-matrix cell (crc_equal / crc_differs /
    crc_equal_postprocess)

`crc_scope` in the manifest says what the CRC covers: engine A's composed
256x192 picture (runtime/gpu2d.c, from g_screen). Anything drawn after that
(GL post-process, HD letter atlas, window layout) cannot appear in it, so the
options in that class are asserted `crc_equal_postprocess`: they must leave the
emulated framebuffer alone, which is exactly what identity proves.

Exits 0 on success, 1 on any failure, 2 if the corpus is not captured yet
(so CI can skip gracefully).
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
MANIFEST = HERE / "corpus.json"
CORPUS = HERE / "corpus"

EQUAL_KINDS = ("crc_equal", "crc_equal_postprocess")


def main():
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if not CORPUS.is_dir() or not any(CORPUS.iterdir()):
        print("golden corpus: not captured yet (run tests/golden/run.sh); skipping")
        return 2

    known_kinds = set(manifest.get("assertions", {}))
    failures = []
    counts = {kind: 0 for kind in EQUAL_KINDS + ("crc_differs",)}
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
        assertion = cell["assertion"]
        if known_kinds and assertion not in known_kinds:
            failures.append(f"{cell['combo']}: unknown assertion {assertion}")
            continue
        pa = CORPUS / cid_a / "crc.txt"
        pb = CORPUS / cid_b / "crc.txt"
        if not (pa.is_file() and pb.is_file()):
            failures.append(f"{cell['combo']}: missing crc for {cid_a} or {cid_b}")
            continue
        a = pa.read_bytes()
        b = pb.read_bytes()
        counts[assertion] = counts.get(assertion, 0) + 1
        if assertion in EQUAL_KINDS:
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
    equal = counts.get("crc_equal", 0)
    post = counts.get("crc_equal_postprocess", 0)
    differs = counts.get("crc_differs", 0)
    print(
        f"golden corpus: all {len(manifest['cases'])} cases and "
        f"{len(manifest['matrix'])} matrix cells OK "
        f"({equal} crc_equal, {post} crc_equal_postprocess, {differs} crc_differs)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
