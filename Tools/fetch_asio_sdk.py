#!/usr/bin/env python3
"""Download and extract the Steinberg ASIO SDK for a Windows build.

The SDK is fetched from Steinberg's official address at build time and is
never committed or packaged. When Validation/asio-sdk.json pins a SHA-256,
the archive must match it; an unpinned archive prints its digest so it can
be pinned. Prints (and optionally exports) the SDK root that CMake expects:
the directory containing common/iasiodrv.h.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import sys
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
POLICY = ROOT / "Validation" / "asio-sdk.json"


def sdk_root(extracted: Path) -> Path:
    matches = sorted(p.parent.parent for p in extracted.rglob("iasiodrv.h") if p.parent.name == "common")
    if len(matches) != 1:
        raise SystemExit(f"Expected one common/iasiodrv.h in the ASIO SDK archive, found {len(matches)}")
    return matches[0]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dest", type=Path, required=True, help="Empty directory outside the source tree")
    parser.add_argument("--github-env", action="store_true", help="Append CHIMERA_ASIO_SDK_DIR to $GITHUB_ENV")
    parser.add_argument("--require-pin", action="store_true", help="Fail unless the policy pins a SHA-256")
    args = parser.parse_args()
    policy = json.loads(POLICY.read_text(encoding="utf-8"))
    pinned = policy.get("sha256")
    if args.require_pin and not pinned:
        raise SystemExit("Validation/asio-sdk.json does not pin the ASIO SDK SHA-256")
    dest = args.dest.resolve()
    if ROOT in (dest, *dest.parents):
        raise SystemExit("Extract the ASIO SDK outside the source tree")
    dest.mkdir(parents=True, exist_ok=True)
    archive = dest / "asiosdk.zip"
    request = urllib.request.Request(policy["url"], headers={"User-Agent": "Chimera-build"})
    with urllib.request.urlopen(request, timeout=120) as response, archive.open("wb") as out:
        final_url = response.geturl()
        out.write(response.read())
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    print(f"ASIO SDK source: {final_url}")
    print(f"ASIO SDK sha256: {digest}")
    if pinned and digest != pinned.lower():
        raise SystemExit(f"ASIO SDK digest {digest} differs from the pinned {pinned}")
    if not pinned:
        print("ASIO SDK digest is not pinned yet; record it in Validation/asio-sdk.json", file=sys.stderr)
    extracted = dest / "sdk"
    with zipfile.ZipFile(archive) as bundle:
        for member in bundle.namelist():
            target = (extracted / member).resolve()
            if extracted.resolve() not in target.parents and target != extracted.resolve():
                raise SystemExit(f"Unsafe path in ASIO SDK archive: {member}")
        bundle.extractall(extracted)
    root = sdk_root(extracted)
    print(f"CHIMERA_ASIO_SDK_DIR={root}")
    if args.github_env:
        with open(os.environ["GITHUB_ENV"], "a", encoding="utf-8") as env:
            env.write(f"CHIMERA_ASIO_SDK_DIR={root}\n")
            env.write(f"CHIMERA_ASIO_SDK_SHA256={digest}\n")
        # An annotation survives log truncation; later steps can re-print the env.
        print(f"::notice title=ASIO SDK provenance::sha256={digest} source={final_url}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
