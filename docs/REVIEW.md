# Code review — cycle 1 (M1)

The review M1 asks for: the risky core read once, its invariants and weak
points written down, and every known defect given a severity. Findings are
grouped by module and carry the decision taken (fix now, fix in M2, harden in
M3, or deliberately leave alone with the reason).

Reviewed in cycle 1: `runtime/air.c`, `runtime/session.c`, `runtime/relay.c`,
`runtime/wfc.c`, `tools/relay/worker.js`, `tools/launcher/launcher.cpp`, and a
targeted pass over `runtime/gpu3d.c`, `runtime/gl_render.c`,
`runtime/menu_ingame.c` (polygon clipping, vertex buffers, texture upload
paths, text fitting) rather than a full line-by-line read. `runtime/wifi.c` and
`runtime/frontend_sdl.c` are the largest files we did **not** read this cycle;
they are first in line for cycle 2.

Severity: **P0** data loss / crash / broken session · **P1** wrong behaviour ·
**P2** cosmetic, hardening, or developer-facing.

## Triage of what is known

The tracker has no open issues (0 open on `rakmahefa/dqix-enhanced` as of
2026-11-27), so the defect list is the one the roadmap carries. Proposed
labels, to be mirrored into issues as they are worked:

| Report | Where | Severity | Cycle |
|---|---|---|---|
| Turnstile in Gortress's prison drawn away from its shadow, one arm stretched across the floor (also Classic) | `ROADMAP.md` v35 | P1 | M2 |
| Letters cut short in battle panels ("Lv '1") | `ROADMAP.md` v35 | P1 | M2 |
| Zoom menu smears (Dynamic Widescreen) | `ROADMAP.md` v35 | P2 | M2 |
| Flicker after leaving the attribute-points menu | `ROADMAP.md` v35 | P2 | M2 |
| Paler backup sky visible at the far sides in battles | `ROADMAP.md` v35 | P2 | M2 |
| Upover inn window light flickers (not on a DS) | `ROADMAP.md` v35 | P2 | M2 |
| Lag with four players | `ROADMAP.md` polish | P1 | M2 (measured first) |
| Relay hit Cloudflare's 100,000-request daily cap (2026-10-30) | `ROADMAP.md` polish, `docs/STATE.md` | P1 | M2 |
| A coup de grâce goes wrong in some battles (no report yet) | `ROADMAP.md` | P1 | open — needs an F10 report |
| One test save never canvasses another in tag mode | `ROADMAP.md` loose ends | P2 | open |
| 528 small jump stubs on the slower dispatch path | `ROADMAP.md` loose ends | P2 (speed) | M2 |
| macOS claimed experimental, never run on a real Mac | `README.md` | P2 (claim) | M5 |
| Steam Deck claimed "on the way" / experimental, untested | `README.md`, `ROADMAP.md` | P2 (claim) | M4/M5 |

No P0 is known. The two P1 code defects (turnstile, cut letters) are both in
the recompiled core and both need a reference emulator frame to close, which is
why they lead M2 rather than being patched blind.

## `runtime/air.c` — shared memory between consoles

**Invariants (sound, keep them).** One ring per sender, single writer, so no
locks; `wr` published with a release store and read with an acquire load
(`st`/`ld`); a receiver's `rd[me][k]` is written only by that receiver; the deaf
window (`g_deaf_until`) is derived from ticks only, so every PC drops the same
frames and a session cannot drift because one PC was faster.

- **P2 — `air_input_put` keeps writing past the ring when the reader is `gone`.**
  The wait loop is `while (c - read >= INRING && !con[k].gone)`, so a console
  that left stops blocking the writer, which is intended; but the ring index
  then collides with what `air_input_peek` still answers for the last
  `INRING` frames. A late `air_input_get` (a hidden console catching up) can
  read inputs of another frame. Low impact today because a `gone` console is
  also told `AIR_INPUT_END`, but the two mechanisms should not depend on each
  other. Decision: add a comment and an explicit early-return on `gone` in the
  peek path when the frame is older than the ring, in M3.
- **Note.** `air_wait_describe` / `air_status` are used from the network thread
  while written from the game thread; fields are plain `int`/`double`, so a
  torn read is possible in the diagnostic strings only. Not a bug we will
  spend a cycle on, but it is why those strings occasionally show a stale
  tick.

## `runtime/session.c` — lobby, lockstep, hosted consoles

**Invariants (sound).** One message-prefixed stream per peer, length-prefixed
and re-framed by `peer_msg`; inputs are ring-buffered through `air.c` so the
lockstep does not need the network thread to be in step; a build fingerprint
(`build_id`) is exchanged and merely logged, not enforced — only
`DQIX_VERSION` is, which is the right call (separately compiled builds are
usually byte-identical in behaviour).

- **P1 — desync detection can go silent.** `got_hash` keeps at most 64 pending
  fingerprints (`g_pending[64]`); beyond that a new one is dropped without a
  trace. A long stall is exactly when many arrive at once, so the moment
  detection matters most is the moment it can thin out. Decision: count the
  drops and report them (a line in `front.log`, not a player-facing warning),
  M2 alongside the lag work.
- **P2 — the network thread wakes every millisecond.** `net_thread` ends each
  pass with `sleep_ms(1)`, doing `peer_io` on up to four sockets, `send_mine`,
  `check_hashes`, `check_children` and `watch` each time. That is 1000 wakeups
  per second per player whether or not anything happened. Measured cost is not
  known yet (see `docs/PERF.md`), so it is listed, not changed: the fix is to
  block on `select` with the same 1 ms ceiling and only run the periodic work
  when its own timer expired.
