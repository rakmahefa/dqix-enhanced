#!/usr/bin/env bash
set -euo pipefail

: "${GITHUB_REPOSITORY:?GITHUB_REPOSITORY is required}"
: "${GH_TOKEN:?GH_TOKEN is required}"
: "${SOURCE_RUN_ID:?SOURCE_RUN_ID is required}"
: "${TAG_NAME:?TAG_NAME is required}"
: "${WORKFLOW_REF:?WORKFLOW_REF is required}"
: "${GITHUB_OUTPUT:?GITHUB_OUTPUT is required}"

if ! [[ "$SOURCE_RUN_ID" =~ ^[0-9]+$ ]]; then
  echo "source_run_id must be a numeric GitHub Actions run ID" >&2
  exit 1
fi
if ! [[ "$TAG_NAME" =~ ^[A-Za-z0-9._-]+$ ]] || ! git check-ref-format "refs/tags/$TAG_NAME"; then
  echo "tag_name is not a valid safe Git tag name" >&2
  exit 1
fi
case "$WORKFLOW_REF" in
  refs/heads/main|refs/heads/stable) ;;
  *) echo "Release workflow must be dispatched from main or stable (got $WORKFLOW_REF)" >&2; exit 1 ;;
esac

RUN_JSON="$(gh api "repos/$GITHUB_REPOSITORY/actions/runs/$SOURCE_RUN_ID")"
WORKFLOW_NAME="$(jq -r '.name // empty' <<< "$RUN_JSON")"
WORKFLOW_PATH="$(jq -r '.path // empty' <<< "$RUN_JSON")"
WORKFLOW_ID="$(jq -r '.workflow_id // empty' <<< "$RUN_JSON")"
RUN_BRANCH="$(jq -r '.head_branch // empty' <<< "$RUN_JSON")"
HEAD_SHA="$(jq -r '.head_sha // empty' <<< "$RUN_JSON")"
RUN_STATUS="$(jq -r '.status // empty' <<< "$RUN_JSON")"
RUN_CONCLUSION="$(jq -r '.conclusion // empty' <<< "$RUN_JSON")"
RUN_EVENT="$(jq -r '.event // empty' <<< "$RUN_JSON")"
RUN_REPOSITORY="$(jq -r '.head_repository.full_name // empty' <<< "$RUN_JSON")"
TARGET_REPOSITORY="$(jq -r '.repository.full_name // empty' <<< "$RUN_JSON")"

[ "$WORKFLOW_NAME" = "Linux CI + AppImage" ] || { echo "Source run uses the wrong workflow" >&2; exit 1; }
EXPECTED_WORKFLOW_ID="$(gh api "repos/$GITHUB_REPOSITORY/actions/workflows/linux-ci-appimage.yml" --jq '.id')"
[ "$WORKFLOW_ID" = "$EXPECTED_WORKFLOW_ID" ] || { echo "Source workflow ID does not match the repository CI workflow" >&2; exit 1; }
[ "$WORKFLOW_PATH" = ".github/workflows/linux-ci-appimage.yml" ] || { echo "Source workflow path does not match" >&2; exit 1; }
[ "$RUN_STATUS" = "completed" ] && [ "$RUN_CONCLUSION" = "success" ] || {
  echo "Source workflow run has not completed successfully" >&2
  exit 1
}
[ "$RUN_EVENT" = "push" ] || { echo "Only a successful push run is eligible for release provenance" >&2; exit 1; }
case "$RUN_BRANCH" in
  main|stable) ;;
  *) echo "Source artifact must come from main or stable (got $RUN_BRANCH)" >&2; exit 1 ;;
esac
[ "$RUN_REPOSITORY" = "$GITHUB_REPOSITORY" ] && [ "$TARGET_REPOSITORY" = "$GITHUB_REPOSITORY" ] || {
  echo "Source run must belong to this repository, not a fork" >&2
  exit 1
}
[[ "$HEAD_SHA" =~ ^[0-9a-f]{40}$ ]] || { echo "Source commit SHA is missing or malformed" >&2; exit 1; }

