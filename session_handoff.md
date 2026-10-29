# Session handoff — DQIX Enhanced unified Linux SDL2 launcher

**Last updated:** 2026-10-29  
**Repository:** https://github.com/rakmahefa/dqix-enhanced  
**Branch:** `plan/v32-linux-launcher-settings`  
**Base:** `main` at `ac8c5ae5c9f9b30e17b7ae379c6fd6413891949a`.

## 1. Required architecture decisions

- SDL2 + SDL_Renderer is the launcher UI stack and shared multimedia base. No Dear ImGui, SDL3, Vulkan, SDL_ttf, GTK, Qt or other UI toolkit is added.
- **The Tk builder has been replaced by the SDL2 launcher.** `packaging/linux/gui.py` is deleted, removed from AppImage payload allowlisting and explicitly forbidden by the payload test.
- The one SDL2 window owns ROM selection, workspace selection, build progress/output, errors/logs, settings/diagnostics and Play. No second graphical app or hand-off should return.
- Use a small in-app file/folder browser and path editor. Background source staging and compilation run outside SDL's render/event loop.
- Keep canonical Windows/shared files (`runtime/`, root `build.mk`, root `tools/build-game.sh`, and Win32 resources) unchanged. Ordered patches, manifest, checked-in mirrors and `tools/sync_linux_sources.py --check` now define the Linux-only build path.
- BuildJob requires `third_party/extra/linux/src/tools/build-game.sh`; it selects the mirror makefile and frontend. Do not add a fallback to the canonical script.
- The mirrored frontend emits a `renderer-diagnostics` block to stderr, captured in workspace `devlog.log`, with fallback state/reason and OpenGL or SDL_Renderer details. A real AppImage run is still needed to confirm the contents.
- The Linux-only overlay is now wired into the patch-derived frontend mirror via ordered patch `0004-linux-ingame-overlay.patch`: Escape toggles the overlay, pauses via the runtime pause loop, keyboard/gamepad input is captured for navigation/selection, and presentation has an OpenGL helper plus an SDL_Renderer streaming-texture path. This is implementation, not runtime acceptance: the real-game input/focus/fullscreen/shutdown/renderer-fallback matrix is still pending.

## 2. Implemented native SDL2 builder

- `third_party/extra/linux/ui/launcher_main.cpp` now supports AppImage configuration arguments `--source-root`, `--default-workspace`, `--version`, plus the existing `--game-dir`, `--rom`, `--help` and `--self-test` switches.
- Game/Build page presents ROM and workspace paths, select buttons, build state, approximate stage progress, animated indeterminate progress during long phases, recent output with scrolling, and Play Now. Settings/graphics/display/audio/controls/mods/wireless/system pages remain.
- In-app browser supports directory navigation, parent/home, `.nds` filtering, folder selection, text path entry and Ctrl+A/Ctrl+V. Directory scanning is capped to avoid unbounded enumeration on slow/large mounts.
- `third_party/extra/linux/builder/build_job.cpp` stages source files using exclusions ported from the old builder, preserves generated/private `game/` and `build/` output directories, runs the staged `tools/build-game.sh ROM` child process in the workspace, streams combined output to a UI event queue and persistent `launcher-build.log`, maps known build stages to approximate progress and reports actual exit status. Each build line is also timestamped and appended to workspace-root `devlog.log`; log-only workspaces do not trigger a false merge warning. The production game-launch helper is tested with synthetic stdout/stderr, including safe fallback when the devlog path is a symlink and writing the exit record to the active fallback log.
- Workspace/ROM validation rejects unsafe paths, checks merge confirmation for a non-empty unmarked workspace and blocks writes through destination symlinks. The builder cancels/reaps child work on launcher shutdown.
- After a successful build, the launcher checks for the built executable, ROM and generated code before enabling Play. The game launches via `game/play.sh` when present, with the previous direct executable invocation as fallback. The child game's stdout/stderr are directed to workspace-root `devlog.log`, which starts with a timestamped session header containing version, workspace, game path and launch mode. Existing runtime SDL/OpenGL messages and errors reach that file; `launcher.log` remains separate. If `devlog.log` cannot be opened, the launcher falls back to `launcher.log` and reports the fallback.
- `packaging/linux/AppRun` verifies/extracts the bundled toolchain as before and then execs the SDL2 launcher directly with source root, default workspace and version. Its self-test exercises the launcher in dummy-video/software-renderer mode with the same integrated configuration arguments. Existing CLI build/rebuild flows are retained.
- `.github/workflows/linux-ci-appimage.yml` builds all SDL2 targets and runs CTest before packaging. The temporary diagnostics workflow asserts the native AppRun path, protected-source boundaries and Linux-only source placement.
- The workspace builder is a native Linux-only extension: `build.mk`, `tools/build-game.sh` and Windows/runtime sources remain untouched.

