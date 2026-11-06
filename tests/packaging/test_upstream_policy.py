"""Guard the upstream branch cleanup against accidental AppImage resurrection."""

from __future__ import annotations

import subprocess
import unittest


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


if __name__ == "__main__":
    unittest.main()
