#!/usr/bin/env python3
"""Compare actual runtime SETTINGS evidence with generated/package identity."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import beta_lifecycle
import package_release


def verify(root: Path, build: Path) -> None:
    metadata = json.loads((build / "chimera-beta-lifecycle.json").read_text(encoding="utf-8"))
    compiled = json.loads((build / "beta-lifecycle-evidence/compiled-beta-identity.json").read_text(encoding="utf-8"))
    if compiled != metadata:
        raise ValueError("Runtime compiled identity differs from package metadata")
    beta_lifecycle.validate_metadata(metadata, root, metadata["source_sha"])
    # Exercise the real package stager, including the configure-time fingerprint.
    staged = build / "beta-lifecycle-evidence/staged-documents"
    staged.mkdir(parents=True, exist_ok=True)
    package_release.lifecycle_documents(staged, build, version={"source_sha": metadata["source_sha"]})
    notice = (staged / "BETA_LIFECYCLE.txt").read_text(encoding="utf-8")
    settings = (build / "beta-lifecycle-evidence/settings-compiled-notice.txt").read_text(encoding="utf-8")
    for name, value in (("version", metadata["version"]), ("source", metadata["source_sha"]),
                        ("build", metadata["build_id"]), ("UTC", metadata["expires_at_utc"]),
                        ("KST", beta_lifecycle.expiry_kst(metadata))):
        if not value or value not in settings or value not in notice:
            raise ValueError(f"SETTINGS/package notice mismatch: {name}")
    if "Remaining: 1 day 01 hours 01 minutes 01 seconds." not in settings:
        raise ValueError("Runtime evidence did not include the injected-clock countdown")
    print("PASS: policy, compiled runtime, SETTINGS, generated JSON/fingerprint and staged UTC/KST notice agree")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, required=True)
    args = parser.parse_args()
    verify(Path(__file__).resolve().parents[1], args.build.resolve())
