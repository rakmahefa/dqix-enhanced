"""Check the release artifact contract and that the archive is reproducible.

tools/launcher/package.py is what a player downloads, so its name, layout,
checksum and determinism are part of the M5 contract, not details.
"""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
import tarfile
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
PACKAGE = ROOT / "tools/launcher/package.py"
NAME = re.compile(r"^DQIX-Enhanced-.+-Linux-(x86_64|arm64)\.tar\.gz$")
VERSION = "v0.0-contract"


def sha256(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            value.update(block)
    return value.hexdigest()


class ArtifactContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.temp = tempfile.TemporaryDirectory(prefix="dqix-contract-")
        base = Path(cls.temp.name)
        cls.payload = base / "payload"
        for relative in (
            "include/SDL2/SDL.h",
            "lib/libSDL2-2.0.so.0",
            "source/Makefile",
            "source/third_party/imgui/imgui.h",
        ):
            target = cls.payload / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text("contract\n", encoding="utf-8")
        (cls.payload / "payload.json").write_text(
            json.dumps({"version": VERSION, "source_commit": "0" * 12}) + "\n",
            encoding="utf-8",
        )
        cls.launcher = base / "dqix-launcher"
        cls.launcher.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
        cls.launcher.chmod(0o755)
        cls.outputs = []
        for index in (1, 2):
            out = base / f"out{index}"
            result = subprocess.run(
                [
                    sys.executable, str(PACKAGE),
                    "--repo", str(ROOT),
                    "--launcher", str(cls.launcher),
                    "--payload", str(cls.payload),
                    "--out", str(out),
                    "--version", VERSION,
                ],
                cwd=ROOT, check=False, capture_output=True, text=True,
            )
            if result.returncode != 0:
                raise AssertionError(f"package.py failed:\n{result.stdout}\n{result.stderr}")
            cls.outputs.append(out)
        cls.archive = next(cls.outputs[0].glob("DQIX-Enhanced-*.tar.gz"))

    @classmethod
    def tearDownClass(cls) -> None:
        cls.temp.cleanup()

    def test_archive_name_matches_the_contract(self) -> None:
        self.assertRegex(self.archive.name, NAME)
        self.assertIn(f"DQIX-Enhanced-{VERSION}-Linux-", self.archive.name)

    def test_checksum_file_names_the_archive(self) -> None:
        arch = "arm64" if platform.machine().lower() in {"aarch64", "arm64"} else "x86_64"
        sums = self.outputs[0] / f"SHA256SUMS-Linux-{arch}.txt"
        self.assertTrue(sums.is_file(), f"missing {sums.name}")
        self.assertEqual(sums.read_text(encoding="ascii"), f"{sha256(self.archive)}  {self.archive.name}\n")

    def test_archive_contains_an_executable_launcher_and_the_payload(self) -> None:
        with tarfile.open(self.archive, "r:gz") as tar:
            members = {member.name: member for member in tar.getmembers()}
        launcher = members.get("DQIX-Enhanced/dqix-launcher")
        self.assertIsNotNone(launcher, "launcher missing from the archive")
        assert launcher is not None
        self.assertTrue(launcher.mode & 0o111, "launcher is not executable in the archive")
        self.assertIn("DQIX-Enhanced/payload/payload.json", members)
        self.assertIn("DQIX-Enhanced/LICENSE", members)
        self.assertIn("DQIX-Enhanced/INSTALL-LINUX.txt", members)
        self.assertIn("DQIX-Enhanced/SOURCE-COMMIT.txt", members)

    def test_archive_carries_no_rom_saves_or_generated_game(self) -> None:
        with tarfile.open(self.archive, "r:gz") as tar:
            names = [member.name for member in tar.getmembers()]
        forbidden = [
            name for name in names
            if name.lower().endswith((".nds", ".sav", ".o", ".pyc"))
            or "/data/code/" in name
            or name.endswith("/game/dqix")
        ]
        self.assertEqual(forbidden, [], f"release archive contains generated game data: {forbidden}")

    def test_provenance_file_records_commit_and_version(self) -> None:
        with tarfile.open(self.archive, "r:gz") as tar:
            extracted = tar.extractfile("DQIX-Enhanced/SOURCE-COMMIT.txt")
            assert extracted is not None
            text = extracted.read().decode("utf-8")
        self.assertIn("Source commit: " + "0" * 12, text)
        self.assertIn("Payload version: " + VERSION, text)

    def test_same_input_produces_the_same_archive(self) -> None:
        other = next(self.outputs[1].glob("DQIX-Enhanced-*.tar.gz"))
        self.assertEqual(sha256(self.archive), sha256(other), "release archive is not reproducible")

    def test_members_are_sorted_and_owned_by_nobody_in_particular(self) -> None:
        with tarfile.open(self.archive, "r:gz") as tar:
            members = tar.getmembers()
        names = [member.name for member in members]
        self.assertEqual(names, sorted(names), "archive members are not in a stable order")
        for member in members:
            self.assertEqual((member.uid, member.gid), (0, 0), member.name)
            self.assertEqual((member.uname, member.gname), ("", ""), member.name)
            self.assertEqual(member.mtime, 0, member.name)


if __name__ == "__main__":
    unittest.main()
