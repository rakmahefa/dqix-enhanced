# Linux launcher and builder — SDL2

## Architecture

The native Linux launcher uses the project's SDL2 stack and SDL_Renderer with a small project-owned UI. It does not use Dear ImGui, SDL3, Vulkan, SDL_ttf, GTK, Qt or Tk as its GUI toolkit. The same SDL2 window provides setup/build, progress/logs, settings, diagnostics and Play.

No in-game overlay is claimed: a separate pre-Play process does not share the game's runtime window, event queue or pause state.

## Implementation

- `ui/launcher_main.cpp` draws the UI with SDL_Renderer and an embedded 5x7 bitmap font. Keyboard, mouse and basic gamepad navigation are supported.
- The Game page offers ROM and workspace selection via an in-app file/folder browser with path entry, plus build status/progress, scrollable recent output and Play Now.
- `builder/build_job.cpp` validates ROM/workspace paths, confirms merging non-empty unmarked workspaces, stages project sources while excluding generated/private output, and protects against writes through destination symlinks.
- Source staging and compilation run outside the SDL render/event loop. `BuildJob` captures combined build output, sends events to the UI and persists the full log at `launcher-build.log`. Approximate build stages are shown; code-generation/compilation use indeterminate progress where no precise counter is available.
- After a successful build, the launcher validates the generated game layout and launches via `game/play.sh` when present, with a direct-executable fallback.
- Graphics/display/audio/controls/mods/wireless/system pages remain. Some controls are still explicitly informational or not implemented (notably controls remapping and free-form text editing); saving a setting does not guarantee that the runtime applies it.
- `packaging/linux/AppRun` checks the bundled toolchain as before and starts the SDL2 launcher directly. The former Tk app `packaging/linux/gui.py` is deleted from the branch and excluded by the AppImage payload policy. The build toolchain still contains Python for code generation/build/package tasks; this does not create a Tk UI.
- No Windows/runtime source, `build.mk` or `tools/build-game.sh` was modified. Native Linux code and tests are under `third_party/extra/linux/`.

## Verification

The integrated implementation passed temporary CI run [40818245989](https://github.com/rakmahefa/dqix-enhanced/actions/runs/40818245989), on implementation commit `4e41e34114d3a5a09a3792e8a8393625aabd80a5`:

- System SDL2 build: 4/4 CTest tests passed.
- Locked Conda SDL2 build: 4/4 CTest tests passed.
- Native builder dry-run tests cover synthetic build success and failure, log/exit-status reporting, staging exclusions, source/workspace overlap and destination-symlink protection. No real ROM or generated game data is used by CI.
- The complete x86_64 AppImage was assembled and tested with cold/warm start and its bundled SDL2 software-renderer self-test. A corrupted toolchain archive was rejected; the payload policy passed; checksums and ELF audit passed.
- ELF audit reported 718 ELF files and zero findings; the Conda SBOM has 96 components. The AppImage artifact and diagnostic artifacts were uploaded in the run.

The permanent production AppImage workflow has not yet been confirmed on an allowed PR/main/workflow-dispatch event. Manual interactive testing with a real user-provided ROM and a built game workspace is still required to validate the full local gameplay journey.

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

1. With a user-provided ROM, exercise the in-app browser, existing-workspace confirmation, full build log, retry after failure, settings file location and Play/game exit status.
2. Confirm the production AppImage workflow on its intended trigger.
3. Keep the in-game menu/overlay blocked until Linux-only integration proves keyboard/gamepad focus, event capture, pause/resume, OpenGL/software fallback, fullscreen and shutdown without touching protected Windows code.
