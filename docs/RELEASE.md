# Release artifacts and CI

M5 starts from a stated problem: the `{launcher binary + payload}` system exists
in the tree, but what players actually download has drifted away from it. This
page is the audit that opens the milestone, the contract the checks enforce, and
the budget each CI job runs against.

## The artifact contract

The product is **`dqix-launcher` plus its payload**, nothing else:

- one archive per architecture, named
  `DQIX-Enhanced-<version>-Linux-<arch>.tar.gz` (`arch` = `x86_64` or `arm64`);
- inside it: `dqix-launcher` (executable), `payload/` (tracked source snapshot,
  SDL2 headers and runtime, license notices), `LICENSE`, `INSTALL-LINUX.txt`
  and `SOURCE-COMMIT.txt` (source commit and payload version);
- a checksum next to it: `SHA256SUMS-Linux-<arch>.txt` per architecture, and
  an aggregate `SHA256SUMS.txt` on the release;
- no ROM, no save, no generated game code, no build output;
- same commit ⇒ same archive (sorted entries, fixed owner and timestamps).

`tools/launcher/package.py` builds it (`make release`), the `publish` job
attaches it, and `tests/packaging/test_artifact_contract.py` checks the naming,
the layout, the checksum and the reproducibility on fake inputs.

## Audit: every asset the workflows can publish

| Asset | Made by | Workflow / job | Contract |
|---|---|---|---|
| `DQIX-Enhanced-<version>-Linux-x86_64.tar.gz` | `make release` → `tools/launcher/package.py` | `linux-release.yml`, build (x86_64) | **is the product** |
| `DQIX-Enhanced-<version>-Linux-arm64.tar.gz` | same | `linux-release.yml`, build (arm64) | **is the product** |
| `SHA256SUMS-Linux-<arch>.txt` | `package.py` | build | checksum, per arch |
| `SHA256SUMS.txt` | `sha256sum` | publish | aggregate checksum |
| `DQIX-Enhanced-<version>-Windows-x64.zip`, `-Linux-x64.zip` | `tools/make-release.py` | `recomplauncher-zips.yml`, on a published stable release | secondary channel, **not checksummed** |
| `DQIX-PC-<version>.zip`, `-extras.zip` | `tools/make-release.py` | no workflow; made locally | secondary channel, not attached to a release |

## Delta: what does not match the contract

1. **The Windows headline download is a source zip.** `README.md` told Windows
   players to take *Source code (zip)* from another repository's release page
   (`sackchief/dqix-enhanced`), while the Linux section pointed at this
   repository. That is exactly the drift the milestone names: a source archive
   presented as the product, and two different release pages for one project.
   Windows has no launcher artifact yet, so the fix is to name it honestly as
   the build-from-source channel (the README now does) and keep the launcher +
   payload as the Linux product.
2. **RecompLauncher zips are a second artifact class.** They are named with the
   same `DQIX-Enhanced-` prefix as the Linux archive, are not checksummed, and
   were not documented as a separate channel. They stay, but as a declared
   secondary channel; the contract test and the naming rule only cover the
   launcher archive.
3. **The archive was not reproducible.** `package.py` added entries in
   filesystem order and kept each file's mtime, and the gzip header carried the
   current time. Fixed: entries are sorted, `uid`/`gid`/`mtime` are fixed, the
   gzip header has no name and `mtime = 0`. A test builds the same archive twice
   and compares the SHA256.
4. **The full matrix ran on every push, and nothing else did.** A push to
   `upstream-dev` started the two-architecture build (and, on `feat/**`, another
   one) while the fast checks lived only inside it. Split: `checks.yml` runs
   the fast, required checks on every push and pull request; the build matrix
   runs on pull requests, tags and manually.
5. **macOS is not tested on the branch we release from.** `macos-ci.yml` runs
   on `main`/`stable` only, so `upstream-dev` can break the macOS build
   unnoticed. Recorded, not changed: making it run on `upstream-dev` doubles
   macOS minutes and is a budget decision for the owner.
6. **No live asset list in this audit.** The list above is what the workflows
   can publish. Reading the last real release's assets needs a GitHub API token,
   which this environment does not have; that part of the audit stays open.

## CI jobs and budgets

Local numbers are from this development container (Ubuntu 24.04, clang 18),
wall clock, 2026-11-28:

| Job | Workflow | Trigger | Local measurement | Budget |
|---|---|---|---|---|
| Fast required checks | `checks.yml` | every push and pull request | options model 0.06 s, ownership 0.05 s, semantics subset (500 cases / 27,000 checks) 3.1 s, packaging 36 tests 0.6 s | 10 min |
| Recompiler semantic tests | `linux-release.yml` | pull request, tag, manual | semantics 2000 cases / 48,000 checks | 15 min |
| Build Linux x86_64 / arm64 | `linux-release.yml` | pull request, tag, manual | payload + launcher + screenshots + release archive + packaging tests | 45 min |
| Publish GitHub Release | `linux-release.yml` | tag `v*` | checksums, two assets | 15 min |
| macOS recompiler + runtime | `macos-ci.yml` | `main`/`stable`, manual | not measured (does not run on this branch) | none enforced |

Every job now has `timeout-minutes`, so a hung check fails instead of burning
the queue. Hosted durations: the M1 evidence (#36, #38) measured ~2 min for the
previous single-purpose workflow; the first run of the split pipeline is the
number to record here, and until it exists this table says **local only**.

Flakiness policy: a check that fails without a code change is quarantined with
an issue and removed from the required set; it is not silently retried. No
retry loops are configured in either workflow.

## What M5 still has to do

- Read the last published release's asset list with an API token, and record
  the delta against this audit.
- Publish the launcher + payload from a tag through one documented command, and
  verify the published archive on a clean machine for x86_64 and arm64.
- State stable/nightly channels and rollback for the launcher artifact.
- Run the upstream sync drill and record conflicts and time (`docs/UPSTREAM-SYNC.md`).
- Keep `docs/OWNERSHIP.md` and the conflict set green in `make test`.

## Checking it

```
make test          # options model, semantics, packaging + ownership + contract
make release       # the archive, twice if you want to compare the two SHA256s
```
