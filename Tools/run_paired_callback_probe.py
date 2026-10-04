#!/usr/bin/env python3
"""Run baseline and candidate UI probes in alternating process order."""
import argparse
import hashlib
import json
import subprocess
from datetime import datetime, timezone
from pathlib import Path


def _sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _run(binary, report, revision, timeout):
    subprocess.run(
        [str(binary), str(report), revision, "1"],
        check=True,
        timeout=timeout,
    )
    value = json.loads(report.read_text(encoding="utf-8"))
    if value.get("source_revision") != revision:
        raise ValueError(f"Probe revision mismatch: {report}")
    return value


def _append(aggregate, report, pair, order, position):
    if aggregate is None:
        aggregate = {key: value for key, value in report.items() if key != "rows"}
        aggregate["rows"] = []
    else:
        for key in ("os", "cpu", "juce", "sample_rate", "block_size", "fixture", "scope"):
            if aggregate.get(key) != report.get(key):
                raise ValueError(f"Probe condition changed within run: {key}")
    for row in report["rows"]:
        copied = dict(row)
        copied.update(pair=pair, execution_order=order, execution_position=position)
        aggregate["rows"].append(copied)
    return aggregate


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("baseline_binary", type=Path)
    parser.add_argument("candidate_binary", type=Path)
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("baseline_revision")
    parser.add_argument("candidate_revision")
    parser.add_argument("--pairs", type=int, default=5)
    parser.add_argument("--timeout-seconds", type=int, default=900)
    args = parser.parse_args()
    if args.pairs < 5:
        raise SystemExit("At least five pairs are required")
    for binary in (args.baseline_binary, args.candidate_binary):
        if not binary.is_file():
            raise SystemExit(f"Missing probe binary: {binary}")

    args.output_dir.mkdir(parents=True, exist_ok=True)
    started = datetime.now(timezone.utc).isoformat()
    aggregate = {"baseline": None, "candidate": None}
    binaries = {
        "baseline": (args.baseline_binary, args.baseline_revision),
        "candidate": (args.candidate_binary, args.candidate_revision),
    }
    orders = []
    for pair in range(args.pairs):
        order = ["baseline", "candidate"] if pair % 2 == 0 else ["candidate", "baseline"]
        orders.append(order)
        for position, label in enumerate(order):
            binary, revision = binaries[label]
            raw = args.output_dir / f"{label}-pair-{pair + 1}.json"
            report = _run(binary.resolve(), raw.resolve(), revision, args.timeout_seconds)
            aggregate[label] = _append(aggregate[label], report, pair + 1, "-".join(order), position + 1)

    for label, report in aggregate.items():
        (args.output_dir / f"{label}.json").write_text(
            json.dumps(report, indent=2) + "\n", encoding="utf-8"
        )
    protocol = {
        "schema": "spectralforge.chimera.callback-paired-run",
        "schema_version": 1,
        "started_utc": started,
        "finished_utc": datetime.now(timezone.utc).isoformat(),
        "pairs": args.pairs,
        "orders": orders,
        "baseline": {
            "revision": args.baseline_revision,
            "binary_sha256": _sha256(args.baseline_binary),
        },
        "candidate": {
            "revision": args.candidate_revision,
            "binary_sha256": _sha256(args.candidate_binary),
        },
        "release_gate_status": "BLOCKED",
        "scope": "Alternating synthetic desktop-peer probe; not a DAW, driver, or workstation soak.",
    }
    (args.output_dir / "protocol.json").write_text(
        json.dumps(protocol, indent=2) + "\n", encoding="utf-8"
    )


if __name__ == "__main__":
    main()
