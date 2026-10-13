#!/usr/bin/env python3
"""Reverify the raw CI evidence embedded in a publication gate archive.

Archive provenance remains the publisher's responsibility. This helper binds the
gate's input fingerprints to real embedded bytes and reruns the existing raw
receipt validators. A digest-valid archive containing stale PASS JSON alone is
insufficient. No ZIP path is extracted directly and no approval is created.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import stat
import tempfile
import xml.etree.ElementTree as ET

import evaluate_release_gate as gate
import release_evidence as evidence
import weekly_publication as weekly

MAX_ARCHIVE_BYTES = 16 * 1024 * 1024
MAX_ARCHIVE_ENTRIES = 4096
require = gate.require


def _parse(data):
    return json.loads(data.decode("utf-8-sig"), object_pairs_hook=gate.unique_object,
                      parse_constant=gate.invalid_constant)


def _positive_id(value):
    return (type(value) is int and value > 0
            or isinstance(value, str) and re.fullmatch(r"[1-9][0-9]*", value) is not None)


def _safe_path(name, *, directory=False):
    require(isinstance(name, str) and name and "\\" not in name and "\x00" not in name,
            "Unsafe embedded evidence path")
    path = PurePosixPath(name)
    canonical = path.as_posix() + ("/" if directory else "")
    require(not path.is_absolute() and ".." not in path.parts and path.parts
            and ":" not in path.parts[0] and name == canonical,
            "Unsafe or aliased embedded evidence path")
    return path


def verify_embedded_evidence(bundle, verdict, policy, checks, source_sha, run_id, run_attempt,
                             *, profile, ci_check_map=None):
    """Verify four raw bundles and regenerate their 21 automatic check reports."""
    require(isinstance(source_sha, str) and re.fullmatch(r"[0-9a-f]{40}", source_sha)
            and _positive_id(run_id) and _positive_id(run_attempt), "Exact evidence source/run/attempt required")
    revision = {"commit_sha": source_sha, "run_id": str(run_id), "run_attempt": str(run_attempt)}
    require(profile in (weekly.BETA_PROFILE, weekly.FULL_PROFILE)
            and policy.get("release_scope", {}).get("profile") == profile,
            "Embedded evidence profile differs from the source policy")
    entries = bundle.infolist()
    names = [entry.filename for entry in entries]
    require(len(entries) <= MAX_ARCHIVE_ENTRIES and len(names) == len(set(names))
            and sum(entry.file_size for entry in entries) <= MAX_ARCHIVE_BYTES,
            "Duplicate or oversized embedded evidence archive")
    files = {}
    for entry in entries:
        path = _safe_path(entry.filename, directory=entry.is_dir())
        mode = entry.external_attr >> 16
        expected_type = stat.S_IFDIR if entry.is_dir() else stat.S_IFREG
        require(stat.S_IFMT(mode) in (0, expected_type), "Non-regular embedded evidence entry")
        if not entry.is_dir():
            files[path] = entry
    gate_paths = [path for path in files if path.name == "release-gate.json"]
    require(len(gate_paths) == 1, "Expected exactly one embedded release gate")
    root = gate_paths[0].parent
    require(weekly.digest(_parse(bundle.read(files[gate_paths[0]]))) == weekly.digest(verdict),
            "Supplied gate differs from the embedded release gate")
    producer = verdict.get("producer", {})
    require(isinstance(producer, dict) and producer.get("name") == "consolidate_release_gate"
            and producer.get("errors") == [], "Embedded evidence producer has errors")
    inputs = producer.get("inputs")
    require(isinstance(inputs, list) and len(inputs) == len(evidence.KINDS)
            and all(isinstance(item, dict) and isinstance(item.get("kind"), str) for item in inputs)
            and {item["kind"] for item in inputs} == set(evidence.KINDS)
            and all(item.get("revision") == revision
                    and isinstance(item.get("manifest_sha256"), str)
                    and re.fullmatch(r"[0-9a-f]{64}", item["manifest_sha256"]) for item in inputs),
            "Missing, duplicate or stale embedded evidence identity")
    fingerprints = {item["kind"]: item["manifest_sha256"] for item in inputs}
    ci_check_map = gate.load_json(evidence.ROOT / "Validation/ci-check-map.json") if ci_check_map is None else ci_check_map
    require(weekly.digest(ci_check_map) == weekly.CI_CHECK_MAP_SHA256
            and ci_check_map.get("policy_version") == policy.get("policy_version"),
            "Embedded evidence CI ownership contract changed")
    mapping = ci_check_map["checks"]
    require(isinstance(checks, dict) and set(mapping) == weekly.CI_IDS and weekly.CI_IDS <= set(checks),
            "Embedded evidence automatic check inventory is incomplete")
    received = {}
    with tempfile.TemporaryDirectory(prefix="chimera-publication-evidence-") as temporary:
        working = Path(temporary)
        for kind in evidence.KINDS:
            prefix = root / "evidence" / kind
            manifest = prefix / "evidence.json"
            require(manifest in files, f"Missing embedded evidence manifest: {kind}")
            manifest_bytes = bundle.read(files[manifest])
            require(hashlib.sha256(manifest_bytes).hexdigest() == fingerprints[kind],
                    f"Embedded evidence manifest hash mismatch: {kind}")
            folder = working / kind
            folder.mkdir()
            # Create fresh ordinary files only after path and archive bounds
            # checks; never honor ZIP links, modes or extraction destinations.
            for path, entry in files.items():
                if not path.is_relative_to(prefix):
                    continue
                relative = path.relative_to(prefix)
                require(relative.parts, "Evidence manifest is not inside its platform folder")
                destination = folder.joinpath(*relative.parts)
                destination.parent.mkdir(parents=True, exist_ok=True)
                with destination.open("xb") as output:
                    output.write(bundle.read(entry))
            try:
                doc, paths = evidence.read_bundle(folder, kind, revision, policy, profile=profile)
                received[kind] = evidence.reports_for_bundle(doc, paths, policy, mapping)
            except (ValueError, TypeError, KeyError, AttributeError, ET.ParseError) as exc:
                raise gate.ValidationError(f"Malformed embedded raw evidence: {kind}") from exc
    for cid, spec in mapping.items():
        platforms = (["assembly"] if spec["producer"] == "workflow:assemble-candidate" else
                     list(evidence.PLATFORMS) if spec["platform"] == "all" else [spec["platform"]])
        for kind in platforms:
            report = received[kind].get(cid, {})
            # Sparse validators retain descriptive reason strings on success.
            # Recreate the consolidator's public check below, whose failure
            # field must be null, rather than trust those internal reason fields.
            require(report.get("execution", {}).get("executed") is True
                    and type(report.get("execution", {}).get("exit_code")) is int
                    and report["execution"]["exit_code"] == 0
                    and report.get("assertion") == {"passed": True},
                    f"Automatic check is not supported by embedded raw evidence: {cid} ({kind})")
        expected = evidence.make_report(policy, cid, source_sha, passed=True,
            scope={"producer": spec["producer"], "platforms": platforms, "synthetic_only": True,
                   "run_id": str(run_id), "run_attempt": str(run_attempt)},
            artifacts=["evidence/" + kind + "/evidence.json" for kind in platforms])
        require(weekly.digest(checks[cid]) == weekly.digest(expected),
                f"Archived automatic check differs from regenerated raw evidence: {cid}")
    # Strict publication may add evidence for manual/target-hardware checks.
    # Never accept a PASS report whose referenced evidence is absent from the
    # immutable archive, even though its producer is outside this CI map.
    for cid, check in checks.items():
        if check.get("execution", {}).get("executed") is not True:
            continue
        artifacts = check.get("evidence", {}).get("artifacts")
        require(isinstance(artifacts, list) and artifacts, f"Executed check has no embedded evidence: {cid}")
        for artifact in artifacts:
            path = root / _safe_path(artifact)
            require(path in files and files[path].file_size > 0,
                    f"Executed check references missing or empty embedded evidence: {cid}")
