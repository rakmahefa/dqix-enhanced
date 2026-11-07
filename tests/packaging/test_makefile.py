"""Check that the root Makefile has replaced the old shell orchestrator."""

from __future__ import annotations

from pathlib import Path
import tempfile
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]


class RootMakefileTests(unittest.TestCase):
    def test_help_target_is_executable(self) -> None:
        result = subprocess.run(
            ["make", "--no-print-directory", "help"],
            cwd=ROOT, check=True, capture_output=True, text=True,
        )
        for target in ("make build", "make test", "make launcher", "make screenshots"):
            self.assertIn(target, result.stdout)

    def test_makefile_keeps_the_lower_level_build_mk(self) -> None:
        makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
        self.assertIn("build.mk", makefile)
        for target in (
            "check-rom:", "extract:", "region:", "recompile-arm9:",
            "recompile-arm7:", "compile:", "assemble:", "build:", "run:",
        ):
            self.assertIn(target, makefile)

    def test_workspace_paths_with_spaces_are_kept_as_single_make_arguments(self) -> None:
        with tempfile.TemporaryDirectory(prefix="dqix workspace ") as directory:
            root = Path(directory)
            rom = root / "my personal ROM.nds"
            header = bytearray(0x160)
            header[0x0C:0x10] = b"YDQE"
            rom.write_bytes(header)
            workspace = root / "game workspace"
            result = subprocess.run(
                [
                    "make", "--no-print-directory", "--dry-run", "build",
                    f"ROM={rom}", f"WORKSPACE={workspace}",
                ],
                cwd=ROOT, check=False, capture_output=True, text=True,
            )
            output = (result.stdout + chr(10) + result.stderr)[-8000:]
            self.assertEqual(result.returncode, 0, f"make --dry-run failed:{chr(10)}{output}")
            self.assertIn(str(rom), result.stdout)
            self.assertIn(str(workspace), result.stdout)
            self.assertIn("make -C", result.stdout)

    def test_shell_build_orchestrator_is_removed(self) -> None:
        self.assertTrue((ROOT / "Makefile").is_file())
        self.assertTrue((ROOT / "build.mk").is_file())
        self.assertFalse((ROOT / "tools/build-game.sh").exists())


if __name__ == "__main__":
    unittest.main()
