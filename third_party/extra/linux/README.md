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
