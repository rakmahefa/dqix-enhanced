# Linux launcher and builder — SDL2

## Architecture

The native Linux launcher uses the project's SDL2 stack and SDL_Renderer with a small project-owned UI. It does not use Dear ImGui, SDL3, Vulkan, SDL_ttf, GTK, Qt or Tk as its GUI toolkit. The same SDL2 window provides setup/build, progress/logs, settings, diagnostics and Play.

No in-game overlay is claimed: a separate pre-Play process does not share the game's runtime window, event queue or pause state.

## Implementation

- `ui/launcher_main.cpp` draws the UI with SDL_Renderer and an embedded 5x7 bitmap font. Keyboard, mouse and basic gamepad navigation are supported.
- The Game page offers ROM and workspace selection via an in-app file/folder browser with path entry, plus build status/progress, scrollable recent output and Play Now.
- `builder/build_job.cpp` validates ROM/workspace paths, confirms merging non-empty unmarked workspaces, stages project sources while excluding generated/private output, and protects against writes through destination symlinks.
- Source staging and compilation run outside the SDL render/event loop. `BuildJob` captures combined build output, sends events to the UI and persists the raw build transcript at `launcher-build.log`. It also mirrors timestamped build messages into workspace-root `devlog.log`.
- After a successful build, the launcher validates the generated game layout and launches via `game/play.sh` when present, with a direct-executable fallback. Game stdout/stderr are appended to workspace-root `devlog.log` under a timestamped session header. Existing SDL/OpenGL messages (renderer name, shader/link errors and other diagnostics emitted by the runtime) are captured there; `game/launcher.log` remains the launcher/settings log. If `devlog.log` cannot be opened, output falls back to `launcher.log` and the UI reports the fallback.
- Graphics/display/audio/controls/mods/wireless/system pages remain. Some controls are still explicitly informational or not implemented (notably controls remapping and free-form text editing); saving a setting does not guarantee that the runtime applies it.
- `packaging/linux/AppRun` checks the bundled toolchain as before and starts the SDL2 launcher directly. The former Tk app `packaging/linux/gui.py` is deleted from the branch and excluded by the AppImage payload policy. The build toolchain still contains Python for code generation/build/package tasks; this does not create a Tk UI.
- No Windows/runtime source, `build.mk` or `tools/build-game.sh` was modified. Native Linux code and tests are under `third_party/extra/linux/`.

## Verification

The completed temporary E2E run [40823508014](https://github.com/rakmahefa/dqix-enhanced/actions/runs/40823508014) passed all three jobs with the devlog capture code:

- System SDL2: 4/4 CTest tests passed.
- Locked Conda SDL2: 4/4 CTest tests passed, including timestamped build records in workspace `devlog.log`.
- The x86_64 AppImage assembled and passed cold/warm startup, the integrated SDL2 software-renderer self-test, corrupted toolchain rejection, payload policy, checksums and ELF audit.
- ELF audit: 718 ELF files, zero findings; SBOM: 96 Conda components.

Artifacts: [AppImage](https://github.com/rakmahefa/dqix-enhanced/actions/runs/40823508014/artifacts/12875252792) · [E2E diagnostics](https://github.com/rakmahefa/dqix-enhanced/actions/runs/40823508014/artifacts/12874794549).

The user has confirmed the previous AppImage builds and launches the game with a real ROM/workspace. The new log redirection has compile/CTest and AppImage-smoke coverage; one real game run should confirm the contents of the updated `devlog.log`. The fallback-reporting fix passed [40823840655](https://github.com/rakmahefa/dqix-enhanced/actions/runs/40823840655). The permanent production AppImage workflow has not yet been confirmed on an allowed trigger.

## Build and test locally

Install SDL2 development files, CMake, Ninja, a C++17 compiler and pkg-config:

```sh
cmake -S third_party/extra/linux -B build-linux-ui -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-linux-ui --verbose
ctest --test-dir build-linux-ui --output-on-failure
```

The four CTest cases cover builder dry-run success/failure and symlink protection, settings-model/runtime schema parity, launcher CLI help, and a hidden SDL2 software-renderer smoke test. They require no real ROM or desktop GPU.

## Launcher arguments

```sh
dqix-linux-ui-launcher \
  --source-root /path/to/bundled/source \
  --default-workspace "$HOME/.local/share/dqix-enhanced-linux/workspace" \
  --version 1.2.3
dqix-linux-ui-launcher --game-dir /absolute/path/to/game
dqix-linux-ui-launcher --help
dqix-linux-ui-launcher --self-test
```

The integrated AppImage path passes the first group of arguments automatically. Before Play, the game directory must contain an executable `dqix`, `data/dqix.nds` and `data/code/`. No game binary, ROM, save or generated game asset is bundled.

## Remaining manual checks

1. With the updated AppImage, check workspace-root `devlog.log` for the timestamped session header, the runtime's stdout/stderr, SDL/OpenGL messages and exit status. `devlog.log` records diagnostics the runtime already emits; it does not export every frame's in-memory performance counters.
2. Confirm the production AppImage workflow on its intended trigger.
3. Keep the in-game overlay blocked until a Linux-only integration architecture is approved; see `IN_GAME_OVERLAY_GATE.md`.
