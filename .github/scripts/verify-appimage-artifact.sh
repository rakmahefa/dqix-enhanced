#!/usr/bin/env bash
set -euo pipefail

: "${GITHUB_REPOSITORY:?GITHUB_REPOSITORY is required}"
: "${GH_TOKEN:?GH_TOKEN is required}"
: "${RUNNER_TEMP:?RUNNER_TEMP is required}"
: "${ARTIFACT_ID:?ARTIFACT_ID is required}"
: "${ARTIFACT_DIGEST:?ARTIFACT_DIGEST is required}"
: "${SOURCE_SHA:?SOURCE_SHA is required}"

API_URL="${GITHUB_API_URL:-https://api.github.com}"
WORK_DIR="$(mktemp -d "$RUNNER_TEMP/dqix-appimage-preflight.XXXXXX")"
ZIP_PATH="$WORK_DIR/artifact.zip"
ASSETS_DIR="$WORK_DIR/assets"
mkdir -p "$ASSETS_DIR"

curl --fail --location --retry 3 --silent --show-error \
  -H "Accept: application/vnd.github+json" \
  -H "Authorization: Bearer $GH_TOKEN" \
  "$API_URL/repos/$GITHUB_REPOSITORY/actions/artifacts/$ARTIFACT_ID/zip" \
  --output "$ZIP_PATH"

ACTUAL_DIGEST="sha256:$(sha256sum "$ZIP_PATH" | awk '{print $1}')"
[ "$ACTUAL_DIGEST" = "$ARTIFACT_DIGEST" ] || {
  echo "Downloaded artifact ZIP digest does not match GitHub provenance metadata" >&2
  exit 1
}
unzip -q "$ZIP_PATH" -d "$ASSETS_DIR"

if find "$ASSETS_DIR" -mindepth 1 ! -type f -print -quit | grep -q .; then
  echo "Unexpected directory or non-regular entry in release artifact" >&2
  exit 1
fi
mapfile -t ACTUAL_FILES < <(find "$ASSETS_DIR" -mindepth 1 -maxdepth 1 -type f -printf '%f\n' | LC_ALL=C sort)
EXPECTED_FILES=("DQIX-Enhanced-Builder-x86_64.AppImage" "SHA256SUMS" "build-info.txt")
if [[ "${ACTUAL_FILES[*]}" != "${EXPECTED_FILES[*]}" ]]; then
  printf 'Unexpected artifact file list: %s\n' "${ACTUAL_FILES[*]}" >&2
  exit 1
fi

mapfile -t CHECKSUM_FILES < <(awk '{print $2}' "$ASSETS_DIR/SHA256SUMS" | LC_ALL=C sort)
EXPECTED_CHECKSUM_FILES=("DQIX-Enhanced-Builder-x86_64.AppImage" "build-info.txt")
if [[ "${CHECKSUM_FILES[*]}" != "${EXPECTED_CHECKSUM_FILES[*]}" ]]; then
  echo "SHA256SUMS must cover exactly the AppImage and build-info.txt" >&2
  exit 1
fi
(cd "$ASSETS_DIR" && sha256sum --check SHA256SUMS)
[ "$(grep -Fxc "Source commit: $SOURCE_SHA" "$ASSETS_DIR/build-info.txt")" -eq 1 ] || {
  echo "build-info.txt source commit does not match the selected CI run" >&2
  exit 1
}
VERSION="$(sed -n 's/^Project version: //p' "$ASSETS_DIR/build-info.txt")"
[ -n "$VERSION" ] || { echo "Project version missing from build-info.txt" >&2; exit 1; }

IMAGE="$ASSETS_DIR/DQIX-Enhanced-Builder-x86_64.AppImage"
chmod +x "$IMAGE"
export HOME="$WORK_DIR/home"
export XDG_CACHE_HOME="$WORK_DIR/cache"
export XDG_DATA_HOME="$WORK_DIR/data"
export APPIMAGE_EXTRACT_AND_RUN=1
mkdir -p "$HOME" "$XDG_CACHE_HOME" "$XDG_DATA_HOME"
ACTUAL_VERSION="$("$IMAGE" --version)"
[ "$ACTUAL_VERSION" = "$VERSION" ] || {
  echo "AppImage CLI version does not match build-info.txt" >&2
  exit 1
}
HELP_OUTPUT="$("$IMAGE" --help)"
printf '%s\n' "$HELP_OUTPUT" | grep -F -- "--self-test" >/dev/null || {
  echo "AppImage --help does not expose --self-test" >&2
  exit 1
}
"$IMAGE" --self-test
printf 'Release artifact passed provenance, ZIP digest, file manifest, checksums and CLI checks (version %s).\n' "$VERSION"
printf 'assets_dir=%s\n' "$ASSETS_DIR" >> "$GITHUB_OUTPUT"
