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
| `make build` (extract + recompiler + native compile of generated C) | **128 s clean, `JOBS=2`** | `game/dqix` 21.5 MB + `game/dqixcore.so` 21.3 MB | measured with a USA cartridge image (SHA-1 `c7c3014c…`). Stages below |

Clean build, stage by stage (`JOBS=2`, 2 vCPUs, no ccache):

| Stage | Time | What it does |
|---|---|---|
| `extract` | 1 s | cartridge → `arm9.bin`, overlays, ARM7 code |
| `region` | <1 s | region detection, files list |
| `recompile-arm9` | 5 s | 14,815 functions → 63 C files, 914k lines |
| `recompile-arm7` | 3 s | 962 functions → 6 C files |
| **`compile`** | **118 s** | 69 generated C files at `-O1` (the bulk) + 40 runtime objects + link |
| `assemble` | 1 s | game folder, ROM copy, skins, fonts |
| **total** | **128 s** | |

The recompiler is not the cost; the host compiler is. A second full build is
128 s again — nothing in this path is incremental except the launcher (§2).
That is fine for a player (one build per install, not per run) and is why the
plan does not chase it further: the win would need a build cache, not
algorithmic work.

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

One packaging trap, found while building the game in a container whose SDL2
lives outside `/usr/include`: Debian's `SDL2/SDL_config.h` is only an
`#include <SDL2/_real_SDL_config.h>` indirection, and the real file sits under
the **multiarch** dir (`/usr/include/x86_64-linux-gnu/SDL2/`). If `sdl2-config
--cflags` yields `-I/usr/include/SDL2` alone and that dir is not inside a
default include path, the build stops at `fatal error: SDL2/_real_SDL_config.h:
No such file or directory`. Adding `-I<prefix>/include` fixes it; the message
names the file but not the reason.

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

## 4. Inside the game (USA cartridge image, headless)

Measured with the image the milestone was unblocked with (USA, SHA-1
`c7c3014c237900c8281289b8bc76a781969b6278`), `FRONTEND=none`, no window:

| Measurement | Value | How |
|---|---|---|
| Startup to the end of frame 1 | **236 ms** (min of 3; median 237) | `SDL_VIDEODRIVER=dummy ./dqix <rom> <extract> 1 0` |
| Frame time, title screen | **3.0 ms/frame** wall, 3600 frames in 10.8 s | `DQIX_FRAMETIME=0`, 8-bucket histogram |
| — of which 2D render | 3.3 s / 3600 frames = **0.9 ms/frame** | the same report's `total:` line |
| — 3D render | 0 ms (no 3D scene reaches the renderer here) | the same |
| Slowest frame in those 3600 | 21 ms | histogram tail |
| Determinism | 3600 frame CRCs identical across two runs | `DQIX_CRC` + `cmp` |

The window build paces itself to 60 fps (`frames=601` takes 10.236 s), so it
cannot be used to measure CPU cost; the headless build can, and its 3.0 ms/frame
is the number to compare against after a change. Two runs of the same build
produce the same per-frame checksum, which is what makes a frame a usable
regression proof.

Still not measurable here, and why:

- any in-game scene (battles, towns, the menus): reaching one needs a save file
  or a bug report's replay; the ROM alone starts at the title screen;
- frame time in Enhanced and in each layout: needs a GPU (this host has none);
- session latency (input → visible movement) at 2 and 4 players, and relay
  round-trip: needs two live PCs on a real network.

The instrumentation exists (`DQIX_INPUT`, `DQIX_CRC`, `DQIX_DUMP`,
`DQIX_FRAMETIME`, `session/front.log`, `DQIX_SYNCLOG`); what is missing is a
saved position and a reference frame, not a tool.

## 5. Ranked optimisation plan

Rule: an entry needs a measured baseline, an expected win, a risk, a rollback,
and the exact benchmark that will prove it. Anything without a benchmark is
dropped from the list rather than estimated.

1. **Relay message count (fewer messages, same feel).** *Baseline measured* with
   `tests/relay_bench/run.sh` (four players, 60 Hz, 20 s, counted at a
   stand-in relay that speaks the room protocol): **117.3 messages/s**, i.e.
   100,000 in 14 minutes of four-player co-op. Split: 3 friends × 29.3/s
   uplink, host 29.3/s bundled `RM_MULTI` — about one message per two frames,
   which is what the 30 ms `COALESCE_MS` window already buys.
   *Sweep, same harness, `-DCOALESCE_MS=`:*

   | Window | Messages/s | vs. 30 ms | Frames per message |
   |---|---|---|---|
   | 30 ms (current) | 117.3 | — | 2 |
   | 45 ms | 78.2 | −33 % | 3 |
   | 60 ms | 58.5 | −50 % | 4 |

   *Decision:* the win is real and the roadmap's estimate (a third to a half)
   is right, but it is bought entirely with input queueing: every extra 15 ms
   of window is 15 ms a friend's press spends in the bridge before it leaves.
   The session's delay budget (`compute_delay`, at least 3 frames) has about
   58 ms of margin for jitter, so a 45 ms window would leave ~13 ms of it, and
   that latency cannot be measured here (it needs two PCs and a real network).
   The default therefore **stays at 30 ms**; the knob is a compile-time
   constant and the harness reproduces the table in one command. The free
   half of this entry is elsewhere: each friend's input crosses the relay
   **twice** (friend → relay → host, then host → relay → the others). Letting
   the relay fan out `RM_DATA`/`RM_MULTI` server-side would remove the host's
   second hop without touching latency — a worker.js + protocol change, next
   cycle, with the same harness to prove it. **This was M2's headline; the
   number is now known and the change is deliberately not flipped blind.**
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
4. **`net_thread`'s 1 ms polling.** *Baseline:* blocked. A local two-player
   session (`DQIX_SESSION=local:2`) crashes at frame 15 with a null-pointer
   write (ITCM via the null-pointer mirror) — the same fault as the provided
   save, likely a version mismatch — so no CPU baseline could be taken on this
   host. Structurally it is 1000 wakeups/s per player doing four non-blocking
   socket reads, a send pass and three housekeeping calls. *Expected:* an idle
   co-op session should cost almost nothing; this is a wakeup and CPU-budget
   win, not a latency win. *Risk:* low, but a `select` timeout is exactly
   where a latency regression hides. *Rollback:* one loop body. *Proof:* CPU
   time of an idle 2-player session over 60 s, plus unchanged input latency in
   the same log. **Deferred.**
5. **`wfc_frame` scan (0.017 ms/frame).** Measured. **Dropped** — the win is
   0.1 % and any cache is a correctness risk.
6. **`ram_hash` (0.46 ms / 2 s).** Measured. **Dropped** — 0.02 %, and this is
   the function that answers "are we still in sync".

Not on the list, because no benchmark exists and no number is claimed: the
recompile stage, the generated-C compile, startup, shader compilation, texture
upload and the four-player lag felt by players (§4 has the instrumentation to
produce the numbers; M2 starts from them).
