"""Regression test for the runtime environment and argv used by play.sh."""

from __future__ import annotations

import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class PlayScriptTests(unittest.TestCase):
    def test_bundled_sdl_path_and_game_arguments_are_preserved_with_spaces(self) -> None:
        with tempfile.TemporaryDirectory(prefix="dqix game workspace ") as temp:
            game = Path(temp) / "game folder"
            game.mkdir()
            probe = game / "dqix"
            probe.write_text(
                "#!/bin/sh\n"
                "printf 'LD_LIBRARY_PATH=<%s>\\n' \"$LD_LIBRARY_PATH\"\n"
                "printf 'PWD=<%s>\\n' \"$PWD\"\n"
                "printf 'ARGS=<%s>\\n' \"$*\"\n",
                encoding="utf-8",
            )
            probe.chmod(0o755)
            script = game / "play.sh"
            script.write_bytes((ROOT / "tools/play.sh").read_bytes())
            script.chmod(0o755)

            env = os.environ.copy()
            env["LD_LIBRARY_PATH"] = "/opt/existing-libraries"
            result = subprocess.run([str(script)], check=True, capture_output=True, text=True, env=env)

            expected_path = f"{game}:/opt/existing-libraries"
            self.assertIn(f"LD_LIBRARY_PATH=<{expected_path}>", result.stdout)
            self.assertIn(f"PWD=<{game}>", result.stdout)
            self.assertIn("ARGS=<data/dqix.nds data/code 0 1 dqix.sav>", result.stdout)


if __name__ == "__main__":
    unittest.main()
