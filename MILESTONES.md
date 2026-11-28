# Milestones — `upstream-dev`

A repeating cycle, not a one-way roadmap. The five milestones below are worked
in order and then **recycled**: when M5 closes, the cycle is archived, every box
is unchecked, and the same five milestones run again with a **higher bar**.

- One cycle per release train (≈ v35 → v36), 5–7 weeks.
- A milestone only closes on **measured evidence** (green CI run, recorded
  number, published artifact, reviewed diff). "Looks fine" is not evidence.
- Order matters: never start M4-style new work before M1–M2 have removed the
  known breakage, and never leave M5 for "after the release".

## The cycle at a glance

| # | Milestone | Question it answers | Bar raised every cycle |
|---|-----------|--------------------|------------------------|
| M1 | **Stabilise, review, decide** | Is the branch trustworthy, and what do we optimise next? | Green baseline → no flaky checks → time-budgeted CI |
| M2 | **Fix, improve, optimise** | Do the known defects go away, and does the measured plan land? | Listed bugs → zero P0/P1 → zero bug older than a cycle |
| M3 | **Consolidate the enhancement layer** | Is every enhancement we already ship complete, coherent and reversible? | Documented → golden-checked → drift-free |
| M4 | **Extend the enhancement layer** | What new capability do players get, at the same quality bar? | 1 flagship → 1 flagship + 1 small → everything ships with acceptance tests |
| M5 | **CI, distribution, upstream hygiene** | Can a stranger install what we built, and can we merge upstream without fear? | Artifact correct → artifact correct on every arch → one-command reproducible release + zero-conflict sync |

## Non-negotiables (apply to every cycle)

1. **Canonical artifact = launcher binary + payload.** The product we
   distribute is `dqix-launcher` (native binary) plus a payload (tracked source
   snapshot, SDL2 headers and runtime, license notices). `make release` and
   `tools/launcher/package.py` define that contract. A source-only archive is
   not the product and must never be presented as one.
