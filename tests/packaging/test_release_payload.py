"""Validate the Linux payload created by tools/launcher/payload.py."""

from __future__ import annotations

import json
import os
import shlex
from pathlib import Path
import subprocess
import unittest


class ReleasePayloadTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        configured = os.environ.get("DQIX_PAYLOAD_DIR", "build/payload")
        cls.payload = Path(configured).resolve()
        cls.manifest_path = cls.payload / "payload.json"
        if not cls.manifest_path.is_file():
            raise unittest.SkipTest("payload not built; set DQIX_PAYLOAD_DIR to test it")
        cls.manifest = json.loads(cls.manifest_path.read_text(encoding="utf-8"))

    def test_payload_does_not_bundle_python(self) -> None:
        self.assertNotIn("python", self.manifest.get("paths", {}))
        self.assertNotIn("Python", self.manifest.get("versions", {}))
        self.assertNotIn("PyYAML", self.manifest.get("versions", {}))
        self.assertFalse((self.payload / "tools").exists())
        requirements = self.manifest.get("system_requirements", [])
        self.assertIn("Python 3", requirements)
        self.assertIn("PyYAML for Python 3", requirements)

    def test_payload_has_sdl2_headers_and_runtime(self) -> None:
        include_dir = self.payload / "include/SDL2"
        self.assertTrue((include_dir / "SDL.h").is_file())
        self.assertTrue((self.payload / "lib/libSDL2-2.0.so.0").is_file())
        self.assertTrue((self.payload / "lib/libSDL2.so").exists())

        # Compile the same SDL header entry point used by runtime/frontend_sdl.c.
        # This catches Debian's non-portable _real_SDL_config.h indirection.
        compiler = shlex.split(os.environ.get("CC", "cc"))
        result = subprocess.run(
            compiler + ["-fsyntax-only", "-x", "c", "-I", str(include_dir), "-"],
            input="#include <SDL.h>\nint main(void) { return SDL_INIT_VIDEO == 0; }\n",
            capture_output=True,
            text=True,
        )
        self.assertEqual(
            result.returncode,
            0,
            "bundled SDL2 headers are not self-contained:\n" + result.stderr,
        )

        # Also syntax-check the exact translation unit that failed for users.
        root = Path(__file__).resolve().parents[2]
        frontend = root / "runtime/frontend_sdl.c"
        frontend_result = subprocess.run(
            compiler + [
                "-fsyntax-only", "-fPIC",
                "-Wno-unused-parameter", "-DRECOMP_TRACE",
                "-I", str(include_dir), "-Iruntime", str(frontend),
            ],
            cwd=root,
            capture_output=True,
            text=True,
        )
        self.assertEqual(
            frontend_result.returncode,
            0,
            "runtime/frontend_sdl.c does not compile against the bundled SDL2 headers:\\n"
            + frontend_result.stderr,
        )

    def test_source_snapshot_has_makefile_launcher_and_imgui(self) -> None:
        self.assertTrue((self.payload / "source/Makefile").is_file())
        self.assertTrue((self.payload / "source/third_party/imgui/imgui.h").is_file())
        self.assertTrue((self.payload / "source/tools/launcher/launcher.cpp").is_file())

    def test_payload_contains_no_rom_saves_or_generated_game_tree(self) -> None:
        forbidden_rom_saves = {".nds", ".sav"}
        offenders = []
        for path in self.payload.rglob("*"):
            if not path.is_file():
                continue
            relative = path.relative_to(self.payload)
            if path.suffix.lower() in forbidden_rom_saves:
                offenders.append(relative.as_posix())
                continue
            if relative.parts and relative.parts[0] == "source" and len(relative.parts) > 1:
                if relative.parts[1] in {"build", "game", "rom", "extract", "obj", "gen", "gen7"}:
                    offenders.append(relative.as_posix())
                    continue
            if path.name.lower() in {
                "dqix", "dqix.exe", "dqixcore.so", "dqixcore.dll",
                "dqix.nds", "dqix.sav",
            }:
                offenders.append(relative.as_posix())
        self.assertEqual(offenders, [], f"ROM/save/generated game files found in payload: {offenders}")


if __name__ == "__main__":
    unittest.main()
