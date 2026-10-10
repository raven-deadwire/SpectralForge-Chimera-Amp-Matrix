#!/usr/bin/env python3
"""Verify the public IR allowlist and reject capture/research payloads.

This is a distribution inventory check. It neither inspects nor modifies user
libraries and does not treat a user import as redistribution permission.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ("Assets/IRs/guitar_v30_sm57.wav", "Assets/IRs/guitar_jensen_sm57.wav")
CATALOGS = ("referenceIRCatalog", "externalBassIRCatalog", "ravenIRCatalog")
AUDIO_SUFFIXES = {".wav", ".wave", ".aif", ".aiff", ".aifc", ".flac", ".ogg", ".opus", ".mp3", ".m4a"}
ARCHIVE_SUFFIXES = {".zip", ".7z", ".rar", ".tar", ".gz", ".tgz", ".bz2", ".xz"}
RESTRICTED_PARTS = {"reference", "reference-audio", "chimera-personal-irs", "evidence"}
RESTRICTED_DOCUMENTS = {"raven_ir.md", "nam_reference_results.md", "open_beta_nam_validation.md",
                        "native_nam_calibration.md", "models_and_reference.md"}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f"Duplicate IR policy key: {key}")
        result[key] = value
    return result


def validate_source(root: Path = ROOT) -> dict:
    root = Path(root)
    policy = json.loads((root / "Validation/ir-distribution-policy.json").read_text(encoding="utf-8"),
                        object_pairs_hook=unique_object)
    require(policy.get("schema") == "spectralforge.ir-distribution.v1", "Unknown IR distribution policy")
    rows = policy.get("allowed_embedded")
    require(isinstance(rows, list) and len(rows) == 2, "Public IR inventory must explicitly contain two sources")
    require([row.get("path") for row in rows] == list(ASSETS), "Unexpected public embedded IR source")
    require([row.get("source_id") for row in rows] == [1, 2], "Stored factory source IDs must remain 1 and 2")
    actual = {p.relative_to(root).as_posix() for p in (root / "Assets/IRs").rglob("*") if p.is_file()}
    require(actual == set(ASSETS), "Unapproved/missing file in embedded IR directory")
    other_audio = {p.relative_to(root).as_posix() for p in (root / "Assets").rglob("*")
                   if p.is_file() and p.suffix.casefold() in AUDIO_SUFFIXES | {".nam"}}
    require(other_audio == set(ASSETS), "Unapproved audio/capture elsewhere in product assets")
    for row in rows:
        path = root / row["path"]
        require(not path.is_symlink() and not path.parent.is_symlink(), f"Symlinked embedded IR: {path}")
        require(row.get("license") == "CC-BY-4.0"
                and row.get("license_url") == "https://creativecommons.org/licenses/by/4.0/"
                and row.get("author") == "jesterdyne", "Embedded IR needs recorded attribution and license")
        require(isinstance(row.get("sha256"), str) and re.fullmatch(r"[0-9a-f]{64}", row["sha256"]),
                "Invalid approved IR SHA-256")
        require(path.stat().st_size == row.get("bytes") and digest(path) == row["sha256"],
                f"Embedded IR bytes differ from approved source: {row['path']}")
    cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    blocks = re.findall(r"juce_add_binary_data\s*\(\s*ChimeraIRs\b([^)]*)\)", cmake, re.DOTALL)
    require(len(blocks) == 1 and "SOURCES" in blocks[0], "Cannot identify the embedded IR build inventory")
    sources = blocks[0].split("SOURCES", 1)[1].split()
    require(sources == list(ASSETS), "CMake embeds an unapproved or reordered IR source")
    # Any additional explicit audio input needs its own reviewed policy change.
    named_audio = set(re.findall(r"Assets/IRs/[^\s)\";]+", cmake))
    require(named_audio == set(ASSETS), "CMake references an unapproved IR asset")
    all_audio = set(re.findall(r'[^\s()\";]+\.(?:wav|wave|aif|aiff|aifc|flac|ogg|opus|mp3|m4a|nam)\b', cmake, re.IGNORECASE))
    require(all_audio == set(ASSETS), "CMake embeds audio outside the reviewed IR inventory")
    catalog = (root / "Source/IRReferenceCatalog.h").read_text(encoding="utf-8")
    require(re.search(r"publicIRFactoryCount\s*=\s*2\s*;", catalog) is not None,
            "Public factory catalog count changed")
    for name in CATALOGS:
        entries = re.findall(re.escape(name) + r'\s*=\s*R"IRCAT\((.*?)\)IRCAT";', catalog, re.DOTALL)
        require(len(entries) == 1 and json.loads(entries[0]) == [],
                f"Unapproved reference catalog included in the product: {name}")
    notices = (root / "docs/THIRD_PARTY_NOTICES.md").read_text(encoding="utf-8")
    for row in rows:
        require(all(value in notices for value in (row["path"], row["author"], row["source_url"], row["license_url"])),
                f"Missing packaged attribution for {row['path']}")
    return {"schema": policy["schema"], "allowed_embedded": [
        {key: row[key] for key in ("path", "source_id", "bytes", "sha256")} for row in rows],
        "reference_entries": 0, "loose_audio_allowed": False}


def validate_member_name(name: str) -> None:
    require("\\" not in name, f"Noncanonical public payload path: {name}")
    path = PurePosixPath(name)
    parts = {part.casefold() for part in path.parts}
    require(bool(path.parts) and not path.is_absolute() and ".." not in path.parts and ":" not in name,
            f"Unsafe public payload path: {name}")
    require(not parts.intersection(RESTRICTED_PARTS), f"Restricted reference/capture payload: {name}")
    require(path.name.casefold() not in RESTRICTED_DOCUMENTS, f"Development reference document in public payload: {name}")
    require(path.suffix.casefold() != ".nam", f"Private model capture in public payload: {name}")
    require(path.suffix.casefold() not in AUDIO_SUFFIXES, f"Loose audio is not a public package input: {name}")
    require(path.suffix.casefold() not in ARCHIVE_SUFFIXES, f"Unreviewed nested capture archive: {name}")


def validate_bytes_prefix(name: str, header: bytes) -> None:
    wave = len(header) >= 12 and header[:4] in (b"RIFF", b"RF64") and header[8:12] == b"WAVE"
    aiff = len(header) >= 12 and header[:4] == b"FORM" and header[8:12] in (b"AIFF", b"AIFC")
    compressed_audio = header.startswith((b"fLaC", b"OggS", b"ID3"))
    archive = header.startswith((b"PK\x03\x04", b"7z\xbc\xaf\x27\x1c", b"Rar!\x1a\x07", b"\x1f\x8b"))
    require(not (wave or aiff or compressed_audio or archive),
            f"Audio/archive bytes disguised as a public product file: {name}")


def validate_stage(stage: Path) -> None:
    stage = Path(stage)
    require(stage.is_dir(), f"Public payload directory missing: {stage}")
    root = stage.resolve()
    for path in sorted(stage.rglob("*")):
        name = path.relative_to(stage).as_posix()
        validate_member_name(name)
        if path.is_symlink():
            require(path.resolve().is_relative_to(root), f"Public payload symlink escapes stage: {name}")
        if path.is_file():
            with path.open("rb") as stream:
                validate_bytes_prefix(name, stream.read(16))


def validate_zip_payload(archive: zipfile.ZipFile) -> None:
    for member in archive.infolist():
        validate_member_name(member.filename.rstrip("/"))
        if not member.is_dir():
            with archive.open(member) as stream:
                validate_bytes_prefix(member.filename, stream.read(16))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--stage", type=Path)
    args = parser.parse_args()
    report = validate_source(args.root)
    if args.stage:
        validate_stage(args.stage)
        report["stage"] = str(args.stage)
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