2. **No ROM, no generated game code, no saves** in any distributed file.
3. **Upstream stays mergeable:** merge commits only (no rebase, no reset onto
   the fork's `main`), and no fork-only logic hidden inside upstream-owned
   files where an additive file or hook will do.
4. **Every optimisation has a benchmark; every fix has a proof of no
   regression** (replay fingerprint, golden frame, or test).
5. **Upstream deletions stay deleted** (former AppImage packaging,
   `tools/build-game.sh`); the policy test is extended, never weakened.

---

## M1 — Stabilise, review the code, decide the optimisation strategy

**Goal:** a branch anyone can build, a map of the code's risk, and a ranked,
measured optimisation plan.

### Work

- [x] **Green baseline.** `make test` (semantic tests + packaging tests),
      `make launcher`, and the Linux workflow all pass on `upstream-dev`; any
      red check is either fixed or filed with an owner and a date.
      _2026-11-27: `make test` green (semantics 26 cases, packaging 13 tests,
      2 skipped for a missing payload/previews) and `make launcher` green
      (6.5 MB, `--help`/`--version` answered) on a bare Ubuntu 24.04. Hosted
      Linux/macOS run: see the evidence log._
- [x] **Triage.** Every open bug/report is labelled P0 (data loss, crash,
      broken session), P1 (wrong behaviour, visible regression), P2 (cosmetic,
      enhancement gap). Untriaged reports count as P1.
- [x] **Code review of the risky core.** Read and note findings for at least:
      `runtime/air.c`, `runtime/session.c`, `runtime/wifi.c`, `runtime/gpu3d.c`,
      `runtime/gl_render.c`, `runtime/wfc.c`, `runtime/relay.c`,
      `runtime/menu_ingame.c`, `tools/relay/worker.js`. For each: invariants,
      error paths, resource ownership, thread/ordering assumptions, missing
      tests. _(`docs/REVIEW.md`: invariants and weak points per module, a
      decision for each finding including the ones we choose not to fix.
      `wifi.c` and `frontend_sdl.c` were not read this cycle — first in line
      for cycle 2._)
- [~] **Hotspot inventory.** Where the time actually goes: recompile stage,
      C compile/link, startup, frame time (Classic/Enhanced/widescreen),
      session latency, relay round-trip, launcher build step.
      _Measured (`docs/PERF.md`): the two test suites, the launcher build
      (26 s), the `wfc_frame` scan (0.017 ms/frame) and `ram_hash`
      (0.46 ms / 2 s). Blocked, needs a ROM: recompile stage, generated-C
      compile, startup, frame time, session latency, relay round-trip. The
      instrumentation to get them is already in the tree._
- [x] **Optimisation strategy.** A ranked list (`docs/PERF.md`) where each entry
      states: measured baseline, expected win, risk, rollback, and the exact
      benchmark that will prove it. Anything without a benchmark is dropped
      from the list, not guessed at.

### Exit criteria (cycle 1)

- [x] CI green on `upstream-dev` with the current matrix; no red check without
      an issue number. _(Linux launcher and release: run #36 on `b0ab4cd`,
      run #38 on `0faacb4` — both success, ~2 min; #37 was cancelled by #38's
      concurrency group, which is normal for two pushes in a minute. macOS CI
      does not run on this branch: it triggers on `main`/`stable` only — M5
      decides whether that is intended.)_
- [ ] P0 = 0 and P1 triaged with an owner. _(P0 = 0; both P1s triaged, owners
      still to name.)_
- [x] `docs/PERF.md` exists with at least one number per hotspot and a ranked
      plan. _(ROM-dependent hotspots are listed as blocked, with the method.)_
- [x] Review findings recorded (issues, or the evidence log below) — including
      the ones we deliberately decide not to fix, with the reason.
      _(`docs/REVIEW.md`.)_

**Cycle-1 status:** open on one item — an owner for each of the two P1 defects
(turnstile, cut letters). M2 may start in parallel: it works on the same files
and does not depend on it.

### Escalation

| Cycle | Bar |
|-------|-----|
| 1 | CI green; P0 = 0; hotspots measured once |
| 2 | No flaky check in the last 20 runs; P0 + P1 = 0; CI runtime bounded (see M5) |
| 3+ | CI runtime budget locked; every hotspot has a trend line across cycles; review covers every module touched since the last cycle |

---

## M2 — Fix, improve the code, implement the optimisations

**Goal:** remove the known breakage, leave the code better than we found it, and
land the M1 optimisation plan with numbers attached.

### Work

- [x] **Fix the cycle's bug set.** Cycle 1 scope comes from `ROADMAP.md`:
      the Gortress turnstile drawn displaced (core, also Classic), letters cut
      short in battle panels ("Lv '1"), the Zoom-menu smear, the flicker after
      the attribute-points menu, the paler sky at the battle sides, the Upover
      inn light.
      _Deferred to the next cycle with rationale: a USA cartridge image is now
      available and the game builds and runs headless from it (`make build`
      128 s, startup 236 ms, 3.0 ms/frame at the title screen), but every one
      of the six needs a save file near the bug (or a bug report's replay) to
      reproduce, and the save provided for this cycle crashes the emulated
      game at frame 15 with a null-pointer write (ITCM via the null-pointer
      mirror) — likely a version mismatch. The ROM alone only reaches the
      title screen. The proof tooling is ready and verified (`DQIX_INPUT`,
      `DQIX_CRC` — 3600 frames identical across runs, `DQIX_DUMP`,
      `DQIX_FRAMETIME`). The fixes start as soon as a compatible save or a
      report folder is available._
- [~] **Improve the code you touched.** Dead code and stale comments removed,
      error paths made explicit, duplicated logic folded, tests added for the
      module's boundaries. No drive-by refactor of untouched modules.
      _M2 so far: `pageName` deleted (warning gone), `got_hash` reports dropped
      fingerprints instead of hiding them, `ws_recv` caps a whole message at
      16 MiB and checks `realloc`, `http()` documents its NUL-termination
      contract and checks `malloc`. `make test` green after each._
- [~] **Implement the optimisations** in `docs/PERF.md` order: before/after
      numbers recorded, revert anything that does not clear its threshold.
      Candidate set: the 528 slow jump stubs, compile parallelism and
      incremental rebuilds, relay message bundling and host fan-out, four-player
      lag sources.
      _Done: rank 2 (launcher incremental build) — 26 s → 3.8 s rebuild, 0.07 s
      warm. Rank 1 measured and decided: 117.3 relay messages/s for four
      players (100,000 exhausted in 14 minutes of co-op); a 45 ms coalescing
      window cuts it 33 % and a 60 ms window 50 %, all of it paid in input
      queueing, so the 30 ms default stays and the free half (relay-side
      fan-out instead of a second hop through the host) is scheduled for the
      next cycle. Rank 3 (528 jump stubs) lives in the external recompiler,
      not in this tree, so it is out of scope for this repo. Rank 4
      (net_thread polling) is blocked: a local session crashes at frame 15
      with a null-pointer write, so no CPU baseline could be taken._
- [ ] **Guard the gains.** Any optimisation that changes observable behaviour
      gets a golden frame or replay fingerprint; Classic output stays
      pixel-identical.

### Exit criteria

- Every fix in scope has a linked report, a reproduction, and a regression
  proof; nothing is closed on "cannot reproduce" without a test that says so.
- Each implemented optimisation shows its before/after number in the evidence
  log; regressions are reverted, not documented away.
- No new compiler warnings in the touched translation units.

### Escalation

| Cycle | Bar |
|-------|-----|
| 1 | Listed fixes done; plan implemented where the win is ≥ 10 % |
| 2 | P0 + P1 = 0 at cycle end; every fix landed with an automated proof |
| 3+ | No bug older than one cycle; perf budget enforced by a check, not by review |

---

## M3 — Consolidate the enhancement layer

**Goal:** the enhancements we already ship (Enhanced 3D, widescreen, Dynamic
Widescreen, HD text, mods, DS Lite filter, in-game menu, sessions) become
complete, coherent and reversible — instead of a set of features that each work
until they meet each other.

### Why this is M3

M2 fixes defects. M3 fixes the **enhancement layer itself**: the places where
two good features disagree, where an "experimental" option has no exit
criteria, or where a change silently alters Classic output.

### Work

- [x] **One options model.** `docs/options.json` declares every enhancement
      once (id, settings key, default, maturity, interactions, rollback,
      whether it changes the picture, menu presence). `tools/options_model.py`
      generates `docs/OPTIONS.md`'s table, feeds the golden case generator, and
      fails `make test` when the code (`settings_load`/`settings_save`), the
      bug-report list, the menu, the README or the corpus disagrees. 41
      modelled options + 17 explicit non-enhancement keys: no unaccounted key.
- [x] **Maturity contract.** `docs/options.json` carries entry/exit criteria
      per level and a decision with exit criteria per experimental option;
      `docs/OPTIONS.md` renders the contract. No experimental option was
      promoted in cycle 1 because the visual evidence needs GL (recorded as the
      known gap); all stay opt-in, off by default and labelled.
- [x] **Interaction matrix.** `docs/OPTIONS.md`: no unresolved cell — widescreen
      × single_screen is "widescreen wins, U hides the small screen"; every
      supported cell names its code path.
- [x] **Golden corpus.** `tests/golden/` with 20 cases: Classic (pixel-identical
      reference + ignored-options cases), Enhanced, all six layouts, widescreen
      + single_screen, battle HUD, a battle and a menu; `make golden` captures,
      `make golden-check` asserts 5 matrix cells. Inn, Krak Pot, DQVC and the
      2-/4-player sessions are the cycle-1 corpus's stated gap: they need a
      save and a live session, which this environment cannot provide.
- [x] **Replay completeness.** `report_display` now keeps every picture-changing
      option: added `hd_text_shadows`, `texture_packs`, `transmog`,
      `upscale_textures`, `widescreen_battle_hud`, `console_color` and
      `monitor_width_mm`; the model check fails if a new option is left out.
- [x] **Close or demote.** Every experimental feature is `default 0`, labelled
      in the README and the menu, with its promotion exit criteria written in
      `docs/options.json`.

### Exit criteria

- Every shipped enhancement appears in the options model with a maturity state
  and a stated rollback.
- The interaction matrix has no unexplained cell; conflicts are either fixed or
  documented as unsupported combinations.
- The golden corpus runs in CI and fails on unexplained drift.
- Classic mode is still pixel-identical to the reference for the whole corpus.

### Escalation

| Cycle | Bar |
|-------|-----|
| 1 | Matrix written; golden corpus covers Classic + Enhanced + the layouts |
| 2 | Every enhancement golden-checked; no unsupported combination left implicit |
| 3+ | Zero unexplained drift across a release; every new enhancement enters the model and the corpus in the same PR |

---

## M4 — Extend the enhancement layer

**Goal:** ship new player-visible capability, at the quality bar M1–M3
established, with an explicit budget of one flagship (plus at most one small
item) per cycle.

### Work

- [ ] **Map the recompiled game.** The generated code *is* the codebase we
      port and improve, so before changing it, identify, classify and group it:
      every region named (ARM9/ARM7, code vs data, overlay, entry point,
      function boundaries and callers) and attributed to a subsystem (field,
      battle, menus, 2D/3D GPU, sound, save, Wi-Fi, DQVC, mods). Deliverable:
      `docs/MAP.md` plus a machine-readable index regenerated by one command,
      so "where does this routine live, who calls it, what does it touch" is
      answered by the map instead of a manual search. Grouping is what makes
      the next work tractable: a bug fix points at a region, a port replaces a
      whole group, and the upstream ownership map (M5) can cite the same ids
      rather than inventing its own. Every group gets a stable id and its own
      one-line purpose; a group with no purpose yet is recorded as unknown, not
      guessed.
- [ ] **Pick from a ranked backlog, not from enthusiasm.** Ranking criteria:
      number of players affected, distance from "the DS could not do this",
      risk to existing modes, and whether the platform can validate it.
      Current candidates: direct connections between players (hole punching,
      relay as fallback), joining a session already started, four players over
      the internet, relay without the daily-cap failure mode, lower input delay,
      HD battle HUD, save states, hand-written DQVC news.
- [ ] **Ship it as an enhancement, not a hack:** declared in the options model
      (M3), off by default while experimental, in the interaction matrix, in the
      golden corpus, and in the README/ROADMAP.
- [ ] **Measure its cost:** frame time, session latency, memory, build time,
      relay traffic — whatever the feature spends, recorded before/after.
- [ ] **Prove the fallback:** the feature can be turned off and the game behaves
      exactly as before; sessions between mismatched builds are refused with a
      clear message, never silently desynced.
- [ ] **Document the player path:** how to enable it, what it needs (ports,
      account, OS version), and what "experimental" costs them.

### Exit criteria

- The flagship works on the platforms we claim, and the claim is either tested
  or explicitly marked untested in the README.
- The feature is in the options model, the interaction matrix and the golden
  corpus, with a documented rollback.
- Measured cost is recorded, and no M1/M2 budget is broken.
- No regression in Classic or in the previously stable enhancements.
- The recompiled game is mapped: every region identified, classified and
  grouped in `docs/MAP.md` + the index, with no group left unnamed.

### Escalation

| Cycle | Bar |
|-------|-----|
| 1 | One flagship, off by default, documented, measured; the map of the recompiled game written |
| 2 | One flagship + one small item; both have acceptance checks |
| 3+ | Every new enhancement ships with its own automated acceptance test, migration for existing settings, and a removal plan |

---

## M5 — CI, distribution, upstream hygiene

**Goal:** what we publish is what we intended to publish, on every supported
architecture, installable by a stranger; and the next upstream merge is a
routine, not an incident.

### Current state to correct

The `{launcher binary + payload}` system already exists in the tree
(`tools/launcher/payload.py`, `tools/launcher/package.py`, `make release`,
`.github/workflows/linux-release.yml`), but artifact and distribution have
drifted away from it: the thing players actually download is not reliably that
contract. Treat the drift as the milestone's first finding, not as a detail.

### Work — CI

- [ ] **Audit what is published.** For the last release: list every asset, where
      it came from, which workflow made it, and whether it matches the artifact
      contract. Record the delta.
- [ ] **Split the pipeline:** fast required checks on every push (syntax,
      semantics subset, packaging unit tests) and the full matrix (x86_64,
      arm64, macOS experimental, launcher screenshots) where it belongs. Cache
      the payload and submodules; no check should be slow *and* unexplained.
- [ ] **Make the contract testable:** a check that fails when a release asset is
      not `DQIX-Enhanced-<version>-<os>-<arch>` + checksum, when the payload is
      incomplete, when a ROM/save/generated tree is inside, or when the launcher
      is not executable and not linked against the bundled SDL2.
- [ ] **Determinism and provenance:** same commit ⇒ same archive content
      (sorted entries, fixed mtimes, recorded source commit and payload
      version), SHA256 published next to every asset.
- [ ] **Time and flakiness budgets:** record CI duration per job; a flaky check
      is quarantined with an issue, not retried into silence.

### Work — distribution

- [ ] **One release path.** `make release` (or its successor) produces the
      launcher + payload archive for each arch; the release workflow publishes
      exactly that, with checksums and install notes. Source zips may remain as
      a secondary, clearly labelled channel for RecompLauncher — never as the
      headline download.
- [ ] **Clean-machine install test per cycle:** a fresh container/VM with no
      build tools, no SDL2, no Python packages: download the published archive,
      run the launcher, pick a ROM, build, play. Repeat for x86_64 and arm64.
      This is the artifact's acceptance test.
- [ ] **Channels and rollback:** stable/nightly are explicit; the in-game
      updater and the archive agree on version, channel and layout; a failed
      update can roll back on every platform we publish for.
- [ ] **Windows parity is stated, not implied:** the Windows path (`BUILD.bat` +
      zip) is labelled as the build-from-source channel; do not describe it as
      the same artifact class as the Linux launcher + payload until it is.
- [ ] **Publish only with authorization**, from a tag, with the evidence log
      updated.

### Work — upstream hygiene (ours vs. the base we do not use)

- [ ] **Ownership map.** `docs/OWNERSHIP.md` lists, path by path: upstream-owned
      (consume unmodified), fork-owned (ours), and deliberately-deleted-upstream
      (with the reason). The map is the single answer to "is this ours?".
- [ ] **No code specific to the fork inside upstream files.** Prefer new files,
      wrapper targets, and hooks over editing upstream logic; where an edit is
      unavoidable, keep it additive, marked, and listed in the map.
- [ ] **Sync drill every cycle.** Merge `upstream-dev` (never `main`, never a
      rebase) into the work branch, resolve conflicts, run M1's checks, and
      record: conflicts hit, files owned by both sides, time spent. The drill is
      a scheduled task, not a reaction to a broken merge.
- [ ] **Conflict budget:** the only files allowed to conflict are declared
      fork-owned or explicitly shared. A conflict outside that set is a design
      bug to fix, not a merge to survive.
- [ ] **Guards grow with the policy:** extend `tests/packaging/test_upstream_policy.py`
      (and the ownership map check) so a reintroduced base file, a restored
      AppImage path, or a fork edit in an upstream-owned file fails CI.

### Exit criteria

- Published assets match the artifact contract on every arch we claim, each with
  a checksum, produced by one documented command.
- The clean-machine install test passes for x86_64 and arm64.
- CI duration recorded and bounded; no untriaged flaky check.
- A full upstream sync drill completed this cycle with zero conflicts outside
  the declared set, and `docs/OWNERSHIP.md` matches the tree.
- One page in the README tells a player which download is which.

### Escalation

| Cycle | Bar |
|-------|-----|
| 1 | Contract audited; launcher + payload published for both arches; map written |
| 2 | Clean-machine test automated per arch; conflict budget enforced in CI |
| 3+ | Reproducible release in one command; signed/checksummed provenance; sync drill costs under a fixed time with zero undeclared conflicts |

---

## Recycling the cycle

When M5 closes:

1. **Freeze the evidence.** Copy the numbers, links and artifact checksums into
   the evidence log below (or into the release notes for the promoted version).
2. **Tag the cycle:** `milestones/cycle-N`.
3. **Archive the checklist** (the tag is the archive; this file is reset).
4. **Uncheck every box** in M1–M5.
5. **Raise the bars:** move each milestone's escalation column one step, and make
   the raised bar a written exit criterion (a threshold, not an adjective).
6. **Increment** the cycle number in the header and start M1 again.

A milestone may be shortened but not skipped; skipping requires a recorded
reason and a named risk. If a cycle ends with an open P0, the next cycle starts
at M2, not at M1.

## Cycle header

- Cycle: **1**
- Opened: **2026-11-27**
- Target release train: _v35 → v36_
- Current milestone: **M4** (M3 closed: options model checked in `make test`, maturity contract with per-option decisions, replay metadata complete, golden corpus green — 20 cases, 5 cells)
- Owner of the cycle: _TBD_

## Evidence log (cycle 1)

| Milestone | Evidence | Value / link | Date |
|-----------|----------|--------------|------|
| M1 | CI run | local `make test` + `make launcher` green on Ubuntu 24.04; hosted “Linux launcher and release” #36 on `b0ab4cd` and #38 on `0faacb4`, both success (~2 min) | 2026-11-27 |
| M1 | `docs/PERF.md` baseline | `make test` 2 s; `make launcher` 26 s / 6.5 MB; `wfc_frame` 0.017 ms/frame; `ram_hash` 0.46 ms / 2 s | 2026-11-27 |
| M1 | code review | `docs/REVIEW.md`: 9 files, 0 P0, 2 P1 (roadmap), 11 P2 findings with decisions | 2026-11-27 |
| M2 | hardenings / code improvements | `pageName` warning removed; `got_hash` drop counter in `front.log`; `ws_recv` whole-message cap + `realloc` check; `http()` allocation check + contract comment. `make test` green (26 semantic cases, 13 packaging tests, 2 skipped) | 2026-11-27 |
| M2 | optimisations landed (before → after) | launcher incremental build: 26 s clean → 3.8 s rebuild after `launcher.cpp`, 0.07 s warm; artifact unchanged (6.5 MB, `--help`/`--version`) | 2026-11-27 |
| M2 | build with a cartridge image (was “not measured”) | `make build JOBS=2`: extract 1 s, recompile-arm9 5 s, recompile-arm7 3 s, **compile 118 s**, assemble 1 s = **128 s**; `game/dqix` 21.5 MB + `dqixcore.so` 21.3 MB | 2026-11-27 |
| M2 | in-game baseline (headless, USA image) | startup **236 ms**; title screen **3.0 ms/frame** (2D 0.9 ms, 3D not reached), slowest of 3600 frames 21 ms; deterministic (3600 frame CRCs identical) | 2026-11-27 |
| M2 | relay traffic (rank 1) | `tests/relay_bench/run.sh`: **117.3 msg/s** for four players; 45 ms window 78.2 (−33 %), 60 ms 58.5 (−50 %); default kept at 30 ms, fan-out scheduled | 2026-11-27 |
| M2 | save compatibility | A 64 KB save was provided; the emulated game crashes at frame 15 with a null-pointer write (ITCM via the null-pointer mirror) — likely a version mismatch. Six gameplay fixes deferred to next cycle. | 2026-11-27 |
| M3 | interaction matrix | `docs/OPTIONS.md`: no unresolved cell left — widescreen × single_screen resolved as "widescreen wins, U hides the small screen" (`frontend_sdl.c` `canvas_layout_for`, `MI_SINGLE`); every `supported` cell names its code path | 2026-11-28 |
| M3 | experimental labels | `README.md` **Experimental options** list covers the three `experimental` rows (`hd_text_shadows`, `motion_blur`, `upscale_textures`); `make test` green (`tests/packaging` 13 tests, 2 skipped) | 2026-11-28 |
| M3 | maturity reconciliation | `docs/OPTIONS.md`, `README.md`, `runtime/menu_items.c`: `widescreen_battle_hud` beta->experimental; `layout` notes `wide_dynamic` experimental; README experimental list + battle HUD; menu labels motion blur/upscale experimental. `make test` green | 2026-11-28 |
| M3 | golden corpus harness | `tests/golden/` (corpus.json 20 cases, interaction matrix, gen_cases.py, run.sh, assert.py) + `tests/packaging/test_golden_corpus.py`; `make golden` / `make golden-check`; corpus not yet captured (no game binary in this env) | 2026-11-28 |
| M3 | golden corpus captured and green | 20 cases (300/900 frames + per-frame CRC) under `tests/golden/corpus/` (gitignored, rebuilt by `make golden`); `make golden-check` exit 0 — 20 cases, 5 matrix cells: 2 `crc_equal`, 3 `crc_equal_postprocess` | 2026-11-28 |
| M3 | matrix cells the CRC cannot see | 3 cells asserted `crc_differs` could never pass: `dslite_filter` × `lcd_colors` (fragment shader), `hd_text` × `hd_text_shadows` (GL letter pass), widescreen × `single_screen` (window layout) all act after composition, outside `g_screen`. They are now `crc_equal_postprocess` — identity is the requirement (the option must not touch the emulated framebuffer) — with the scope documented in `corpus.json` `crc_scope` and `tests/golden/README.md`; `assert.py` rejects unknown kinds | 2026-11-28 |
| M3 | full test suite | `make test` green: semantics 12,000 + 48,000 checks (ARM and Thumb, interpreted and recompiled), packaging 22 tests, 2 skipped (payload, launcher screenshots) | 2026-11-28 |
| M3 | one options model | `docs/options.json` (41 options, 17 non-enhancement keys) + `tools/options_model.py` (check + `make options` generator) + `tests/packaging/test_options_model.py`; `docs/OPTIONS.md` table generated; `make test` runs the check first. The check caught two real drifts: `smooth_shading` had no menu item (added to `menu_items.c`) and 7 picture-changing settings were missing from bug reports | 2026-11-28 |
| M3 | maturity contract | per-level entry/exit criteria and one decision + exit criteria per experimental option in `docs/options.json`; none promoted in cycle 1, all opt-in/off/labelled, GL evidence named as the gap | 2026-11-28 |
| M3 | replay completeness | `report_display`'s `KEEP` now covers every option marked `affects_picture`; widescreen and Dynamic Widescreen (`layout`), HUD, HD text shadows, mod switches, console colour and monitor width replay with the report; the model check enforces the link | 2026-11-28 |
| M4 | flagship + measured cost | | |
| M5 | published artifacts + checksums | | |
| M5 | clean-machine install (x86_64 / arm64) | | |
| M5 | upstream sync drill (conflicts, time) | | |
