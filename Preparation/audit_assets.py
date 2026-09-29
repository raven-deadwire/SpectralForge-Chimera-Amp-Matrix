#!/usr/bin/env python3
"""Read-only NAM/WAV/AIFF preflight. It does not execute NAM, verify badges online,
prove hardware fidelity, grant a license, or modify/resample/normalize source files.
Evidence is reviewer supplied and bound to the exact SHA-256, not the filename.
"""
from __future__ import annotations
import argparse
from datetime import date
import hashlib
import json
import math
from pathlib import Path
import sys
from typing import Any

MAX_BYTES = 64 * 1024 * 1024
MAX_JSON_DEPTH = 64
IR_EXTENSIONS = {".wav", ".aif", ".aiff"}
SCOPES = {"amp_head", "preamp", "amp_cab", "pedal", "outboard", "cabinet_ir", "reverb_ir"}


def finite_number(value: Any) -> bool:
    return type(value) in (int, float) and math.isfinite(value)


def strict_json(text: str) -> Any:
    def pairs(items):
        result = {}
        for key, value in items:
            if key in result:
                raise ValueError(f"Duplicate JSON key: {key}")
            result[key] = value
        return result
    def invalid(token):
        raise ValueError(f"Nonfinite JSON number: {token}")
    result = json.loads(text, object_pairs_hook=pairs, parse_constant=invalid)
    # Check overflowed exponent literals too (1e999), not just NaN/Infinity.
    for _, value in walk(result):
        if type(value) is float and not math.isfinite(value):
            raise ValueError("Nonfinite numeric value")
    return result


def walk(value: Any, path: str = "$", depth: int = 0):
    if depth > MAX_JSON_DEPTH:
        raise ValueError("JSON nesting limit exceeded")
    yield path, value
    if isinstance(value, dict):
        for key, child in value.items():
            yield from walk(child, f"{path}.{key}", depth + 1)
    elif isinstance(value, list):
        for i, child in enumerate(value):
            yield from walk(child, f"{path}[{i}]", depth + 1)


def issue(report: dict, code: str, text: str, severity: str = "review") -> None:
    report["issues"].append({"code": code, "severity": severity, "message": text})


def review_evidence(report: dict, evidence: dict, expected_scope: str | None) -> None:
    # A local JSON claim is not a web/API verification: preserve that distinction.
    report["reviewer_evidence"] = evidence
    report["creator_verification"] = "reviewer_attested" if evidence.get("creator_verified") is True else "unconfirmed"
    try:
        date.fromisoformat(evidence.get("verified_checked_on", ""))
        checked_date = True
    except (TypeError, ValueError):
        checked_date = False
    source = evidence.get("source_url", "")
    badge_source = evidence.get("verified_evidence_url", "")
    if not (isinstance(source, str) and source.startswith("https://")):
        issue(report, "SOURCE_MISSING", "Exact source page required; filename is not provenance")
    if not (evidence.get("creator_verified") is True and checked_date and
            isinstance(badge_source, str) and badge_source.startswith("https://")):
        issue(report, "VERIFIED_UNCONFIRMED", "Verified creator needs reviewer evidence URL and check date")
    if evidence.get("capture_scope") not in SCOPES:
        issue(report, "SCOPE_UNCONFIRMED", "Head/preamp/full rig/pedal/cabinet/space must be distinguished")
    if expected_scope and evidence.get("capture_scope") != expected_scope:
        issue(report, "SCOPE_MISMATCH", f"Expected {expected_scope}; do not substitute another signal-chain scope")
    if not evidence.get("hardware"):
        issue(report, "HARDWARE_UNCONFIRMED", "Exact hardware/revision/clone lineage is not documented")
    report["distribution"] = {
        "automatic_permission": False,
        "reviewer_claim": evidence.get("redistribution", "unknown"),
        "evidence_url": evidence.get("redistribution_evidence_url"),
    }
    if evidence.get("redistribution") != "allowed" or not evidence.get("redistribution_evidence_url"):
        issue(report, "BUNDLE_PERMISSION_UNCONFIRMED", "Do not bundle data; obtain documented permission", "warning")


