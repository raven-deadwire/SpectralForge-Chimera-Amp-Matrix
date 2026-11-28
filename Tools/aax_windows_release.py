#!/usr/bin/env python3
"""Validate the identity and bytes of a separately PACE-verified Windows AAX handoff.

A pinned receipt attests the external PACE checks; this module does not run WrapTool
or verify Authenticode. The installer build checks Authenticode on the frozen copy.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil

from aax_native import BUNDLE, plain_path, verify_bundle
from chimera_version import identity

SCHEMA = "spectralforge.chimera.windows-aax-release.v1"
BINARY = f"Contents/x64/{BUNDLE}"
FILES = {BINARY, "Plugin.ico", "desktop.ini"}


def validate(package: Path, receipt_sha256: str, expected: dict) -> dict:
    plain_path(package)
    path = package / "aax-release-receipt.json"
    plain_path(path)
    raw = path.read_bytes()
    if not re.fullmatch(r"[0-9a-f]{64}", receipt_sha256) or hashlib.sha256(raw).hexdigest() != receipt_sha256:
        raise ValueError("AAX receipt does not match the separately pinned SHA-256")
    receipt = json.loads(raw)
    if (receipt.get("schema") != SCHEMA or receipt.get("platform") != "windows"
            or receipt.get("pace_verification") != "PASS"
            or receipt.get("architectures") != ["x86_64"]):
        raise ValueError("Expected the approved Windows x64 PACE verification receipt")
    for key in ("source_sha", "version", "product_version"):
        if receipt.get(key) != expected[key]:
            raise ValueError(f"AAX receipt and installer disagree on {key}")
    bundle = package / BUNDLE
    actual = verify_bundle(bundle, "windows")
    if set(actual["files"]) != FILES or receipt.get("files") != actual["files"]:
        raise ValueError("AAX bundle file set or SHA-256 inventory differs from the authorized input")
    return actual


def prepare(package: Path, receipt_sha256: str, expected: dict, target: Path) -> None:
    validate(package, receipt_sha256, expected)
    plain_path(target)
    target.mkdir(exist_ok=False)
    # A copy, not a move or another signing pass. Recheck the independent receipt
    # against the final compiler inputs, not only the caller's original files.
    shutil.copyfile(package / "aax-release-receipt.json", target / "aax-release-receipt.json")
    for name in sorted(FILES):
        source, dest = package / BUNDLE / name, target / BUNDLE / name
        plain_path(source)
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, dest)
    validate(target, receipt_sha256, expected)


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("package", type=Path)
    p.add_argument("receipt_sha256")
    p.add_argument("--prepare", type=Path)
    a = p.parse_args()
    try:
        expected = identity()
        if a.prepare:
            prepare(a.package, a.receipt_sha256, expected, a.prepare)
        else:
            validate(a.package, a.receipt_sha256, expected)
    except (OSError, ValueError, TypeError, AttributeError) as error:
        p.exit(1, f"Invalid signed AAX input: {error}\n")


if __name__ == "__main__":
    main()
