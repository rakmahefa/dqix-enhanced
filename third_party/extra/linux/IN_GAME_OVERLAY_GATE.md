# In-game SDL2 overlay integration gate

**Status: architecture selected; overlay implementation and runtime acceptance tests are still pending.**

## Findings from the existing Linux build path

- `runtime/frontend.h` exposes the frontend lifecycle: `frontend_init()`, `frontend_frame()` and `frontend_shutdown()`.
- `runtime/frontend_sdl.c` owns the game's SDL2 event loop, presentation and pause/resume behavior. The runtime has an OpenGL path and an SDL/software fallback.
- The launcher is a separate process and cannot share the game's window, event queue or pause state merely because both use SDL2.
- Root `build.mk` selects `runtime/frontend_$(FRONTEND).c`; its runtime object paths and explicit frontend rule are rooted under `runtime/`.
- Root `tools/build-game.sh` calls root `build.mk` directly. There is no existing argument to inject a Linux-only lifecycle hook.
- Root `runtime/frontend_sdl.c`, `build.mk` and `tools/build-game.sh` are shared with the Windows/MSYS2 path. Editing those canonical files directly would dirty the Windows/upstream path.

## Selected architecture: patch-derived Linux source mirror

Keep canonical files untouched and compile a small, deterministic Linux mirror under `third_party/extra/linux/src/`.

The expected first mirrored inputs are:

- `runtime/frontend_sdl.c` — narrow in-process lifecycle/event/render/pause hook.
- `build.mk` — Linux-specific source/object selection for the mirrored frontend and Linux-only overlay objects.
- `tools/build-game.sh` — the Linux build entry-point copy, configured to use the mirrored makefile and an explicit workspace root.

Implementation rules:

1. Store ordered unified patches in `third_party/extra/linux/patches/`. The patches target copies, never the canonical files.
2. A developer sync tool regenerates the checked-in mirrors from their canonical input plus the ordered patches. Keep the mirrored file set minimal, preserve upstream copyright/license headers and record each input path, base revision/hash, patch order and result hash.
3. CI regenerates the mirror in a temporary directory and compares hashes/content with the checked-in copy. Patch failures, source drift or unexpected fuzz/offsets must fail closed.
4. The Linux build uses the checked-in `third_party/extra/linux/src/tools/build-game.sh`, its `src/build.mk` and `src/runtime/frontend_sdl.c`. It must not silently fall back to root files. The runtime build must continue to use the workspace for generated C, objects and `game/` outputs; AppImage source remains read-only.
5. Do not mirror unrelated runtime files without a demonstrated need. If the patch footprint grows substantially, stop and revisit whether this architecture remains easier to maintain than upstreaming a generic extension API.

This approach is worthwhile because a small patch set can modify Linux runtime behavior while keeping Windows sources and the upstream-facing diff clean. Its key cost is upstream drift; source-manifest checks and deterministic regeneration are mandatory, not optional hygiene.

## Runtime integration constraints

- Extend the game's existing window, event and pause/resume lifecycle. Do not launch a second SDL process as a substitute for an in-game overlay.
- Render through the runtime's active path. Do not assume an `SDL_Renderer` can simply be layered over an OpenGL context, and do not create another window or renderer.
- Keep overlay interfaces/implementation under `third_party/extra/linux/`.
- Do not use SDL symbol interposition, `LD_PRELOAD`, a new UI toolkit, SDL3, Vulkan or a hand-maintained whole-runtime fork.
- Keep root `runtime/`, root `build.mk` and root `tools/build-game.sh` unchanged for Windows/upstream use.

## Required acceptance tests

Do not mark this gate passed until all are checked:

1. CI proves root canonical source/build files are unchanged and identifies the mirrored source actually compiled by the Linux build.
2. Mirror regeneration, patch apply, source hashes and clean-workspace Linux build pass.
3. Escape opens/closes the overlay without triggering a DS action; keyboard/gamepad focus and input capture work.
4. Pause/resume demonstrably stops/restarts game ticks.
5. The overlay works with the OpenGL path and has correct behavior with the SDL/software fallback.
6. Fullscreen, resize, focus loss, shutdown, error paths and ordinary gameplay with the overlay closed work.
7. Synthetic/dry-run tests pass without a ROM; real-game validation happens locally only. No ROM, save or protected asset enters CI artifacts.

## Current development logging

Workspace-root `devlog.log` includes timestamped build messages and game stdout/stderr under a session header. This captures renderer messages already emitted by the runtime; it does **not** synthesize every frame's in-memory performance counters. A future supported runtime export or user-triggered report is required to persist that performance ring.
