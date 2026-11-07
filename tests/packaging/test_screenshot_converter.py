"""Tests conversion of launcher preview BMPs into PNG files."""

from __future__ import annotations

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]


class ScreenshotConverterTests(unittest.TestCase):
    def test_bmp_preview_is_converted_to_png(self) -> None:
        with tempfile.TemporaryDirectory(prefix="dqix screenshots ") as temp:
            directory = Path(temp)
            source = directory / "launcher-preview.bmp"
            Image.new("RGB", (1280, 820), (24, 32, 56)).save(source, format="BMP")
            subprocess.run(
                [sys.executable, str(ROOT / "tools/launcher/convert-screenshots.py"), str(directory)],
                check=True, capture_output=True, text=True,
            )
            output = directory / "launcher-preview.png"
            self.assertTrue(output.is_file())
            with Image.open(output) as image:
                self.assertEqual(image.size, (1280, 820))
                self.assertEqual(image.mode, "RGBA")


if __name__ == "__main__":
    unittest.main()
