#!/usr/bin/env python3
"""Static regression tests for release workflow safety defaults and permissions."""
from __future__ import annotations

from pathlib import Path
import re

import yaml

ROOT = Path(__file__).resolve().parents[2]
WORKFLOW_PATH = ROOT / ".github" / "workflows" / "release-appimage.yml"
VALIDATE_SCRIPT = ROOT / ".github" / "scripts" / "validate-release-source.sh"
VERIFY_SCRIPT = ROOT / ".github" / "scripts" / "verify-appimage-artifact.sh"
CI_WORKFLOW_PATH = ROOT / ".github" / "workflows" / "linux-ci-appimage.yml"


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
    assert ".workflow_run.repository_id == $repo_id" in validate
    assert "| select(.repository_id == $repo_id)" not in validate, "Artifact repository ID must be read from workflow_run"
    assert "HTTP 404" in validate and "git check-ref-format" in validate
    assert "EXPECTED_HEAD_SHA" in validate and "EXPECTED_ARTIFACT_DIGEST" in validate
    assert "sha256sum" in verify and "--check SHA256SUMS" in verify
    assert "Source commit:" in verify and "ARTIFACT_DIGEST" in verify
    assert "--version" in verify and "--help" in verify and "--self-test" in verify
    release_steps = "\n".join(str(step.get("run", "")) for step in publish["steps"])
    assert "git/refs" in release_steps and "POST" in release_steps
    assert "--verify-tag" in release_steps
    for workflow_path in (WORKFLOW_PATH, CI_WORKFLOW_PATH):
        workflow_text = workflow_path.read_text(encoding="utf-8")
        for line in workflow_text.splitlines():
            match = re.match(r"^\s*uses:\s*([^#\s]+)", line)
            if not match:
                continue
            reference = match.group(1)
            if reference.startswith("./"):
                continue
            action_name, separator, revision = reference.rpartition("@")
            assert separator and re.fullmatch(r"[0-9a-f]{40}", revision), (
                f"{workflow_path.relative_to(ROOT)} has an unpinned external action: {reference}"
            )
    print("Release workflow safety tests passed: manual-only trigger, dry-run default, read-only preflight and publish-only write permission.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
