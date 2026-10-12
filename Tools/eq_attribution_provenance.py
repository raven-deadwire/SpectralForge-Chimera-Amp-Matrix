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


def main() -> None:
    build = Path(sys.argv[1]).resolve()
    build.mkdir(parents=True, exist_ok=True)
    patterns = ("Source/*.h", "Source/*.cpp", "Tests/*EQ*.cpp", "Tests/EQAttributionDiagnostics.h", "Tests/NativeMessageLoop.*",
                "Tests/reference/EQSpectrumAnalyzerBeb72f.h", "CMakeLists.txt",
                ".github/workflows/graphical-eq.yml", "Tools/eq_attribution_provenance.py")
    files = sorted({p for pattern in patterns for p in ROOT.glob(pattern) if p.is_file()})
    hashes = {p.relative_to(ROOT).as_posix(): digest(p) for p in files}
    reference = ROOT / "Tests/reference/EQSpectrumAnalyzerBeb72f.h"
    if digest(reference) != "ad0e9f7dc7fe46579b24707291bf63a5ee09363de701b1606c5cedf98f970f26":
        raise RuntimeError("The paired analyzer reference no longer matches the retained transient repair")
    historical = ROOT / "docs/evidence/eq-transient-fix-20261012"
    preserved = {p.relative_to(historical).as_posix(): digest(p)
                 for p in sorted(historical.rglob("*")) if p.is_file()}
    historical_digest = hashlib.sha256("".join(f"{p}:{h}\n" for p, h in preserved.items()).encode()).hexdigest()
    expected_historical_digest = "bb47058bd2798bea744da7d61754aefb5b7dbd5d8488086c313a5ae92897bc2e"
    if len(preserved) != 84 or historical_digest != expected_historical_digest:
        raise RuntimeError("The preserved beb72f transient-repair evidence changed; do not relabel or replace it")
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
        "runner": {"platform": platform.platform(), "machine": platform.machine(),
                   "processor": platform.processor(), "logical_cpus": os.cpu_count(), **environment},
        "build": flags,
        "preserved_transient_repair": {
            "production_commit": "beb72f520721685f3698883297371be7def92198",
            "evidence_commit": "139e98f2614f516fb97975034eacae4e4dc05098",
            "workflow_run": 38224717793,
            "analyzer_git_blob": "34f31791c005c291c294afc3fb2ab4783830b4bd",
            "evidence_files": len(preserved), "evidence_tree_sha256": historical_digest,
            "status": "PREVIOUS_FUNCTIONAL_PASS_PRESERVED",
        },
        "performance_acceptance": "NOT_ASSESSED",
        "physical_daw_device_tested": False,
        "public_beta_changed": False,
        "interpretation": [
            "Functional gates, normal-thread CI production rate, and DAW/device acceptance are separate.",
            "Due-sample pacing can catch up late: report callback lateness independently of aggregate throughput.",
            "CPU seconds are measured separately from elapsed time, sleep and UI scheduling gaps.",
        ],
    }
    (build / "eq-attribution-provenance.json").write_text(json.dumps(data, indent=2) + "\n")
    print(f"EQ attribution source {data['source_commit']}; preserved {len(preserved)} historical files; DAW NOT_ASSESSED")


if __name__ == "__main__":
    main()