if [[ -n "${EXPECTED_HEAD_SHA:-}" ]] && [ "$HEAD_SHA" != "$EXPECTED_HEAD_SHA" ]; then
  echo "Source run SHA changed since preflight" >&2
  exit 1
fi

REPOSITORY_ID="$(gh api "repos/$GITHUB_REPOSITORY" --jq '.id')"
ARTIFACTS_JSON="$(gh api "repos/$GITHUB_REPOSITORY/actions/runs/$SOURCE_RUN_ID/artifacts?per_page=100")"
ARTIFACT_JSON="$(jq -ce \
  --arg name "dqix-enhanced-appimage" \
  --arg sha "$HEAD_SHA" \
  --arg branch "$RUN_BRANCH" \
  --argjson run_id "$SOURCE_RUN_ID" \
  --argjson repo_id "$REPOSITORY_ID" \
  '[.artifacts[]
    | select(.name == $name and .expired == false)
    | select(.workflow_run.id == $run_id)
    | select(.workflow_run.repository_id == $repo_id)
    | select(.workflow_run.head_repository_id == $repo_id)
    | select(.workflow_run.head_sha == $sha)
    | select(.workflow_run.head_branch == $branch)]
   | if length == 1 then .[0] else error("expected exactly one provenance-matched AppImage artifact") end' \
  <<< "$ARTIFACTS_JSON")"
ARTIFACT_ID="$(jq -r '.id // empty' <<< "$ARTIFACT_JSON")"
ARTIFACT_DIGEST="$(jq -r '.digest // empty' <<< "$ARTIFACT_JSON")"
[[ "$ARTIFACT_ID" =~ ^[0-9]+$ ]] || { echo "Artifact ID is missing" >&2; exit 1; }
[[ "$ARTIFACT_DIGEST" =~ ^sha256:[0-9a-f]{64}$ ]] || {
  echo "Artifact has no valid SHA-256 archive digest" >&2
  exit 1
}

ensure_absent() {
  local endpoint="$1"
  local description="$2"
  local response status
  set +e
  response="$(gh api "$endpoint" 2>&1)"
  status=$?
  set -e
  if [ "$status" -eq 0 ]; then
    echo "$description already exists; refusing to overwrite it" >&2
    exit 1
  fi
  if [[ "$response" != *"(HTTP 404)"* ]]; then
    echo "Could not safely determine whether $description exists: $response" >&2
    exit 1
  fi
}

ensure_absent "repos/$GITHUB_REPOSITORY/git/ref/tags/$TAG_NAME" "Tag $TAG_NAME"
ensure_absent "repos/$GITHUB_REPOSITORY/releases/tags/$TAG_NAME" "Release $TAG_NAME"

if [[ -n "${EXPECTED_ARTIFACT_ID:-}" ]] && [ "$ARTIFACT_ID" != "$EXPECTED_ARTIFACT_ID" ]; then
  echo "Artifact ID changed since preflight" >&2
  exit 1
fi
if [[ -n "${EXPECTED_ARTIFACT_DIGEST:-}" ]] && [ "$ARTIFACT_DIGEST" != "$EXPECTED_ARTIFACT_DIGEST" ]; then
  echo "Artifact digest changed since preflight" >&2
  exit 1
fi

{
  printf 'head_sha=%s\n' "$HEAD_SHA"
  printf 'head_branch=%s\n' "$RUN_BRANCH"
  printf 'artifact_id=%s\n' "$ARTIFACT_ID"
  printf 'artifact_digest=%s\n' "$ARTIFACT_DIGEST"
} >> "$GITHUB_OUTPUT"
echo "Validated source run $SOURCE_RUN_ID ($RUN_BRANCH @ $HEAD_SHA), artifact $ARTIFACT_ID ($ARTIFACT_DIGEST)."
echo "Confirmed tag and release names are currently unused; no writes were performed."
