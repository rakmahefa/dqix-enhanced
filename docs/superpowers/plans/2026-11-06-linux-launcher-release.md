# Linux launcher and release implementation plan

> For agentic workers: use the executing-plans skill to implement the plan task by task. Steps use checkbox syntax for tracking.

**Goal:** Ship a DQ8-shaped Linux launcher with a portable source/tool payload and a sync-safe CI release.

**Architecture:** A C++17 Dear ImGui/SDL2 launcher invokes the root Makefile. The payload carries the project source, portable Python/PyYAML and SDL2. CI builds x86_64 and ARM64 archives and produces screenshot captures.

**Tech stack:** C++17, SDL2, pinned Dear ImGui submodule, GNU Make, Python 3.12.15, PyYAML 6.0.2, GitHub Actions.

**Spec:** docs/superpowers/specs/2026-11-06-linux-launcher-release-design.md

## Global constraints

- No ROM, save, extracted code, generated C, game binary or compiler in the release.
- Preserve build.mk as the low-level compiler makefile.
- Delete tools/build-game.sh; update BUILD.bat to invoke the root Makefile.
- Keep the branch based on upstream-dev; do not rebase it onto the fork's main.
- Keep all previously removed AppImage paths absent.

## Review focus

- USA and Europe ROM paths preserve their distinct region config.
- Workspace paths containing spaces remain single process arguments.
- Missing zenity/kdialog leaves drag-and-drop and a typed path usable.
- Cancelling or failing a build leaves logs visible and permits retry.
- Changing the ROM or region invalidates stale extraction and generated C outputs.

---

### Task 1: Replace the shell build orchestrator

Files: Makefile, tools/check-rom.py, tools/play.sh, tools/play.bat, BUILD.bat; delete tools/build-game.sh.

- [ ] Add unit tests for short and supported ROM headers.
- [ ] Verify the ROM test fails for a truncated header.
- [ ] Implement check-rom, extract, region, recompile-arm9, recompile-arm7, compile, assemble, build and run targets.
- [ ] Preserve build.mk as the lower-level build interface and use JOBS for parallel native compilation.
- [ ] Update BUILD.bat to invoke make and remove the old script.
- [ ] Run make help and the complete test suite.

### Task 2: Native DQ8-shaped launcher

Files: .gitmodules, third_party/imgui gitlink, tools/launcher/Makefile, tools/launcher/launcher.cpp.

- [ ] Pin Dear ImGui to f1cc2ae15e53a861a874c3034aae6798fde194ab.
- [ ] Build the five-page UI shell with SDL2 backends.
- [ ] Implement ROM drop/browse, tools checks, workspace options, worker-based build/log/cancel and Play.
- [ ] Implement preview mode that renders seven actual screenshots.
- [ ] Compile and inspect screenshots on a Linux runner.

### Task 3: Portable Linux payload and packaging

Files: tools/launcher/payload.py, tools/launcher/package.py, tests/packaging/test_release_payload.py.

- [ ] Pin standalone CPython 3.12.15 and the PyYAML 6.0.2 architecture wheels by SHA-256.
- [ ] Bundle SDL2 headers/runtime and preserve license notices.
- [ ] Copy tracked source and the pinned submodule while excluding private/generated ROM-derived files.
- [ ] Package dqix-launcher and payload/ under DQIX-Enhanced/.
- [ ] Verify runtime imports, manifest contents and absence of ROM/game artifacts.

### Task 4: CI, screenshots and release artifact

Files: .github/workflows/linux-release.yml, tests/packaging/test_launcher_screenshots.py.

- [ ] Build on Ubuntu 22.04 x86_64 and ARM64.
- [ ] Run semantic tests, ROM/Makefile/payload tests and screenshot smoke tests.
- [ ] Upload seven launcher screenshots and the corresponding release archive as Actions artifacts.
- [ ] Publish a GitHub Release only for v* tags and attach SHA-256 checksums.

### Task 5: Sync policy and documentation

Files: tests/packaging/test_upstream_policy.py, docs/UPSTREAM-SYNC.md, README.md, docs/TECHNICAL.md.

- [ ] Document the normal-merge upstream-dev sync procedure.
- [ ] Guard the former AppImage paths/workflow and the removed build script against reintroduction.
- [ ] Explain the Linux launcher and payload contents and host requirements.
- [ ] Verify the branch ancestry, changed paths and the results of the CI workflow.
