#!/usr/bin/env python3
"""Recheck pinned historical documentation from local public release archives.

This read-only archive audit never invokes an installer, changes an installed
file, fetches a URL, or rewrites the cleanup allowlist. It verifies the complete
outer archive digest, observes the original bytes of each installed document,
and compares them with the recorded variant and embedded payload manifest.
"""
from __future__ import annotations

import argparse
from collections import defaultdict
from datetime import datetime, timezone
import gzip
import hashlib
import json
from pathlib import Path, PurePosixPath
import stat
import struct
import tarfile
from typing import BinaryIO, Iterator
import xml.etree.ElementTree as ET
import zipfile
import zlib

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_INVENTORY = ROOT / "Validation/legacy-document-inventory.json"
MAC_DOCUMENTS = "Library/Application Support/SpectralForge/Chimera/Documentation/"
LINUX_DOCUMENTS = "usr/share/doc/chimera-amp-matrix/"
MAX_DOCUMENT_BYTES = 16 * 1024 * 1024
MAX_TOC_BYTES = 16 * 1024 * 1024


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def read_exact(stream: BinaryIO, length: int) -> bytes:
    data = stream.read(length)
    if len(data) != length:
        raise ValueError("Unexpected end of archive")
    return data


def discard(stream: BinaryIO, length: int) -> None:
    while length:
        chunk = min(length, 1024 * 1024)
        read_exact(stream, chunk)
        length -= chunk


def safe_relative(value: str) -> str:
    path = PurePosixPath(value)
    if not value or path.is_absolute() or ".." in path.parts or "\\" in value or "\0" in value:
        raise ValueError(f"Unsafe archive path: {value!r}")
    return path.as_posix()


def document_path(member: str, platform: str) -> str | None:
    name = safe_relative(member)
    if platform == "windows":
        if name.startswith("reference/"):
            return name
        # Historical Inno Setup installed these stage-root patterns and the
        # two exact JSON files. Other stage trees were not document installs.
        if "/" not in name and (name.endswith((".txt", ".md", ".html"))
                                 or name in ("ARTWORK_PROMPTS.json", "payload-manifest.json")):
            return name
        return None
    if platform == "macos":
        return name.removeprefix(MAC_DOCUMENTS) if name.startswith(MAC_DOCUMENTS) else None
    if platform == "linux":
        # The TAR includes one product/version directory before usr/.
        marker = "/" + LINUX_DOCUMENTS
        if marker in name:
            return name.split(marker, 1)[1]
        return name.removeprefix(LINUX_DOCUMENTS) if name.startswith(LINUX_DOCUMENTS) else None
    raise ValueError(f"Unsupported platform: {platform}")


def document_bytes(stream: BinaryIO, size: int) -> bytes:
    if not 0 <= size <= MAX_DOCUMENT_BYTES:
        raise ValueError("Historical document exceeds bounded audit size")
    return read_exact(stream, size)


def macos_documents(path: Path) -> Iterator[tuple[str, str, bytes]]:
    """Read Apple's historical XAR/gzip/odc-cpio payload without extracting it."""
    with path.open("rb") as source:
        magic, header_size, version, compressed_size, raw_size, _ = struct.unpack(">IHHQQI", read_exact(source, 28))
        if magic != 0x78617221 or version != 1 or header_size != 28:
            raise ValueError("Expected historical XAR version 1 header")
        if not 0 < compressed_size <= MAX_TOC_BYTES or not 0 < raw_size <= MAX_TOC_BYTES:
            raise ValueError("Invalid XAR table size")
        decompressor = zlib.decompressobj()
        table = decompressor.decompress(read_exact(source, compressed_size), MAX_TOC_BYTES + 1)
        if len(table) != raw_size or not decompressor.eof or decompressor.unconsumed_tail:
            raise ValueError("Invalid XAR table compression or size")
        tree = ET.fromstring(table)
        matches = [node for node in tree.findall(".//file") if node.findtext("name") == "Payload"]
        if len(matches) != 1:
            raise ValueError("Expected exactly one XAR Payload")
        offset = int(matches[0].findtext("data/offset", "-1"))
        length = int(matches[0].findtext("data/length", "-1"))
        start = header_size + compressed_size + offset
        if offset < 0 or length < 0 or start + length > path.stat().st_size:
            raise ValueError("Invalid XAR Payload bounds")
        source.seek(start)
        with gzip.GzipFile(fileobj=source) as payload:
            for _ in range(100000):
                header = read_exact(payload, 76)
                if header[:6] != b"070707":
                    raise ValueError("Expected historical odc cpio payload")
                mode = int(header[18:24], 8)
                name_size = int(header[59:65], 8)
                size = int(header[65:76], 8)
                if not 1 <= name_size <= 32768:
                    raise ValueError("Invalid cpio member name length")
                raw_name = read_exact(payload, name_size)
                if raw_name[-1:] != b"\0":
                    raise ValueError("Unterminated cpio member name")
                name = raw_name[:-1].decode("utf-8")
                if name == "TRAILER!!!":
                    return
                relative = document_path(name, "macos")
                if relative is not None:
                    if not stat.S_ISREG(mode):
                        # Directories do not own any document bytes. Reject
                        # links and other special entries in document scope.
                        if stat.S_ISDIR(mode):
                            discard(payload, size)
                            continue
                        raise ValueError(f"Non-regular historical document: {name}")
                    yield relative, name, document_bytes(payload, size)
                else:
                    discard(payload, size)
            raise ValueError("Historical cpio member limit exceeded")


