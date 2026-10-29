# Session handoff — DQIX Enhanced Linux UI (SDL2)

**Last updated:** 2026-10-29  
**Repository:** https://github.com/rakmahefa/dqix-enhanced  
**Branch:** plan/v32-linux-launcher-settings  
**Base:** main at ac8c5ae5c9f9b30e17b7ae379c6fd6413891949a.

## 1. User decisions

- SDL2 is the shared multimedia base for the launcher, existing runtime and new Linux UI.
- Do not introduce Dear ImGui or another UI toolkit.
- Do not add Vulkan in this phase; reconsider only if a concrete requirement justifies it.
- Use SDL2 + SDL_Renderer for the standalone pre-Play launcher and a small project-owned UI layer.
- Share settings/action logic, but do not assume a standalone launcher shares the runtime window, event queue, pause state or renderer.
- Do not modify existing Windows sources or Windows build configuration. All new native Linux sources/headers/tests stay under third_party/extra/linux/.
- Do not modify build.mk or tools/build-game.sh. Do not add source shims or symlinks under runtime/.

## 2. Implemented in the latest code change

- third_party/extra/linux/CMakeLists.txt now locates SDL2 through pkg-config; it does not fetch ImGui or require Vulkan/SDL3.
- third_party/extra/linux/ui/launcher_main.cpp is now a minimal SDL2 + SDL_Renderer launcher with an embedded bitmap font, sidebar/pages, mouse/keyboard/basic gamepad navigation and a software-renderer self-test.
- The launcher reads and atomically writes a subset of settings through settings/settings_model.cpp, checks the game folder, launches dqix with the existing arguments and records diagnostics in launcher.log.
- third_party/extra/linux/tests/smoke-ui.sh runs the dummy-video software-renderer self-test without Vulkan or Xvfb.
- .github/workflows/linux-dev-sandbox.yml now installs SDL2 and runs CMake/CTest. It uploads logs even on failure.
- README and the plan were updated to distinguish implemented controls from unfinished Controls remapping/free-form text editing.
- **Latest CI proof:** workflow run [40812361377](https://github.com/rakmahefa/dqix-enhanced/actions/runs/40812361377), commit `131293700095f9d27948b5fde09e4153b0ef54a6`, completed successfully with both jobs green. The system-SDL2 job passed configure/build, all 3 CTest tests, static AppImage payload/safety tests and the Windows/shared-path boundary check. The second job verified the locked Conda environment, built against SDL2 2.32.56, found no unresolved shared libraries or build-time Conda RPATH, and passed all 3 CTest tests. The optional repository-wide/semantic tests were skipped. The permanent AppImage workflow has not yet completed on this branch, so final AppRun/AppImage assembly remains unverified.

## 3. Still not done

- The launcher is not connected to the Tk builder's launch button or packaged in the published AppImage.
- settings.ini and controls.ini path/migration behavior needs a more complete audit. The current launcher reads settings.ini under its selected game directory.
- The in-game menu/bridge is not implemented. A separate SDL2 process cannot overlay the game's window, share its events or pause the runtime automatically.
- The source tree still contains an old ImGui license file from the superseded prototype; it is not used by the current CMake target and should be removed as cleanup if no other remaining source refers to it.
- No changes should be made to protected Windows code/build files.

## 4. Runtime facts from prior audit

- runtime/frontend.h exposes frontend_init(), frontend_frame() and frontend_shutdown().
- runtime/frontend_sdl.c owns the SDL2 window, input dispatch, frame pacing, presentation and pause loop. The game uses OpenGL when available and has an SDL/software fallback.
- runtime/controls.c maps F5 to mode switching; behavior depends on renderer capability and still needs reproduction with logs.
- runtime/menu_win32.c and some dialogs are Windows-only and must remain untouched.
- build.mk selects a frontend with a path fixed under runtime/. No direct injection point for third_party/extra/linux/ was found.
- tools/build-game.sh serves both Windows and Linux, so Linux integration should be orchestrated in Linux-specific code.
- packaging/linux/gui.py is the existing Tk ROM/workspace builder and launch UI; its current launch action suppresses stdout/stderr.

## 5. Next steps

1. P0 standalone SDL2 proof is now confirmed by workflow run 40810860108 (3/3 CTest passed and source-boundary check passed). Keep the log artifact for diagnostics.
2. Run optional repository-wide/package/semantic checks when appropriate; they were not part of that successful run.
3. Add tests for missing/unwritable settings files, key/schema parity and persistence. Do not claim that writing a setting guarantees runtime support.
4. Integrate the launcher with the builder and AppImage only after the standalone target is verified.
5. Keep the in-game menu blocked until a Linux-only runtime integration proof covers events, focus, pause/resume, OpenGL and the software/SDL fallback.
6. Review the final diff against main for Windows/source-boundary compliance.

## 6. Final checks

- [ ] No protected Windows source/configuration changed.
- [ ] All new native Linux sources are under third_party/extra/linux/.
- [ ] Launcher and smoke test use SDL2/SDL_Renderer only; no ImGui/SDL3/Vulkan build dependency.
- [ ] A real CI run confirms configuration, build and tests.
- [ ] Settings schema tests run and their output is inspected.
- [ ] No ROM, save or protected game assets enter CI/AppImage artifacts.
- [ ] No in-game menu support is claimed before integration tests pass.
- [ ] The builder/AppImage source integration and pinned Conda SDL2 build are validated in the temporary CI; the complete packaged AppImage/AppRun end-to-end test remains pending.
