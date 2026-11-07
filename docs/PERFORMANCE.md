# Performance instrumentation

The runtime writes one CSV per game process to `perf_logs/session_TIMESTAMP_PID.csv`.
The directory is created at startup. The log starts with comment-style metadata rows
(session timestamp, `DQIX_VERSION`, Git commit when supplied by the build, clock and
sampling thresholds), followed by a CSV header and sampled frame/scope events.

## Default sampling

- Frames taking at least **16,600 µs** are recorded as `slow_frame` incidents.
- One normal frame is recorded every **60 frames** as `periodic_sample`.
- Scopes taking at least **1,000 µs** are recorded as `slow_scope` incidents.
- A session file is capped at **32 MiB** by default. At startup, old session logs are
  cleaned up so that at most 9 prior logs plus the current one remain.

The frame clock measures wall time between emulator frame boundaries, so it includes
pacing, rendering, and waits. Use it to detect frame-level stalls; it is not a pure
CPU-time measurement. Scope times use a monotonic clock and the scope counters are
deltas from scope entry to exit.

## Environment controls

| Variable | Default | Meaning |
| --- | ---: | --- |
| `DQIX_PERF` | `1` | Set to `0`, `false`, `off`, or `no` to disable logging. |
| `DQIX_PERF_FRAME_THRESHOLD_US` | `16600` | Record frames at or above this duration. |
| `DQIX_PERF_SCOPE_THRESHOLD_US` | `1000` | Record scopes at or above this duration. |
| `DQIX_PERF_SAMPLE_EVERY` | `60` | Record one normal sample every N frames. |
| `DQIX_PERF_MAX_MB` | `32` | Maximum size for one session file (1–256 MiB). |
| `DQIX_PERF_SCENE` | unset | Force a fixed scene label, e.g. `combat`, for the full session. |

The logger labels known runtime contexts as `loading`, `gameplay`, and `menu`.
There is not yet a verified game-memory scene classifier for the game's finer
locations or battle state. Game-specific code can update the label through
`perf_set_scene("combat")`, `perf_set_scene("overworld")`, etc. The fixed
`DQIX_PERF_SCENE` override is useful when reproducing a known scene.

## Metrics and interpretation

- `memory_lookups`: memory-bus read/write accesses, with ARM9 and ARM7 subtotals.
- `emulated_calls`: calls entering the interpreter fallback, with CPU subtotals.
- `allocations` and `allocated_bytes`: successful `malloc`, `calloc`,
  `realloc` and C++ `new` requests. Bytes are cumulative requested sizes, not
  current live heap size, resident memory, or a fragmentation measurement.
- Scope rows contain counter deltas measured across the scope lifetime. Frame rows
  contain counters accumulated since the previous emulator frame boundary.

The C runtime uses `perf_scope_begin()/perf_scope_end()`; C++ code can use
`ScopedPerfLogger` from `runtime/perf_raii.hpp`. The counters are atomic because
ARM7 and ARM9 may run on separate threads. A long scope crossing a frame boundary
still reports its lifetime delta; frame rows use a moving cumulative-counter baseline.
