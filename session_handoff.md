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
- `third_party/extra/linux/builder/build_job.cpp` stages source files using exclusions ported from the old builder, preserves generated/private `game/` and `build/` output directories, runs the staged `tools/build-game.sh ROM` child process in the workspace, streams combined output to a UI event queue and persistent `launcher-build.log`, maps known build stages to approximate progress and reports actual exit status.
- Workspace/ROM validation rejects unsafe paths, checks merge confirmation for a non-empty unmarked workspace and blocks writes through destination symlinks. The builder cancels/reaps child work on launcher shutdown.
- After a successful build, the launcher checks for the built executable, ROM and generated code before enabling Play. The game launches via `game/play.sh` when present, with the previous direct executable invocation as fallback.
- `packaging/linux/AppRun` verifies/extracts the bundled toolchain as before and then execs the SDL2 launcher directly with source root, default workspace and version. Its self-test exercises the launcher in dummy-video/software-renderer mode with the same integrated configuration arguments. Existing CLI build/rebuild flows are retained.
- `.github/workflows/linux-ci-appimage.yml` builds all SDL2 targets and runs CTest before packaging. The temporary diagnostics workflow asserts the native AppRun path, protected-source boundaries and Linux-only source placement.
- The workspace builder is a native Linux-only extension: `build.mk`, `tools/build-game.sh` and Windows/runtime sources remain untouched.

## 3. Verification status

Latest relevant CI run: [40818245989](https://github.com/rakmahefa/dqix-enhanced/actions/runs/40818245989) against implementation commit `4e41e34114d3a5a09a3792e8a8393625aabd80a5`.

- Locked Conda SDL2 launcher build and test job passed all four CTest cases: builder dry-run/symlink safety, settings-model/runtime schema parity, CLI help and hidden SDL2 software-renderer smoke test.
- System-SDL2 build and payload-policy tests are in progress as of this handoff update.
- The full AppImage assembly/cold-warm startup/checksum/ELF audit job has not yet been confirmed by this run. The older successful AppImage run [40815344443](https://github.com/rakmahefa/dqix-enhanced/actions/runs/40815344443) predates the unified builder and is baseline evidence only.
- No real user ROM or complete local gameplay session is run in CI. Manual validation with the user's own ROM and a successfully built workspace remains required before claiming end-to-end gameplay validation.
- The permanent production AppImage workflow has not yet been confirmed on an allowed PR/main/workflow-dispatch event.

## 4. Known limitations and next work

1. Inspect the final result/artifacts of run [40818245989](https://github.com/rakmahefa/dqix-enhanced/actions/runs/40818245989). If its AppImage package job passes, record the artifact/diagnostics and the exact checks reported.
2. Manually exercise the interactive browser, confirm workspace-merge prompt, build/retry/error logs, settings path and Play action with a user-provided ROM and real workspace. No ROM belongs in CI or AppImage.
3. Check controls/settings path migration and audit settings for unsupported/startup-only/runtime-dependent values. Writing a setting is not proof the runtime applies it.
4. Run the permanent production workflow on an allowed event and review the complete diff against `main`.
5. Keep the in-game menu blocked until Escape/focus capture, pause/resume, OpenGL/software fallback, fullscreen and shutdown are proven without protected Windows changes.

## 5. Final guard rails

- [x] Tk GUI removed from source and AppImage payload; AppRun defaults to the SDL2 launcher.
- [x] All new native Linux sources/headers/tests added under `third_party/extra/linux/`.
- [x] No ImGui/SDL3/Vulkan/new UI-toolkit dependency added.
- [x] Source-boundary CI has verified no protected runtime/Windows/shared build paths changed.
- [x] Builder dry-run tests cover success/failure, path rejection, staging exclusions, output logs and destination symlink safety (locked Conda SDL2 suite passed).
- [ ] Full AppImage assembly/self-test/checksum/ELF audit passes for the unified launcher.
- [ ] Permanent production AppImage workflow passes on an allowed event.
- [ ] Interactive real-Workspace build/launch tested locally with a user ROM.
- [ ] No in-game overlay support claimed without runtime integration tests.
