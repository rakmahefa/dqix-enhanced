# Session handoff — DQIX Enhanced unified Linux SDL2 launcher

**Last updated:** 2026-10-29  
**Repository:** https://github.com/rakmahefa/dqix-enhanced  
**Branch:** `plan/v32-linux-launcher-settings`  
**Base:** `main` at `ac8c5ae5c9f9b30e17b7ae379c6fd6413891949a`.

## 1. Decisions that must be preserved

- SDL2 is the shared multimedia base for the launcher and existing runtime.
- Do not introduce Dear ImGui, SDL3, Vulkan, SDL_ttf, GTK, Qt, Tk or another UI toolkit for the launcher.
- **Replace the Tk builder with the existing SDL2 launcher.** The launcher must become the one graphical AppImage application for ROM/workspace selection, building/progress/logs, settings, diagnostics and Play. Do not leave a separate Tk window or an “Open launcher” hand-off.
- SDL2 has no built-in file picker, so implement a small in-app file/folder browser and path editor inside the SDL2 UI.
- Build orchestration may run in a worker thread and a child process; the SDL2 application remains the sole graphical interface. UI rendering/input must stay responsive.
- Do not modify existing Windows sources/configuration. All new native Linux sources, headers, adapters and tests remain under `third_party/extra/linux/`.
- Do not modify `build.mk` or `tools/build-game.sh`; invoke the latter from a workspace staged by the Linux-only builder.
- Do not add shims or symlinks under `runtime/`.
- The in-game menu/overlay is a separate, blocked project phase. A standalone SDL2 launcher does not share the game's window/events/pause state.

