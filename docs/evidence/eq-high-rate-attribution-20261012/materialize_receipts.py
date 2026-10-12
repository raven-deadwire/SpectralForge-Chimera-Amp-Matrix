#!/usr/bin/env python3
"""Verify stored/raw receipt hashes and optionally materialize an empty directory.

This is an evidence reader, not a benchmark or a performance acceptance gate.
No source receipt is changed. Python standard library only.
"""
from __future__ import annotations

import argparse
import gzip
import hashlib
import json
from pathlib import Path, PurePosixPath


def safe_relative(value: str) -> Path:
    path = PurePosixPath(value)
    if path.is_absolute() or not path.parts or any(x in (".", "..") for x in path.parts):
        raise ValueError(f"Unsafe manifest path: {value!r}")
    if "\\" in value:
        raise ValueError(f"Non-POSIX manifest path: {value!r}")
    return Path(*path.parts)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("destination", nargs="?", type=Path,
                        help="optional new or empty scratch output directory")
    args = parser.parse_args()
    manifest_path = args.manifest.resolve(strict=True)
    manifest = json.loads(manifest_path.read_bytes())
    source_root = manifest_path.parent
    destination = args.destination.resolve() if args.destination else None
    if destination is not None:
        if destination == source_root or source_root in destination.parents:
            raise ValueError("Destination must be outside the retained evidence directory")
        if destination.exists() and (not destination.is_dir() or any(destination.iterdir())):
            raise ValueError("Destination must be a new or empty directory")

    raw_outputs = []
    seen = set()
    for entry in manifest["files"]:
        stored_path = source_root / safe_relative(entry["path"])
        raw_path = safe_relative(entry["source_path"])
        if raw_path in seen:
            raise ValueError(f"Duplicate source path: {raw_path}")
        seen.add(raw_path)
        stored = stored_path.read_bytes()
        if hashlib.sha256(stored).hexdigest() != entry["stored_sha256"]:
            raise ValueError(f"Stored-byte hash mismatch: {stored_path}")
        compression = entry.get("compression")
        if compression is None:
            raw = stored
        elif compression == "gzip; mtime=0":
            raw = gzip.decompress(stored)
        else:
            raise ValueError(f"Unknown compression: {compression!r}")
        if len(raw) != entry["raw_bytes"] or hashlib.sha256(raw).hexdigest() != entry["raw_sha256"]:
            raise ValueError(f"Raw-byte hash/length mismatch: {stored_path}")
        raw_outputs.append((raw_path, raw))

    # Finish every integrity check before writing optional scratch output.
    if destination is not None:
        destination.mkdir(parents=True, exist_ok=True)
        for relative, raw in raw_outputs:
            output = destination / relative
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_bytes(raw)
    print(json.dumps({
        "source_commit": manifest["source_commit"],
        "run_id": manifest.get("run_id"),
        "receipt_byte_integrity": "VERIFIED",
        "files_verified": len(raw_outputs),
        "raw_bytes_verified": sum(len(raw) for _, raw in raw_outputs),
        "destination": str(destination) if destination is not None else None,
        "performance_acceptance": "NOT_ASSESSED",
    }))


if __name__ == "__main__":
    main()
