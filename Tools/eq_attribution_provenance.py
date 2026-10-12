#!/usr/bin/env python3
"""Record source/runner identity for observational EQ performance evidence.

The original transient repair is an immutable input, not a new diagnosis.
No environment dump, credentials, or physical-device performance claims.
"""
from __future__ import annotations

import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]


def command(*args: str) -> str:
    result = subprocess.run(args, cwd=ROOT, capture_output=True, text=True, check=False)
    return result.stdout.strip() if result.returncode == 0 else "UNAVAILABLE"


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def git_bytes(*args: str) -> bytes:
    # Read HEAD directly: this also works with actions/checkout's shallow clone.
    return subprocess.run(("git", *args), cwd=ROOT, capture_output=True, check=True).stdout


def checkout_config(key: str) -> str | None:
    result = subprocess.run(("git", "config", "--get", key), cwd=ROOT,
                            capture_output=True, text=True, check=False)
    if result.returncode == 1:
        return None  # Unset is distinct from an explicitly configured false/input.
    result.check_returncode()
    return result.stdout.strip()


def tree_digest(hashes: dict[str, str]) -> str:
    return hashlib.sha256("".join(f"{p}:{h}\n" for p, h in sorted(hashes.items())).encode()).hexdigest()


def inspect_retained_inputs() -> dict:
    errors = []
    reference_path = "Tests/reference/EQSpectrumAnalyzerBeb72f.h"
    reference_git = git_bytes("cat-file", "blob", f"HEAD:{reference_path}")
    reference_worktree = (ROOT / reference_path).read_bytes()
    reference_git_sha256 = hashlib.sha256(reference_git).hexdigest()
    reference_worktree_sha256 = hashlib.sha256(reference_worktree).hexdigest()
    if reference_git_sha256 != "ad0e9f7dc7fe46579b24707291bf63a5ee09363de701b1606c5cedf98f970f26":
        errors.append({"code": "REFERENCE_GIT_BLOB_CHANGED", "path": reference_path})
    if reference_worktree != reference_git:
        errors.append({"code": "REFERENCE_WORKTREE_DIFFERS_FROM_GIT_BLOB", "path": reference_path})

    historical_path = "docs/evidence/eq-transient-fix-20261012"
    historical = ROOT / historical_path
    prefix = historical_path + "/"
    tracked_paths = git_bytes("ls-tree", "-r", "-z", "--name-only", "HEAD", "--", prefix).decode().split("\0")
    canonical = {p[len(prefix):]: hashlib.sha256(git_bytes("cat-file", "blob", f"HEAD:{p}")).hexdigest()
                 for p in tracked_paths if p}
    preserved = {p.relative_to(historical).as_posix(): digest(p)
                 for p in sorted(historical.rglob("*")) if p.is_file()}
    historical_digest = tree_digest(canonical)
    worktree_digest = tree_digest(preserved)
    expected_historical_digest = "bb47058bd2798bea744da7d61754aefb5b7dbd5d8488086c313a5ae92897bc2e"
    if len(canonical) != 84 or historical_digest != expected_historical_digest:
        errors.append({"code": "HISTORICAL_GIT_BLOBS_CHANGED"})
    if set(preserved) != set(canonical):
        errors.append({"code": "HISTORICAL_WORKTREE_FILE_SET_DIFFERS",
                       "missing": sorted(set(canonical) - set(preserved)),
                       "extra": sorted(set(preserved) - set(canonical))})
    changed = sorted(p for p in canonical.keys() & preserved.keys() if canonical[p] != preserved[p])
    if changed:
        errors.append({"code": "HISTORICAL_WORKTREE_BYTES_DIFFER", "paths": changed})
    return {
        "production_commit": "beb72f520721685f3698883297371be7def92198",
        "evidence_commit": "139e98f2614f516fb97975034eacae4e4dc05098",
        "workflow_run": 38224717793,
        "analyzer_git_blob": "34f31791c005c291c294afc3fb2ab4783830b4bd",
        "reference_git_sha256": reference_git_sha256,
        "reference_worktree_sha256": reference_worktree_sha256,
        "evidence_files": len(canonical),
        "evidence_worktree_files": len(preserved),
        "evidence_tree_sha256": historical_digest,
        "evidence_tree_sha256_basis": "Canonical Git blobs at HEAD; original LF/CRLF bytes are preserved",
        "evidence_worktree_tree_sha256": worktree_digest,
        "worktree_matches_git_blobs": reference_worktree == reference_git and preserved == canonical,
        "status": "IMMUTABLE_INPUT_VALIDATION_FAILED" if errors else "PREVIOUS_FUNCTIONAL_PASS_PRESERVED",
        "validation_errors": errors,
    }


