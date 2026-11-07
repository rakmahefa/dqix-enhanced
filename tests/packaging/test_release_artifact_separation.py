"""Ensure UI PNGs can never be mistaken for release archives by the publish job."""

from __future__ import annotations

from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


class ReleaseArtifactSeparationTests(unittest.TestCase):
    def test_screenshots_use_a_distinct_artifact_outside_release_download_pattern(self) -> None:
        workflow = (ROOT / ".github/workflows/linux-release.yml").read_text(encoding="utf-8")
        release_step = workflow.split("- name: Upload Linux release archive", 1)[1].split(
            "- name: Upload launcher PNG screenshots separately", 1
        )[0]
        screenshot_step = workflow.split("- name: Upload launcher PNG screenshots separately", 1)[1].split(
            "\n  publish:", 1
        )[0]

        self.assertIn("name: dqix-linux-${{ matrix.arch }}", release_step)
        self.assertNotIn("screenshots", release_step)
        self.assertNotIn("*.png", release_step)
        self.assertIn("name: dqix-launcher-screenshots-${{ matrix.arch }}", screenshot_step)
        self.assertIn("path: build/launcher-screenshots/*.png", screenshot_step)
        self.assertIn("pattern: dqix-linux-*", workflow)


if __name__ == "__main__":
    unittest.main()