def archive_documents(path: Path, platform: str) -> Iterator[tuple[str, str, bytes]]:
    if platform == "macos":
        yield from macos_documents(path)
        return
    if platform == "windows":
        with zipfile.ZipFile(path) as archive:
            for member in archive.infolist():
                if member.is_dir():
                    continue
                relative = document_path(member.filename, platform)
                if relative is None:
                    continue
                mode = member.external_attr >> 16
                if stat.S_ISLNK(mode):
                    raise ValueError("Linked historical document in ZIP")
                with archive.open(member) as source:
                    yield relative, member.filename, document_bytes(source, member.file_size)
        return
    if platform == "linux":
        with tarfile.open(path, "r:gz") as archive:
            for member in archive:
                relative = document_path(member.name, platform)
                if relative is None or member.isdir():
                    continue
                if not member.isfile():
                    raise ValueError("Non-regular historical document in TAR")
                with archive.extractfile(member) as source:
                    yield relative, member.name, document_bytes(source, member.size)
        return
    raise ValueError(f"Unsupported platform: {platform}")


def audit(inventory_path: Path, packages_dir: Path) -> dict:
    inventory = json.loads(inventory_path.read_text(encoding="utf-8"))
    if inventory["schema"] != "spectralforge.legacy-document-inventory.v1":
        raise ValueError("Unsupported historical inventory schema")
    expected = defaultdict(dict)
    for entry in inventory["files"]:
        safe_relative(entry["path"])
        for variant in entry["variants"]:
            for proof in variant["evidence"]:
                if proof["kind"] != "release-package":
                    raise ValueError("Cleanup digest lacks observed public archive evidence")
                name = proof["package_name"]
                if entry["path"] in expected[name]:
                    raise ValueError(f"Duplicate document evidence: {name}/{entry['path']}")
                if proof["platform"] not in entry["platforms"] or proof["platform"] not in variant["platforms"]:
                    raise ValueError("Document proof platform is outside its variant")
                expected[name][entry["path"]] = (variant, proof)
    package_reports = []
    seen_packages = set()
    for release in inventory["releases"]:
        for package in release["packages"]:
            name = package["name"]
            if safe_relative(name) != name or "/" in name or name in seen_packages:
                raise ValueError("Invalid or duplicate package filename")
            seen_packages.add(name)
            archive = packages_dir / name
            if archive.is_symlink() or archive.stat().st_size != package["size"] or digest(archive) != package["sha256"]:
                raise ValueError(f"Historical outer archive hash or size mismatch: {name}")
            platform = package["platform"]
            records = {}
            for relative, member_name, data in archive_documents(archive, platform):
                if relative in records:
                    raise ValueError(f"Duplicate installed archive path: {relative}")
                records[relative] = (member_name, data)
            if set(records) != set(expected[name]):
                raise ValueError(f"Historical document inventory differs for {name}")
            payload_bytes = records["payload-manifest.json"][1]
            payload = json.loads(payload_bytes)
            payload_hash = hashlib.sha256(payload_bytes).hexdigest()
            payload_rows = {row["path"]: row for row in payload["files"]}
            payload_checks = 0
            for relative, (member_name, data) in records.items():
                variant, proof = expected[name][relative]
                if len(data) != variant["size"] or hashlib.sha256(data).hexdigest() != variant["sha256"]:
                    raise ValueError(f"Historical document bytes differ: {name}/{relative}")
                if (proof["tag"] != release["tag"] or proof["source_sha"] != release["source_sha"]
                        or proof["archive_sha256"] != package["sha256"] or proof["platform"] != platform
                        or proof["package_path"] != member_name or proof["payload_manifest_sha256"] != payload_hash
                        or proof["payload_source_sha"] != payload.get("source_sha")):
                    raise ValueError(f"Historical provenance mismatch: {name}/{relative}")
                if relative == "payload-manifest.json":
                    continue
                manifest_name = safe_relative(member_name)
                if platform == "linux":
                    manifest_name = "usr/" + manifest_name.split("/usr/", 1)[1]
                row = payload_rows[manifest_name]
                if row["bytes"] != len(data) or row["sha256"] != variant["sha256"]:
                    raise ValueError(f"Embedded payload manifest differs: {name}/{relative}")
                payload_checks += 1
            package_reports.append({"tag": release["tag"], "package": name, "platform": platform,
                                    "sha256": package["sha256"], "documents": len(records),
                                    "payload_entries_verified": payload_checks,
                                    "archive_verified": True, "actual_install_verified": False})
    if seen_packages != set(expected):
        raise ValueError("Document evidence points outside the audited release packages")
    return {"schema": "spectralforge.legacy-document-archive-audit.v1",
            "recorded_at_utc": datetime.now(timezone.utc).isoformat(),
            "inventory_sha256": digest(inventory_path), "status": "pass",
            "package_count": len(package_reports),
            "document_occurrences": sum(row["documents"] for row in package_reports),
            "payload_entries_verified": sum(row["payload_entries_verified"] for row in package_reports),
            "actual_install_verified": False,
            "scope": "Read-only ZIP/PKG/TAR contents and digest verification; no installer execution.",
            "packages": package_reports}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inventory", type=Path, default=DEFAULT_INVENTORY)
    parser.add_argument("--packages-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, help="Write the complete read-only audit result as JSON")
    args = parser.parse_args()
    report = audit(args.inventory, args.packages_dir)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({key: report[key] for key in ("status", "package_count", "document_occurrences",
                                                   "payload_entries_verified", "actual_install_verified")}, indent=2))


if __name__ == "__main__":
    main()
