# In-game SDL2 overlay integration gate

**Status: blocked by the current strict source-boundary rule; no new overlay is implemented.**

## Findings from the current Linux build path

- `runtime/frontend.h` exposes the frontend lifecycle: `frontend_init()`, `frontend_frame()`, and `frontend_shutdown()`.
- `runtime/frontend_sdl.c` implements the runtime event loop, frame presentation and pause/resume handling. It already has an internal menu/pause path, a GL renderer path and an SDL renderer fallback.
- These are internal functions, not an external Linux-only callback interface. The current launcher is a separate process; sharing SDL2 as a dependency does not share its window, renderer, event queue or pause state.
- `build.mk` builds its runtime list from `runtime/*.c`, excludes the frontend variants, then selects `runtime/frontend_$(FRONTEND).c`. The compile/object rules are rooted under `runtime/`.
- `tools/build-game.sh` invokes `build.mk` with `FRONTEND=sdl`; it has no supported parameter for injecting a separate Linux-only frame/event hook. `RES` is documented as an extra program object (for example an icon), not as a runtime lifecycle extension point.
- `runtime/frontend_sdl.c` is used by the Windows/MSYS2 build too. Editing it, editing `build.mk` or `tools/build-game.sh`, or adding a shim under `runtime/` would break the current strict source-boundary rule.

## Safe conclusion

Do not fake an in-game overlay by launching another SDL process, by interposing SDL symbols/using `LD_PRELOAD`, or by permanently copying/forking the full SDL frontend. Those approaches do not provide a reliable supported hook for keyboard capture, pause, OpenGL/SDL fallback and shutdown.

The next architecture decision is one of:

1. **Keep the strict rule:** design a dedicated Linux runtime build/integration path that does not alter Windows/shared sources. This fits the current constraint but needs a deliberate build-design step before implementation.
2. **Approve a narrow common-source extension:** add a small Linux-only callback in the shared SDL frontend, guarded so Windows behavior is unchanged, with the overlay implementation staying under `third_party/extra/linux/`. This is technically cleaner but modifies a shared source file and therefore needs an explicit change to the current strict boundary.

The runtime already has pause/settings/menu functionality; the target here would be an additional overlay, not a replacement for that working code. Do not mark the gate passed until Escape/focus, gamepad capture, pause/resume, OpenGL and software fallback, fullscreen and shutdown are exercised against the real runtime.

## Current development logging

Workspace-root `devlog.log` now includes timestamped build messages plus the game's stdout/stderr in a clearly marked session. Existing renderer diagnostics emitted to stdout/stderr are captured; it does **not** synthesize every frame's performance counters. The runtime keeps a rolling in-memory performance record, so exporting that ring to a persistent log would need a supported runtime hook or explicit user-triggered report.
