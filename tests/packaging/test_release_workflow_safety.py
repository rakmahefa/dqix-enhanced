#!/usr/bin/env python3
"""Static regression tests for release workflow safety defaults and permissions."""
from __future__ import annotations

from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[2]
WORKFLOW_PATH = ROOT / ".github" / "workflows" / "release-appimage.yml"
VALIDATE_SCRIPT = ROOT / ".github" / "scripts" / "validate-release-source.sh"
VERIFY_SCRIPT = ROOT / ".github" / "scripts" / "verify-appimage-artifact.sh"


def main() -> int:
    workflow = yaml.load(WORKFLOW_PATH.read_text(encoding="utf-8"), Loader=yaml.BaseLoader)
    assert set(workflow["on"]) == {"workflow_dispatch"}, "release workflow must be manual-only"
    inputs = workflow["on"]["workflow_dispatch"]["inputs"]
    assert inputs["dry_run"]["type"] == "boolean"
    assert inputs["dry_run"]["default"] == "true", "release workflow must default to read-only"

    assert workflow["permissions"]["contents"] == "read", "workflow-wide permissions must not write contents"
    jobs = workflow["jobs"]
    assert set(jobs) == {"preflight", "publish"}
    preflight = jobs["preflight"]
    publish = jobs["publish"]
    assert preflight["permissions"]["contents"] == "read"
    assert preflight["permissions"]["actions"] == "read"
    assert "write" not in preflight["permissions"].values()
    assert publish["if"] == "${{ inputs.dry_run == false }}"
    assert publish["needs"] == "preflight"
    assert publish["permissions"]["contents"] == "write"
    for name, job in jobs.items():
        if name != "publish":
            assert all(value != "write" for value in job.get("permissions", {}).values()), name

    validate = VALIDATE_SCRIPT.read_text(encoding="utf-8")
    verify = VERIFY_SCRIPT.read_text(encoding="utf-8")
    assert 'RUN_EVENT" = "push"' in validate
    assert ".github/workflows/linux-ci-appimage.yml" in validate
    assert ".head_sha" in validate and ".workflow_run.head_sha" in validate
    assert ".digest" in validate and ".workflow_run.head_repository_id" in validate
    assert "HTTP 404" in validate and "git check-ref-format" in validate
    assert "EXPECTED_HEAD_SHA" in validate and "EXPECTED_ARTIFACT_DIGEST" in validate
    assert "sha256sum" in verify and "--check SHA256SUMS" in verify
    assert "Source commit:" in verify and "ARTIFACT_DIGEST" in verify
    assert "--version" in verify and "--help" in verify and "--self-test" in verify
    release_steps = "\n".join(str(step.get("run", "")) for step in publish["steps"])
    assert "git/refs" in release_steps and "POST" in release_steps
    assert "--verify-tag" in release_steps
    print("Release workflow safety tests passed: manual-only trigger, dry-run default, read-only preflight and publish-only write permission.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
