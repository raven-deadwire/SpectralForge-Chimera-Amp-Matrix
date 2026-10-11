#!/usr/bin/env python3
"""Bounded, diagnostic-only ABBA investigation of frozen macOS CAB timings.

The ordinary CTest gate runs unchanged first. Each later run uses a separate
directory; failed tests and every raw row are retained. Higher caller QoS is
an experimental condition, never a replacement acceptance condition.
"""
from __future__ import annotations

import argparse
import csv
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import platform
import shutil
import sys
import traceback
import xml.etree.ElementTree as ET

sys.dont_write_bytecode = True

FROZEN_SHA = "7b20cd9d1f90e3e2969c250712b199e893cf50cd"
JUCE_SHA = "d6181bde38d858c283c3b7bf699ce6340c050b5d"
TESTS = ("ChimeraOriginalCabIntegrationTests", "ChimeraOriginalCabRealtimeTests",
         "ChimeraCabExpansionIntegrationTests", "ChimeraCabExtendedTimingTests",
         "ChimeraCabLayoutIntegrationTests")
SUITES = (("original", []), ("expanded", ["--expanded"]),
          ("layouts", ["--layouts"]),
          ("extended", ["--expanded", "--extended-buffers"]))
ORDER = (("01-A", "inherited"), ("02-B", "user-interactive"),
         ("03-B", "user-interactive"), ("04-A", "inherited"))


def write_json(path, obj):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(obj, indent=2, allow_nan=False) + "\n")


def sha256(path):
    h = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def expected_routes(sentinel=False):
    if sentinel:
        return {("scheduler-sentinel-" + suite, "sentinel-" + engine, sr, block, channels)
                for suite, engine, sr, block, channels in expected_routes()}
    result = {(suite, engine, sr, block, channels)
              for suite, engine, blocks in
              (("integration", "original-v1", (64, 256)),
               ("integration", "expanded-v2", (64, 256)),
               ("integration", "array-v3-6x10", (64, 256)),
               ("extended", "expanded-v2", (32, 64, 128, 256, 512)))
              for sr in (44100, 48000, 96000) for block in blocks for channels in (1, 2)}
    return result


