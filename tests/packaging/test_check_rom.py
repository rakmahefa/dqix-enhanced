"""Tests for the ROM header check run before extraction."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("check_rom", ROOT / "tools/check-rom.py")
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class CheckRomTests(unittest.TestCase):
    def test_rejects_a_truncated_file(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            rom = Path(directory) / "short.nds"
            rom.write_bytes(b"not a Nintendo DS ROM")
            with self.assertRaisesRegex(ValueError, "too small"):
                MODULE.inspect_rom(rom)

    def test_accepts_usa_game_code(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            rom = Path(directory) / "usa.nds"
            rom.write_bytes(bytes(12) + b"YDQE")
            self.assertEqual(MODULE.inspect_rom(rom), "YDQE")

    def test_accepts_europe_game_code(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            rom = Path(directory) / "europe.nds"
            rom.write_bytes(bytes(12) + b"YDQP")
            self.assertEqual(MODULE.inspect_rom(rom), "YDQP")


if __name__ == "__main__":
    unittest.main()
