# Linux launcher and release design

## Intent

Provide a Linux release for DQIX Enhanced that follows the DQ8-Recompiled launcher experience: one downloadable archive containing a native launcher and its companion payload. The user supplies their own DQIX ROM; no ROM contents, generated game code or compiled game may be distributed.

## Launcher experience

The launcher follows the same five-step layout and order used by DQ8:

1. Your ROM — select or drop a personal .nds file.
2. Tools — report compiler, Make and graphics development prerequisites; offer distro package installation.
3. Options — choose the persistent workspace and number of parallel native C compilation jobs. ARM9/ARM7 code generation is currently sequential.
4. Build — show build milestones, live output, cancellation and retry.
5. Play — launch the game from the selected workspace.

The shell uses the reference design's navy background, framed panel, subtle shadow, left navigation, header, fixed footer and gold accent. CI renders every page and converts the captures to PNG. Screenshot artifacts must be separate from the Linux release archive artifacts.

## Build architecture

- The native C++17 launcher uses the existing SDL2 runtime and a pinned Dear ImGui submodule with the SDL2 platform and renderer backends.
- The root Makefile replaces tools/build-game.sh and orchestrates ROM validation, extraction, region mapping, ARM9/ARM7 recompilation, native compilation and assembly.
- Existing build.mk remains the lower-level compiler/linker makefile.
- The Linux payload contains a tracked source snapshot, standalone CPython 3.12.15, a pinned PyYAML 6.0.2 wheel, SDL2 headers/runtime and license notices.
- The host supplies a C compiler, GNU Make and OpenGL development headers; the recompiler emits C, which is built through `CC`. The launcher can install those packages on Debian/Ubuntu, Fedora-family and Arch-family systems.
- Release archives use a folder containing dqix-launcher and payload/. No AppImage is produced.

## CI and release

The Linux workflow builds x86_64 and ARM64 archives on Ubuntu runners, compiles the launcher, runs semantic and packaging tests, generates seven PNG UI screenshots, and uploads screenshots separately from release archives. A v* tag publishes only the tarballs and SHA-256 checksums as a GitHub Release.

## Upstream sync

The feature branch descends from upstream-dev after the AppImage cleanup merge. Sync with normal merges into upstream-dev and then into the feature branch. CI guards the deleted AppImage paths, old workflow and tools/build-game.sh against reintroduction. Do not rebase the branch onto the fork's unrelated main history.

## Non-goals

- No AppImage, Flatpak, Snap or distro-specific bundle.
- No Windows/macOS launcher in this iteration; the existing Windows entry point remains supported through the new Makefile.
- No C compiler or build-essential toolchain is bundled in the payload.
- No ROM, extracted game code, generated C, executable game or save bundled in the release.

## Verification boundary

The local environment cannot resolve github.com, so it cannot clone the repositories or execute the Linux build here. The CI workflow is therefore the required compiler and screenshot verification surface before merge; its actual build and screenshot results must be checked before claiming the release is ready.
