#!/usr/bin/env python3
"""Assemble the Linux release payload: tracked source and SDL2."""

from __future__ import annotations

import argparse
import json
import os
import platform
import shutil
import subprocess
from pathlib import Path, PurePosixPath

SDL2_VERSION_MIN = (2, 0, 18)
PRIVATE_ROOTS = {
    ".git", ".worktrees", "build", "game", "rom", "extract", "obj",
    "gen", "gen7", "release", "reports", "bookmarks", "session", "update",
    "testbuild",
}
PRIVATE_SUFFIXES = {".nds", ".sav", ".o", ".exe", ".dll", ".so", ".pyc"}


def run(args: list[str], *, cwd: Path | None = None) -> str:
    completed = subprocess.run(
        [str(arg) for arg in args],
        cwd=cwd,
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    return completed.stdout.strip()


def host_arch() -> str:
    machine = platform.machine().lower()
    if machine in {"x86_64", "amd64"}:
        return "x86_64"
    if machine in {"aarch64", "arm64"}:
        return "aarch64"
    raise RuntimeError(f"unsupported Linux architecture: {machine}")


def source_files(repo: Path) -> list[str]:
    output = run(["git", "ls-files", "--recurse-submodules", "-z"], cwd=repo)
    paths = [item for item in output.split("\0") if item]
    if not paths:
        raise RuntimeError("git returned no tracked source files; run this in a checkout")
    return paths


def copy_source(repo: Path, destination: Path) -> None:
    print("payload: copying tracked source and the pinned ImGui submodule", flush=True)
    destination.mkdir(parents=True, exist_ok=True)
    copied = 0
    for relative in source_files(repo):
        posix = PurePosixPath(relative)
        if posix.parts and posix.parts[0] in PRIVATE_ROOTS:
            continue
        if posix.suffix.lower() in PRIVATE_SUFFIXES:
            continue
        if any(part in {".git", "__pycache__"} for part in posix.parts):
            continue
        source = repo.joinpath(*posix.parts)
        if not source.exists() and not source.is_symlink():
            continue
        target = destination.joinpath(*posix.parts)
        target.parent.mkdir(parents=True, exist_ok=True)
        if source.is_symlink():
            target.symlink_to(os.readlink(source))
        elif source.is_file():
            shutil.copy2(source, target)
            copied += 1
    if not (destination / "third_party/imgui/imgui.h").is_file():
        raise RuntimeError(
            "Dear ImGui submodule contents are missing. "
            "Initialize submodules recursively before building the payload."
        )
    if not (destination / "Makefile").is_file():
        raise RuntimeError("the tracked source snapshot is missing the root Makefile")
    print(f"payload: copied {copied} tracked files", flush=True)


def copy_sdl2(out: Path) -> dict[str, str]:
    print("payload: collecting the SDL2 runtime and headers from the pinned CI image", flush=True)
    include_root = Path(run(["pkg-config", "--variable=includedir", "sdl2"])) / "SDL2"
    lib_root = Path(run(["pkg-config", "--variable=libdir", "sdl2"]))
    library = lib_root / "libSDL2-2.0.so.0"
    if not include_root.is_dir() or not (include_root / "SDL.h").is_file():
        raise RuntimeError("SDL2 development headers are missing; install libsdl2-dev")
    if not library.exists():
        candidates = list(lib_root.glob("libSDL2-2.0.so.0.*"))
        if not candidates:
            raise RuntimeError("SDL2 runtime library libSDL2-2.0.so.0 was not found")
        library = candidates[0]

    include_destination = out / "include/SDL2"
    shutil.copytree(include_root, include_destination, dirs_exist_ok=True)

    # Debian/Ubuntu ship a generic SDL_config.h wrapper which includes the
    # architecture-specific header from /usr/include/<multiarch>/SDL2. That
    # system path is not present on users' machines, so make the payload
    # self-contained and resolve the wrapper relative to its own directory.
    config_header = include_destination / "SDL_config.h"
    config_text = config_header.read_text(encoding="utf-8") if config_header.is_file() else ""
    debian_include = "#include <SDL2/_real_SDL_config.h>"
    if debian_include in config_text:
        multiarch = run(["cc", "-print-multiarch"])
        candidates = []
        if multiarch:
            candidates.append(Path("/usr/include") / multiarch / "SDL2" / "_real_SDL_config.h")
        candidates.extend(sorted(Path("/usr/include").glob("*/SDL2/_real_SDL_config.h")))
        real_config = next((path for path in candidates if path.is_file()), None)
        if real_config is None:
            raise RuntimeError(
                "Debian SDL2 headers use an architecture-specific SDL_config.h, "
                "but cc -print-multiarch did not resolve _real_SDL_config.h"
            )
        shutil.copy2(real_config, include_destination / "_real_SDL_config.h")
        config_header.write_text(
            config_text.replace(debian_include, '#include "_real_SDL_config.h"'),
            encoding="utf-8",
        )
        print(f"payload: bundled SDL2 configuration for {multiarch}", flush=True)

    lib_destination = out / "lib"
    lib_destination.mkdir(parents=True, exist_ok=True)
    shutil.copy2(library.resolve(), lib_destination / "libSDL2-2.0.so.0")
    link = lib_destination / "libSDL2.so"
    if link.exists() or link.is_symlink():
        link.unlink()
    link.symlink_to("libSDL2-2.0.so.0")

    sdl_version = run(["pkg-config", "--modversion", "sdl2"])
    if tuple(int(part) for part in sdl_version.split(".")[:3]) < SDL2_VERSION_MIN:
        raise RuntimeError(f"SDL2 {sdl_version} is too old; need at least 2.0.18")
    notice_candidates = [
        Path("/usr/share/doc/libsdl2-2.0-0/copyright"),
        Path("/usr/share/doc/libsdl2-dev/copyright"),
    ]
    notice = next((path for path in notice_candidates if path.is_file()), None)
    if notice:
        license_dir = out / "licenses/SDL2"
        license_dir.mkdir(parents=True, exist_ok=True)
        shutil.copy2(notice, license_dir / "copyright")
    return {"sdl2_version": sdl_version, "sdl2_library": "lib/libSDL2-2.0.so.0"}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--version", default="")
    args = parser.parse_args()

    if platform.system() != "Linux":
        parser.error("the first portable payload targets Linux only")
    arch = host_arch()
    repo = args.repo.resolve()
    out = args.out.resolve()
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True, exist_ok=True)

    copy_source(repo, out / "source")
    sdl_info = copy_sdl2(out)
    commit = run(["git", "rev-parse", "--short=12", "HEAD"], cwd=repo)
    version = args.version or commit
    manifest = {
        "project": "DQIX Enhanced",
        "version": version,
        "source_commit": commit,
        "platform": f"linux-{arch}",
        "paths": {
            "source": "source",
            "sdl2_include": "include/SDL2",
            "sdl2_library": sdl_info["sdl2_library"],
        },
        "versions": {
            "SDL2": sdl_info["sdl2_version"],
        },
        "system_requirements": [
            "Python 3",
            "PyYAML for Python 3",
            "C compiler",
            "GNU Make",
            "OpenGL development headers",
        ],
    }
    (out / "payload.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    notices = out / "licenses/README.txt"
    notices.parent.mkdir(parents=True, exist_ok=True)
    notices.write_text(
        "DQIX Enhanced Linux payload notices\n"
        "====================================\n"
        "Python and PyYAML are provided by the host Linux distribution.\n"
        "SDL2 retains its upstream license; see SDL2/copyright.\n"
        "Dear ImGui is bundled as source in source/third_party/imgui and retains its MIT license.\n"
        "The host system supplies Python 3, PyYAML, the C/C++ compilers and GNU Make.\n",
        encoding="utf-8",
    )
    print(f"payload: ready at {out} (linux-{arch}, {version})", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