def inspect_nam(data: bytes, report: dict, evidence: dict) -> None:
    model = strict_json(data.decode("utf-8-sig"))
    if not isinstance(model, dict):
        raise ValueError("NAM root must be a JSON object")
    for field in ("architecture", "config", "weights"):
        if field not in model:
            raise ValueError(f"Missing NAM field: {field}")
    if not isinstance(model["architecture"], str) or not isinstance(model["config"], dict):
        raise ValueError("Invalid NAM architecture/config types")
    weights = model["weights"]
    if not isinstance(weights, list) or not weights or not all(finite_number(x) for x in weights):
        raise ValueError("Expected a nonempty flat list of finite NAM weights")
    architecture = model["architecture"]
    report["nam"] = {"architecture": architecture, "weight_count": len(weights),
                     "version": model.get("version"), "sample_rate": model.get("sample_rate"),
                     "slimmable_candidate": architecture == "SlimmableContainer",
                     "runtime_load": "not_tested", "rendering": "not_performed"}
    # A SlimmableContainer wrapper alone does not prove the exact A2 network shape.
    if evidence.get("a2_confirmed") is not True or not evidence.get("a2_evidence_url"):
        issue(report, "A2_UNCONFIRMED", "Architecture wrapper/name alone is not A2 certification")
    if architecture != "SlimmableContainer":
        issue(report, "ARCHITECTURE_REVIEW", "Not the expected slimmable wrapper; verify with the NAM core")
    rate = model.get("sample_rate")
    if not finite_number(rate) or rate <= 0:
        issue(report, "SAMPLE_RATE_MISSING", "A positive model sample rate is required")
    metadata = model.get("metadata", {})
    if not isinstance(metadata, dict):
        raise ValueError("NAM metadata must be an object when present")
    calibrations = {}
    for key in ("input_level_dbu", "output_level_dbu"):
        # Keep the location and conflicting values; never silently use a rounded page value.
        found = [{"path": p + "." + key, "value": v[key]} for p, v in walk(metadata)
                 if isinstance(v, dict) and key in v]
        if key in model:
            found.append({"path": "$." + key, "value": model[key]})
        calibrations[key] = found
        if not found or any(not finite_number(x["value"]) for x in found):
            issue(report, key.upper() + "_MISSING", "Missing/non-numeric file calibration; no fabricated dBu",
                  "review" if key == "input_level_dbu" else "warning")
        elif len({x["value"] for x in found}) > 1:
            issue(report, "CALIBRATION_CONFLICT", f"Conflicting {key} values in file")
    report["nam"]["calibration"] = calibrations
    flags = []
    for path, value in walk(metadata):
        if isinstance(value, dict) and isinstance(value.get("checks"), dict):
            flags.append({"path": path + ".checks", "value": value["checks"]})
            if value["checks"].get("passed") is not True:
                issue(report, "TRAINING_CHECKS_REVIEW", "Source checks did not explicitly pass")
        if isinstance(value, dict) and "ignore_checks" in value:
            flags.append({"path": path + ".ignore_checks", "value": value["ignore_checks"]})
            if value["ignore_checks"] is not False:
                issue(report, "TRAINING_CHECKS_BYPASSED", "Source checks bypassed or invalid flag type")
    report["nam"]["training_flags"] = flags
    if not flags:
        issue(report, "TRAINING_LOG_UNAVAILABLE", "File does not establish source training-check status")
    lineage = evidence.get("training_lineage", "unknown")
    report["nam"]["training_lineage_reviewer_claim"] = lineage
    if lineage not in {"dry_wet", "convert"}:
        issue(report, "LINEAGE_UNCONFIRMED", "Distinguish original dry/wet training from model-output conversion")
    elif lineage == "convert":
        issue(report, "CONVERT_REFERENCE", "Converted reference retains upstream-model limitations", "warning")


def inspect_ir(path: Path, report: dict) -> None:
    try:
        import numpy as np
        import soundfile as sf
    except ImportError as exc:
        raise ValueError("IR analysis requires numpy and soundfile; see requirements.txt") from exc
    with sf.SoundFile(str(path)) as audio:
        if audio.frames <= 0 or audio.samplerate <= 0 or audio.channels > 16:
            raise ValueError("Invalid/unsupported audio dimensions")
        if audio.frames * audio.channels > 16_000_000:
            raise ValueError("Decoded sample budget exceeded")
        samples = audio.read(dtype="float64", always_2d=True)
        if samples.shape != (audio.frames, audio.channels) or not np.isfinite(samples).all():
            raise ValueError("Truncated audio or nonfinite samples")
        peaks = np.max(np.abs(samples), axis=0)
        channel_metrics = []
        for c in range(audio.channels):
            x = samples[:, c]; energy = float(np.dot(x, x)); peak = float(peaks[c])
            onset = np.flatnonzero(np.abs(x) >= peak * 0.001) if peak else np.array([], dtype=int)
            first = int(onset[0]) if len(onset) else None
            tail = x[-min(len(x), max(1, round(audio.samplerate * .01))):]
            rms = math.sqrt(energy / len(x))
            channel_metrics.append({
                "channel": c, "peak": peak, "peak_dbfs": 20 * math.log10(peak) if peak else None,
                "rms_dbfs": 20 * math.log10(rms) if rms else None, "dc_mean": float(np.mean(x)),
                "first_above_minus60db_relative_peak_sample": first,
                "onset_ms": 1000 * first / audio.samplerate if first is not None else None,
                "peak_sample": int(np.argmax(np.abs(x))) if peak else None,
                "energy": energy, "tail_10ms_energy_fraction": float(np.dot(tail, tail)) / energy if energy else None,
                "samples_at_or_above_0_999": int(np.count_nonzero(np.abs(x) >= .999)),
            })
        report["ir"] = {"format": audio.format, "subtype": audio.subtype, "sample_rate": audio.samplerate,
                        "frames": audio.frames, "channels": audio.channels,
                        "duration_seconds": audio.frames / audio.samplerate, "per_channel": channel_metrics,
                        "a2_requirement": "not_applicable", "source_processing": "none",
                        "timing_note": "Onset diagnostic, NOT plugin latency or proof of acoustic delay"}
        if not np.any(peaks):
            issue(report, "SILENT_IR", "All-zero impulse response", "error")
        elif np.any(peaks == 0):
            issue(report, "SILENT_CHANNEL", "At least one channel is silent")
        if np.any(peaks >= .999):
            issue(report, "NEAR_FULL_SCALE", "Review headroom; sample count alone does not prove clipping", "warning")
        if audio.channels > 2:
            issue(report, "MULTICHANNEL_REVIEW", "Plugin mono/stereo loading contract needs review")
        if audio.frames / audio.samplerate > 1:
            issue(report, "LONG_IR", "Review convolution cost and cabinet vs reverb role; not auto-trimmed", "warning")


