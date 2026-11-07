"""Smoke-check actual PNG captures produced by dqix-launcher --preview."""

from __future__ import annotations

import os
from pathlib import Path
import struct
import unittest


class LauncherScreenshotTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.directory = Path(
            os.environ.get("DQIX_LAUNCHER_SCREENSHOTS", "build/launcher-screenshots")
        )
        if not cls.directory.is_dir():
            raise unittest.SkipTest("launcher preview screenshots have not been generated")

    def test_all_launcher_pages_have_bmp_screenshots(self) -> None:
        expected = {
            "launcher-disc-empty.png",
            "launcher-tools-missing.png",
            "launcher-tools-ready.png",
            "launcher-options.png",
            "launcher-building.png",
            "launcher-build-failed.png",
            "launcher-play.png",
        }
        for name in sorted(expected):
            path = self.directory / name
            self.assertTrue(path.is_file(), f"missing screenshot: {name}")
            self.assertGreater(path.stat().st_size, 1_000, f"screenshot is unexpectedly small: {name}")
            with path.open("rb") as stream:
                header = stream.read(24)
            self.assertEqual(header[:8], b"\x89PNG\r\n\x1a\n", f"{name} is not a PNG")
            width, height = struct.unpack_from(">II", header, 16)
            self.assertGreaterEqual(width, 1100, name)
            self.assertGreaterEqual(height, 700, name)


if __name__ == "__main__":
    unittest.main()
