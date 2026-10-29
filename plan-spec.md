# Plan-spec — Linux v32: unified SDL2 builder/launcher and future in-game menu

**Plan updated:** 2026-10-29  
**Repository:** https://github.com/rakmahefa/dqix-enhanced  
**Work branch:** `plan/v32-linux-launcher-settings`  
**Base:** `main` at `ac8c5ae5c9f9b30e17b7ae379c6fd6413891949a`.

**Implementation update (2026-10-29):** the native SDL2 launcher replaces the former Tk builder and `AppRun` starts it directly. The Linux frontend/overlay integration is compiled from the patch-derived mirror; run [40840985795](https://github.com/rakmahefa/dqix-enhanced/actions/runs/40840985795) passed all three jobs after settings-schema parity and Discord ID validation fixes. Patch 0006 now adds aspect-fit overlay presentation with geometry assertions; the new revision must pass CI and live-game behavior remains unverified. See `third_party/extra/linux/FEATURE_PARITY_AUDIT.md` and `third_party/extra/linux/IN_GAME_OVERLAY_GATE.md`.



## 0. Prime directive — preserve the Windows upstream path

**Keep canonical Windows/upstream files pristine; Linux may compile patch-derived copies under `third_party/extra/linux/src/`.**

- Do not edit canonical `runtime/` sources, Win32 menus/resources, root `build.mk`, or root `tools/build-game.sh` in place to implement Linux-only behavior. Windows/MSYS2 continues using those originals.
- All Linux-specific runtime code, adapters, scripts, tests and patch definitions belong under `third_party/extra/linux/`.
- Store ordered, named unified patches under `third_party/extra/linux/patches/`. A patch must be applied **only to a copy** of its canonical input, never to the original checkout file.
- Materialize and commit the required upstream-derived files at matching relative paths under `third_party/extra/linux/src/` (for example `src/runtime/frontend_sdl.c`, `src/build.mk` and `src/tools/build-game.sh`) after applying the patch series to those copies. The normal AppImage/workspace build consumes these checked-in mirrors directly; it must not try to write into the AppImage's read-only source tree, and it must not silently fall back to canonical files.
- Keep the mirrored set minimal. Preserve upstream copyright/license headers and record the source revision or SHA-256 of each base input. If an upstream update makes a patch fail or a mirror stale, fail closed and require deliberate rebase/regeneration.
- Use Linux-only preparation/build entry points from `third_party/extra/linux/`; the copies of `build.mk` and `tools/build-game.sh` exist only to redirect the Linux build to the patched mirror and must retain the canonical build's code-generation, assembly and validation behavior.
- Do not add source shims or symlinks under canonical `runtime/`.
- Before a PR, verify the canonical Windows/shared paths are unchanged against the base, prove which mirrored source was compiled, and run the Linux patch/sync checks.

## 1. Product decision — one SDL2 graphical application

The Tk builder is to be **replaced**, not kept as a second graphical front end. The existing SDL2 launcher becomes the single graphical entry point for the AppImage and owns the full pre-Play journey.

The same SDL2 window must let the user:

1. Select their own `.nds` ROM and choose a safe workspace directory.
2. Initialize/update the workspace from the source bundled in the AppImage.
3. Start a build, see progress and live output, and understand success or failure without the window freezing.
4. Open the built game's settings, inspect readiness/diagnostics, and launch the game.
5. Return to the launcher to review status/logs and change launcher-exposed settings.

After implementation, the normal AppImage launch path must **not open Tk, spawn a Tk window, or require a separate “Open launcher” hand-off**. The SDL2 launcher itself owns the setup/build/settings/Play screens. Background build and game processes are allowed; they are workers launched and monitored by this UI, not alternative GUI applications.

### Dependency and UI constraints

- Use the existing SDL2 dependency and `SDL_Renderer` for the launcher's own UI.
- Do not introduce Dear ImGui, SDL3, Vulkan, SDL_ttf, GTK, Qt, Tk or another UI toolkit for the launcher.
- Because SDL2 does not provide native file/folder dialogs, implement a small in-app file browser/path selector for ROM and workspace selection. Include keyboard text entry/editing for paths and visible keyboard/mouse/gamepad focus; do not silently rely on a new external dialog library.
- Keep the existing embedded bitmap font or another already-available project-owned approach. Ensure paths, logs, status and errors remain readable even if the current bitmap font only has limited glyph coverage.
- Treat the in-game overlay as a separate runtime phase (P4), not as another screen in the pre-Play launcher. A standalone launcher window is not an in-game menu.

## 2. Audited runtime and packaging facts

| Area | Current fact | Consequence for the plan |
|---|---|---|
| `third_party/extra/linux/ui/launcher_main.cpp` | The SDL2 launcher owns navigation/settings, ROM/workspace selection, an in-app file browser, asynchronous build/progress/log handling, diagnostics and Play. | Extend this app into the unified UI; avoid a second UI implementation. |
| `packaging/linux/gui.py` | The former Tk builder behavior has been ported into the SDL2 launcher; `gui.py` has been removed and no second graphical frontend is packaged. | This is the behavioral reference for migration; port the needed behavior into the SDL2 app and retire Tk from the user-facing launch path. |
| `packaging/linux/AppRun` | Verifies the bundled toolchain, then runs the native SDL2 GUI for the default and `--gui` entry points; terminal build/rebuild actions remain available. | Switch the default graphical entry point to the bundled SDL2 executable and pass explicit source/workspace/version configuration. Preserve supported CLI modes. |
| `runtime/frontend.h` | `frontend_init()`, `frontend_frame()` and `frontend_shutdown()` define the frontend lifecycle. | Respect this lifecycle boundary. |
| `runtime/frontend_sdl.c` | SDL2 owns runtime window, input, frame pacing, pause and presentation. OpenGL may have an SDL/software fallback. | Keep this canonical file unchanged; a Linux-only patched copy under `third_party/extra/linux/src/runtime/` may host the narrow integration hook and must be the source compiled by the Linux build. |
| `runtime/gl_render.c` | Existing gameplay OpenGL renderer. | Do not rewrite it for the launcher. |
| `runtime/controls.c` | Existing key/gamepad mappings and actions; F5 requests Classic/Enhanced. | Reuse action semantics only when an actual runtime bridge exists. |
| `runtime/menu_win32.c` | Menu/dialog implementation includes Win32 behavior. | Keep unchanged; any future in-game Linux UI must be Linux-only. |
| `build.mk` | It selects `runtime/frontend_$(FRONTEND).c` and has object rules rooted under `runtime/`. | The canonical makefile stays untouched. A patch-derived copy under `third_party/extra/linux/src/build.mk` must select the Linux mirror explicitly. |
| `tools/build-game.sh` | Shared by Windows and Linux; it calls the root `build.mk` directly. | Keep the canonical script unchanged. Route Linux builds through a patch-derived copy under `third_party/extra/linux/src/tools/`, with an explicit workspace root and Linux makefile selection. |
| AppImage toolchain | AppRun validates the bundled toolchain archive and prepares a cached Conda environment used by build tools. | Keep toolchain integrity/environment setup working when AppRun switches to SDL2. Python may remain a build-tool dependency; it must no longer be the graphical UI runtime. |
| Settings | Runtime paths can depend on working directory and values are saved from runtime memory. | Define explicit paths/migration and avoid silently overwriting user preferences. |

## 3. Unified UI requirements

### 3.1 Home / Build / Game page

The first screen adapts to the user's state:

- If no valid built game exists, show a setup/build flow prominently.
- Let the user choose a `.nds` ROM and a workspace using the in-app file browser, display their full paths, and validate each before enabling Build.
- Offer the default workspace from AppRun (currently under `XDG_DATA_HOME` or `~/.local/share/dqix-enhanced-linux/workspace`) while allowing a different user-selected path.
- Report whether a build is absent, ready, running, failed, or needs rebuilding. A valid game must be checked against the existing expected layout, not inferred from a marker alone.
- After a successful build, make **Play Now** and settings available in the same window. Do not launch another GUI process.
- If a valid game already exists, let the user select/use that game directory without requiring a new ROM or forcing a rebuild.
- Show the workspace path, ROM path, selected game's path, build/log status and actionable errors. Never expose ROM contents in the UI log.

### 3.2 Port the existing builder's behavior

Port the former Tk builder's working behavior into the native SDL2 app:

- Validate that a selected ROM exists, is a regular `.nds` file, and that the workspace path is valid and writable.
- Refuse unsafe workspace choices such as the filesystem root, the user's home directory itself, the bundled read-only source, or a path overlapping the source tree. Resolve/canonicalize paths carefully and account for symlinks before destructive/copy operations.
- For a non-empty directory that lacks the project's workspace marker, ask the user to confirm before merging/copying. Clearly explain that matching source files can be refreshed while existing generated `build/` and `game/` outputs are preserved.
- Stage the packaged project sources into the workspace with exclusion behavior equivalent to the current `source_ignore()`: preserve user build/game and generated/private output folders; do not copy bundled ROMs, saves, compiled binaries, object files, generated images/audio or cache files from source.
- Write/update the workspace marker only after appropriate validation. Do not overwrite the user-selected ROM or existing game/save data as a side effect of source staging.
- Run the workspace's `tools/build-game.sh` using the selected ROM as a separate child process with the workspace as its working directory. Pass argument vectors directly; do not interpolate user paths into a shell command string.
- Capture combined stdout/stderr as it arrives. Display lines in a scrollable log and persist useful diagnostics in the workspace. Include the actual exit code and distinguish staging failure, ROM/build failure, launch failure and cancellation if cancellation is implemented.
- Map known build-stage messages to monotonic approximate progress, following the current Tk behavior, and show indeterminate progress during the expensive code-generation/compile stages. Progress must not be presented as precise work completed when the script provides no precise counter.
- Keep the SDL event/render loop responsive: filesystem staging and build orchestration must not block rendering/input. Use a worker thread or equivalent job abstraction plus a thread-safe event/progress/log queue. Only the UI thread may update SDL UI state.
- On completion, re-check the actual game layout and executable permissions before enabling Play. On failure, keep the log accessible, show a useful error and allow retry without restarting the AppImage.
- Handle child pipes, process exit/reaping, shutdown and temporary files safely. Never leave an un-reaped child or a background build process running unintentionally when the launcher closes.

### 3.3 In-app file browser

A small, project-owned browser is required because the dependency policy forbids adding a GUI toolkit merely to obtain file dialogs.

- Support directory navigation, parent/home navigation, visible current path, selection of `.nds` files, and choosing the current directory as workspace.
- Support mouse and keyboard, including text-input/path editing, navigation, confirm and cancel. Add gamepad navigation where it fits the existing focus model.
- Sort entries predictably and handle unreadable directories, broken links, non-UTF-8/unsupported names and very long paths without crashing or silently selecting a different file.
- Do not scan recursively. Directory enumeration and validation must be bounded and must not freeze the render loop on a slow or large mount; move expensive enumeration off the UI thread if needed.
- Keep path selection separate from final validation. Revalidate the exact chosen path when the user starts the operation.

### 3.4 Settings, diagnostics and gameplay launch

- Retain the existing pages for Game, Graphics, Screen/Display, Audio, Controls, Mods, Wireless/Online and System/Diagnostics, refining navigation as needed to fit Build and Logs.
- The Game/Build page reports the selected workspace, `dqix` availability, `data/dqix.nds`, `data/code/`, settings status, build state and log location.
- Compare the present launch command against the workspace's supported launch entry point (including `game/play.sh`). Preserve correct arguments, working directory, environment and process status; do not accidentally bypass required runtime setup while porting the Tk button.
- Keep the launcher visible and responsive while the game process is running, unless a deliberate, tested UX decision says otherwise. Report the game process exit status and preserve diagnostic output.
- Keep requested setting separate from renderer-effective state. A value written to `settings.ini` is not proof that the game applied it. Mark startup-only, unsupported and renderer-dependent features honestly.
- Controls remapping/free-form settings editing remain disabled or clearly marked unfinished until input capture, validation, conflict handling and persistence are implemented.
- The launcher process uses its own SDL2 renderer; this says nothing by itself about gameplay OpenGL/software renderer capability.

## 4. AppRun, AppImage and command-line contract

- The normal no-argument and `--gui` graphical paths must execute the bundled `dqix-linux-ui-launcher`, not Python/Tk. AppRun must pass explicit configuration (at minimum bundled source root, default workspace and version) instead of relying on the current working directory.
- Preserve archive checksum verification, cached toolchain setup and required compiler/build environment. Child build jobs must inherit the expected environment and run against the staged workspace.
- Keep non-interactive automation/terminal paths such as `--help`, `--version`, `--self-test`, `--build-only`, `--rebuild` and ROM-path invocation working where currently supported. Document any intentional CLI behavior change before implementing it.
- Convert `--self-test` to validate the native SDL2 launcher, software-renderer UI, configuration parsing and relevant packaging/toolchain assumptions. It must no longer require importing Tk to prove the GUI works.
- The Tk GUI source and launch hand-off have now been removed. The AppImage payload filter and its test explicitly reject `packaging/linux/gui.py`. Keep Python/compiler/build-tool dependencies that remain in use by source generation and packaging.
- Update Linux desktop entry/icon labels and AppRun messages so the product is described as the **DQIX Enhanced Linux Launcher**, not a separate Tk builder. Keep filenames or compatibility aliases only when packaging/install compatibility requires them.
- Continue to package the SDL2 runtime dependencies consistently and verify the executable resolves the bundled/system libraries as intended. Do not bundle ROMs, saves or generated game content.

## 5. Existing settings model and diagnostics

- Keep `third_party/extra/linux/settings/settings_model.cpp` independent of SDL/UI. Preserve comments, order, duplicate entries and unknown keys; validate known keys and atomically replace the file.
- Maintain a schema parity test against settings read/written by `runtime/frontend_sdl.c`.
- Resolve `settings.ini` and `controls.ini` paths explicitly, independent of implicit working-directory assumptions; provide a safe migration and test override.
- Report missing, loaded, partially invalid and error states distinctly. Do not overwrite unreadable settings silently.
- Test missing/unwritable files, malformed known values, unknown entries, migration, atomic writes and schema parity.
- Keep requested preference separate from renderer-effective state. Missing OpenGL capability must not silently persist Classic if Enhanced was requested.
- Log SDL initialization/renderer errors, settings I/O errors, workspace staging/build errors and child process status. Do not put ROM/save contents or protected assets in logs/artifacts.

## 6. In-game menu integration — patch-derived Linux source mirror

The in-game menu/overlay is feasible, and the selected architecture is a **Linux-only mirror generated from narrowly patched copies of upstream source files**. This permits a real integration into the game's existing SDL2 window/event/render/pause lifecycle without changing the canonical Windows path.

### 6.1 Feasibility and trade-offs

The current build is not extensible by adding an object alone: root `build.mk` selects `runtime/frontend_$(FRONTEND).c`, maps runtime objects through rules rooted at `runtime/`, and `tools/build-game.sh` invokes that root makefile directly. Copying only `frontend_sdl.c` would therefore leave the Linux build compiling the original file.

The Linux-only build path must explicitly use patch-derived copies of every shared file that truly needs a change. The expected starting set is:
- `runtime/frontend_sdl.c`: the narrow lifecycle hook into the existing runtime window, event handling, pause and active render path.
- `build.mk`: object/source selection that compiles the copied Linux frontend and links Linux-only overlay code.
- `tools/build-game.sh`: an entry-point copy that keeps the current code-generation/assembly workflow but invokes the mirrored makefile and uses an explicit workspace root.

Do not duplicate the entire repository or create a free-standing frontend fork. Mirror only files that need patching; overlay implementation, interfaces and tests stay native/Linux-only under `third_party/extra/linux/`. Preserve the original source headers and license notices in all copies.

**Why this is worthwhile:** canonical Windows files and upstream PR diffs remain clean, the game overlay shares the real runtime lifecycle, and patch-sized changes are reviewable. **Main cost:** mirrored files can drift as upstream changes. The patch application and mirror verification must therefore be deterministic and required in CI. If the necessary changes spread across many shared files or require a large divergence from upstream, stop and reassess before growing the mirror.

### 6.2 Source-copy and patch contract

1. Keep ordered patches in `third_party/extra/linux/patches/`; each patch has a stable name and a stated upstream input path.
2. A developer sync tool regenerates the checked-in mirrors by copying each canonical input into `third_party/extra/linux/src/` at its mirrored relative path, then applying patches to the copy only. Patch application must be strict and non-interactive; failure, fuzz/ambiguous application, or an unexpected input hash stops regeneration.
3. Store a manifest recording each upstream path, base revision/hash, patch order and resulting hash. CI check mode regenerates into a temporary directory and verifies the checked-in mirrors exactly match the canonical input plus patch series. The end-user AppImage build consumes the committed mirrors directly; it does not regenerate them.
4. The Linux launcher/build wrapper invokes the mirrored `src/tools/build-game.sh`; that copy selects `src/build.mk`, and the patched makefile selects `src/runtime/frontend_sdl.c` and links only the Linux overlay objects. Pass the workspace root explicitly rather than deriving it from the mirrored script's nested directory.
5. The existing code generation and final `game/` assembly must remain equivalent to the original script. Keep generated game data/objects in the selected workspace, never in the AppImage's read-only source directory.
6. Never modify originals in place and never replace the canonical Windows source with a symlink, include shim or conditional behavior change. The normal Windows build must continue to consume the same canonical files as before.

### 6.3 Runtime and acceptance gate

The copied frontend hook must integrate with the **active runtime renderer**, not try to create a second SDL window/renderer on top of the game's window. Keep all overlay code under `third_party/extra/linux/`; no new UI toolkit, SDL3 or Vulkan is introduced.

Before marking this complete, verify:
1. CI proves canonical Windows/shared files are unchanged and verifies the copied/patch-derived source is the one compiled.
2. Patch apply, source-mirror hash/sync check and Linux build work from a clean checkout/workspace.
3. Escape opens/closes the overlay without triggering a game action; keyboard/gamepad focus and capture work.
4. Pause/resume demonstrably stops/restarts game tick processing.
5. Rendering works with the OpenGL path and behavior is correct for the SDL/software fallback.
6. Fullscreen, resize, focus loss, shutdown, error handling and no-overlay gameplay remain correct; specifically accept overlay aspect ratio and mouse-hit alignment on both renderers.
7. Dry-run/synthetic tests still pass without a ROM; real gameplay validation is performed locally without placing ROM/save/protected assets in CI.

A separate SDL2 launcher process still cannot control the game's event queue or pause state. The overlay is implemented only when this in-process integration gate passes.
 
## 7. Execution plan

### Temporary development CI

Continue using `.github/workflows/linux-dev-sandbox.yml` for fast diagnostics; keep the permanent AppImage workflow separate until packaging parity is ready.

- Run for relevant branch changes and allow manual execution.
- Record OS/architecture/dependency versions and configuration/build/CTest logs.
- Upload logs with `if: always()`, including on compilation/test failure.
- Enforce the protected Windows/shared-path boundary and native Linux source placement.
- Build and run targeted tests by default. Larger semantic and full-AppImage checks may remain opt-in during development.
- Do not require a real ROM or place ROM/save/protected game assets in CI.

### P0 — Preserve baseline and keep the native SDL2 target buildable

The existing temporary run [40815344443](https://github.com/rakmahefa/dqix-enhanced/actions/runs/40815344443) establishes the pre-adjustment baseline: system SDL2 and locked Conda SDL2 builds and test suites passed, and the AppImage package/self-test/checksum/ELF audit passed. Re-run these checks after the unified builder changes; the old run is not acceptance proof for the new UI.

- Configure SDL2 through pkg-config without network fetch, ImGui, SDL3, Vulkan or a new UI toolkit.
- Keep `--help` and a hidden dummy-video SDL2 software-renderer smoke test.
- Preserve settings schema parity and test output.
- Keep Linux orchestration in Linux-only tooling. Root `build.mk` and `tools/build-game.sh` stay unchanged; only the patch-derived copies under `third_party/extra/linux/src/` may select the Linux runtime mirror.

### P1 — Native SDL2 builder integration (implemented; verification ongoing)

1. **Implemented:** the launcher accepts source root, default workspace, version and optional game/ROM paths.
2. **Implemented:** ROM/workspace state, in-app browser, path entry, workspace validation and confirmation for non-empty unmarked workspaces are integrated.
3. **Implemented:** Linux-only source staging ports the old ignore behavior and marker flow while preserving generated `game/` and `build/` output.
4. **Implemented:** `BuildJob` runs staging and `bash tools/build-game.sh ROM` off the SDL render/event loop; stdout/stderr feed the UI queue and persistent `launcher-build.log`, while timestamped copies of build messages are mirrored into the workspace-root `devlog.log`. Known stages map to approximate progress, and failures expose the real process status.
5. **Implemented:** success rechecks game readiness and enables Play in the same window; launching prefers `game/play.sh` and keeps a direct-executable fallback.
6. **Implemented:** synthetic dry-run tests exercise successful and failed child builds, output/log persistence, exit-status reporting and source exclusions without any real ROM or generated game data.
7. **Implemented:** ROM type/readability and source/workspace overlap validation, non-empty workspace confirmation and destination-symlink write protection have coverage. Broader missing/unwritable and long-path interaction checks remain follow-up hardening.
8. **Implemented:** Linux BuildJob requires the patch-derived `third_party/extra/linux/src/tools/build-game.sh`; there is no fallback to the canonical Windows/shared build script.

**P1 exit:** native build-job compilation and synthetic build success/failure, game stdout/stderr capture, safe log fallback and exit-record routing tests pass in the locked Conda SDL2 job. The user has confirmed Build/Play works with a real game; emitted renderer output still needs a quick manual log check.

### P2 — SDL2-only AppImage GUI entry point (implemented; temporary end-to-end packaging verified)

1. **Implemented:** default and `--gui` AppRun paths exec the native launcher with source-root, workspace and version arguments.
2. **Preserved:** toolchain checksum validation, cached extraction, compiler/build environment and CLI build/rebuild modes.
3. **Implemented:** AppRun `--self-test` passes integrated configuration arguments to the launcher and runs its hidden SDL2 software-renderer check without importing Tk.
4. **Implemented:** temporary CI checks the native entry point; the payload policy excludes and tests against `packaging/linux/gui.py`.
5. **Implemented:** desktop labels now describe the Linux Launcher; handoff/spec docs have been refreshed.
6. **Implemented:** `packaging/linux/gui.py` was deleted, and the AppImage source filter/payload test were updated accordingly.
7. **Verified in run 40818245989:** AppImage assembly, cold/warm startup, launcher self-test, toolchain-integrity rejection, checksums, payload policy and ELF audit all passed on the unified builder.

**P2 exit:** AppImage packaging and E2E checks are verified and the user confirms Build/Play work with a real ROM. The added `devlog.log` capture needs a manual content check, and the permanent production workflow remains pending.

### P3 — Settings reliability, interaction polish and development logs

1. Continue settings parse/validation, missing/unwritable file, unknown-key, migration and atomic-write coverage.
2. Verify parity against runtime read/write keys and audit startup-only/renderer-dependent values.
3. Improve text input, file-browser navigation, focus, disabled/busy/error states, keyboard, mouse and controller interaction.
4. Keep workspace-root `devlog.log` for timestamped builder output and game stdout/stderr; keep `launcher-build.log` as the raw build transcript and `game/launcher.log` for launcher/settings lifecycle messages.
5. **Implemented in the patch-derived Linux frontend:** emit a `renderer-diagnostics` startup block that explicitly reports fallback status/reason, SDL and video-driver details, window/display information, OpenGL vendor/renderer/version/context attributes, or SDL_Renderer capabilities, attempted-backend failures, output size and texture details.
5. Do not claim per-frame profiling is captured just because stderr is redirected. Existing renderer messages (GL renderer name, shader/link failures and runtime-emitted diagnostics) are captured; the runtime's in-memory performance ring is not automatically exported per frame.

**P3 exit:** Build/Play status and settings behavior remain truthful, log paths are visible, build output has automated coverage, and a real run confirms the new game-output log on the user's AppImage.

### P4 — In-game menu/overlay through patch-derived Linux sources

**Runtime integration implemented; live acceptance pending:** preserve canonical sources and compile a deterministic Linux-only source mirror. A Linux-only frontend patch wires Escape, pause/resume, keyboard/gamepad input capture and in-process presentation through GL and SDL_Renderer. The 512×768 transparent RGBA menu canvas is dimmed via a separate fullscreen pass and aspect-fitted into the viewport; mouse hit-testing follows the fitted rectangle. ROM-free CTest checks raster pixels and geometry, but a real game session has not yet verified the lifecycle.

1. **Implemented:** the minimal mirrors cover `runtime/frontend_sdl.c`, `build.mk`, and `tools/build-game.sh` only.
2. **Implemented:** ordered patches, source manifest and sync/check tool regenerate copies in a temporary directory and fail closed on upstream blob drift or patch failure.
3. **Implemented:** the mirror build entry point preserves code generation/assembly and selects the mirrored makefile; Linux BuildJob calls it explicitly.
4. **CI wired in this commit:** development CI checks mirror integrity and syntax-compiles the patched frontend; production workflow checks mirror integrity.
5. [x] Wire the RGBA overlay painter into the mirrored Linux frontend: Escape/pause, input capture/navigation, GL and SDL_Renderer output, and resource cleanup. Patch 0006 preserves aspect ratio on resize/fullscreen. Do not introduce a second window, symbol interposition, `LD_PRELOAD`, ImGui, SDL3, Vulkan or another UI toolkit.
6. Extend development CI with frontend syntax/build checks and runtime-focused tests; preserve dry-run/E2E diagnostics and verify canonical protected paths remain unchanged.
7. Run the runtime acceptance matrix from Section 6.3 against the real game on Linux before claiming completion.

**P4 exit:** mirrors reproducibly derive from declared upstream inputs and patch series; the Linux build demonstrably compiles the mirrored runtime source; root Windows/shared files remain unchanged; overlay lifecycle, input, pause, renderer fallback, fullscreen and shutdown tests pass.

### P5 — Final packaging and upstream review

- Run the permanent AppImage workflow on an allowed PR/main/workflow-dispatch event when possible.
- Manually test the native flow using a real user-provided ROM and a built workspace locally only; verify Build, retry/error logs, launch, settings path and game exit status.
- Verify no ROM/save/protected assets entered CI/AppImage artifacts.
- Review the diff against `main`; prove all protected Windows/shared files remain unchanged.
- Update handoff and docs with exact passing runs and clearly list any remaining manual checks.

## 8. Acceptance checklist

- [x] No protected Windows source or shared Windows build configuration changes (temporary CI source-boundary check passed).
- [x] All new native Linux sources/headers/tests are under `third_party/extra/linux/`.
- [x] SDL2 + SDL_Renderer is the launcher UI stack; no ImGui, SDL3, Vulkan, SDL_ttf, GTK or Qt dependency is added; the Tk GUI source/launch path is removed.
- [x] One SDL2 window owns ROM/workspace setup, Build/progress/logs, Settings, diagnostics and Play in the implementation.
- [x] File/folder selection is implemented inside the app without an external UI toolkit (manual interaction still needs checking).
- [x] Build runs off the UI thread; output goes to the event queue and persistent log; actual child exit status is reported.
- [x] Workspace validation/copy exclusions preserve generated game/build output and reject destination symlinks.
- [x] Simulated/dry-run tests cover successful and failed build jobs without a real ROM, including timestamped build records in workspace `devlog.log`.
- [x] Settings schema parity and persistence CTest passed in system and locked Conda SDL2 builds.
- [x] `AppRun` defaults to the native launcher and passes explicit paths/configuration; CLI automation modes are preserved.
- [x] Native `--self-test` needs neither a real ROM nor a desktop GPU/Vulkan device.
- [x] UI messages distinguish saved preferences from actual runtime support.
- [x] Temporary CI AppImage cold/warm startup, integrated launcher smoke, toolchain-integrity rejection, checksums and ELF checks passed with devlog capture (run 40823508014).
- [ ] Permanent production AppImage workflow run is confirmed.
- [x] User manually confirmed Build/Play work with a real ROM/workspace before devlog capture was added.
- [ ] Manually confirm the updated AppImage's `devlog.log` contains actual game stdout/stderr and expected renderer diagnostics.
- [x] AppImage payload policy rejects ROM/save/protected assets and the retired Tk GUI (run 40818245989).
- [x] Canonical Windows/upstream path remains clean according to source-boundary CI.
- [x] Ordered Linux-only patch series, source mirror manifest and sync/check tool exist under `third_party/extra/linux/`.
- [x] Linux BuildJob requires the mirrored build entry point; its makefile explicitly selects the mirrored frontend with no canonical-script fallback.
- [x] Temporary CI checks patch application/content hashes and syntax-compiles the patched frontend; production CI checks mirror integrity.
- [ ] Manually verify the renderer diagnostic block in workspace `devlog.log` with the updated AppImage and real gameplay.
- [x] Linux-only in-game overlay event/pause and GL/SDL_Renderer presentation wiring is in the patch-derived mirror; live-game acceptance remains pending in `third_party/extra/linux/IN_GAME_OVERLAY_GATE.md`.
- [x] Settings schema parity is checked against the Linux mirror, including loaded keys and custom `discord_app_id` persistence.
- [ ] All runtime settings are exposed in the launcher UI; see `third_party/extra/linux/FEATURE_PARITY_AUDIT.md` for the current coverage gap.
- [ ] Full gameplay feature/settings parity is verified on a real game build.

## 9. Out of scope

- Replacing the gameplay renderer with Vulkan.
- Adding Dear ImGui, SDL3, SDL_ttf or any other UI toolkit for the native launcher.
- Keeping Tk as a second graphical app or spawning it from the SDL2 launcher.
- Directly modifying canonical Windows/shared source files, root `build.mk` or root `tools/build-game.sh` for Linux-only behavior (patch-derived copies under `third_party/extra/linux/src/` are the approved exception).
- Claiming Enhanced/graphics features work when the active renderer lacks the necessary capability.
- Implementing an in-game overlay before the runtime integration gate passes.
