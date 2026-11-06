"""Check that the root Makefile has replaced the old shell orchestrator."""

from __future__ import annotations

from pathlib import Path
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

    def test_shell_build_orchestrator_is_removed(self) -> None:
        self.assertTrue((ROOT / "Makefile").is_file())
        self.assertTrue((ROOT / "build.mk").is_file())
        self.assertFalse((ROOT / "tools/build-game.sh").exists())


if __name__ == "__main__":
    unittest.main()
