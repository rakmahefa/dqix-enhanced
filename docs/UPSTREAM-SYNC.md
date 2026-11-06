# Syncing the Linux launcher branch with upstream

This feature branch is based on upstream-dev, after the merge that removed the former AppImage packaging. Keep that commit in the ancestry of the work.

## Safe sync procedure

1. Fetch the latest changes from the upstream repository.
2. Merge the upstream changes into the local upstream-dev tracking branch.
3. Merge the updated upstream-dev into feat/linux-launcher-release using a normal merge.
4. Resolve source conflicts in the feature branch. Do not restore the deleted AppImage packaging or the former shell build script.
5. Run the root Makefile tests, compile the launcher, and inspect the CI screenshot artifact.
6. Push the feature branch and update its pull request.

Do not rebase this feature onto the fork's main branch and do not reset it to a synchronized main branch. The fork's main has a separate history and can reintroduce the files that the cleanup merge deliberately removed.

## Automated guard

tests/packaging/test_upstream_policy.py makes CI fail if any former AppImage workflow or source path returns, or if tools/build-game.sh is restored. It deliberately does not prohibit unrelated future workflows: upstream can add valid workflows without weakening this deletion policy.

The intended sync unit is upstream-dev, not main. Keep changes as normal merge commits so the cleanup ancestry remains visible to Git's merge algorithm.
