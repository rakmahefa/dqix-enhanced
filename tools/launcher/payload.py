#!/usr/bin/env python3
"""Assemble the Linux release payload: source, portable Python/PyYAML and SDL2."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request
import zipfile
from pathlib import Path, PurePosixPath

PYTHON_RELEASE = "20261003"
PYTHON_VERSION = "3.12.15"
PYTHON_BUILDS = {
    "x86_64": (
        "x86_64-unknown-linux-gnu",
        "731af898886c5f821890dc901eca3c651cca8e51fa7308c159d12a1194aeac91",
    ),
    "aarch64": (
        "aarch64-unknown-linux-gnu",
        "6541297dd1798dec8b98c3ad7492808a5b9d1c126801ceb2011e7754cd20d1ce",
    ),
}
PYAML_VERSION = "6.0.2"
PYAML_WHEELS = {
    "x86_64": (
        "manylinux2014_x86_64",
        "80bab7bfc629882493af4aa31a4cfa43a4c57c83813253626916b8c7ada83476",
    ),
    "aarch64": (
        "manylinux2014_aarch64",
        "1f71ea527786de97d1a0cc0eacd1defc0985dcf6b3f17bb77dcfc8c34bec4dc5",
    ),
}
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


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def download(url: str, expected_sha256: str, destination: Path) -> Path:
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.is_file() and sha256(destination) == expected_sha256:
        return destination
    if destination.exists():
        destination.unlink()
    print(f"payload: downloading {destination.name}", flush=True)
    request = urllib.request.Request(url, headers={"User-Agent": "DQIX-Enhanced-release/1"})
    with urllib.request.urlopen(request, timeout=120) as response, destination.open("wb") as out:
        shutil.copyfileobj(response, out)
    actual = sha256(destination)
    if actual != expected_sha256:
        destination.unlink(missing_ok=True)
        raise RuntimeError(
            f"SHA-256 mismatch for {url}: expected {expected_sha256}, got {actual}"
        )
    return destination


def safe_extract_tar(archive: Path, destination: Path) -> None:
    destination.mkdir(parents=True, exist_ok=True)
    root = destination.resolve()
    with tarfile.open(archive, "r:gz") as tar:
        for member in tar.getmembers():
            target = (destination / member.name).resolve()
            if target != root and root not in target.parents:
                raise RuntimeError(f"unsafe path in Python archive: {member.name}")
            if member.issym() or member.islnk():
                link = (target.parent / member.linkname).resolve()
                if link != root and root not in link.parents:
                    raise RuntimeError(f"unsafe link in Python archive: {member.name}")
        tar.extractall(destination)


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


def build_python(arch: str, out: Path, cache: Path, work: Path) -> dict[str, str]:
    triple, expected = PYTHON_BUILDS[arch]
    archive_name = f"cpython-{PYTHON_VERSION}+{PYTHON_RELEASE}-{triple}-install_only_stripped.tar.gz"
    url = (
        f"https://github.com/astral-sh/python-build-standalone/releases/download/{PYTHON_RELEASE}/"
        + archive_name.replace("+", "%2B")
    )
    archive = download(url, expected, cache / archive_name)
    tools_dir = out / "tools"
    safe_extract_tar(archive, tools_dir)
    python = tools_dir / "python/bin/python3"
    if not python.is_file():
        raise RuntimeError("the standalone Python archive did not contain tools/python/bin/python3")

    platform_tag, expected_wheel = PYAML_WHEELS[arch]
    wheel_dir = work / "wheels"
    wheel_dir.mkdir(parents=True, exist_ok=True)
    pip_command = [
        sys.executable, "-m", "pip", "download",
        "--only-binary=:all:", "--no-deps",
        "--implementation", "cp", "--python-version", "3.12", "--abi", "cp312",
        "--platform", platform_tag, "--dest", str(wheel_dir),
        f"PyYAML=={PYAML_VERSION}",
    ]
    print("payload: downloading the pinned PyYAML wheel", flush=True)
    run(pip_command)
    wheels = list(wheel_dir.glob("PyYAML-*.whl")) + list(wheel_dir.glob("pyyaml-*.whl"))
    if len(wheels) != 1:
        raise RuntimeError(f"expected one PyYAML wheel, found {len(wheels)}")
    wheel = wheels[0]
    actual = sha256(wheel)
    if actual != expected_wheel:
        raise RuntimeError(
            f"PyYAML wheel SHA-256 mismatch: expected {expected_wheel}, got {actual}"
        )

    site_packages = tools_dir / "python/lib/python3.12/site-packages"
    site_packages.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(wheel) as archive_file:
        for name in archive_file.namelist():
            target = PurePosixPath(name)
            if target.is_absolute() or ".." in target.parts:
                raise RuntimeError(f"unsafe path in PyYAML wheel: {name}")
        archive_file.extractall(site_packages)
        licenses = [
            name for name in archive_file.namelist()
            if "license" in name.lower() and not name.endswith("/")
        ]
        if licenses:
            license_path = out / "licenses/PyYAML/LICENSE"
            license_path.parent.mkdir(parents=True, exist_ok=True)
            license_path.write_bytes(archive_file.read(licenses[0]))

    python_license = next(iter(sorted(tools_dir.glob("python/**/LICENSE.txt"))), None)
    if python_license is None:
        python_license = next(iter(sorted(tools_dir.glob("python/**/LICENSE"))), None)
    if python_license is None:
        raise RuntimeError("the standalone Python distribution has no license file")
    license_dir = out / "licenses/Python"
    license_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(python_license, license_dir / python_license.name)

    probe = run([python, "-c", "import yaml, sys; print(sys.version.split()[0], yaml.__version__)"])
    print(f"payload: bundled Python and PyYAML {probe}", flush=True)
    return {
        "python": "tools/python/bin/python3",
        "python_version": PYTHON_VERSION,
        "pyyaml_version": PYAML_VERSION,
    }


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
    parser.add_argument("--cache", type=Path)
    parser.add_argument("--work", type=Path)
    parser.add_argument("--version", default="")
    args = parser.parse_args()

    if platform.system() != "Linux":
        parser.error("the first portable payload targets Linux only")
    arch = host_arch()
    repo = args.repo.resolve()
    out = args.out.resolve()
    cache = (args.cache or out.parent / "payload-cache").resolve()
    work = (args.work or out.parent / "payload-work").resolve()
    if out.exists():
        shutil.rmtree(out)
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)

    copy_source(repo, out / "source")
    python_info = build_python(arch, out, cache, work)
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
            "python": python_info["python"],
            "sdl2_include": "include/SDL2",
            "sdl2_library": sdl_info["sdl2_library"],
        },
        "versions": {
            "Python": python_info["python_version"],
            "PyYAML": python_info["pyyaml_version"],
            "SDL2": sdl_info["sdl2_version"],
        },
        "system_requirements": ["C compiler", "GNU Make", "OpenGL development headers"],
    }
    (out / "payload.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    notices = out / "licenses/README.txt"
    notices.parent.mkdir(parents=True, exist_ok=True)
    notices.write_text(
        "DQIX Enhanced Linux payload notices\n"
        "====================================\n"
        "Python and PyYAML license texts are in their respective folders.\n"
        "SDL2 retains its upstream license; see SDL2/copyright.\n"
        "Dear ImGui is bundled as source in source/third_party/imgui and retains its MIT license.\n"
        "The host system supplies the C++ compiler and GNU Make; neither is bundled.\n",
        encoding="utf-8",
    )
    print(f"payload: ready at {out} (linux-{arch}, {version})", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
