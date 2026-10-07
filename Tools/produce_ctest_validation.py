#!/usr/bin/env python3
"""Promote only executed, passing CTest cases from the current CI artifact.

All unowned A-stage, calibration, actual DI and DAW checks stay BLOCKED.
The platform matrix check requires every configured test, including appended
integration regressions, to appear and execute successfully in JUnit.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "Tools"))
import produce_validation_check as producer
import evaluate_release_gate as gate

def test_results(junit: Path, inventory: Path) -> tuple[dict[str, bool], bool]:
    names = [test["name"] for test in gate.load_json(inventory)["tests"]]
    if any(not isinstance(name, str) or not name for name in names) or len(names) != len(set(names)):
        raise ValueError("Invalid or duplicate configured CTest name")
    expected = set(names)
    if not expected:
        raise ValueError("Empty CTest inventory cannot certify a matrix")
    cases: dict[str, bool] = {}
    for case in ET.parse(junit).getroot().iter("testcase"):
        name = case.attrib["name"]
        if name in cases:
            raise ValueError(f"Duplicate CTest case {name}")
        cases[name] = (case.attrib.get("status") == "run"
                       and case.find("failure") is None and case.find("error") is None
                       and case.find("skipped") is None)
    return cases, set(cases) == expected and all(cases.values())

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--commit", required=True)
    parser.add_argument("--platform", choices=["windows", "linux", "macos"], required=True)
    parser.add_argument("--junit", type=Path, required=True)
    parser.add_argument("--inventory", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    actual = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    if actual != args.commit:
        raise ValueError("Producer commit must be the checked-out HEAD")
    subprocess.run([sys.executable, str(ROOT / "Tools/seed_validation_reports.py"),
                    "--commit", args.commit, "--output", str(args.output)], check=True, stdout=subprocess.DEVNULL)
    policy = json.loads((ROOT / "Validation/release-policy.json").read_text())
    mapping = json.loads((ROOT / "Validation/ci-check-map.json").read_text())
    if mapping["policy_version"] != policy["policy_version"]:
        raise ValueError("Stale CI ownership map")
    # Absent artifacts leave the seeded checks blocked, never an empty PASS.
    if not args.junit.is_file() or not args.inventory.is_file():
        print("BLOCKED CTest artifact missing; all seeded checks remain unexecuted")
        return 0
    cases, matrix_ok = test_results(args.junit, args.inventory)
    artifact_hash = hashlib.sha256(args.junit.read_bytes()).hexdigest()
    for cid, spec in mapping["checks"].items():
        if spec["platform"] not in ("all", args.platform):
            continue
        name = spec["producer"]
        if name.startswith("ctest:"):
            test = name.partition(":")[2]
            if test not in cases:
                continue
            passed = cases[test]
        elif name == "workflow:build/" + args.platform:
            passed = matrix_ok
        else:
            continue
        report_args = SimpleNamespace(check_id=cid, name=spec["name"], commit=args.commit,
            not_applicable=False, not_executed=False, exit_code=0 if passed else 1,
            assertion="pass" if passed else "fail", depends_on=None,
            artifact=[str(args.junit)], fixture_sha256=None,
            scope_json=json.dumps({"platform": args.platform, "producer": name,
                                   "junit_sha256": artifact_hash, "synthetic_only": True}),
            waiver_id=None, failure_type=None if passed else "TEST_FAILURE",
            failure_message=None if passed else "CTest failed, skipped or matrix inventory incomplete")
        report = producer.build_report(policy, report_args)
        (args.output / "checks" / (cid + ".json")).write_text(json.dumps(report, indent=2) + "\n")
    reports = {p.stem: json.loads(p.read_text()) for p in (args.output / "checks").glob("*.json")}
    result = gate.evaluate_release(policy, json.loads((ROOT / "Validation/waivers.json").read_text()), reports, "pull_request", args.commit)
    (args.output / "release-gate.json").write_text(json.dumps(result, indent=2) + "\n")
    (args.output / "release-gate.md").write_text(gate.render_summary(result))
    print(f"Produced concrete {args.platform} CTest evidence; release verdict={result['verdict']}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
