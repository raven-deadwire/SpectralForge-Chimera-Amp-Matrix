#!/usr/bin/env python3
"""Local OBS test kit. No OBS control, network access, or compatibility certification."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import sys
import wave

CASES = (
    "scan_load", "mono_stereo", "editor_lifecycle", "preset_ir_recall",
    "scene_restart", "classic_dual_matrix", "bypass", "transpose_sync",
    "multiple_instances", "one_hour_recording", "audio_restart",
    "remove_source_exit", "real_di_listening",
)
RATE = 48000


def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def required(value, name):
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{name} is required")
    return value.strip()


def fingerprint(bundle):
    """Hash one explicit bundle; never enumerate user presets, scenes or logs."""
    if bundle.is_symlink() or not bundle.is_dir() or bundle.suffix.lower() != ".vst3":
        raise ValueError("plugin must be a real .vst3 bundle directory")
    records = []
    for path in sorted(bundle.rglob("*")):
        if path.is_symlink():
            raise ValueError("symlinks in plugin bundles are not supported by this collector")
        if path.is_file():
            records.append({"path": path.relative_to(bundle).as_posix(),
                            "size": path.stat().st_size, "sha256": digest(path)})
    if not records:
        raise ValueError("plugin bundle is empty")
    encoded = json.dumps(records, sort_keys=True, separators=(",", ":")).encode()
    return {"files": records, "manifest_sha256": hashlib.sha256(encoded).hexdigest()}


def write_json(path, value):
    # Replace a sibling temporary file so an interrupted write preserves the old record.
    temp = path.with_name(path.name + ".tmp")
    if temp.exists() or temp.is_symlink():
        raise ValueError(f"temporary file already exists: {temp}")
    with temp.open("x", encoding="utf-8") as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2, allow_nan=False)
        stream.write("\n")
    temp.replace(path)


def signals(output):
    output.mkdir(parents=True, exist_ok=False)
    # 8 s: quiet head, left-only 440 Hz, right-only 880 Hz, silence, timed markers.
    stereo = bytearray()
    mono = bytearray()
    for n in range(8 * RATE):
        t = n / RATE
        left = 0.1 * math.sin(2 * math.pi * 440 * t) if 1 <= t < 2 else 0.0
        right = 0.1 * math.sin(2 * math.pi * 880 * t) if 3 <= t < 4 else 0.0
        if n in (5 * RATE, 6 * RATE):
            left = right = 0.1
        stereo.extend(struct.pack("<hh", round(left * 32767), round(right * 32767)))
        # Alternating A0/B0 bursts with 5 ms ramps; routing/listening aid, not real DI.
        pos = t % 1.0
        env = min(pos / .005, (.5 - pos) / .005, 1.0) if pos < .5 else 0.0
        freq = 27.5 if int(t) % 2 == 0 else 30.8677063285
        sample = .1 * max(0.0, env) * math.sin(2 * math.pi * freq * t)
        mono.extend(struct.pack("<h", round(sample * 32767)))
    files = []
    for name, channels, data in (("stereo-routing.wav", 2, stereo),
                                 ("low-a-b-bursts.wav", 1, mono)):
        path = output / name
        with wave.open(str(path), "wb") as wav:
            wav.setnchannels(channels)
            wav.setsampwidth(2)
            wav.setframerate(RATE)
            wav.writeframes(data)
        files.append({"path": name, "sha256": digest(path), "channels": channels,
                      "sample_rate": RATE, "seconds": 8})
    write_json(output / "signals.json", {"synthetic_only": True, "files": files})
    return files


def initialize(output, plugin, build_info, environment):
    identity = json.loads(build_info.read_text(encoding="utf-8"))
    source = required(identity.get("source_sha"), "build source_sha")
    if not re.fullmatch(r"[0-9a-f]{40}", source):
        raise ValueError("build source_sha must be a full lowercase Git SHA")
    version = required(identity.get("version"), "build version")
    for key in ("obs_version", "atkaudio_version", "os_arch", "audio_path"):
        required(environment.get(key), key)
    plugin_info = fingerprint(plugin)
    output.mkdir(parents=True, exist_ok=False)
    (output / "evidence").mkdir()
    clips = signals(output / "signals")
    session = {"schema_version": 1, "scope": "obs-atkaudio-vst3-manual",
               "declared_build": {"source_sha": source, "version": version,
                                  "metadata_sha256": digest(build_info)},
               "plugin": plugin_info, "environment": environment, "signals": clips,
               "cases": [{"id": key, "result": "BLOCKED", "note": "Not executed",
                          "evidence": [], "duration_seconds": None} for key in CASES]}
    write_json(output / "session.json", session)
    return session


def evidence_file(root, relative):
    path = Path(required(relative, "evidence path"))
    # Evidence must be manually selected and placed inside this kit; no absolute paths.
    if path.is_absolute() or ".." in path.parts or not path.parts or path.parts[0] != "evidence":
        raise ValueError("evidence must be under this session's evidence/ directory")
    target = root
    for part in path.parts:
        target = target / part
        if target.is_symlink():
            raise ValueError("evidence symlinks are not allowed")
    if not target.is_file() or not target.stat().st_size:
        raise ValueError(f"missing or empty evidence: {relative}")
    return target


def validate(session, root):
    if session.get("schema_version") != 1 or session.get("scope") != "obs-atkaudio-vst3-manual":
        raise ValueError("unsupported session format")
    if not re.fullmatch(r"[0-9a-f]{40}", session["declared_build"]["source_sha"]):
        raise ValueError("invalid build source SHA")
    required(session["declared_build"].get("version"), "build version")
    for key in ("obs_version", "atkaudio_version", "os_arch", "audio_path"):
        required(session["environment"].get(key), key)
    ids = [case["id"] for case in session["cases"]]
    if sorted(ids) != sorted(CASES):
        raise ValueError("missing, unknown or duplicated test cases")
    counts = {status: 0 for status in ("PASS", "FAIL", "BLOCKED")}
    for case in session["cases"]:
        if case["result"] not in counts:
            raise ValueError("unknown result")
        required(case.get("note"), "test note")
        if case["result"] in ("PASS", "FAIL") and not case["evidence"]:
            raise ValueError(f"{case['id']}: PASS/FAIL needs local evidence")
        for item in case["evidence"]:
            if digest(evidence_file(root, item["path"])) != item["sha256"]:
                raise ValueError(f"{case['id']}: evidence hash changed")
        if case["id"] == "one_hour_recording" and case["result"] == "PASS":
            duration = case.get("duration_seconds")
            if type(duration) not in (int, float) or not math.isfinite(duration) or duration < 3600:
                raise ValueError("one_hour_recording PASS requires at least 3600 seconds")
        counts[case["result"]] += 1
    result = "FAIL" if counts["FAIL"] else ("BLOCKED" if counts["BLOCKED"] else "REVIEW_REQUIRED")
    return {"result": result, "counts": counts, "host_compatibility_certified": False,
            "note": "Checks user-entered records and file integrity only. Human host/audio review required."}


def record(session_path, case_id, result, note, evidence, duration=None):
    session = json.loads(session_path.read_text(encoding="utf-8"))
    validate(session, session_path.parent)
    entries = [{"path": item, "sha256": digest(evidence_file(session_path.parent, item))}
               for item in evidence]
    case = next(item for item in session["cases"] if item["id"] == case_id)
    case.update(result=result, note=required(note, "note"), evidence=entries, duration_seconds=duration)
    validate(session, session_path.parent)
    write_json(session_path, session)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    generate = sub.add_parser("signals", help="create synthetic routing aids in a new directory")
    generate.add_argument("--output", type=Path, required=True)
    init = sub.add_parser("init", help="fingerprint a supplied VST3 and create a BLOCKED session")
    init.add_argument("--output", type=Path, required=True)
    init.add_argument("--plugin", type=Path, required=True)
    init.add_argument("--build-info", type=Path, required=True)
    for flag in ("obs-version", "atkaudio-version", "os-arch", "audio-path"):
        init.add_argument("--" + flag, required=True)
    rec = sub.add_parser("record", help="record an executed case with manually reviewed evidence")
    rec.add_argument("--session", type=Path, required=True)
    rec.add_argument("--case", choices=CASES, required=True)
    rec.add_argument("--result", choices=("PASS", "FAIL", "BLOCKED"), required=True)
    rec.add_argument("--note", required=True)
    rec.add_argument("--evidence", action="append", default=[])
    rec.add_argument("--duration-seconds", type=float)
    report = sub.add_parser("report", help="validate local records; does not certify compatibility")
    report.add_argument("--session", type=Path, required=True)
    report.add_argument("--plugin", type=Path, help="also check that the installed VST3 has not changed")
    args = parser.parse_args()
    try:
        if args.command == "signals":
            print(json.dumps(signals(args.output), indent=2))
        elif args.command == "init":
            env = {key: getattr(args, key) for key in ("obs_version", "atkaudio_version", "os_arch", "audio_path")}
            initialize(args.output, args.plugin, args.build_info, env)
            print("Created session.json: all 13 host checks BLOCKED; no tests executed.")
        elif args.command == "record":
            record(args.session, args.case, args.result, args.note, args.evidence, args.duration_seconds)
            print("Recorded manual result; no compatibility certification granted.")
        else:
            session = json.loads(args.session.read_text(encoding="utf-8"))
            if args.plugin and fingerprint(args.plugin) != session["plugin"]:
                raise ValueError("installed plugin changed; start a new session")
            summary = validate(session, args.session.parent)
            print(json.dumps(summary, indent=2))
            return {"FAIL": 1, "BLOCKED": 2, "REVIEW_REQUIRED": 0}[summary["result"]]
    except (OSError, ValueError, KeyError, TypeError, StopIteration) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
