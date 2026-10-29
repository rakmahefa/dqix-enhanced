# Linux SDL3/Vulkan launcher prototype

This directory contains the **first pre-Play Linux launcher prototype** plus the
settings model and the integration-boundary tests. It validates the SDL3 + Dear
ImGui + Vulkan UI independently of the existing game runtime. The native launcher
is still a preview: it is not an in-game overlay and does not yet control the game's
SDL2 event loop or pause/tick state.

## What it builds

- A native sidebar with Game, Graphics, Screen & Display, Audio & Music, Controls,
  Mods, Wireless & Online, and System pages.
- An editable game-folder path, readiness checks, and a Play action that starts
  the built `dqix` executable from the selected folder with the expected relative
  ROM/code/save arguments.
- Controls that read and write the existing `settings.ini` schema through
  `IniDocument`, including explicit Save buttons for player name and WFC server.
- A System page that identifies the launcher's Vulkan device, shows the settings
  and log paths, and explains that gameplay still uses the existing SDL2/OpenGL
  or software renderer.
- Diagnostic messages written to `<game-dir>/launcher.log` as well as stderr,
  including SDL/Vulkan startup failures.
- A non-graphical `--help` CTest and a graphical smoke test under Xvfb.

Dear ImGui is fetched from its official repository at immutable commit
`c1aaac65e896a45f81f47a4d68f9959e0f86c859` (release v1.93.0, MIT license).
SDL3 and Vulkan headers/loader are provided by the Linux development environment.

## Build and test

```sh
cmake -S third_party/extra/linux -B build-linux-ui -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-linux-ui --verbose
ctest --test-dir build-linux-ui --output-on-failure
```

A working Vulkan device is needed for the graphical smoke test. If the runner has
no device, the test is reported as skipped by default rather than passed. Set
`REQUIRE_VULKAN_DEVICE=true` to make missing Vulkan availability a hard failure;
the temporary development workflow separately exposes this as a manual input.

## Explicit non-goals

- Does not compile or modify any Windows source or build configuration.
- Does not link to, pause, inject into, or control the existing DQIX runtime.
- Does not claim that the game's renderer uses Vulkan.
- Does not yet provide an in-game Escape overlay, pause/resume bridge, or access to
  live runtime state. Those require a separate integration-boundary decision.
- Does not verify that every saved option is supported by the active gameplay renderer.

## Settings model (early P1)

The Linux-only `settings/settings_model.cpp` layer can read and safely edit the existing
`settings.ini` format without discarding comments, ordering, duplicate keys, or keys that
the current UI does not know about. Edits through `set_validated()` are limited to an
audited schema based on the keys parsed by `runtime/frontend_sdl.c`. Values are range- and
format-checked before modification. Writes use a same-directory temporary file, flush it,
and atomically replace the destination; existing file permissions are retained.

CTest includes a no-ROM, no-window settings test. It exercises validation, preservation,
round-tripping, atomic save, and compares the schema to the key names read/written by the
existing runtime source. This test has been added but is **not yet confirmed passing in CI**.

This is a data layer only: it does not yet render launcher pages, start the game, migrate
settings paths, or claim an in-game settings bridge exists. The runtime still resolves
`settings.ini` relative to its working directory.

## Launcher invocation

Run `dqix-linux-ui-prototype --game-dir /absolute/path/to/game` (or set
`DQIX_GAME_DIR`). The selected directory must contain the built `dqix` executable
and the `data/dqix.nds` and `data/code/` paths before Play is enabled. No game
binary, ROM, save, or game assets are included by this source prototype.
