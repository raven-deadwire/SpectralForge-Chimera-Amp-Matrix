#!/usr/bin/env python3
"""Bounded child-process comparison of actual original/patched JUCE code."""
import argparse
import json
import subprocess
import time
from pathlib import Path


def run_case(executable: Path, variant: str, scenario: str):
    started = time.monotonic()
    record = {"variant": variant, "scenario": scenario}
    try:
        result = subprocess.run([str(executable.resolve()), "--scenario", scenario], capture_output=True, text=True, timeout=2.0)
        record.update(exit_code=result.returncode, wall_ms=round((time.monotonic()-started)*1000, 3), stdout=result.stdout, stderr=result.stderr, timed_out=False)
        payloads = [line.removeprefix("RESULT ") for line in result.stdout.splitlines() if line.startswith("RESULT ")]
        if result.returncode == 0 and len(payloads) == 1:
            record["measurement"] = json.loads(payloads[0])
        record["passed"] = (variant, scenario) != ("baseline", "always-fail") and result.returncode == 0 and len(payloads) == 1 and record["measurement"]["shutdown_ms"] < 1000 and record["measurement"]["active_threads"] == 0
    except subprocess.TimeoutExpired as error:
        stdout = error.stdout or b""
        if isinstance(stdout, bytes):
            stdout = stdout.decode("utf-8", errors="replace")
        record.update(timed_out=True, wall_ms=round((time.monotonic()-started)*1000, 3), stdout=stdout)
        record["passed"] = (variant, scenario) == ("baseline", "always-fail") and "STAGE destruct scenario=always-fail" in stdout
    return record


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--patched", type=Path, required=True)
    parser.add_argument("--extraction-report", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    cases = []
    for variant, executable in (("baseline", args.baseline), ("patched", args.patched)):
        for scenario in ("always-fail", "success", "mixed"):
            case = run_case(executable, variant, scenario)
            cases.append(case)
            outcome = "expected destructor hang" if case["timed_out"] and case["passed"] else f"shutdown {case.get('measurement', {}).get('shutdown_ms', 'unavailable')} ms"
            print(f"{'PASS' if case['passed'] else 'FAIL'} {variant}/{scenario}: {outcome}", flush=True)
            if not case["passed"]:
                print(case.get("stdout", "") + case.get("stderr", ""), flush=True)
    report = {"success": all(case["passed"] for case in cases), "scope": "Exact JUCE VBlankThread class bodies with fake Thread/AsyncUpdater/IDXGIOutput adapters. This isolates the persistent WaitForVBlank failure shutdown loop; it is not a real GPU, Windows driver or Studio One reproduction.", "deadline_ms": 1000, "parent_timeout_ms": 2000, "extraction": json.loads(args.extraction_report.read_text(encoding="utf-8")), "cases": cases}
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    raise SystemExit(0 if report["success"] else 1)


if __name__ == "__main__":
    main()