def summarize(root, *, sentinel=False, diagnostic=True):
    """Recompute unchanged p99 and misses; never discard invalid/missing rows."""
    routes, misses, observed = [], [], set()
    for path in sorted(root.rglob("*.csv")):
        with path.open(newline="") as f:
            rows = list(csv.DictReader(f))
        if len(rows) != 1600:
            raise ValueError(f"{path}: expected 1600 raw rows, got {len(rows)}")
        first = rows[0]
        key = (first["suite"], first["engine"], int(first["sample_rate"]),
               int(first["block_size"]), int(first["channels"]))
        if key in observed:
            raise ValueError(f"Duplicate raw population {key}")
        observed.add(key)
        budget = key[3] * 1e6 / key[2]
        durations = []
        case_misses = []
        for i, row in enumerate(rows):
            actual = (row["suite"], row["engine"], int(row["sample_rate"]),
                      int(row["block_size"]), int(row["channels"]))
            if actual != key or int(row["callback_index"]) != i:
                raise ValueError(f"{path}: identity/index mismatch at {i}")
            wall, cpu = float(row["wall_us"]), float(row["thread_cpu_us"])
            if not (math.isfinite(wall) and wall >= 0 and math.isfinite(cpu)):
                raise ValueError(f"{path}: non-finite/negative clock at {i}")
            if not math.isclose(float(row["budget_us"]), budget, rel_tol=1e-12):
                raise ValueError(f"{path}: changed deadline at {i}")
            if int(row["deadline_miss"]) != int(wall > budget):
                raise ValueError(f"{path}: incorrect deadline miss at {i}")
            phase = (("paced_control" if i < 1200 else "burst_control") if sentinel else
                     ("worker_and_automation" if i < 1200 else "forced_six_slot_publication"))
            requested = 0 if sentinel else int(i < 80)
            if row["phase"] != phase or int(row["automation_requested"]) != requested:
                raise ValueError(f"{path}: incorrect phase/automation at {i}")
            if int(row["phase_index"]) != (i if i < 1200 else i - 1200):
                raise ValueError(f"{path}: incorrect phase index at {i}")
            if sentinel and (row["workload"] != "arithmetic_only" or row["worker_running"] != "0"):
                raise ValueError(f"{path}: incorrect sentinel workload at {i}")
            if row["thread_cpu_valid"] != "1" or cpu < 0:
                raise ValueError(f"{path}: invalid native CPU clock at {i}")
            if any(int(row[k]) for k in ("clock_start_error", "clock_end_error")):
                raise ValueError(f"{path}: native clock query error at {i}")
            start_cpu, end_cpu = float(row["thread_cpu_start_us"]), float(row["thread_cpu_end_us"])
            if not (math.isfinite(start_cpu) and math.isfinite(end_cpu) and 0 <= start_cpu <= end_cpu):
                raise ValueError(f"{path}: invalid raw CPU endpoints at {i}")
            if not math.isclose(cpu, end_cpu - start_cpu, rel_tol=1e-12, abs_tol=1e-9):
                raise ValueError(f"{path}: paired CPU delta mismatch at {i}")
            durations.append(wall)
            if wall > budget:
                case_misses.append(dict(row, csv_path=str(path.relative_to(root))))
        ordered = sorted(durations)
        maximum = rows[durations.index(max(durations))]
        routes.append({"key": key, "samples": len(rows), "budget_us": budget,
                       "p50_us": ordered[800], "p99_us": ordered[1584],
                       "p99_pass": ordered[1584] < budget,
                       "max_wall_us": ordered[-1], "max_callback_index": int(maximum["callback_index"]),
                       "max_paired_cpu_us": float(maximum["thread_cpu_us"]),
                       "deadline_misses": len(case_misses),
                       "cpu_over_budget_misses": sum(float(r["thread_cpu_us"]) > budget for r in case_misses),
                       "csv_path": str(path.relative_to(root)), "sha256": sha256(path)})
        misses.extend(case_misses)
    expected = expected_routes(sentinel)
    if observed != expected:
        raise ValueError(f"Unexpected population grid: missing={expected-observed}; extra={observed-expected}")
    return {"raw_root": str(root), "diagnostic_only": diagnostic,
            "routes": routes, "route_count": len(routes),
            "callbacks": sum(r["samples"] for r in routes),
            "p99_pass_count": sum(r["p99_pass"] for r in routes),
            "deadline_misses": len(misses), "max_wall_us": max(r["max_wall_us"] for r in routes),
            "misses_with_cpu_over_budget": sum(r["cpu_over_budget_misses"] for r in routes),
            "all_misses": misses}