def inspect(path: Path, evidence_by_hash: dict | None = None, expected_scope: str | None = None) -> dict:
    report = {"file": path.name, "issues": [], "status": "invalid", "hardware_fidelity": "not_evaluated"}
    try:
        if not path.is_file():
            raise ValueError("Input file does not exist")
        if path.stat().st_size > MAX_BYTES:
            raise ValueError("Input exceeds 64 MiB limit")
        data = path.read_bytes()
        if len(data) > MAX_BYTES:
            raise ValueError("Input grew beyond size limit")
        digest = hashlib.sha256(data).hexdigest()
        report.update(sha256=digest, bytes=len(data))
        evidence = (evidence_by_hash or {}).get(digest, {})
        if not isinstance(evidence, dict):
            raise ValueError("Evidence entry must be an object")
        ext = path.suffix.lower()
        if ext == ".nam":
            report["kind"] = "nam"; inspect_nam(data, report, evidence)
        elif ext in IR_EXTENSIONS:
            report["kind"] = "ir"; inspect_ir(path, report)
            # Decode occurs via file handle; check the file did not change during the audit.
            if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
                raise ValueError("Input changed during inspection")
        else:
            raise ValueError("Supported formats are .nam, .wav, .aif and .aiff")
        review_evidence(report, evidence, expected_scope)
        severities = {x["severity"] for x in report["issues"]}
        report["status"] = "invalid" if "error" in severities else "needs_review" if "review" in severities else "metadata_ready"
        report["next_step"] = "NAM core load/render and DI listening required" if ext == ".nam" else "Listening, phase/blend and target-loader tests required"
    except (OSError, ValueError, RuntimeError, RecursionError, OverflowError, UnicodeError) as exc:
        issue(report, "INVALID_FILE", str(exc), "error")
        report["status"] = "invalid"
    return report


def batch(paths: list[Path], evidence: dict | None = None, expected_scope: str | None = None) -> dict:
    reports = [inspect(p, evidence, expected_scope) for p in paths]
    duplicates: dict[str, list[int]] = {}
    for i, report in enumerate(reports):
        if "sha256" in report:
            duplicates.setdefault(report["sha256"], []).append(i)
    return {"schema_version": 1, "mode": "read_only_preflight", "online_verification_performed": False,
            "reports": reports, "duplicate_groups": [{"sha256": h, "report_indices": indices}
              for h, indices in duplicates.items() if len(indices) > 1]}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("files", nargs="+", type=Path)
    parser.add_argument("--evidence", type=Path, help="JSON: {by_sha256: {hash: reviewer evidence}}")
    parser.add_argument("--expected-scope", choices=sorted(SCOPES))
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    try:
        evidence = {}
        if args.evidence:
            if args.evidence.stat().st_size > 4 * 1024 * 1024:
                raise ValueError("Evidence file too large")
            document = strict_json(args.evidence.read_text(encoding="utf-8-sig"))
            if not isinstance(document, dict) or not isinstance(document.get("by_sha256"), dict):
                raise ValueError("Evidence must contain a by_sha256 object")
            evidence = document["by_sha256"]
        if args.output and args.output.resolve() in {p.resolve() for p in args.files + ([args.evidence] if args.evidence else [])}:
            raise ValueError("Report output must not overwrite source/evidence")
        result = batch(args.files, evidence, args.expected_scope)
        text = json.dumps(result, ensure_ascii=False, indent=2, allow_nan=False)
        if args.output:
            args.output.write_text(text + "\n", encoding="utf-8")
        else:
            print(text)
        return 0 if all(r["status"] == "metadata_ready" for r in result["reports"]) else 1
    except (OSError, ValueError, RecursionError) as exc:
        print(f"Audit configuration error: {exc}", file=sys.stderr)
        return 2

if __name__ == "__main__":
    raise SystemExit(main())
