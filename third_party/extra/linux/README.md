# Linux UI launcher — SDL2

**Architecture target:** SDL2 + SDL_Renderer, with a small UI layer maintained in this project. Dear ImGui, SDL3, Vulkan and external UI toolkits are not dependencies of the target.

## Implementation state

- The launcher source has been migrated from the earlier SDL3/ImGui/Vulkan prototype to a minimal SDL2 application.
- ui/launcher_main.cpp draws controls and text with SDL_Renderer and a compact embedded 5x7 bitmap font.
- The settings model in settings/ remains independent of SDL and graphics, and can be tested without a window or ROM.
- CLI help is available. The self-test opens a hidden SDL2 window with the software renderer under SDL's dummy video driver and renders one frame.
- The UI currently edits a subset of audited settings, checks for dqix, data/dqix.nds and data/code/, and can launch the existing game binary. Keyboard, mouse and basic gamepad navigation are supported.
- Controls remapping and free-form editing of player/server text fields are not implemented. A saved setting does not guarantee the game renderer can apply it.
- CI run [40810860108](https://github.com/rakmahefa/dqix-enhanced/actions/runs/40810860108) completed successfully on `b1bc1d7c47fd62db725c33fe4844607d43087d0c`: configuration/build succeeded and all 3 CTest cases passed, including settings/runtime schema parity and the SDL2 software-renderer smoke test. Optional project-wide/package/semantic checks were not run.
- The Tk builder and AppRun are now wired to the native launcher in the source branch, and the permanent AppImage workflow builds/packages it against the locked Conda SDL2. That packaging path still needs a completed AppImage CI run. The in-game menu/bridge is not implemented.

## Build and test

Install SDL2 development files, CMake, Ninja, a C++17 compiler and pkg-config, then run:

    cmake -S third_party/extra/linux -B build-linux-ui -G Ninja -DCMAKE_BUILD_TYPE=Debug
    cmake --build build-linux-ui --verbose
    ctest --test-dir build-linux-ui --output-on-failure

CTest checks settings-model/runtime schema parity, CLI help and an SDL2 software-renderer smoke test. The smoke test needs no Vulkan device, desktop session or ROM.

## CLI

    dqix-linux-ui-launcher --game-dir /absolute/path/to/game
    dqix-linux-ui-launcher --help
    dqix-linux-ui-launcher --self-test

The selected directory must contain an executable dqix, data/dqix.nds and data/code/ before Play is enabled. No game binary, ROM, save or game asset is included in this source tree.

## Next work

1. Inspect the actual temporary CI logs and fix any compiler or test failures.
2. Improve settings path/migration coverage and safely add missing UI controls only when their behavior is implemented.
3. Prove a Linux-only runtime integration path for the in-game menu without touching Windows sources or shared build configuration.
4. Validate the permanent AppImage CI: bundled launcher build, dependency resolution, AppRun self-test and GUI `--launcher` argument. Then run a manual UI test with a built game folder.