def main() -> None:
    build = Path(sys.argv[1]).resolve()
    build.mkdir(parents=True, exist_ok=True)
    patterns = ("Source/*.h", "Source/*.cpp", "Tests/*EQ*.cpp", "Tests/EQAttributionDiagnostics.h", "Tests/NativeMessageLoop.*",
                "Tests/reference/EQSpectrumAnalyzerBeb72f.h", "CMakeLists.txt", ".gitattributes",
                ".github/workflows/graphical-eq.yml", "Tools/eq_attribution_provenance.py")
    files = sorted({p for pattern in patterns for p in ROOT.glob(pattern) if p.is_file()})
    hashes = {p.relative_to(ROOT).as_posix(): digest(p) for p in files}
    retained = inspect_retained_inputs()
    cache = build / "CMakeCache.txt"
    flags = {}
    if cache.exists():
        for line in cache.read_text(errors="replace").splitlines():
            if line.startswith(("CMAKE_CXX_COMPILER:", "CMAKE_CXX_FLAGS:", "CMAKE_CXX_FLAGS_RELEASE:",
                                "CMAKE_BUILD_TYPE:", "JUCE_SOURCE_DIR:", "CMAKE_GENERATOR:")):
                key, value = line.split("=", 1)
                flags[key] = value
    environment = {key: os.environ.get(key) for key in
                   ("RUNNER_OS", "RUNNER_ARCH", "ImageOS", "ImageVersion", "GITHUB_RUN_ID", "GITHUB_RUN_ATTEMPT")}
    data = {
        "recorded_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "source_commit": command("git", "rev-parse", "HEAD"),
        "tracked_changes": command("git", "status", "--porcelain", "--untracked-files=no"),
        "source_sha256": hashes,
        "source_sha256_basis": "Exact read_bytes() of the listed worktree files; no EOL or text normalization",
        "checkout": {"core.autocrlf": checkout_config("core.autocrlf"),
                     "core.eol": checkout_config("core.eol"), "unset_config_values_are": "null"},
        "runner": {"platform": platform.platform(), "machine": platform.machine(),
                   "processor": platform.processor(), "logical_cpus": os.cpu_count(), **environment},
        "build": flags,
        "preserved_transient_repair": retained,
        "performance_acceptance": "NOT_ASSESSED",
        "physical_daw_device_tested": False,
        "public_beta_changed": False,
        "interpretation": [
            "Functional gates, normal-thread CI production rate, and DAW/device acceptance are separate.",
            "Due-sample pacing can catch up late: report callback lateness independently of aggregate throughput.",
            "CPU seconds are measured separately from elapsed time, sleep and UI scheduling gaps.",
        ],
    }
    # Retain checkout/hash diagnostics on a validation failure, then fail closed.
    (build / "eq-attribution-provenance.json").write_text(json.dumps(data, indent=2) + "\n")
    if retained["validation_errors"]:
        raise RuntimeError("Immutable EQ input validation failed: " + ", ".join(
            error["code"] for error in retained["validation_errors"]))
    print(f"EQ attribution source {data['source_commit']}; preserved {retained['evidence_files']} historical files; DAW NOT_ASSESSED")


if __name__ == "__main__":
    main()