The detailed specification is now in [plan-spec.md](https://github.com/rakmahefa/dqix-enhanced/blob/plan/v32-linux-launcher-settings/plan-spec.md).

## 2. Current implementation (before the unified-builder work)

- `third_party/extra/linux/CMakeLists.txt` locates SDL2 through pkg-config. It does not fetch ImGui or require Vulkan/SDL3.
- `third_party/extra/linux/ui/launcher_main.cpp` is an initial SDL2 + SDL_Renderer app with an embedded bitmap font, pages, keyboard/mouse/basic gamepad navigation, a hidden software-renderer self-test, settings access and game launch with `--game-dir`. It **does not yet** select a ROM/workspace or run the build itself.
- `third_party/extra/linux/settings/settings_model.cpp` implements the settings model/persistence helpers.
- `third_party/extra/linux/tests/smoke-ui.sh` runs the dummy-video software-renderer smoke test without Vulkan or Xvfb.
- `.github/workflows/linux-dev-sandbox.yml` builds/tests the SDL2 prototype and uploads diagnostics/logs even on failure.
- `packaging/linux/gui.py` is still the current Tk builder: it chooses a ROM and workspace, stages the source tree (excluding generated/private outputs), runs `tools/build-game.sh` in a worker thread, streams build output into a log/progress UI, and then offers an “Open launcher” button.
- `packaging/linux/AppRun` still defaults to that Tk GUI after checking/preparing the bundled toolchain. Its CLI modes include help/version/self-test/build-only/rebuild/ROM-path flows. The default GUI path and Tk-oriented self-test must be migrated.
- Current SDL2 launcher launch behavior uses `dqix` with explicit arguments and logs output. Compare this with the workspace's `game/play.sh` contract before porting the build button; do not accidentally bypass required environment or runtime setup.
- `runtime/frontend.h` defines the runtime lifecycle; `runtime/frontend_sdl.c` owns SDL2 runtime events/rendering/pause; `runtime/menu_win32.c` remains protected. Do not edit these.

## 3. CI status and evidence

Temporary workflow run [40815344443](https://github.com/rakmahefa/dqix-enhanced/actions/runs/40815344443), commit `7092d1920fbc529b6a5529e0cf3273a992431768`, passed all three jobs:

- System SDL2 and locked Conda SDL2 2.32.56 builds passed their 3/3 CTest suites.
- A real x86_64 AppImage was assembled; cold/warm startup, bundled launcher self-test, modified-toolchain archive rejection, checksums and ELF audit passed.
- ELF audit reported 718 ELF files and zero findings; SBOM contained 96 components. Artifacts/diagnostics were uploaded.

This is **pre-migration baseline evidence only**. It proves the existing SDL2 prototype and package can build, not that the full native builder has been implemented. After migration, rerun the relevant checks and update this handoff with the new commit/run. The permanent production AppImage workflow has not yet been confirmed on an allowed PR/main/workflow-dispatch event.

## 4. Required unified behavior

The first SDL2 screen should show setup/build when no valid game is present, or game readiness and Play when a game already exists. Inside that same window users need to:

- Choose a `.nds` ROM and workspace through an in-app file browser.
- Validate paths and confirm before merging into a non-empty, unmarked workspace; protect existing `build/`, `game/`, save and generated/private outputs during source staging.
- Stage source from the AppImage into the workspace with behavior equivalent to the current Tk `source_ignore()`.
- Run the staged `tools/build-game.sh` through a child process, stream stdout/stderr into a persistent log and UI queue, show approximate stage progress, and report the actual exit status.
- Keep SDL event/render handling responsive through staging/building; update UI state only on the SDL thread.
- Recheck the built game layout and provide Play/settings directly in the same app. Keep errors/logs available and allow retry.
- Retain settings/graphics/display/audio/controls/mods/wireless/system pages, while honestly distinguishing saved preferences from runtime-effective capabilities.
- Have no separate Tk window or native launcher's second GUI process.

Use simulated/dry-run fixtures in CI for file staging and build-job success/failure/progress/log/cleanup. CI must not need or package a real ROM, save or protected game data.

## 5. Next steps

1. **P1 — Native builder core:** extend `launcher_main.cpp` and add small testable Linux-only modules under `third_party/extra/linux/` for app configuration, path selection/validation, source staging and build-job orchestration. Add the in-app file/folder browser, text entry, safe workspace handling and progress/log queue. Do not put blocking filesystem/process work in the render loop.
2. Add tests for source-copy exclusions, workspace marker/overlap checks, missing/unwritable paths, ROM validation, simulated child success/failure, output streaming, exit status and child reaping. Keep the hidden SDL software-renderer test working.
3. **P2 — AppRun cutover:** have no-argument and `--gui` paths exec the native SDL2 launcher with explicit source root/default workspace/version. Preserve toolchain archive verification and the build environment; keep supported CLI modes working. Replace the Tk import in `--self-test` with native launcher/software-renderer checks.
4. Update `.github/workflows/linux-dev-sandbox.yml`, `.github/workflows/linux-ci-appimage.yml` and packaging assertions so they test the SDL2-only graphical path. Update desktop entry/icon/app labels. Once parity and tests pass, remove `gui.py` and Tk-only hooks if no references remain; don't remove Python from the toolchain until its use by build/code-generation tools is audited.
5. Verify the actual game launch command against `game/play.sh` and test the full built-workspace path locally with a user-provided ROM. Never use a real ROM in CI.
6. Run a real AppImage build, cold/warm startup, SDL self-test, simulated builder tests, toolchain-integrity rejection, checksum and ELF audits. Then try the permanent workflow on an allowed event and review the final diff against `main`.
7. Keep the in-game overlay blocked until a Linux-only runtime integration proof covers event ownership, focus/capture, pause/resume, OpenGL and software fallback without protected Windows changes.

## 6. Final guard rails

- [ ] No protected Windows source or shared Windows build configuration changed.
- [ ] New native Linux source/header/test files live under `third_party/extra/linux/`.
- [ ] SDL2 + SDL_Renderer remains the only launcher UI stack; no ImGui/SDL3/Vulkan/font/toolkit dependency added.
- [ ] One SDL2 window owns setup, build, logs, settings, diagnostics and Play.
- [ ] Build and directory operations cannot freeze the UI.
- [ ] Test fixtures/dry-run cover builder success and failure without a ROM.
- [ ] Settings schema/persistence tests pass and output is inspected.
- [ ] AppRun defaults to the native launcher; CLI modes and toolchain integrity remain tested.
- [ ] No ROM/save/protected assets enter CI or AppImage artifacts.
- [ ] Permanent AppImage workflow and real local built-workspace launch are verified.
- [ ] No in-game overlay support is claimed without runtime integration tests.
