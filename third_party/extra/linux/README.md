# Linux UI launcher — SDL2

**Architecture target:** SDL2 + SDL_Renderer, with a small UI layer maintained in this project. Dear ImGui, SDL3, Vulkan and external UI toolkits are not dependencies of the target.

## Implementation state

- The launcher source has been migrated from the earlier SDL3/ImGui/Vulkan prototype to a minimal SDL2 application.
- ui/launcher_main.cpp draws controls and text with SDL_Renderer and a compact embedded 5x7 bitmap font.
- The settings model in settings/ remains independent of SDL and graphics, and can be tested without a window or ROM.
- CLI help is available. The self-test opens a hidden SDL2 window with the software renderer under SDL's dummy video driver and renders one frame.
- The UI currently edits a subset of audited settings, checks for dqix, data/dqix.nds and data/code/, and can launch the existing game binary. Keyboard, mouse and basic gamepad navigation are supported.
- Controls remapping and free-form editing of player/server text fields are not implemented. A saved setting does not guarantee the game renderer can apply it.
- Temporary E2E workflow run [40815344443](https://github.com/rakmahefa/dqix-enhanced/actions/runs/40815344443) passed all three jobs. The system and locked Conda SDL2 builds each passed 3/3 CTest. It also assembled and uploaded a real x86_64 AppImage; cold/warm startup, bundled launcher self-test, corrupted-toolchain rejection and checksum checks passed. The ELF audit found 718 ELF files and zero dependency issues; the SBOM contains 96 Conda components.
- The Tk builder and AppRun are wired to the native launcher, and the temporary E2E job has validated full AppImage assembly/self-test. The permanent production workflow has not yet run on this branch, and the Tk button has not been manually exercised against a built game workspace. The in-game menu/bridge is not implemented.

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

1. The latest temporary E2E CI run passed all three jobs. The artifact and diagnostic reports are available from the run page.
2. Improve settings path/migration coverage and safely add missing UI controls only when their behavior is implemented.
3. Prove a Linux-only runtime integration path for the in-game menu without touching Windows sources or shared build configuration.
4. Run the permanent AppImage workflow from an allowed trigger and validate final AppDir assembly, bundled `AppRun --self-test`, and the GUI `--launcher` path; then run a manual UI test with a built game folder.
