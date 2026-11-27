# Performance baseline — cycle 1 (M1)

What the machine actually spends, measured rather than guessed, and the ranked
plan that follows from it. A number that is not here was not measured; the
blocked ones say so and say how to measure them.

Method: this container (Ubuntu 24.04, x86_64, gcc 13.3, clang 18.1.3, Python
3.12) with the repository checked out at `upstream-dev`. Wall-clock numbers are
`SECONDS` around the command, single run, warm page cache. The two
micro-benchmarks are compiled copies of the real loops, run twice to show the
spread; they measure the loop, not the game around it.

## 1. Checks we run on every change

| Check | Command | Time | Result |
|---|---|---|---|
| Semantic tests (recompiler vs native, ARM + Thumb + interpreter) | `python3 tests/semantics/run.py 2000` | ~2 s | pass (26 cases, 2000 iterations; interpreter at 500) |
| Packaging tests | `python3 -m unittest discover -s tests/packaging` | ~0.2 s | pass, 13 tests, 2 skipped (payload not built, screenshots not generated) |
| `make test` (both of the above) | `make test` | 2 s | pass |

The semantic suite compiles `cases.c` twice with clang, links with `lld`,
recompiles it with our recompiler, then runs both paths on the same random
inputs. Two seconds for that is cheap; the CI runtime is dominated elsewhere.

## 2. Builds

| Build | Time | Artifact | Notes |
|---|---|---|---|
| `make launcher` (7 translation units) | **27 s clean, 3.8 s after touching `launcher.cpp`, 0.07 s warm** | `dqix-launcher`, 6.5 MB, dynamically linked against SDL2 | per-file objects with `-MMD -MP` dependency tracking (M2); unchanged ImGui objects are not recompiled |
| `make build` (recompiler + native compile of generated C) | **not measured** | — | needs a ROM; `rom/` ships only a README |

`make launcher` used to be 26 s of pure serial compilation of 6 ImGui sources
that never change. M2 split it into per-file objects with `-MMD -MP`
dependency tracking: a clean build is unchanged (27 s, same artifact), a
rebuild after touching `launcher.cpp` is 3.8 s, and a no-op rebuild is 0.07 s.
Same artifact, no behavioural change. **Rank 2 below — done.**

A bare container also needs `libsdl2-dev` **and** `libx11-dev` (ImGui's SDL2
backend includes `SDL_syswm.h`, which includes `X11/Xlib.h`). The CI image has
both; a contributor's minimal image does not, and the failure message
(`SDL_syswm.h:69: fatal error: X11/Xlib.h`) does not say what to install. Worth
a line in the README's Linux prerequisites (M5).

## 3. Things that run inside the game loop

| Loop | Where | Measured | Share of a 16.7 ms frame |
|---|---|---|---|
| `wfc_frame` overlay scan: 1 MiB, every 8 frames | `runtime/wfc.c` | 0.017 ms/frame (1.00 ms per second of play) | 0.1 % |
| `ram_hash`: FNV over 4 MiB main RAM, every 120 frames | `runtime/session.c` | 0.46 ms per call = 0.46 ms every 2 s | 0.02 % |

**Both are below the noise floor and are recorded here so nobody optimises
them.** In particular the lockstep fingerprint (`ram_hash`) is not worth
incrementalising: a dirty-page scheme would save 0.02 % of the frame budget and
cost a correctness risk in the one function that decides whether a session is in
sync. Same for `wfc_frame`: 0.017 ms is 8 scans a second over a megabyte; a
cache would have to be invalidated by any write to that range, which is worse
than the scan.

## 4. Blocked: needs a ROM

Not measured, and cannot be in this environment:

- the recompiler stage (`make extract region recompile-arm9 recompile-arm7`);
- the native compile of the generated C (the bulk of a first build);
- startup time to the first frame;
- frame time in Classic, Enhanced, and each layout;
- session latency (input → visible movement) at 2 and 4 players;
- relay round-trip and the number of relay messages per second of co-op.

Each of these has a cheap instrumentation point already in the tree: the launch
log, `session/front.log` (which already timestamps every message and every
"STUCK" wait), and `DQIX_SYNCLOG`. Cycle 1 leaves them open with the plan to
record them on a machine with a cartridge image, which is also what M2's relay
work needs before it changes anything.

## 5. Ranked optimisation plan

Rule: an entry needs a measured baseline, an expected win, a risk, a rollback,
and the exact benchmark that will prove it. Anything without a benchmark is
dropped from the list rather than estimated.

1. **Relay message count (fewer messages, same feel).** *Baseline:* not yet
   measured (see §4) — the roadmap records that the free plan's 100,000 daily
   requests ran out on 2026-10-30, and `COALESCE_MS` is 30 ms at each end.
   *Expected:* the roadmap's own estimate — 2-3 frames of inputs per message
   takes a third to a half of the requests; the host already bundles all
   friends into one `RM_MULTI` message. *Risk:* more input delay per frame if
   bundling is done naively (the delay budget in `compute_delay` assumes one
   round trip). *Rollback:* `compute_delay` and the bundle size are two
   constants; revert both. *Proof:* count the relay messages the client sends
   in 60 s of a 4-player session, before and after, and log the input→motion
   latency from `front.log`'s timestamps. **This is M2's headline.**
2. **Launcher build: compile ImGui once.** **DONE (M2).** *Baseline:* 26 s
   for `make launcher` on this machine, 7 TUs in one command. *After:* per-file
   objects (`-MMD -MP`): 27 s clean, **3.8 s** after touching `launcher.cpp`,
   **0.07 s** warm. *Risk:* none to the artifact; only the Makefile changed.
   *Rollback:* restore the one-command link line. *Proof:* `make launcher`
   from clean and from warm, before and after; the binary is still 6.5 MB and
   `--help`/`--version` answer.
3. **The 528 slow jump stubs.** *Baseline:* unmeasured (needs a ROM and a
   scene that jumps a lot). *Expected:* the roadmap calls it the remaining
   dispatch overhead; realistic ceiling is a few percent of frame time in
   script-heavy scenes. *Risk:* medium — it is the recompiler's dispatch path,
   the one place a subtle bug breaks every game in a way tests may miss.
   *Rollback:* the stub generation is a recompiler table entry. *Proof:* the
   semantic suite plus a replay fingerprint of a scene that exercises them,
   before and after, and frame time in that replay.
4. **`net_thread`'s 1 ms polling.** *Baseline:* unmeasured; structurally it is
   1000 wakeups/s per player doing four non-blocking socket reads, a send pass
   and three housekeeping calls. *Expected:* an idle co-op session should cost
   almost nothing; this is a wakeup and CPU-budget win, not a latency win.
   *Risk:* low, but a `select` timeout is exactly where a latency regression
   hides. *Rollback:* one loop body. *Proof:* CPU time of an idle 2-player
   session over 60 s, plus unchanged input latency in the same log.
5. **`wfc_frame` scan (0.017 ms/frame).** Measured. **Dropped** — the win is
   0.1 % and any cache is a correctness risk.
6. **`ram_hash` (0.46 ms / 2 s).** Measured. **Dropped** — 0.02 %, and this is
   the function that answers "are we still in sync".

Not on the list, because no benchmark exists and no number is claimed: the
recompile stage, the generated-C compile, startup, shader compilation, texture
upload and the four-player lag felt by players (§4 has the instrumentation to
produce the numbers; M2 starts from them).
