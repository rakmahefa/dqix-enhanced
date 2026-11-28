"""The options model must match the code, the docs, the README and the corpus."""
from __future__ import annotations

import importlib.util
import json
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools" / "options_model.py"
MODEL = ROOT / "docs" / "options.json"


class OptionsModelTests(unittest.TestCase):
    def test_model_check_passes(self) -> None:
        result = subprocess.run(
            [sys.executable, str(TOOL)], capture_output=True, text=True, check=False
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_check_passes_without_a_generated_settings_ini(self) -> None:
        # A fresh checkout has no settings.ini (the game writes it on first
        # run), so CI must pass without one. Keep this failure out of CI for
        # good: the tracked-key guarantee comes from settings_save.
        spec = importlib.util.spec_from_file_location("dqix_options_model", TOOL)
        assert spec and spec.loader
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        module.SETTINGS = ROOT / "tests" / "packaging" / "does-not-exist.ini"
        errors = module.check()
        self.assertEqual([e for e in errors if "settings.ini" in e], [])

    def test_every_option_has_maturity_and_rollback(self) -> None:
        model = json.loads(MODEL.read_text(encoding="utf-8"))
        levels = set(model["maturity_levels"])
        for option in model["options"]:
            self.assertIn(option["maturity"], levels, option["id"])
            self.assertTrue(option["rollback"].strip(), option["id"])

    def test_each_maturity_has_entry_and_exit_criteria(self) -> None:
        model = json.loads(MODEL.read_text(encoding="utf-8"))
        for name, level in model["maturity_levels"].items():
            self.assertTrue(level["entry"], name)
            self.assertTrue(level["exit"], name)

    def test_experimental_options_have_a_decision(self) -> None:
        model = json.loads(MODEL.read_text(encoding="utf-8"))
        experimental = {o["id"] for o in model["options"] if o["maturity"] == "experimental"}
        decisions = {d["option"]: d for d in model["experimental_decisions"]}
        self.assertLessEqual(experimental, set(decisions))
        for option_id, decision in decisions.items():
            if option_id not in experimental:
                # a value of a stable option (e.g. layout = wide_dynamic) is
                # allowed to be experimental, but must say which value
                self.assertIn("value", decision, option_id)


if __name__ == "__main__":
    unittest.main()
