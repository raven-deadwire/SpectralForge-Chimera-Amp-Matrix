#!/usr/bin/env python3
"""Build the small POSIX document-maintenance executable from reviewed bytes.

The inventory is a build input, never an installer-supplied deletion authority.
The generated executable needs only the target OS C library (no Python runtime).
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
INVENTORY = ROOT / "Validation/legacy-document-inventory.json"
SCHEMA = "spectralforge.legacy-document-inventory.v1"
MAX_DOCUMENT_BYTES = 64 * 1024 * 1024


def _path(value: str) -> str:
    if not isinstance(value, str) or not value or len(value.encode()) > 2048:
        raise ValueError("Invalid document path")
    p = PurePosixPath(value)
    if (p.is_absolute() or p.as_posix() != value or "\\" in value
            or any(part in {".", ".."} for part in value.split("/"))
            or any(ord(c) < 32 for c in value)):
        raise ValueError(f"Unsafe document path: {value!r}")
    return value


def _license(path: str) -> bool:
    # A manifest cannot weaken the mandatory notice boundary.
    return bool(re.search(r"(?:copyright|licen[cs]e|notice|attribution)", path, re.I))


def document_rules(*, platform: str, inventory: Path | None = None,
                   current_documents: Path | None = None) -> list[dict]:
    if platform not in {"linux", "macos"}:
        raise ValueError("POSIX maintenance supports Linux and macOS")
    data = json.loads(Path(inventory or INVENTORY).read_text(encoding="utf-8"))
    if data.get("schema") != SCHEMA:
        raise ValueError("Unrecognized historical document inventory")
    rules: dict[str, dict] = {}
    for entry in data["files"]:
        path = _path(entry["path"])
        if path in rules:
            raise ValueError(f"Duplicate document path: {path}")
        if platform not in entry.get("platforms", []):
            continue
        variants = set()
        for variant in entry["variants"]:
            if platform not in variant.get("platforms", entry["platforms"]):
                continue
            size, digest = variant["size"], variant["sha256"]
            if (type(size) is not int or not 0 <= size <= MAX_DOCUMENT_BYTES
                    or not isinstance(digest, str) or not re.fullmatch(r"[0-9a-f]{64}", digest)):
                raise ValueError(f"Invalid raw-byte fingerprint: {path}")
            variants.add((size, digest))
        if not variants:
            continue
        always = bool(entry.get("preserve_always")) or _license(path)
        rules[path] = {"path": path, "retire": bool(entry.get("retire")) and not always,
                       "always": always, "current": False, "variants": variants}
    if current_documents is not None:
        current_documents = Path(current_documents)
        if current_documents.is_symlink() or not current_documents.is_dir():
            raise ValueError("Current document staging directory is missing or linked")
        for file in sorted(current_documents.rglob("*")):
            if file.is_symlink():
                raise ValueError(f"Linked staged document: {file}")
            if not file.is_file():
                continue
            path = _path(file.relative_to(current_documents).as_posix())
            content = file.read_bytes()
            if len(content) > MAX_DOCUMENT_BYTES:
                raise ValueError(f"Oversized staged document: {path}")
            rule = rules.setdefault(path, {"path": path, "retire": False,
                                           "always": _license(path), "current": True,
                                           "variants": set()})
            rule["current"] = True
            rule["retire"] = False
            rule["variants"].add((len(content), hashlib.sha256(content).hexdigest()))
    return [{**rules[path], "variants": sorted(rules[path]["variants"])} for path in sorted(rules)]


def _c_string(value: str) -> str:
    # UTF-8 octal literals have no escape-extension ambiguity or encoding drift.
    return '"' + "".join(f"\\{value:03o}" for value in value.encode("utf-8")) + '"'


def build_helper(output: Path, *, platform: str = "linux", inventory: Path | None = None,
                 current_documents: Path | None = None) -> Path:
    rules = document_rules(platform=platform, inventory=inventory, current_documents=current_documents)
    canonical = json.dumps(rules, sort_keys=True, separators=(",", ":")).encode()
    digest = hashlib.sha256(canonical).hexdigest()
    fingerprints, entries = [], []
    for rule in rules:
        offset = len(fingerprints)
        fingerprints.extend('{%duLL,"%s"}' % variant for variant in rule["variants"])
        entries.append("{%s,%d,%d,%d,%d,%d}" % (
            _c_string(rule["path"]), rule["retire"], rule["always"], rule["current"],
            offset, len(rule["variants"])))
    header = ('#define RULE_DIGEST "' + digest + '"\n'
              + '#define TARGET_MACOS ' + str(int(platform == "macos")) + '\n'
              + "static const Fingerprint fingerprints[] = {\n" + ",\n".join(fingerprints or ['{0,""}']) + "\n};\n"
              + "static const Rule rules[] = {\n" + ",\n".join(entries or ['{"",0,0,0,0,0}']) + "\n};\n"
              + "static const size_t rule_count = " + str(len(rules)) + ";\n")
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="chimera-doc-build-") as temp:
        include = Path(temp) / "document_rules.h"
        include.write_text(header, encoding="ascii")
        command = ["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                   "-I", temp, str(ROOT / "Tools/legacy_document_cleanup.c"), "-o", str(output)]
        if platform == "macos":
            command[1:1] = ["-arch", "arm64", "-arch", "x86_64", "-mmacosx-version-min=12.0"]
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError("Native document helper compilation failed:\n" + result.stderr)
    output.chmod(0o755)
    return output


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--platform", choices=("linux", "macos"), required=True)
    parser.add_argument("--inventory", type=Path)
    parser.add_argument("--current-documents", type=Path)
    args = parser.parse_args()
    build_helper(args.output, platform=args.platform, inventory=args.inventory,
                 current_documents=args.current_documents)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
