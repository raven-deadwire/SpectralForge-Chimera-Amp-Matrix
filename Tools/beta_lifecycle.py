#!/usr/bin/env python3
"""Generate source-bound beta lifecycle metadata from an explicit UTC expiry."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import re

from chimera_version import identity

UTC_PATTERN = re.compile(r"^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z$")
SHA_PATTERN = re.compile(r"^[0-9a-f]{40}$")
WARNING_LEAD_SECONDS = 14 * 24 * 60 * 60


def parse_utc(value: str) -> datetime:
    if not UTC_PATTERN.fullmatch(value):
        raise ValueError("beta expiry must be an absolute UTC timestamp: YYYY-MM-DDTHH:MM:SSZ")
    try:
        result = datetime.strptime(value, "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc)
    except ValueError as exc:
        raise ValueError("beta expiry is not a valid UTC timestamp") from exc
    if result.strftime("%Y-%m-%dT%H:%M:%SZ") != value:
        raise ValueError("beta expiry is not canonical UTC")
    return result


def build_metadata(root: Path, source_sha: str, expiry_utc: str,
                   now: datetime | None = None) -> dict[str, object]:
    if not SHA_PATTERN.fullmatch(source_sha):
        raise ValueError("source SHA must be exactly 40 lowercase hexadecimal characters")
    release = identity(root, source_sha)
    if "-beta." not in release["version"]:
        raise ValueError("beta lifecycle metadata may only be generated for a beta source")
    expiry = parse_utc(expiry_utc)
    observed = now or datetime.now(timezone.utc)
    if observed.tzinfo is None:
        raise ValueError("current time must be timezone-aware")
    observed = observed.astimezone(timezone.utc)
    if expiry <= observed:
        raise ValueError("beta expiry must be later than the packaging time")
    return {
        "schema": "spectralforge.chimera.beta-lifecycle",
        "schema_version": 1,
        "product": "SpectralForge Chimera",
        "product_version": release["product_version"],
        "version": release["version"],
        "source_sha": source_sha,
        "build_id": release["build_id"],
        "expires_at_utc": expiry_utc,
        "expires_at_unix": int(expiry.timestamp()),
        "warning_lead_seconds": WARNING_LEAD_SECONDS,
        "network_required": False,
        "active_instance_grace": True,
    }


def write_header(path: Path, metadata: dict[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        "#pragma once\n"
        f'#define CHIMERA_BETA_PACKAGE_VERSION "{metadata["version"]}"\n'
        f'#define CHIMERA_BETA_SOURCE_SHA "{metadata["source_sha"]}"\n'
        f'#define CHIMERA_BETA_EXPIRY_UTC "{metadata["expires_at_utc"]}"\n'
        f'#define CHIMERA_BETA_EXPIRY_UNIX {metadata["expires_at_unix"]}LL\n'
        f'#define CHIMERA_BETA_WARNING_LEAD_SECONDS {metadata["warning_lead_seconds"]}LL\n',
        encoding="utf-8",
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--source-sha", required=True)
    parser.add_argument("--expiry-utc", required=True)
    parser.add_argument("--now-utc", help="Injected UTC packaging time for deterministic verification")
    parser.add_argument("--json", type=Path, required=True)
    parser.add_argument("--header", type=Path, required=True)
    args = parser.parse_args()
    now = parse_utc(args.now_utc) if args.now_utc else None
    metadata = build_metadata(args.root, args.source_sha, args.expiry_utc, now)
    args.json.parent.mkdir(parents=True, exist_ok=True)
    args.json.write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    write_header(args.header, metadata)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