def load_frozen_driver(source):
    spec = importlib.util.spec_from_file_location("frozen_cab_driver", source / "Tools/run_cab_realtime_ci.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def run_logged(driver, *args, **kwargs):
    started = datetime.now(timezone.utc).isoformat()
    receipt = driver.run_logged(*args, **kwargs)
    return dict(receipt, started_utc=started, finished_utc=datetime.now(timezone.utc).isoformat())


def scheduling_receipts(log):
    records = []
    for line in log.read_text().splitlines():
        if line.startswith(("SCHEDULING_DIAGNOSTIC_CONTEXT ", "SCHEDULING_DIAGNOSTIC_QOS ",
                            "SCHEDULING_DIAGNOSTIC_RESULT ")):
            records.append(line)
    return records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source, build, output = [p.resolve() for p in (args.source, args.build, args.output)]
    controller = Path(__file__).resolve().parent
    if platform.system() != "Darwin":
        raise RuntimeError("Native scheduling experiment requires macOS; Linux is not macOS evidence")
    if output.exists() or build.exists():
        raise RuntimeError("Output or build already exists; preserve earlier runs and use fresh directories")
    output.mkdir(parents=True)
    driver = load_frozen_driver(source)
    report = {"schema_version": 1, "diagnostic_only": True, "product_fix_demonstrated": False,
              "frozen_product_sha": FROZEN_SHA, "planned_order": ORDER,
              "planned_scope": "one unchanged ordinary CTest gate; 4 CAB and 4 arithmetic-only populations",
              "commands": [], "trials": [], "complete": False,
              "limitations": ["No CoreAudio device, DAW, editor, or real-time thread policy is created.",
                              "The caller QoS intervention also covers setup and sleeps; it is not an audio product fix.",
                              "A hosted guest CPU clock cannot identify every host/guest scheduling cause.",
                              "Higher-QoS diagnostics never replace the ordinary test gate.",
                              "No rerun-until-pass, discarded warmup population, or changed deadline."]}
    failure = None
    try:
        report["source_before"] = driver.source_snapshot(source, FROZEN_SHA)
        report["environment_before"] = driver.environment_snapshot(output / "before")
        report["controller"] = {p.name: sha256(p) for p in controller.iterdir() if p.is_file()}
        configure = ["cmake", "-S", controller, "-B", build, "-DCMAKE_BUILD_TYPE=Release",
                     f"-DCHIMERA_FROZEN_SOURCE={source}", f"-DCHIMERA_FROZEN_SHA={FROZEN_SHA}",
                     "-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0",
                     f"-DCMAKE_OSX_ARCHITECTURES={platform.machine()}", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"]
        for cmd, log in ((configure, "configure.log"),
                         (["cmake", "--build", build, "--config", "Release", "--target",
                           "ChimeraOriginalCabIntegrationTests", "ChimeraOriginalCabRealtimeTests",
                           "ChimeraCabSchedulingProbe", "--parallel", "2"], "build.log")):
            record = run_logged(driver, cmd, controller, output / log)
            report["commands"].append(record)
            if record["returncode"]:
                raise RuntimeError(f"{log} failed; original log retained")
        product_build = build / "frozen-product"
        probe = build / "ChimeraCabSchedulingProbe_artefacts/Release/ChimeraCabSchedulingProbe"
        report["probe_sha256"] = sha256(probe)
        report["product_binaries"] = {name: sha256(product_build / f"{name}_artefacts/Release/{name}")
                                      for name in ("ChimeraOriginalCabIntegrationTests", "ChimeraOriginalCabRealtimeTests")}
        juce = build / "_deps/juce-src"
        report["juce_sha"] = driver.capture(["git", "rev-parse", "HEAD"], juce)
        if report["juce_sha"] != JUCE_SHA:
            raise RuntimeError("Fetched JUCE tag moved; re-audit dependency before measuring")
        metadata = output / "build-metadata"
        metadata.mkdir()
        for p in [build / "CMakeCache.txt", build / "compile_commands.json", *build.glob("CMakeFiles/*/CMakeCXXCompiler.cmake")]:
            if p.is_file():
                shutil.copy2(p, metadata / p.name)
        normal = output / "ordinary-gate"
        normal.mkdir()
        regex = "^(" + "|".join(TESTS) + ")$"
        inventory = driver.capture(["ctest", "--test-dir", product_build, "-C", "Release", "-R", regex, "--show-only=json-v1"])
        inventory_data = json.loads(inventory)
        write_json(normal / "ctest-inventory.json", inventory_data)
        if sorted(t["name"] for t in inventory_data["tests"]) != sorted(TESTS):
            raise RuntimeError("Ordinary CTest inventory differs from the frozen five-test selection")
        command = ["ctest", "--test-dir", product_build, "-C", "Release", "-R", regex,
                   "--parallel", "1", "--verbose", "--no-tests=error", "--timeout", "600",
                   "--test-output-size-passed", "10485760", "--test-output-size-failed", "10485760",
                   "--output-junit", normal / "ctest-results.xml"]
        report["ordinary_gate"] = run_logged(driver, command, source, normal / "ctest-console.log", measured=True)
        junit = ET.parse(normal / "ctest-results.xml").getroot()
        cases = list(junit.iter("testcase"))
        if sorted(t.attrib["name"] for t in cases) != sorted(TESTS):
            raise RuntimeError("Ordinary JUnit result inventory is incomplete or duplicated")
        report["ordinary_gate"]["tests"] = [{"name": t.attrib["name"],
            "passed": t.attrib.get("status") == "run" and t.find("failure") is None
                      and t.find("error") is None and t.find("skipped") is None,
            "seconds": float(t.attrib.get("time", "0"))} for t in cases]
        if any("This part of the test output was removed" in t.findtext("system-out", "") for t in cases):
            raise RuntimeError("Ordinary CTest output was truncated; all available output retained")
        shutil.copytree(product_build / "cab-timing-diagnostics", normal / "raw")
        shutil.copy2(product_build / "Testing/Temporary/LastTest.log", normal / "LastTest.log")
        report["ordinary_gate"]["summary"] = summarize(normal / "raw", diagnostic=False)
        write_json(normal / "summary.json", report["ordinary_gate"]["summary"])
        for label, qos in ORDER:
            trial = output / "trials" / label
            trial.mkdir(parents=True)
            entry = {"label": label, "requested_caller_qos": qos, "diagnostic_only": True, "commands": []}
            report["trials"].append(entry)
            for name, flags in SUITES:
                command = [probe, "--caller-qos", qos, *flags]
                receipt = run_logged(driver, command, trial, trial / f"{name}.log", timeout=600, measured=True)
                receipt["scheduling"] = scheduling_receipts(trial / f"{name}.log")
                entry["commands"].append(receipt)
            entry["cab_summary"] = summarize(trial / "cab-timing-diagnostics")
            write_json(trial / "cab-summary.json", entry["cab_summary"])
            sentinel = trial / "sentinel"
            sentinel.mkdir()
            entry["sentinel_commands"] = []
            for name, flags in SUITES:
                receipt = run_logged(driver,
                    [probe, "--caller-qos", qos, "--scheduler-sentinel", *flags],
                    sentinel, sentinel / f"{name}.log", timeout=600, measured=True)
                receipt["scheduling"] = scheduling_receipts(sentinel / f"{name}.log")
                entry["sentinel_commands"].append(receipt)
            entry["sentinel_summary"] = summarize(sentinel / "cab-timing-diagnostics", sentinel=True)
            write_json(sentinel / "summary.json", entry["sentinel_summary"])
            write_json(output / "report.partial.json", report)
        report["source_after"] = driver.source_snapshot(source, FROZEN_SHA)
        report["environment_after"] = driver.environment_snapshot(output / "after")
        report["complete"] = True
    except BaseException as error:
        failure = error
        report["error"] = repr(error)
        (output / "driver-error.log").write_text(traceback.format_exc())
    finally:
        write_json(output / "report.json", report)
        write_json(output / "SHA256SUMS.json", {str(p.relative_to(output)): sha256(p)
                                               for p in sorted(output.rglob("*")) if p.is_file()
                                               and p.name != "SHA256SUMS.json"})
    if failure:
        raise failure
    # All failures remain failures even when later conditions pass.
    codes = [report["ordinary_gate"]["returncode"]]
    codes.append(0 if all(t["passed"] for t in report["ordinary_gate"]["tests"]) else 1)
    for trial in report["trials"]:
        codes += [x["returncode"] for x in trial["commands"]]
        codes += [x["returncode"] for x in trial["sentinel_commands"]]
    return 0 if all(x == 0 for x in codes) else 1


if __name__ == "__main__":
    sys.exit(main())