## 3. Verification status

Latest full temporary E2E run: [40838080231](https://github.com/rakmahefa/dqix-enhanced/actions/runs/40838080231), triggered after the overlay patch correction. **All three jobs succeeded.**

- Source mirror validation applied all four ordered patches without fuzz and confirmed the canonical input hashes and checked-in mirror hashes match.
- The native Linux SDL2 job compiled the overlay raster and OpenGL helper and passed all five CTest cases. The locked Conda SDL2 launcher build also passed.
- The x86_64 AppImage assembled and passed cold/warm startup, the bundled SDL2 launcher self-test, payload checks and rejection of a corrupted toolchain archive. The ELF audit reported 718 ELF files and zero findings; the SBOM contains 96 components.
- Artifacts: [AppImage](https://github.com/rakmahefa/dqix-enhanced/actions/runs/40838080231/artifacts/12882115300), [E2E diagnostics](https://github.com/rakmahefa/dqix-enhanced/actions/runs/40838080231/artifacts/12881985842), [SDL2 toolchain diagnostics](https://github.com/rakmahefa/dqix-enhanced/actions/runs/40838080231/artifacts/12881512941), [Linux development diagnostics](https://github.com/rakmahefa/dqix-enhanced/actions/runs/40838080231/artifacts/12881073759).
- These CI checks validate the patch/mirror regeneration, source syntax, helper build and AppImage launcher packaging; they do not execute a game ROM or verify the live in-game overlay.
- The user manually confirmed Build/Play work with a real ROM/workspace before the new renderer diagnostics and overlay changes. Still needed: one updated AppImage run with a local real ROM/workspace, checking workspace-root `devlog.log` for renderer diagnostics and exercising the overlay.
- The AppImage source policy excludes the former Tk GUI, ROMs, saves and protected game data. CI confirms the Windows/shared-source boundary remains clean.

The in-game overlay is a separate architecture decision; no unsupported second-process hack or shared Windows-source modification was introduced. See `third_party/extra/linux/IN_GAME_OVERLAY_GATE.md`.

The permanent production AppImage workflow has not yet been confirmed on an allowed PR/main/workflow-dispatch event.

## 4. Known limitations and next work

1. Run the updated AppImage with a local real ROM/workspace. Check workspace-root `devlog.log` for the session header, `renderer-diagnostics: game_renderer_fallback=`, OpenGL vendor/renderer/version or SDL_Renderer capabilities, fallback reason, game stdout/stderr and the exit record. No ROM belongs in CI or AppImage.
2. Complete the in-game overlay acceptance matrix on a real game: Escape open/close without triggering a DS action, pause/resume tick behavior, keyboard and gamepad capture, focus loss, fullscreen/resize, shutdown, OpenGL presentation and SDL_Renderer/software fallback. Fix any issues and add ROM-free tests where practical.
3. Continue auditing settings path migration and unsupported/startup-only/runtime-dependent options. Saving a setting is not proof the runtime applies it.
4. Run the permanent production AppImage workflow on an allowed event and review the complete diff against `main`.

## 5. Final guard rails

- [x] Tk GUI removed from source and AppImage payload; AppRun defaults to the SDL2 launcher.
- [x] All new native Linux sources/headers/tests added under `third_party/extra/linux/`.
- [x] No ImGui/SDL3/Vulkan/new UI-toolkit dependency added.
- [x] Source-boundary CI has verified no protected runtime/Windows/shared build paths changed.
- [x] Builder dry-run tests cover success/failure, path rejection, staging exclusions, output logs and destination symlink safety (locked Conda SDL2 suite passed).
- [x] Temporary CI AppImage assembly, cold/warm startup, bundled self-test, checksum-rejection test and ELF audit pass for the unified launcher (run 40838080231).
- [x] Linux source mirrors regenerate with all four ordered patches and no fuzz (run 40838080231).
- [x] Native Linux SDL2 CMake build and all five CTest cases pass, including the overlay raster test (run 40838080231).
- [ ] Live overlay behavior on a real game accepted on both OpenGL and SDL_Renderer fallback paths.
- [ ] Permanent production AppImage workflow passes on an allowed event.
- [x] User confirmed the AppImage build/launch flow works with a real ROM/workspace (before devlog capture).
- [ ] Updated AppImage's `devlog.log` verified against a real gameplay session.
- [x] Linux-only in-game overlay rendering/input/pause implementation is wired into the patch-derived mirror; the live-game acceptance gate remains open in `third_party/extra/linux/IN_GAME_OVERLAY_GATE.md`.