- **P2 — `kill(0, SIGTERM)` in `kill_children`.** Intentional and guarded: the
  process becomes its own group leader first (`setpgid(0, 0)`), so it only
  reaches our own hidden consoles. Kept; noted so a future reader does not
  "fix" it into a leak of orphan consoles.
- **P2 — `session_boot` spins on the main thread** (`while (g_state != 3)`)
  calling `session_lobby_hook` or sleeping 10 ms. Fine for a lobby screen, but
  it is also the path taken when a join fails, so the game cannot start its
  own frame loop; if the lobby hook ever goes away, this becomes a 100 Hz spin.

## `runtime/relay.c` — WebSocket client and TCP bridge

**Invariants (sound).** Every message is `[type, id, data...]`; a host's
`RM_MULTI` bundle is length-prefixed per 64 KB piece and the receiver's
`total` computation in `flush_all_host` matches the writer's header size
exactly; `R.c[id]` is indexed by a byte, so `MAXC` 256 covers the protocol's id
space legitimately.

- **P2 — `ws_recv` accumulates fragments with no upper bound.** Each fragment
  is capped at 16 MiB, but a message made of many fragments is reallocated
  without a total limit, and the payload is then `realloc`'d again per
  fragment. A hostile or broken relay can make the client allocate until the
  OS says no. Decision: cap a whole message at 16 MiB too, and treat exceeding
  it as "the relay connection ended", M3 (a hardening, nobody has hit it).
- **P2 — allocation results are not checked** in `send_msg`, `flush` and
  `flush_all_host`. A failed `malloc` there is a crash at the exact moment the
  machine is already under pressure. Decision: fold into the same hardening
  pass as above.
- **Note.** `COALESCE_MS 30` is a latency/size trade-off, not a bug: it is the
  first knob M2's relay work should measure (fewer messages = the daily cap
  lasts longer, but every byte waits up to 30 ms).

## `runtime/wfc.c` — the built-in Nintendo Wi-Fi server

**Invariants (sound).** Answers are built from the same files on every PC of a
session, and its RNG is a private deterministic LCG (`wfc_rand`) rather than
the C library's, so a download looks identical on every console. The NoSSL
patch rewrites only strings that name Nintendo's servers or the download path,
and only in main RAM.

- **P2 — the NoSSL patch assumes 4-byte alignment.** `wfc_frame` walks
  `0x200000..0x300000` in steps of 4 and only tests `p[0] == 'h'`. It works
  today because the game's strings are word-aligned, but an unaligned string
  would be silently skipped and the login would fail with a certificate error
  that looks like a network problem. Decision: leave the fast path, and add a
  one-line diagnostic when a `https://` string exists unaligned but was not
  patched (M3).
- **P2 — `http()` does not check its allocation** (`malloc(4096 + 65536)`) and
  assumes the request buffer is NUL-terminated (`sscanf`, `strstr`). The
  caller does terminate it today; the assumption is undocumented. Decision:
  document it at the function, check the allocation — M2, it is a two-line
  change in a file M2 will touch anyway.
- **Note.** `wfc_frame`'s scan measures 0.017 ms per frame (see
  `docs/PERF.md`), i.e. ~0.1 % of a frame. It is not a hotspot; do not
  "optimise" it into a cache without a new measurement.

## `tools/relay/worker.js` — the Cloudflare Worker

- **Invariants (sound).** A code is 8 characters from a 32-letter alphabet
  (~10^12), one host per code, join ids reused only after they are freed, and
  every send is wrapped in `safe()` so a departed socket cannot take the room
  down.
- **P2 — no per-address session cap and no lifetime limit.** The only limit is
  Cloudflare's daily request allowance; a code that leaks lets anyone holding
  it join, and a session can stay up indefinitely. The roadmap already asks for
  both (`the relay caps sessions per address and how long one can last`).
  Decision: M2, with the relay message-count work.

## `runtime/gpu3d.c` and `runtime/gl_render.c`

- **P2 — `clip_plane` writes into `Vtx temp[10]` without a bound.** Ten is
  exactly the worst case for a 4-vertex polygon clipped against one plane
  (4 kept + 2 new per offending vertex, plus the 2 carried when
  `clipstart == 2`). Nothing today produces a polygon with more vertices, so
  this is not a live bug — but a stack overflow here would corrupt the frame
  silently. Decision: keep the array, add the explicit bound and a debug
  assert (M3, with the golden corpus so a regression is visible).
- **Note — `gl_render.c`'s fixed scratch buffers are correct but tight.**
  `static uint8_t big[64 * TXG_S * 64 * TXG_S * 4]`, `up_src[256 * 256 * 4]`
  and `g_upbuf = malloc(512 * 512 * 16)` match the guards that gate them
  (`*w * *h <= 256 * 256`, `f <= 4`), so the upscaled output lands exactly at
  the buffer size. They are shared statics, which is only safe because texture
  upload happens on the render thread; that assumption should be written down
  where the buffers are declared.

## `tools/launcher/launcher.cpp`

- **P2 — `pageName(Page)` is defined and never used** (`launcher.cpp:158`), so
  every build prints
  `warning: 'std::string pageName(Page)' defined but not used`. The build does
  not use `-Werror`, so it is invisible in CI. Decision: M2 — either use it for
  the page titles it was written for or delete it; do not add `-Werror` until
  the tree is warning-clean.
