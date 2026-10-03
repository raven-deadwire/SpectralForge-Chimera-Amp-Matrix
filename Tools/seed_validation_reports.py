#!/usr/bin/env python3
"""Seed a complete current-commit A-stage validation set as explicitly BLOCKED.

This establishes the A1-A14 artifact topology without converting missing evidence
into PASS or NOT_APPLICABLE. Concrete CI producers may replace individual JSON
files after their checks execute.
"""
from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def load_module(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Cannot load {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


producer = load_module("validation_producer", ROOT / "Tools/produce_validation_check.py")
gate = load_module("release_gate", ROOT / "Tools/evaluate_release_gate.py")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--policy", type=Path, default=ROOT / "Validation/release-policy.json")
    parser.add_argument("--waivers", type=Path, default=ROOT / "Validation/waivers.json")
    parser.add_argument("--profile", default="pull_request")
    parser.add_argument("--commit", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    policy = json.loads(args.policy.read_text(encoding="utf-8-sig"))
    waivers = json.loads(args.waivers.read_text(encoding="utf-8-sig"))
    required_stages = policy["profiles"][args.profile]["required_stages"]
    checks_dir = args.output / "checks"
    checks_dir.mkdir(parents=True, exist_ok=True)

    reports = {}
    for stage_id in required_stages:
        stage = policy["stages"][stage_id]
        for check_id in stage["required_checks"]:
            namespace = argparse.Namespace(
                check_id=check_id,
                name=f"{stage['name']} / {check_id}",
                commit=args.commit,
                not_applicable=False,
                not_executed=True,
                exit_code=0,
                assertion=None,
                depends_on=None,
                artifact=None,
                fixture_sha256=None,
                scope_json=json.dumps({"profile": args.profile, "producer": "unclaimed-baseline"}),
                waiver_id=None,
                failure_type="ARTIFACT_MISSING",
                failure_message="No concrete producer has claimed this check for the current commit.",
            )
            report = producer.build_report(policy, namespace)
            (checks_dir / f"{check_id}.json").write_text(
                json.dumps(report, indent=2) + "\n", encoding="utf-8"
            )
            reports[check_id] = report

    result = gate.evaluate_release(policy, waivers, reports, args.profile, args.commit)
    if result["verdict"] != "BLOCKED":
        raise RuntimeError("Unclaimed validation baseline must remain BLOCKED")
    expected = sum(len(policy["stages"][stage]["required_checks"]) for stage in required_stages)
    if len(reports) != expected:
        raise RuntimeError(f"Expected {expected} reports, produced {len(reports)}")

    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "release-gate.json").write_text(
        json.dumps(result, indent=2) + "\n", encoding="utf-8"
    )
    summary = gate.render_summary(result)
    (args.output / "release-gate.md").write_text(summary, encoding="utf-8")
    print(summary, end="")
    print(f"Seeded {len(reports)} current-commit reports as BLOCKED; concrete producers must overwrite them.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
