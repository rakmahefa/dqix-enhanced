#!/usr/bin/env bash
set -uo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 /path/to/dqix-linux-ui-prototype" >&2
  exit 2
fi

app=$1
smoke_seconds=${DQIX_UI_SMOKE_SECONDS:-6}
log_file=$(mktemp)
cleanup() { rm -f "$log_file"; }
trap cleanup EXIT

if ! command -v xvfb-run >/dev/null 2>&1 || ! command -v timeout >/dev/null 2>&1; then
  echo "SKIP: xvfb-run and timeout are required for the graphical smoke test."
  exit 77
fi

if ! command -v vulkaninfo >/dev/null 2>&1; then
  if [[ ${REQUIRE_VULKAN_DEVICE:-false} == true ]]; then
    echo "FAIL: a Vulkan device is required, but vulkaninfo is missing."
    exit 1
  fi
  echo "SKIP: vulkaninfo is unavailable; Vulkan runtime capability was not checked."
  exit 77
fi

if ! vulkaninfo --summary >"$log_file" 2>&1 ||
   ! grep -Eq 'deviceName|deviceType' "$log_file"; then
  cat "$log_file"
  if [[ ${REQUIRE_VULKAN_DEVICE:-false} == true ]]; then
    echo "FAIL: a Vulkan device is required, but none was detected."
    exit 1
  fi
  echo "SKIP: no usable Vulkan device was detected; this is not a rendering pass."
  exit 77
fi

: >"$log_file"
set +e
xvfb-run -a timeout --signal=TERM --kill-after=2s "${smoke_seconds}s" "$app" >"$log_file" 2>&1
status=$?
set -e
cat "$log_file"

if [[ $status -eq 124 || $status -eq 137 ]]; then
  echo "PASS: SDL3 + Vulkan + Dear ImGui stayed alive for ${smoke_seconds}s under Xvfb."
  exit 0
fi

echo "FAIL: the UI prototype exited before the smoke-test timeout (status=$status)." >&2
exit 1
