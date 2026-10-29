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
- Do not modify Windows sources/configuration, `build.mk`, or `tools/build-game.sh`. New native Linux source/header/test code stays under `third_party/extra/linux/`; no shims or symlinks under `runtime/`.
- The in-game overlay is separate and remains blocked until the runtime has a proven Linux-only event/render/pause integration point.

## 2. Implemented native SDL2 builder

- `third_party/extra/linux/ui/launcher_main.cpp` now supports AppImage configuration arguments `--source-root`, `--default-workspace`, `--version`, plus the existing `--game-dir`, `--rom`, `--help` and `--self-test` switches.
- Game/Build page presents ROM and workspace paths, select buttons, build state, approximate stage progress, animated indeterminate progress during long phases, recent output with scrolling, and Play Now. Settings/graphics/display/audio/controls/mods/wireless/system pages remain.
- In-app browser supports directory navigation, parent/home, `.nds` filtering, folder selection, text path entry and Ctrl+A/Ctrl+V. Directory scanning is capped to avoid unbounded enumeration on slow/large mounts.
- `third_party/extra/linux/builder/build_job.cpp` stages source files using exclusions ported from the old builder, preserves generated/private `game/` and `build/` output directories, runs the staged `tools/build-game.sh ROM` child process in the workspace, streams combined output to a UI event queue and persistent `launcher-build.log`, maps known build stages to approximate progress and reports actual exit status. Each build line is also timestamped and appended to workspace-root `devlog.log`; log-only workspaces do not trigger a false merge warning. The production game-launch helper is tested with synthetic stdout/stderr, including safe fallback when the devlog path is a symlink.
- Workspace/ROM validation rejects unsafe paths, checks merge confirmation for a non-empty unmarked workspace and blocks writes through destination symlinks. The builder cancels/reaps child work on launcher shutdown.
- After a successful build, the launcher checks for the built executable, ROM and generated code before enabling Play. The game launches via `game/play.sh` when present, with the previous direct executable invocation as fallback. The child game's stdout/stderr are directed to workspace-root `devlog.log`, which starts with a timestamped session header containing version, workspace, game path and launch mode. Existing runtime SDL/OpenGL messages and errors reach that file; `launcher.log` remains separate. If `devlog.log` cannot be opened, the launcher falls back to `launcher.log` and reports the fallback.
- `packaging/linux/AppRun` verifies/extracts the bundled toolchain as before and then execs the SDL2 launcher directly with source root, default workspace and version. Its self-test exercises the launcher in dummy-video/software-renderer mode with the same integrated configuration arguments. Existing CLI build/rebuild flows are retained.
- `.github/workflows/linux-ci-appimage.yml` builds all SDL2 targets and runs CTest before packaging. The temporary diagnostics workflow asserts the native AppRun path, protected-source boundaries and Linux-only source placement.
- The workspace builder is a native Linux-only extension: `build.mk`, `tools/build-game.sh` and Windows/runtime sources remain untouched.

## 3. Verification status

Latest full E2E run for the devlog implementation: [40823508014](https://github.com/rakmahefa/dqix-enhanced/actions/runs/40823508014) on commit `61138ff0`. All three jobs passed.

- System SDL2 and locked Conda SDL2 builds each passed all four CTest cases, including the new assertion that timestamped build lines appear in workspace `devlog.log`.
- The x86_64 AppImage assembled and passed cold/warm startup, integrated launcher self-test, toolchain archive corruption rejection, payload policy and ELF audit. The audit reported 718 ELF files and zero findings; the SBOM has 96 components.
- Artifacts: [AppImage](https://github.com/rakmahefa/dqix-enhanced/actions/runs/40823508014/artifacts/12875252792), [E2E diagnostics](https://github.com/rakmahefa/dqix-enhanced/actions/runs/40823508014/artifacts/12874794549), [SDL2 toolchain diagnostics](https://github.com/rakmahefa/dqix-enhanced/actions/runs/40823508014/artifacts/12875871549).
- The fallback-reporting fix is included in commit `3c1c49f1`; CI run [40823840655](https://github.com/rakmahefa/dqix-enhanced/actions/runs/40823840655) passed all three jobs.
- The user manually confirmed the earlier AppImage builds and launches the game with a real ROM/workspace. The new log capture has compile/CTest/AppImage-smoke coverage, but its contents still need one manual gameplay check.
- The AppImage source policy excludes the former Tk GUI, ROMs, saves and protected game data. Temporary CI verified the Windows/shared-source boundary remains clean.

The in-game overlay remains a separate architecture decision. `runtime/frontend_sdl.c` already owns frame/event/pause/render handling, but `build.mk` and `tools/build-game.sh` offer no supported Linux-only runtime callback. See `third_party/extra/linux/IN_GAME_OVERLAY_GATE.md`.

The permanent production AppImage workflow has not yet been confirmed on an allowed PR/main/workflow-dispatch event.

## 4. Known limitations and next work

1. Run the updated AppImage once with a real user ROM and check workspace-root `devlog.log` for its session header, game stdout/stderr, SDL/OpenGL renderer messages and exit record. No ROM belongs in CI or AppImage.
2. Continue auditing settings path migration and unsupported/startup-only/runtime-dependent options. Saving a setting is not proof the runtime applies it.
3. Run the permanent production workflow on an allowed event and review the complete diff against `main`.
4. Keep the in-game overlay blocked until a Linux-only integration architecture is approved and Escape/focus, pause/resume, OpenGL/software fallback, fullscreen and shutdown can be tested.

## 5. Final guard rails

- [x] Tk GUI removed from source and AppImage payload; AppRun defaults to the SDL2 launcher.
- [x] All new native Linux sources/headers/tests added under `third_party/extra/linux/`.
- [x] No ImGui/SDL3/Vulkan/new UI-toolkit dependency added.
- [x] Source-boundary CI has verified no protected runtime/Windows/shared build paths changed.
- [x] Builder dry-run tests cover success/failure, path rejection, staging exclusions, output logs and destination symlink safety (locked Conda SDL2 suite passed).
- [x] Temporary CI AppImage assembly/self-test/checksum/ELF audit passes for the unified launcher (run 40818245989).
- [ ] Permanent production AppImage workflow passes on an allowed event.
- [x] User confirmed the AppImage build/launch flow works with a real ROM/workspace (before devlog capture).
- [ ] Updated AppImage's `devlog.log` verified against a real gameplay session.
- [x] No in-game overlay support claimed; the shared-runtime build boundary and two architecture options are documented in `third_party/extra/linux/IN_GAME_OVERLAY_GATE.md`.
