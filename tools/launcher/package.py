#!/usr/bin/env python3
"""Create the portable Linux archive containing dqix-launcher and its payload."""

from __future__ import annotations

import argparse
import hashlib
import json
import platform
import shutil
import subprocess
import tarfile
from pathlib import Path


def command(args: list[str], cwd: Path) -> str:
    return subprocess.run(
        args, cwd=cwd, check=True, capture_output=True, text=True
    ).stdout.strip()


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--launcher", type=Path, required=True)
    parser.add_argument("--payload", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--version", default="")
    args = parser.parse_args()

    repo = args.repo.resolve()
    launcher = args.launcher.resolve()
    payload = args.payload.resolve()
    out = args.out.resolve()
    if not launcher.is_file() or not launcher.stat().st_mode & 0o111:
        raise SystemExit(f"launcher is missing or not executable: {launcher}")
    manifest_path = payload / "payload.json"
    if not manifest_path.is_file():
        raise SystemExit(f"payload manifest is missing: {manifest_path}")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    for required in (
        "tools/python/bin/python3",
        "include/SDL2/SDL.h",
        "lib/libSDL2-2.0.so.0",
        "source/Makefile",
        "source/third_party/imgui/imgui.h",
    ):
        if not (payload / required).exists():
            raise SystemExit(f"payload is incomplete: {required}")

    arch = platform.machine().lower()
    arch = "arm64" if arch in {"aarch64", "arm64"} else "x86_64"
    version = args.version or manifest.get("version") or command(["git", "rev-parse", "--short=12", "HEAD"], repo)
    out.mkdir(parents=True, exist_ok=True)
    staging = out / "DQIX-Enhanced"
    if staging.exists():
        shutil.rmtree(staging)
    staging.mkdir(parents=True)

    shutil.copy2(launcher, staging / "dqix-launcher")
    shutil.copytree(payload, staging / "payload", symlinks=True)
    shutil.copy2(repo / "LICENSE", staging / "LICENSE")
    shutil.copy2(repo / "README.md", staging / "README.md")
    (staging / "SOURCE-COMMIT.txt").write_text(
        f"Source commit: {manifest.get('source_commit', 'unknown')}\n"
        f"Payload version: {manifest.get('version', version)}\n",
        encoding="utf-8",
    )
    (staging / "INSTALL-LINUX.txt").write_text(
        "DQIX Enhanced — Linux release\n"
        "==============================\n\n"
        "1. Extract this archive. Do not launch it from inside the archive.\n"
        "2. Open the DQIX-Enhanced folder and run ./dqix-launcher.\n"
        "3. Drop your own USA or European Dragon Quest IX .nds ROM onto the window.\n"
        "4. Follow Your ROM, Tools, Options, Build and Play.\n\n"
        "The payload contains the source snapshot, portable Python/PyYAML and SDL2.\n"
        "A C++17 compiler, GNU Make and OpenGL development headers are installed by\n"
        "the launcher when missing. No ROM or generated game code is distributed.\n"
        "The default workspace and all saves remain in your user data directory.\n",
        encoding="utf-8",
    )

    archive = out / f"DQIX-Enhanced-{version}-Linux-{arch}.tar.gz"
    with tarfile.open(archive, "w:gz", compresslevel=6) as tar:
        tar.add(staging, arcname="DQIX-Enhanced", recursive=True)
    sums = out / f"SHA256SUMS-Linux-{arch}.txt"
    sums.write_text(f"{digest(archive)}  {archive.name}\n", encoding="ascii")
    print(f"release: {archive} ({archive.stat().st_size:,} bytes)")
    print(f"release: {sums}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
