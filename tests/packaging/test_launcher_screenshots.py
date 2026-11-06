"""Smoke-check actual BMP captures produced by dqix-launcher --preview."""

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
            "launcher-disc-empty.bmp",
            "launcher-tools-missing.bmp",
            "launcher-tools-ready.bmp",
            "launcher-options.bmp",
            "launcher-building.bmp",
            "launcher-build-failed.bmp",
            "launcher-play.bmp",
        }
        for name in sorted(expected):
            path = self.directory / name
            self.assertTrue(path.is_file(), f"missing screenshot: {name}")
            self.assertGreater(path.stat().st_size, 25_000, f"screenshot is unexpectedly small: {name}")
            with path.open("rb") as stream:
                header = stream.read(26)
            self.assertEqual(header[:2], b"BM", f"{name} is not a BMP")
            width, height = struct.unpack_from("<ii", header, 18)
            self.assertGreaterEqual(width, 1100, name)
            self.assertGreaterEqual(height, 700, name)


if __name__ == "__main__":
    unittest.main()
