#!/usr/bin/env python3
"""Source-derived package identity; explicit targets assert it, never relabel it."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
NUMERIC_VERSION = r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)"


def validate_product_version(version: str) -> str:
    if not isinstance(version, str) or not re.fullmatch(NUMERIC_VERSION, version):
        raise ValueError("VERSION must contain exactly major.minor.patch")
    if any(int(part) > 65535 for part in version.split(".")):
        raise ValueError("VERSION components must fit Windows version resources")
    return version


def product_version(root: Path = ROOT) -> str:
    return validate_product_version((root / "VERSION").read_text(encoding="utf-8").strip())


def parse_release_version(value: str) -> tuple[str, str]:
    """Parse publishable versions, without a tag prefix or build metadata.

    Preview identities are local/CI candidates, not publication targets. Release
    suffixes retain SemVer ordering while installer resources stay numeric.
    """
    if not isinstance(value, str) or len(value) > 128:
        raise ValueError("Release version must be major.minor.patch[-beta.N|-rc.N]")
    match = re.fullmatch(rf"(?P<product>{NUMERIC_VERSION})(?:-(?P<channel>(?:beta|rc)\.[1-9][0-9]*))?", value)
    if not match:
        raise ValueError("Release version must be major.minor.patch[-beta.N|-rc.N]")
    return validate_product_version(match["product"]), match["channel"] or "stable"


def release_channel_for_version(value: str) -> str:
    return parse_release_version(value)[1].split(".", 1)[0]


def release_manifest_names(value: str) -> tuple[str, ...]:
    """Canonical receiver feed followed by the shipped beta client's alias."""
    channel = release_channel_for_version(value)
    canonical = f"update-{channel}.json"
    return (canonical,) if channel == "beta" else (canonical, "update-beta.json")


def release_channel(root: Path = ROOT) -> str:
    path = root / "RELEASE_CHANNEL"
    channel = path.read_text(encoding="utf-8").strip() if path.is_file() else "preview"
    if not re.fullmatch(r"preview|stable|(?:beta|rc)\.[1-9][0-9]*", channel):
        raise ValueError("RELEASE_CHANNEL must be preview, stable, beta.N or rc.N (N > 0)")
    if channel != "preview":
        parse_release_version(product_version(root) + ("" if channel == "stable" else "-" + channel))
    return channel


def display_version(root: Path = ROOT, sha: str | None = None) -> str:
    value = identity(root, sha)
    channel = release_channel(root)
    if channel == "preview":
        return f"{value['product_version']} Preview {value['build_id']}"
    if channel == "stable":
        return value["product_version"]
    label, number = channel.split(".")
    return (f"Open Beta {value['product_version']} (beta.{number})" if label == "beta"
            else f"{value['product_version']} Release Candidate {number}")


def revision(root: Path = ROOT) -> str:
    return subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()


def identity(root: Path = ROOT, sha: str | None = None) -> dict:
    sha = revision(root) if sha is None else sha
    if not re.fullmatch(r"[0-9a-f]{40}", sha):
        raise ValueError("Package identity requires a full lowercase source SHA")
    version = product_version(root)
    channel = release_channel(root)
    package = (f"{version}-preview.{sha[:10]}" if channel == "preview" else
               version if channel == "stable" else f"{version}-{channel}")
    return {"version": package, "product_version": version,
            "installer_version": version, "source_sha": sha, "build_id": sha[:10]}


def validate_target_version(target_version: str, root: Path = ROOT, sha: str | None = None) -> dict:
    """Require the requested target to match the source's committed identity."""
    parse_release_version(target_version)
    expected = identity(root, sha)
    if target_version != expected["version"]:
        raise ValueError(f"Target version differs from source: {target_version!r} != {expected['version']!r}")
    return expected


def validate_manifest(manifest: dict, expected: dict, build_id: str | None = None) -> None:
    for key, value in expected.items():
        if manifest.get(key) != value:
            raise ValueError(f"Payload {key} differs from source: {manifest.get(key)!r} != {value!r}")
    if build_id is not None and build_id != expected["build_id"]:
        raise ValueError("Candidate BuildId differs from exact source revision")


def validate_build(build: Path, expected: dict) -> None:
    metadata = json.loads((build / "chimera-build-version.json").read_text(encoding="utf-8"))
    for key in ("product_version", "source_sha", "version"):
        if metadata.get(key) != expected[key]:
            raise ValueError(f"Configured build {key} differs from source; reconfigure and rebuild")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--field", choices=("version", "product_version", "installer_version", "source_sha", "build_id"))
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--build-id")
    parser.add_argument("--target-version", help="Assert a publishable version matching VERSION/RELEASE_CHANNEL")
    parser.add_argument("--source-sha", help="Assert the exact checked-out source SHA")
    args = parser.parse_args()
    expected = identity()
    if args.source_sha is not None and args.source_sha != expected["source_sha"]:
        parser.error("Requested source SHA differs from the checked-out source")
    if args.target_version is not None:
        expected = validate_target_version(args.target_version)
    if args.manifest:
        validate_manifest(json.loads(args.manifest.read_text(encoding="utf-8-sig")), expected, args.build_id)
    print(expected[args.field] if args.field else json.dumps(expected))
