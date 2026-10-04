#!/usr/bin/env python3
"""Current preview identity. VERSION is the sole numeric product version source."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def product_version(root: Path = ROOT) -> str:
    version = (root / "VERSION").read_text(encoding="utf-8").strip()
    if not re.fullmatch(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", version):
        raise ValueError("VERSION must contain exactly major.minor.patch")
    if any(int(part) > 65535 for part in version.split(".")):
        raise ValueError("VERSION components must fit Windows version resources")
    return version


def revision(root: Path = ROOT) -> str:
    return subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()


def identity(root: Path = ROOT, sha: str | None = None) -> dict:
    sha = revision(root) if sha is None else sha
    if not re.fullmatch(r"[0-9a-f]{40}", sha):
        raise ValueError("Preview identity requires a full lowercase source SHA")
    version = product_version(root)
    return {"version": f"{version}-preview.{sha[:10]}", "product_version": version,
            "installer_version": version, "source_sha": sha, "build_id": sha[:10]}


def validate_manifest(manifest: dict, expected: dict, build_id: str | None = None) -> None:
    for key, value in expected.items():
        if manifest.get(key) != value:
            raise ValueError(f"Payload {key} differs from source: {manifest.get(key)!r} != {value!r}")
    if build_id is not None and build_id != expected["build_id"]:
        raise ValueError("Candidate BuildId differs from exact source revision")


def validate_build(build: Path, expected: dict) -> None:
    metadata = json.loads((build / "chimera-build-version.json").read_text(encoding="utf-8"))
    for key in ("product_version", "source_sha"):
        if metadata.get(key) != expected[key]:
            raise ValueError(f"Configured build {key} differs from source; reconfigure and rebuild")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--field", choices=("version", "product_version", "installer_version", "source_sha", "build_id"))
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--build-id")
    args = parser.parse_args()
    expected = identity()
    if args.manifest:
        validate_manifest(json.loads(args.manifest.read_text(encoding="utf-8-sig")), expected, args.build_id)
    print(expected[args.field] if args.field else json.dumps(expected))
