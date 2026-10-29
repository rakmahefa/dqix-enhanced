# Linux P0 UI prototype

This directory is a **standalone integration probe**, not yet the DQIX launcher or
in-game menu. It validates that the target Linux toolchain can compile and run
SDL3 + Dear ImGui + Vulkan together before any attempt to bridge the game runtime.

## What it builds

- The unmodified official Dear ImGui SDL3 + Vulkan example.
- Dear ImGui core and the SDL3/Vulkan backends at the pinned upstream commit below.
- A CTest smoke test that starts the example under Xvfb and checks that it stays
  alive long enough to enter its event/render loop.

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
- Does not yet prove the feasibility of Escape, game pause/resume, in-game overlay,
  settings persistence, or access to runtime state. Those require a separate
  integration-boundary audit before implementation.

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
