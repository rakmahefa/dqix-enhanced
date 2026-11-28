"""Validate the M3 golden corpus manifest and its generator."""
from __future__ import annotations

import json
import os
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GOLDEN = ROOT / "tests" / "golden"
MANIFEST = GOLDEN / "corpus.json"


class GoldenCorpusTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))

    def test_manifest_has_required_fields(self) -> None:
        self.assertIn("version", self.manifest)
        self.assertIn("cases", self.manifest)
        self.assertIn("matrix", self.manifest)
        self.assertGreaterEqual(len(self.manifest["cases"]), 10)

    def test_case_ids_unique(self) -> None:
        ids = [c["id"] for c in self.manifest["cases"]]
        self.assertEqual(len(ids), len(set(ids)))

    def test_every_case_has_settings_and_frames(self) -> None:
        for case in self.manifest["cases"]:
            self.assertIn("settings", case, case["id"])
            self.assertIn("frames", case, case["id"])
            self.assertGreater(case["frames"], 0)

    def test_classic_reference_cases_present(self) -> None:
        ids = {c["id"] for c in self.manifest["cases"]}
        self.assertIn("classic_title", ids)
        self.assertIn("classic_enhanced_opts", ids)
        self.assertIn("classic_wide", ids)

    def test_layouts_covered(self) -> None:
        layouts = {c["settings"].get("layout") for c in self.manifest["cases"] if "layout" in c["settings"]}
        for expected in {"stacked", "side_by_side", "corner", "wide", "wide_dynamic", "two_windows"}:
            self.assertIn(expected, layouts, f"missing layout {expected}")

    def test_battle_and_menu_cases_present(self) -> None:
        tags = {tag for c in self.manifest["cases"] for tag in c.get("tags", [])}
        self.assertIn("battle", tags)
        self.assertIn("menu", tags)

    def test_matrix_cells_reference_existing_cases(self) -> None:
        ids = {c["id"] for c in self.manifest["cases"]}
        for cell in self.manifest["matrix"]:
            self.assertIn(cell["case_a"], ids, cell["combo"])
            self.assertIn(cell["case_b"], ids, cell["combo"])
            self.assertIn(cell["assertion"], {"crc_equal", "crc_differs"})

    def test_gen_cases_produces_files(self) -> None:
        subprocess.run([sys.executable, str(GOLDEN / "gen_cases.py")], check=True)
        for case in self.manifest["cases"]:
            cdir = GOLDEN / "cases" / case["id"]
            self.assertTrue((cdir / "settings.ini").is_file(), case["id"])
            self.assertTrue((cdir / "input.txt").is_file(), case["id"])


if __name__ == "__main__":
    unittest.main()
