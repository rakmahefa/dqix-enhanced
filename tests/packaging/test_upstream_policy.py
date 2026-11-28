"""Guard the upstream branch cleanup and the M5 ownership map."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]


def load_ownership():
    spec = importlib.util.spec_from_file_location("dqix_ownership", ROOT / "tools/ownership.py")
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class UpstreamPolicyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        result = subprocess.run(
            ["git", "ls-tree", "-r", "--name-only", "HEAD"],
            check=True, capture_output=True, text=True,
        )
        cls.paths = set(result.stdout.splitlines())

    def test_removed_appimage_workflow_stays_removed(self) -> None:
        workflows = sorted(
            path for path in self.paths
            if path.startswith(".github/workflows/") and "appimage" in path.lower()
        )
        self.assertEqual(workflows, [], f"removed AppImage workflows were restored: {workflows}")

    def test_removed_appimage_sources_stay_removed(self) -> None:
        forbidden = {
            "docs/LINUX-APPIMAGE.md",
            "packaging/linux/AppRun",
            "packaging/linux/dqix-enhanced-builder.desktop",
            "packaging/linux/dqix-enhanced-builder.svg",
            "packaging/linux/environment.yml",
            "packaging/linux/gui.py",
            "tools/build-game.sh",
        }
        found = sorted(forbidden & self.paths)
        self.assertEqual(found, [], f"removed paths were reintroduced: {found}")
        packaging = sorted(path for path in self.paths if path.startswith("packaging/linux/"))
        self.assertEqual(packaging, [], f"old Linux AppImage packaging returned: {packaging}")

    def test_makefile_replacement_and_existing_native_linux_build_remain(self) -> None:
        self.assertIn("Makefile", self.paths)
        self.assertIn("build.mk", self.paths)
        self.assertNotIn("tools/build-game.sh", self.paths)
        self.assertIn("tools/where.sh", self.paths)
        self.assertIn("docs/TECHNICAL.md", self.paths)

    def test_every_tracked_path_has_one_ownership_category(self) -> None:
        ownership = load_ownership()
        paths = ownership.tracked_paths()
        problems = ownership.check(paths)
        self.assertEqual(problems, [], "ownership map problems:\n" + "\n".join(problems))
        for path in paths:
            category, _ = ownership.classify(path)
            self.assertIn(category, ownership.CATEGORIES, path)

    def test_ownership_document_is_generated_from_the_rules(self) -> None:
        ownership = load_ownership()
        paths = ownership.tracked_paths()
        document = (ROOT / "docs/OWNERSHIP.md").read_text(encoding="utf-8")
        self.assertEqual(document, ownership.render(paths), "docs/OWNERSHIP.md is stale; run tools/ownership.py --write")

    def test_conflict_set_is_declared_and_small(self) -> None:
        ownership = load_ownership()
        shared = sorted(
            path for path in self.paths if ownership.classify(path)[0] == "shared"
        )
        self.assertEqual(
            shared,
            [
                ".github/workflows/linux-release.yml",
                ".gitignore",
                "Makefile",
                "README.md",
                "runtime/frontend_sdl.c",
                "runtime/menu_items.c",
                "tools/launcher/package.py",
            ],
            "the set of files this fork edits upstream-side changed; update docs/OWNERSHIP.md and the policy",
        )


if __name__ == "__main__":
    unittest.main()
