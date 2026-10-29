# Plan-spec — Linux v32: SDL2 launcher and future in-game menu

**Status (2026-10-29):** SDL2-only launcher and development CI are implemented, and P0 standalone CI passed in run [40810860108](https://github.com/rakmahefa/dqix-enhanced/actions/runs/40810860108) (`b1bc1d7c47fd62db725c33fe4844607d43087d0c`, 3/3 CTest passed). The builder/AppImage integration source changes are now staged in this branch but the permanent AppImage workflow has not yet been observed passing.  
**Base:** main at ac8c5ae5c9f9b30e17b7ae379c6fd6413891949a.  
**Work branch:** plan/v32-linux-launcher-settings.

## 0. Prime directive: preserve the Windows upstream path

**Do not modify existing Windows sources or Windows build configuration.**

- Leave runtime/menu_win32.c, resources/dialogs and any source/configuration used by Windows unchanged.
- All new native Linux C/C++ sources, headers, adapters and tests belong under third_party/extra/linux/.
- Linux-only CI/packaging may live in the existing Linux-specific locations.
- build.mk and tools/build-game.sh are shared by Windows and Linux and are out of scope. No source shim or symlink may be added under runtime/.
- Integration must use a real extension point or a standalone Linux-only build path. If neither is possible without modifying protected files, document the blocker instead of bypassing the rule.
- Before a PR, inspect the diff against main and prove protected Windows paths remain unchanged.

## 1. Architecture decision

1. SDL2 is the shared multimedia dependency for the Linux launcher and existing game runtime.
2. Do not add Dear ImGui or another UI toolkit. Implement only the small set of widgets the project needs.
3. Do not add Vulkan in this phase. The launcher uses SDL2 + SDL_Renderer and must run without a Vulkan device.
4. Do not add SDL_ttf or another font library by default; use an embedded bitmap font or suitable existing resources.
5. Separate the settings model, action/capability layer, UI page state and drawing code.
6. The launcher is a separate pre-Play process. SDL2 alone does not share the game's window, event queue, OpenGL context or pause/tick state.
7. Vulkan may be reconsidered later only with a measured requirement, feasibility proof, dependency analysis and migration plan.

The former source was an SDL3 + Dear ImGui + Vulkan prototype. It has been replaced with an initial SDL2 + SDL_Renderer launcher. The successful CI run 40810860108 verified compilation and 3/3 CTest cases, including runtime/settings schema parity and the SDL2 software-renderer smoke test. This proves the standalone prototype's basic build path, not AppImage integration or the in-game menu.

## 2. Audited runtime facts

| Area | Current fact | Consequence |
|---|---|---|
| runtime/frontend.h | frontend_init(), frontend_frame(), frontend_shutdown() define the frontend lifecycle. | Respect that lifecycle boundary. |
| runtime/frontend_sdl.c | SDL2 owns the runtime window, input, frame pacing, pause and presentation. OpenGL is available on supported systems with an SDL/software fallback. | Reuse runtime semantics, but do not assume a separate process can control it. |
| runtime/gl_render.c | Existing gameplay OpenGL renderer. | Do not rewrite it for the launcher. |
| runtime/controls.c | Existing key/gamepad mappings and actions; F5 requests Classic/Enhanced. | Reuse action semantics only when a real runtime bridge exists. |
| runtime/menu_win32.c | Menu and dialogs include Win32-specific behavior. | Keep unchanged; implement a Linux-specific UI. |
| build.mk | Frontend source paths are selected under runtime/. | There is no current direct injection point for third_party/extra/linux/. Do not change this shared file. |
| tools/build-game.sh | Used by both Windows and Linux. | Keep new launcher build orchestration Linux-only. |
| packaging/linux/gui.py | Tk builder handles ROM/workspace/build and launch; its current launch action suppresses output. | Keep the build flow initially; make logs visible in future integration. |
| settings | Runtime paths depend on the working directory and state is saved from runtime memory. | Define explicit paths/migration and avoid silently overwriting user preferences. |

## 3. Feature audit (v32)

Having a key in settings.ini does not prove the runtime applies it. Track requested value, required capability, action, observed effect and persistence separately.

| Area | Audit focus | Acceptance |
|---|---|---|
| Mode/graphics | Classic/Enhanced, 3D resolution, texture filtering, outlines, shadows, HD text | Separate requested/effective mode; only claim effects supported by the active renderer. |
| Display | Layouts, widescreen, multiple windows, scaling, fullscreen, DS Lite filter/shell | Reuse runtime capabilities and mark restart requirements. |
| Audio/music | Volume, mute, music packs, song names, export | Reuse existing services and expose operation status. |
| Language/DLC | European languages and downloadable content | Respect ROM region and application timing. |
| Controls | Keyboard/controller mapping and DS actions | Capture, focus, conflict resolution and Escape cancellation. |
| Mods | Texture packs, upscale/dump/reload and music assets | Check assets and report results. |
| Wireless/session | Server, player name, port, host/join and save transfer | Validate inputs and implement Linux alternatives to Win32 dialogs. |
| Utilities | Pause, fast forward, screenshots, reports, update, Discord | Provide an actual action or a clearly explained unsupported state. |

## 4. Target architecture: SDL2

### 4.1 Standalone pre-Play launcher

- Use SDL2 + SDL_Renderer with a small project-owned UI layer. No ImGui, SDL3, Vulkan, SDL_ttf or other UI toolkit is part of the initial target.
- Planned pages: Game, Graphics, Screen/Display, Audio/Music, Controls, Mods, Wireless/Online and System.
- Game page reports game directory, file readiness, settings status, launch status and log path. Play is available only when dqix, data/dqix.nds and data/code/ exist.
- Launch using a known working directory and explicit arguments; capture runtime output and process status in launcher.log.
- Preserve the current Tk build window initially. Connect its Play/launch action to this native launcher only after the standalone target and packaging are tested.
- Use a small bitmap font when necessary to avoid adding a font dependency. Provide keyboard, mouse and gamepad navigation with visible focus.
- Controls remapping and free-form text editing are not implemented until they actually validate and persist input.
- Keep the settings model independent of SDL and graphics so it can be tested without ROM/window/GPU.

### 4.2 UI rules

- Keep components limited: button, checkbox/toggle, stepper, choice list, basic field, navigation list, confirmation and status/error message.
- Distinguish normal, focused, disabled, busy, success and error states.
- Avoid expensive work in the render loop; report progress for long operations.
- Once in-game integration exists, UI focus must prevent events from also becoming gameplay commands.
- Writing a setting is not proof that the game has applied it. Clearly label startup-only options and capability limitations.

### 4.3 In-game menu integration gate — blocked

A separate SDL2 process cannot draw into the runtime's window, share its event queue or suspend the game tick simply because both link SDL2.

Before P3, prove all of the following on a Linux-only integration path:

1. A real hook joins the runtime event/render lifecycle without changing Windows sources or shared Windows build configuration.
2. Escape opens/closes the UI without also triggering a DS action; keyboard/gamepad focus and capture work.
3. Pause/resume demonstrably stops/restarts game tick processing.
4. The UI renders with OpenGL and behavior is defined for the SDL/software fallback path.
5. Fullscreen, shutdown and error handling are correct.
6. No shim or symlink is added under runtime/.

If this cannot be achieved under the boundary, document the blocker and propose an explicit new architecture decision. A working standalone launcher does not prove in-game feasibility.

## 5. Settings and diagnostics

- Keep settings/settings_model.cpp independent of SDL/UI. Preserve comments, order, duplicate entries and unknown keys; validate known keys and atomically replace the file.
- Maintain a schema parity test against keys read/written by runtime/frontend_sdl.c.
- Resolve settings.ini and controls.ini paths explicitly, independent of implicit working-directory assumptions; provide a safe migration and test override.
- Report missing, loaded, partially invalid and error states distinctly. Do not overwrite unreadable settings silently.
- Keep requested preference separate from renderer-effective state. Missing OpenGL capability must not silently persist Classic if Enhanced was requested.
- Log SDL init/renderer errors, settings I/O errors and child process status. Never put ROM/save contents or protected assets in logs/artifacts.
- The launcher may report its SDL2 UI renderer, but gameplay renderer capability is owned by the runtime and must be validated separately.
- F5, Escape, pause and in-game actions are not claimed as integrated without a tested bridge.

## 6. Execution plan

### Temporary development CI

The temporary workflow is for reproducible scripts, tests, diagnostics and logs without building the full AppImage every iteration. It is now configured for SDL2.

- Trigger for relevant changes on the work branch and allow manual runs.
- Install SDL2 dev files, compiler, CMake/Ninja, pkg-config and diagnostic tools.
- Record OS/architecture/dependency versions and configuration/build/CTest logs.
- Upload logs with if: always(), including when compilation/tests fail.
- Enforce protected Windows/shared-path boundaries and placement of new native Linux sources.
- Build and run targeted tests by default; the larger semantic/package checks stay opt-in.
- Keep the permanent AppImage workflow unchanged for now, then run the full relevant checks before final packaging/PR.

### P0 — SDL2 build and CI proof

1. Configure SDL2 with pkg-config; compile without network fetch, ImGui or Vulkan.
2. Run CLI help and hidden dummy-video software-renderer smoke tests, neither requiring ROM or desktop GPU.
3. Inspect the first actual workflow run/log artifacts; fix real compiler/test errors and rerun.
4. Confirm the settings schema parity test executes in CTest.
5. Reproduce F5/Enhanced and fallback behavior with logs; separate requested preference from effective renderer.
6. Document a Linux-only build/packaging path without modifying build.mk or tools/build-game.sh.

P0 exit: **met for the standalone prototype** on CI run 40810860108: configuration/build succeeded, all 3 CTest tests passed, and the Windows/shared-path boundary check passed. The permanent AppImage and runtime integration gates remain open.

### P1 — settings reliability

1. Test parse/validation, missing and unwritable files, unknown entries, migration and atomic writes.
2. Verify parity against runtime read/write keys.
3. Show capabilities and requested/effective states distinctly.
4. Keep tests free of ROM/window/GPU requirements.

P1 exit: safe persistence and visible capability failures without silent preference loss.

### P2 — full standalone launcher

1. Refine the SDL2 widgets/pages incrementally.
2. Add free-form fields/controls remapping only after capture, validation and persistence are tested.
3. Test readiness checks, process arguments, working directory, log path and child status using a simulated game folder.
4. Connect the launcher to the Tk builder only after the standalone target passes.
5. Package SDL2 consistently with the AppImage and verify cold/warm startup and resolved libraries.
6. Keep the existing Tk build flow until launch and packaging have been verified.

P2 exit: launcher tests pass and AppImage launches it correctly.

### P3 — in-game menu/overlay

1. Start only after the integration gate in 4.3 passes.
2. Add a Linux-only runtime/build integration point without editing Windows sources or build.mk.
3. Share settings/actions/navigation with the standalone launcher.
4. Test Escape, focus, pause/resume, Quit, OpenGL and fallback paths.
5. Audit every v32 setting and explain or disable unsupported capabilities.

P3 exit: actual runtime integration is proven, otherwise the blocker and reduced scope are documented.

### P4 — packaging and upstream review

- Test settings/actions/focus/fallback; run the relevant AppImage checks and inspect logs.
- Verify no ROM/save/assets are present in CI/AppImage artifacts.
- Review the diff against main to ensure protected Windows paths are unchanged.
- Update Linux/AppImage docs and perform a manual test with a user-provided ROM locally only.

## 7. Acceptance checklist

- [ ] No protected Windows source or shared Windows build configuration changes.
- [ ] New native Linux sources are under third_party/extra/linux/.
- [ ] Launcher uses SDL2 + SDL_Renderer only; no ImGui/SDL3/Vulkan build dependency.
- [ ] Smoke tests need neither ROM nor Vulkan device/desktop GPU.
- [ ] Temporary CI uploads logs on failure and skips full AppImage packaging by default.
- [ ] Settings schema parity tests run and match runtime keys.
- [ ] Launcher reports folder readiness, settings path, launch result and errors.
- [ ] No UI action claims runtime effect merely because a file was written.
- [ ] Builder/AppImage integration follows successful standalone validation.
- [ ] In-game menu is not claimed without actual integration tests.
- [ ] CI/AppImage contains no ROM/save/protected asset.
- [ ] Windows upstream path remains clean.

## 8. Out of scope

- Replacing gameplay renderer with Vulkan.
- Adding Dear ImGui, SDL3, SDL_ttf or another UI toolkit in this phase.
- Modifying Windows sources/shared build configuration to simplify Linux integration.
- Claiming Enhanced/graphics features work when the active renderer lacks capability.
- Replacing the Tk builder before the standalone launcher is stable.
