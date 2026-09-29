#!/usr/bin/env python3
"""Verify the previously delivered binary before comparative VST3 lifecycle tests."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import zipfile

SOURCE = "7e9f3381aad5c9f5c66dc94033c83bff38c05657"
RUN = "36498630415"
ARCHIVE = "Chimera-update-7e9f3381aa-win64.zip"


def prepare(folder, output):
    archive = folder / ARCHIVE
    raw = archive.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    assert (folder / (ARCHIVE + ".sha256")).read_text().split()[0] == digest, "Baseline ZIP hash mismatch"
    with zipfile.ZipFile(archive) as package:
        assert package.testzip() is None, "Baseline ZIP CRC failure"
        manifest = json.loads(package.read("build-manifest.json"))
        assert manifest["source_sha"] == SOURCE and str(manifest["run_id"]) == RUN, "Wrong baseline source/run"
        count = 0
        for entry in manifest["files"]:
            path = PurePosixPath(entry["path"])
            assert not path.is_absolute() and ".." not in path.parts, "Unsafe baseline path"
            data = package.read(path.as_posix())
            assert len(data) == entry["bytes"] and hashlib.sha256(data).hexdigest() == entry["sha256"], "Baseline payload mismatch"
            if path.parts[0] == "SpectralForge Chimera.vst3":
                target = output.joinpath(*path.parts)
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
                count += 1
        assert count > 0, "Baseline VST3 missing"
    (output / "baseline-verification.json").write_text(json.dumps({
        "source_sha": SOURCE, "run_id": RUN, "archive_sha256": digest,
        "verified_vst3_files": count, "purpose": "Compare actual prior delivered VST3; not Studio One itself"
    }, indent=2) + "\n")
    print("Verified prior delivered VST3:", SOURCE)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    prepare(args.folder, args.output)
