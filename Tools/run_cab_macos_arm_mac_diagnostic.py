#!/usr/bin/env python3
"""Bounded native macOS ARM spectral MAC diagnostic; never replaces PR42 acceptance.

A is untouched 8097. B has only the root-reviewed ARM spectral MAC source and
independent reference-test patch. Both retain low worker priority and the existing
1600 callback / 80 automation / 1200 worker / 400 forced-publication protocol.
No caller priority, process priority, affinity, power setting, fade, load,
deadline or tolerance is changed. One ABBA, then one extended profile per variant.
The exact two-file patch and source SHA values are pinned below.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

sys.dont_write_bytecode = True
BASE_SHA = "8097ec69594ff728164c77f59e9ea1e39e384643"
BASE_TREE = "f12b4312f559e86e07c37899691b62858f4a4c3c"
JUCE_SHA = "d6181bde38d858c283c3b7bf699ce6340c050b5d"
BRANCH = "refs/heads/diagnostic/cab-macos-arm-mac-20261011"
BUFFERS = (32, 64, 128, 256, 512)
COMMAND_TIMEOUT = 600
PRESERVATION_SECONDS = 300
command_deadline = None
INTEGRATION = "ChimeraOriginalCabIntegrationTests"
PROFILE = "ChimeraCabProfiling"
GUARDS = ("ChimeraIRLibraryTests", "ChimeraOriginalCabRealtimeTests",
          "ChimeraOriginalCabModelTests", "ChimeraCabExpansionModelTests")
TARGETS = (INTEGRATION, PROFILE, *GUARDS)
ORDER = (("01-A-reference", "reference"), ("02-B-simd", "simd"),
         ("03-B-simd", "simd"), ("04-A-reference", "reference"))
OVERLAY_PATHS = ("Source/ModeledCabConvolution.h", "Tests/CabRealtimeRegressionTests.cpp")
OVERLAY_FILE = "Tools/cab_macos_arm_mac_candidate.diff"
OVERLAY_SHA256 = "4d50d84b4148e6b9080fd68384179fcf961f5d011d56286f6214a72f2d7e2e47"
OVERLAY_RESULT_SHA256 = {
    "Source/ModeledCabConvolution.h": "c99ab84e66f1a7cb0e0496f8cdf3d0942a08bdd8d74e0bddc9c35948ae5a4c0a",
    "Tests/CabRealtimeRegressionTests.cpp": "8941f040e59ca5259ddf4f3212589e0bdd371db5ff92e199604fb32878702238",
}
ARM_REFERENCE = {
    "geometries": "60", "partitions": "73", "float_comparisons": "1705864",
    "exact_bits": "PASS", "input_immutable": "PASS", "bounds_guards": "PASS",
}

CSV_FIELDS = (
    "schema_version", "engine", "suite", "sample_rate", "block_size", "channels",
    "callback_index", "phase", "phase_index", "automation_requested", "wall_us",
    "budget_us", "deadline_miss", "thread_cpu_us", "thread_cpu_valid", "thread_cycles",
    "thread_cycles_valid", "clock_start_error", "clock_end_error",
    "thread_cpu_start_us", "thread_cpu_end_us", "thread_cycles_start", "thread_cycles_end")


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def load_driver(source):
    spec = importlib.util.spec_from_file_location("pinned_cab_driver", source / "Tools/run_cab_realtime_ci.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def bounded_run(v, command, cwd, log):
    """Keep each command's 600 seconds intact; never truncate it to fit."""
    remaining = command_deadline - time.monotonic() if command_deadline is not None else float("inf")
    if remaining < COMMAND_TIMEOUT:
        reason = f"Not started: {remaining:.1f}s remain before preservation, below the unchanged 600s command allowance"
        log.parent.mkdir(parents=True, exist_ok=True)
        log.write_text(reason + "\n", encoding="utf-8")
        return {"command": [str(arg) for arg in command], "cwd": str(cwd), "returncode": 125,
                "not_started": True, "reason": reason, "seconds": 0, "log": str(log)}
    return v.run_logged(command, cwd, log, timeout=COMMAND_TIMEOUT)


def tracked_files(v, source):
    result = []
    for name in v.capture(["git", "ls-files", "-z"], source).split("\0"):
        if name:
            path = source / name
            if path.is_symlink():
                data = os.readlink(path).encode("utf-8")
                result.append({"path": name, "symlink": True, "bytes": len(data),
                               "sha256": hashlib.sha256(data).hexdigest()})
            else:
                require(path.is_file(), f"Missing source entry: {name}")
                result.append({"path": name, "bytes": path.stat().st_size, "sha256": v.digest(path)})
    return result


def snapshot(v, source, expected_files, expected_changed):
    require(v.capture(["git", "rev-parse", "HEAD"], source) == BASE_SHA, "Variant base SHA changed")
    require(v.capture(["git", "rev-parse", "HEAD^{tree}"], source) == BASE_TREE, "Variant base tree changed")
    require(not v.capture(["git", "ls-files", "--others", "--exclude-standard"], source),
            "Unexpected untracked variant source")
    changed = set(v.capture(["git", "diff", "HEAD", "--name-only"], source).splitlines())
    require(changed == set(expected_changed), f"Unexpected source overlay: {sorted(changed)}")
    files = tracked_files(v, source)
    require(files == expected_files, "Variant source bytes changed")
    return {"base_sha": BASE_SHA, "base_tree": BASE_TREE, "path": str(source),
            "clean": not changed, "overlay_paths": sorted(changed), "files": files}


def prepare_sources(v, sources, output):
    for source in sources.values():
        v.source_snapshot(source, BASE_SHA)
    original = tracked_files(v, sources["reference"])
    require(original == tracked_files(v, sources["simd"]), "A/B source bytes differ before overlay")
    patch = Path(__file__).resolve().parent.parent / OVERLAY_FILE
    require(len(OVERLAY_SHA256) == 64 and all(c in "0123456789abcdef" for c in OVERLAY_SHA256),
            "Invalid root-reviewed two-file ARM patch SHA")
    require(patch.is_file() and v.digest(patch) == OVERLAY_SHA256, "Missing/changed root-reviewed ARM patch")
    shutil.copyfile(patch, output / "input-simd-overlay.diff")
    v.capture(["git", "apply", "--check", "--binary", str(patch)], sources["simd"])
    v.capture(["git", "apply", "--binary", str(patch)], sources["simd"])
    changed = set(v.capture(["git", "diff", "HEAD", "--name-only"], sources["simd"]).splitlines())
    require(changed == set(OVERLAY_PATHS), f"Unexpected ARM patch paths: {sorted(changed)}")
    changes = []
    for name in OVERLAY_PATHS:
        old, new = ((sources[label] / name).read_bytes() for label in ("reference", "simd"))
        require(old != new, f"Expected ARM patch did not change {name}")
        require(v.digest(sources["simd"] / name) == OVERLAY_RESULT_SHA256[name],
                f"Applied ARM source differs from the root-reviewed bytes: {name}")
        for label, data in (("original", old), ("simd", new)):
            saved = output / "overlay" / label / name
            saved.parent.mkdir(parents=True, exist_ok=True)
            saved.write_bytes(data)
        changes.append({"path": name, "original_sha256": v.digest(sources["reference"] / name),
                        "simd_sha256": v.digest(sources["simd"] / name)})
    # Preserve the exact input patch and the independently generated applied diff.
    diff = subprocess.check_output(["git", "diff", "--binary", "HEAD", "--", *OVERLAY_PATHS],
                                   cwd=sources["simd"], timeout=60)
    (output / "applied-simd-overlay.diff").write_bytes(diff)
    v.write_json(output / "overlay.json", {"base_sha": BASE_SHA, "base_tree": BASE_TREE,
        "b_is_uncommitted_overlay_not_the_base_tree": True, "files": changes,
        "input_patch_sha256": OVERLAY_SHA256,
        "applied_diff_sha256": v.digest(output / "applied-simd-overlay.diff"),
        "timing_gate_unchanged": True, "worker_priority_unchanged": True, "caller_priority_unchanged": True})
    expected = {"reference": original, "simd": tracked_files(v, sources["simd"])}
    ref_map = {row["path"]: row for row in original}
    simd_map = {row["path"]: row for row in expected["simd"]}
    require(ref_map.keys() == simd_map.keys() and {name for name in ref_map if ref_map[name] != simd_map[name]}
            == set(OVERLAY_PATHS), "ARM patch changed unexpected source bytes")
    for label, source in sources.items():
        state = snapshot(v, source, expected[label], () if label == "reference" else OVERLAY_PATHS)
        v.write_json(output / label / "source-before.json", state)
    return expected


def binary(v, build, target):
    if target in ("ChimeraOriginalCabModelTests", "ChimeraCabExpansionModelTests"):
        path = build / target
        require(path.is_file(), f"Missing model test executable: {path}")
        return path
    return v.executable(build, target)


def build_variant(v, source, build, output):
    receipt = {"complete": False, "commands": [], "binaries": []}
    try:
        for command, log in (
            (["cmake", "-S", source, "-B", build, "-DCMAKE_BUILD_TYPE=Release",
              "-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0", "-DCMAKE_OSX_ARCHITECTURES=arm64"], "configure.log"),
            (["cmake", "--build", build, "--config", "Release", "--target", *TARGETS,
              "--parallel", "2"], "build.log")):
            record = bounded_run(v, command, source, output / log)
            receipt["commands"].append(record)
            require(record["returncode"] == 0, f"{log} failed")
        v.preserve_build_metadata(build, output)
        juce = build / "_deps/juce-src"
        require(v.capture(["git", "rev-parse", "HEAD"], juce) == JUCE_SHA, "Unexpected JUCE source")
        juce_diff = v.capture(["git", "diff", "HEAD", "--binary"], juce)
        (output / "juce-source.diff").write_text(juce_diff + "\n", encoding="utf-8")
        v.write_json(output / "juce-source-files.json", tracked_files(v, juce))
        for target in TARGETS:
            path = binary(v, build, target)
            architecture = v.capture(["lipo", "-archs", str(path)])
            require(architecture == "arm64", f"Non-native executable: {target}: {architecture}")
            receipt["binaries"].append({"target": target, "path": str(path),
                "sha256": v.digest(path), "bytes": path.stat().st_size, "architecture": architecture})
        receipt["complete"] = True
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        receipt["error"] = str(error)
    finally:
        try:
            v.preserve_build_metadata(build, output)
        except (OSError, RuntimeError, subprocess.SubprocessError) as error:
            receipt["metadata_error"] = str(error)
        v.write_json(output / "build-receipt.json", receipt)
    return receipt


def correctness(v, build, output, label):
    receipt = {"variant": label, "complete": False, "passed": False}
    regex = "^(" + "|".join(GUARDS) + ")$"
    try:
        inventory = json.loads(v.capture(["ctest", "--test-dir", str(build), "-C", "Release",
                                         "-R", regex, "--show-only=json-v1"]))
        v.write_json(output / "ctest-inventory.json", inventory)
        require({t["name"] for t in inventory["tests"]} == set(GUARDS)
                and len(inventory["tests"]) == len(GUARDS), "Guard CTest inventory changed")
        receipt["command"] = bounded_run(v,
            ["ctest", "--test-dir", build, "-C", "Release", "-R", regex, "--parallel", "1",
             "--verbose", "--no-tests=error", "--timeout", "600",
             "--test-output-size-passed", "10485760", "--test-output-size-failed", "10485760",
             "--output-junit", output / "ctest-results.xml"], build, output / "ctest-console.log")
        require(not receipt["command"].get("not_started"), receipt["command"].get("reason", ""))
        last = build / "Testing/Temporary/LastTest.log"
        if last.is_file():
            shutil.copyfile(last, output / "LastTest.log")
        tests = ET.parse(output / "ctest-results.xml").getroot().findall("testcase")
        require(len(tests) == len(GUARDS) and {t.get("name") for t in tests} == set(GUARDS),
                "Missing or duplicate correctness results")
        receipt["tests"] = [{"name": t.get("name"), "seconds": float(t.get("time", "0")),
            "passed": t.get("status") == "run" and t.find("failure") is None and t.find("skipped") is None}
            for t in tests]
        texts = {t.get("name"): t.findtext("system-out", "") for t in tests}
        require(all("This part of the test output was removed" not in text for text in texts.values()),
                "Truncated correctness output")
        receipt["lifecycle_records"] = [dict(v.ROW.findall(line))
            for line in texts["ChimeraIRLibraryTests"].splitlines()
            if line.startswith("PASS modeled_worker_cancellation ")]
        # The existing test owns its 4000/10000 iteration limits and diagnostic
        # stop timer. A missing record remains a visible guard failure.
        lifecycle = receipt["lifecycle_records"]
        receipt["guard_evidence"] = {
            "lifecycle": len(lifecycle) == 1
                and lifecycle[0].get("latest_six_mics") == "converged"
                and lifecycle[0].get("reprepare") == "synchronous"
                and lifecycle[0].get("resources") == "released"
                and int(lifecycle[0].get("cancelled_builds", "0")) > 0
                and math.isfinite(float(lifecycle[0].get("stop_us", "nan")))
                and float(lifecycle[0].get("stop_us", "-1")) >= 0,
            "v1_bits": "cone_quadrature_reference_v1=864 geometry_doubles=144 exact_double_bits"
                in texts["ChimeraOriginalCabModelTests"],
            "v2_v3_bits": "cone_quadrature_reference_v2_v3=2592 exact_double_bits"
                in texts["ChimeraCabExpansionModelTests"],
            "oracle": "equivalence routes=144 " in texts["ChimeraOriginalCabRealtimeTests"]
                and "six_mic_routes=8 " in texts["ChimeraOriginalCabRealtimeTests"]}
        arm_reference = [dict(v.ROW.findall(line))
            for line in texts["ChimeraOriginalCabRealtimeTests"].splitlines()
            if line.startswith("ARM_SPECTRAL_MAC_REFERENCE ")]
        receipt["arm_spectral_mac_reference_records"] = arm_reference
        if label == "simd":
            receipt["guard_evidence"]["arm_spectral_mac_reference"] = (
                len(arm_reference) == 1 and arm_reference[0] == ARM_REFERENCE)
        else:
            require(not arm_reference, "Unexpected ARM overlay reference in untouched A")
        receipt["complete"] = True
        receipt["passed"] = (receipt["command"]["returncode"] == 0
            and all(t["passed"] for t in receipt["tests"]) and all(receipt["guard_evidence"].values()))
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError, ET.ParseError) as error:
        receipt["error"] = str(error)
    v.write_json(output / "receipt.json", receipt)
    return receipt


def same(a, b):
    return math.isclose(float(a), float(b), rel_tol=5e-11, abs_tol=1e-8)


def validate_samples(v, output, text, suite, expected_priority):
    """Suite-aware extension of 8097's validator, including same-callback pairs.

    The profile-extended name is supported explicitly. CSV time values and the
    strict wall p99 rule are never normalized, renamed, rounded, or substituted.
    The unchanged 8097 raw validator also checks every uninstrumented run.
    """
    require(suite in ("extended", "profile-extended"), "Unexpected diagnostic suite")
    parsed = v.parse_rows(text)
    v.validate_matrix(parsed, "expanded-v2", BUFFERS)
    if suite == "extended":
        v.validate_timing_samples(output, [dict(row, test=v.EXTENDED_TEST) for row in parsed["timing"]])
    groups = {prefix: [] for prefix in ("PAIRED_TIMING", "WORKER_SCHEDULING", "WORKER_ACTIVITY",
                                      "TIMING_EVIDENCE", "CALLER_SCHEDULING", "CALLBACK")}
    for line in text.splitlines():
        for prefix, rows in groups.items():
            if line.startswith(prefix + " "):
                rows.append(dict(v.ROW.findall(line)))
    expected_routes = {v.route_key(row) for row in parsed["timing"]}
    maps = {}
    for prefix in ("WORKER_SCHEDULING", "WORKER_ACTIVITY", "TIMING_EVIDENCE"):
        rows = groups[prefix]
        require(len(rows) == 30 and {v.route_key(r) for r in rows} == expected_routes,
                f"Missing/duplicate {prefix} routes")
        require(all(r["suite"] == suite for r in rows), f"Changed {prefix} suite")
        maps[prefix] = {v.route_key(r): r for r in rows}
    pairs = groups["PAIRED_TIMING"]
    pair_map = {(v.route_key(r), r["phase"]): r for r in pairs}
    phase_names = ("all", "worker_and_automation", "forced_six_slot_publication")
    require(len(pairs) == len(pair_map) == 90 and set(pair_map) ==
            {(key, phase) for key in expected_routes for phase in phase_names}, "Missing/duplicate paired records")
    require(all(r["suite"] == suite for r in pairs), "Changed paired suite")
    phase_map = {(v.route_key(r), r["name"]): r for r in parsed["phases"]}
    require(len(groups["CALLER_SCHEDULING"]) == 1 and groups["CALLER_SCHEDULING"][0].get("phase") == "entry",
            "Missing caller entry context")
    require(groups["CALLER_SCHEDULING"][0].get("caller_context") == "macos_base_qos"
            and int(groups["CALLER_SCHEDULING"][0]["caller_thread_qos"]) > 0, "Invalid caller entry QoS")
    require(len(groups["CALLBACK"]) == 1, "Missing callback allocation report")
    require(groups["CALLBACK"][0].get("callback_new") == groups["CALLBACK"][0].get("callback_delete") == "0",
            "Callback C++ new/delete observed")
    for row in groups["WORKER_SCHEDULING"]:
        require(all(row.get(k) == value for k, value in {
            "start_succeeded": "1", "entered_run": "1", "requested_juce_priority": str(expected_priority),
            "observed_juce_priority": str(expected_priority), "worker_priority_query": "native",
            "caller_context": "macos_base_qos"}.items()), "Worker native priority contract failed")
        require(int(row["caller_thread_qos"]) > 0, "Missing native caller QoS")
    routes, paths = [], set()
    for summary in parsed["timing"]:
        key = v.route_key(summary)
        engine, rate, block, channels = key
        relative = f"cab-timing-diagnostics/{suite}/{engine}/{rate}-{block}-{channels}.csv"
        paths.add(relative)
        reference = maps["TIMING_EVIDENCE"][key]
        require(reference["path"] == relative and reference["samples"] == "1600", "Raw reference mismatch")
        path = output / relative
        with path.open(newline="", encoding="utf-8") as stream:
            reader = csv.DictReader(stream)
            require(tuple(reader.fieldnames or ()) == CSV_FIELDS, "Changed CSV schema")
            samples = list(reader)
        require(len(samples) == 1600, "Incomplete raw callback population")
        budget = 1e6 * block / rate
        previous_cpu_end = -1.0
        for index, row in enumerate(samples):
            expected = {"schema_version": "1", "engine": engine, "suite": suite, "sample_rate": str(rate),
                "block_size": str(block), "channels": str(channels), "callback_index": str(index),
                "phase_index": str(index if index < 1200 else index - 1200),
                "phase": phase_names[1 if index < 1200 else 2],
                "automation_requested": "1" if index < 80 else "0"}
            require(all(row.get(k) == value for k, value in expected.items()), f"Raw callback identity mismatch: {relative}:{index}")
            wall, cpu, first, last = (float(row[k]) for k in
                ("wall_us", "thread_cpu_us", "thread_cpu_start_us", "thread_cpu_end_us"))
            require(all(math.isfinite(x) for x in (wall, cpu, first, last)) and wall >= 0 and cpu >= 0,
                    "Invalid native wall/CPU time")
            require(math.isclose(float(row["budget_us"]), budget, rel_tol=1e-12)
                    and row["deadline_miss"] == str(int(wall > budget)), "Changed deadline or miss flag")
            require(row["thread_cpu_valid"] == "1" and first >= 0 and first >= previous_cpu_end and last >= first
                    and math.isclose(cpu, last-first, rel_tol=1e-12, abs_tol=1e-8), "Unpaired CPU sample")
            require(row["thread_cycles_valid"] == "0" and all(row[k] == "0" for k in
                    ("thread_cycles", "thread_cycles_start", "thread_cycles_end", "clock_start_error", "clock_end_error")),
                    "Unexpected macOS cycle/clock status")
            previous_cpu_end = last
            row.update(wall_us=wall, thread_cpu_us=cpu)
        results = []
        for phase, population in (("all", samples), (phase_names[1], samples[:1200]), (phase_names[2], samples[1200:])):
            ordered = sorted(population, key=lambda r: (r["wall_us"], int(r["callback_index"])))
            p50, p99, maximum = ordered[len(ordered)//2], ordered[int(len(ordered)*.99)], ordered[-1]
            cpu_p99 = sorted(r["thread_cpu_us"] for r in population)[int(len(population)*.99)]
            misses = sum(r["wall_us"] > budget for r in population)
            paired = pair_map[key, phase]
            require(all(int(paired[k]) == value for k, value in {
                "samples": len(population), "thread_cpu_valid_samples": len(population),
                "thread_cycles_valid_samples": 0, "misses": misses, "misses_with_thread_cycles": 0}.items()),
                "Paired population/count mismatch")
            for prefix, row in (("wall_p99", p99), ("wall_max", maximum)):
                require(all(paired[prefix+"_"+k] == row[k] for k in
                    ("callback_index", "thread_cpu_valid", "thread_cycles", "thread_cycles_valid")),
                    "Paired callback index/clock mismatch")
                require(same(paired[prefix+"_wall_us"], row["wall_us"])
                    and same(paired[prefix+"_thread_cpu_us"], row["thread_cpu_us"]), "Wrong callback CPU pair")
            logged = summary if phase == "all" else phase_map[key, phase]
            require(all(same(logged[k], value) for k, value in {
                "p50_us": p50["wall_us"], "p99_us": p99["wall_us"], "max_us": maximum["wall_us"],
                "thread_cpu_p99_us": cpu_p99}.items()), "CSV disagrees with reported quantile")
            require(logged["misses"] == (f"{misses}/1600" if phase == "all" else str(misses)), "CSV miss count mismatch")
            results.append({"phase": phase, "samples": len(population), "misses": misses,
                "p50_us": p50["wall_us"], "p99_us": p99["wall_us"], "max_us": maximum["wall_us"],
                "wall_p99_callback_index": int(p99["callback_index"]), "wall_p99_paired_cpu_us": p99["thread_cpu_us"],
                "wall_max_callback_index": int(maximum["callback_index"]), "wall_max_paired_cpu_us": maximum["thread_cpu_us"]})
        cancelled = int(maps["WORKER_ACTIVITY"][key]["cancelled_builds"])
        require(cancelled >= 0, "Invalid worker cancellation count")
        routes.append({"engine": engine, "sr": rate, "block": block, "channels": channels,
            "budget_us": budget, "strict_p99_pass": results[0]["p99_us"] < budget, "phases": results,
            "cancelled_builds": cancelled, "raw_csv": relative, "sha256": v.digest(path)})
    actual = {p.relative_to(output).as_posix() for p in (output / "cab-timing-diagnostics").glob("*/*/*.csv")}
    require(paths == actual, "Unexpected/missing raw CSV files")
    if suite == "profile-extended":
        require("PROFILE_PROTOCOL diagnostic_only=1 stage_clocks_add_overhead=1 "
                "parameter_refresh_per_callback=1 nested_stage_times=1" in text, "Changed profile protocol")
        seen = {(v.route_key(r), r["phase"], r["stage"]) for r in parsed["profile"]}
        require(len(seen) == len(parsed["profile"]), "Duplicate stage profile")
        require(v.PROFILE_STAGES <= {r["stage"] for r in parsed["profile"]}
                and v.WORKER_STAGES <= {r["stage"] for r in parsed["worker_profile"]}, "Missing stage profile evidence")
        for row in parsed["profile"]:
            require(v.route_key(row) in expected_routes and row["phase"] in v.PHASE_BLOCKS
                and int(row["callbacks"]) == v.PHASE_BLOCKS[row["phase"]], "Changed profile phase counts")
            require(int(row["calls"]) > 0 and all(math.isfinite(float(row[k])) and float(row[k]) >= 0
                for k in ("mean_us", "p50_us", "p99_us", "max_us")), "Invalid stage cost")
        require({v.route_key(r) for r in parsed["profile"]} == expected_routes
                and {v.route_key(r) for r in parsed["worker_profile"]} == expected_routes, "Missing route stage profile")
        for key in expected_routes:
            for phase in v.PHASE_BLOCKS:
                required = v.PROFILE_STAGES - ({"fading_convolution"} if phase == "worker_and_automation" else set())
                actual_stages = {r["stage"] for r in parsed["profile"]
                                 if v.route_key(r) == key and r["phase"] == phase}
                require(required <= actual_stages <= v.PROFILE_STAGES,
                        f"Missing/unexpected callback stage: {key} {phase}: {sorted(required-actual_stages)}")
            actual_worker = {r["stage"] for r in parsed["worker_profile"] if v.route_key(r) == key}
            require(actual_worker == v.WORKER_STAGES, f"Missing/unexpected worker stage: {key}")
        require(len({(v.route_key(r), r["stage"]) for r in parsed["worker_profile"]}) ==
                len(parsed["worker_profile"]), "Duplicate worker stage profile")
        require(all(int(r["calls"]) > 0 and math.isfinite(float(r["total_us"])) and float(r["total_us"]) >= 0
                    for r in parsed["worker_profile"]), "Invalid worker stage cost")
    return {"conditions": 30, "callbacks": 48000, "paired_records": 90, "phase_records": 60,
        "native_cpu_valid_callbacks": 48000, "cycles_available": False, "clock_errors": 0,
        "strict_p99_passes": sum(r["strict_p99_pass"] for r in routes),
        "misses": sum(r["phases"][0]["misses"] for r in routes),
        "misses_by_phase": {phase: sum(r["phases"][n]["misses"] for r in routes)
                           for n, phase in enumerate(phase_names) if n},
        "routes": routes, "worker_scheduling": groups["WORKER_SCHEDULING"],
        "caller_entry": groups["CALLER_SCHEDULING"][0], "callback_allocations": groups["CALLBACK"][0],
        "worker_fading_stage_omissions": [list(key) for key in sorted(expected_routes)
            if suite == "profile-extended" and not any(v.route_key(r) == key
                and r["phase"] == "worker_and_automation" and r["stage"] == "fading_convolution"
                for r in parsed["profile"])],
        "worker_fading_omission_meaning": "Pinned reportProfile omits zero-call stages; this is not evidence of worker CPU occupancy.",
        "profile": parsed["profile"], "worker_profile": parsed["worker_profile"]}


def measure(v, build, output, label, profile=False):
    output.mkdir(parents=True, exist_ok=True)
    suite, target = ("profile-extended", PROFILE) if profile else ("extended", INTEGRATION)
    receipt = {"variant": label, "suite": suite, "diagnostic_only": True, "complete": False, "passed": False}
    try:
        path = binary(v, build, target)
        receipt["binary_before_sha256"] = v.digest(path)
        # Each invocation owns its cwd: no old CSV can satisfy a later run.
        require(not (output / "cab-timing-diagnostics").exists(), "Run output must be fresh")
        receipt["command"] = bounded_run(v, [path, "--expanded", "--extended-buffers"], output,
                                          output / "console.log")
        require(not receipt["command"].get("not_started"), receipt["command"].get("reason", ""))
        text = (output / "console.log").read_text(encoding="utf-8", errors="replace")
        receipt["validation"] = validate_samples(v, output, text, suite, -1)
        receipt["binary_after_sha256"] = v.digest(path)
        require(receipt["binary_before_sha256"] == receipt["binary_after_sha256"], "Measured binary changed")
        receipt["complete"] = True
        receipt["passed"] = receipt["command"]["returncode"] == 0 and receipt["validation"]["strict_p99_passes"] == 30
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError) as error:
        receipt["error"] = str(error)
    finally:
        receipt["raw_files"] = [{"path": p.relative_to(output).as_posix(), "sha256": v.digest(p),
            "bytes": p.stat().st_size} for p in sorted((output / "cab-timing-diagnostics").glob("*/*/*.csv"))]
        v.write_json(output / "receipt.json", receipt)
    return receipt


def disassemble(v, build, output):
    result = {"diagnostic_only": True, "available": False, "complete": False, "commands": []}
    for name in ("llvm-objdump", "otool"):
        try:
            tool = v.capture(["xcrun", "--find", name])
        except (OSError, RuntimeError, subprocess.SubprocessError):
            continue
        result.update(available=True, tool=name, tool_path=tool)
        attempt = []
        for target in (INTEGRATION, PROFILE):
            path = binary(v, build, target)
            flags = ["--disassemble", "--demangle"] if name == "llvm-objdump" else ["-arch", "arm64", "-tvV"]
            record = bounded_run(v, [tool, *flags, path], build, output / (target + "." + name + ".arm64.txt"))
            attempt.append(record)
            result["commands"].append(record)
        if all(r["returncode"] == 0 for r in attempt):
            result["complete"] = True
            break
    # Full text retains accumulate, sumPartitions and any inlined containing
    # process loops. No instruction selection or code generation is changed.
    v.write_json(output / "receipt.json", result)
    return result


def main():
    global command_deadline
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--harness-sha", required=True)
    parser.add_argument("--reference-source", type=Path, required=True)
    parser.add_argument("--simd-source", type=Path, required=True)
    parser.add_argument("--build-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    sources = {"reference": args.reference_source.resolve(), "simd": args.simd_source.resolve()}
    build_root, output = args.build_root.resolve(), args.output.resolve()
    require(not output.exists() or not any(output.iterdir()), "Evidence output must be new or empty")
    output.mkdir(parents=True, exist_ok=True)
    v = load_driver(sources["reference"])
    manifest = {"schema": 1, "diagnostic_only": True, "base_sha": BASE_SHA, "base_tree": BASE_TREE,
        "harness_sha": args.harness_sha, "started_at": v.utc_now(), "result": "DIAGNOSTIC_INCOMPLETE",
        "acceptance_superseded": False, "order": [name for name, _ in ORDER],
        "worker_priority": "unchanged_low_for_both_variants", "overlay_sha256": OVERLAY_SHA256,
        "simd_guard_reference_expected": ARM_REFERENCE,
        "profiles_after_abba": ["reference", "simd"], "variants": {}, "measurements": [], "profiles": [],
        "limits": {"command_timeout_seconds": 600, "job_timeout_minutes": 35, "build_parallel": 2,
            "preservation_reserve_seconds": PRESERVATION_SECONDS,
            "do_not_start_if_remaining_below_seconds": COMMAND_TIMEOUT,
            "measured_callbacks": 1600, "automation_callbacks": 80, "worker_callbacks": 1200,
            "forced_callbacks": 400, "ir_cancellation_iterations": 4000, "latest_convergence_iterations": 10000},
        "caveats": [
            "One bounded ABBA on one hosted VM is diagnostic evidence, not a replacement acceptance run.",
            "Both variants retain the original low IR-worker priority; no caller/process/QoS/affinity change is part of B.",
            "The unchanged stop timer is diagnostic; no hard worker shutdown deadline is asserted.",
            "Worker phase does not prove continuous worker execution; sleep(1) occurs outside timing after n%8==0.",
            "Forced publication has no sleep(1), and the IR worker is stopped before that phase.",
            "CPU clocks bracket a wider interval than wall clocks; paired CPU is not an isolated DSP cycle measurement.",
            "Profile stage clocks and per-callback parameter refresh add overhead; nested stage totals are not additive.",
            "Native base QoS readback does not record effective priority, core placement, clock rate or prove a cause."]}
    v.write_json(output / "manifest.json", manifest)
    try:
        require(platform.system() == "Darwin" and platform.machine() == "arm64", "Requires native macOS arm64")
        require(os.environ.get("GITHUB_REF") == BRANCH, "Diagnostic branch-only execution guard")
        job_start = float(os.environ["CAB_ARM_MAC_JOB_STARTED_EPOCH"])
        elapsed = time.time() - job_start
        require(math.isfinite(job_start) and 0 <= elapsed < 35*60, "Missing/invalid diagnostic budget origin")
        command_deadline = time.monotonic() + 35*60 - PRESERVATION_SECONDS - elapsed
        manifest["budget_origin_epoch"] = job_start
        harness = Path(__file__).resolve().parent.parent
        manifest["harness_source"] = v.source_snapshot(harness, args.harness_sha)
        manifest["environment"] = v.environment_snapshot(output)
        expected = prepare_sources(v, sources, output)
        for label, source in sources.items():
            built = build_variant(v, source, build_root / label, output / label)
            manifest["variants"][label] = {"build": built}
            v.write_json(output / "manifest.json", manifest)
        # Both builds finish before any tests. No test or profile overlaps another.
        for label in sources:
            if manifest["variants"][label]["build"]["complete"]:
                manifest["variants"][label]["guards"] = correctness(v, build_root / label, output / label / "guards", label)
                v.write_json(output / "manifest.json", manifest)
        for name, label in ORDER:
            if manifest["variants"][label]["build"]["complete"]:
                run = measure(v, build_root / label, output / "runs" / name, label)
            else:
                run = {"variant": label, "complete": False, "passed": False, "skipped": "variant build failed"}
            manifest["measurements"].append(dict(run, name=name))
            v.write_json(output / "manifest.json", manifest)
        for label in sources:
            if manifest["variants"][label]["build"]["complete"]:
                run = measure(v, build_root / label, output / "profiles" / label, label, profile=True)
                manifest["profiles"].append(run)
                v.write_json(output / "manifest.json", manifest)
        for label, source in sources.items():
            after = snapshot(v, source, expected[label], () if label == "reference" else OVERLAY_PATHS)
            v.write_json(output / label / "source-after.json", after)
            built = manifest["variants"][label]["build"]
            if built["complete"]:
                require(all(v.digest(Path(row["path"])) == row["sha256"] for row in built["binaries"]),
                        "A built executable changed during diagnostic execution")
                manifest["variants"][label]["disassembly"] = disassemble(v, build_root / label, output / label / "disassembly")
        require(v.source_snapshot(harness, args.harness_sha) == manifest["harness_source"], "Harness changed")
        parts = [*manifest["measurements"], *manifest["profiles"],
                 *(x.get("guards", {}) for x in manifest["variants"].values())]
        assembly_complete = all(not x.get("disassembly", {}).get("available")
            or x["disassembly"]["complete"] for x in manifest["variants"].values())
        complete = (len(manifest["measurements"]) == 4 and len(manifest["profiles"]) == 2
                    and all(x.get("complete") for x in parts) and assembly_complete)
        manifest["evidence_complete"] = complete
        if complete:
            manifest["result"] = "DIAGNOSTIC_COMPLETE_ALL_GATES_PASSED" if all(x.get("passed") for x in parts) else "DIAGNOSTIC_COMPLETE_WITH_FAILURES"
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError) as error:
        manifest["error"] = str(error)
    finally:
        manifest["finished_at"] = v.utc_now()
        v.write_json(output / "manifest.json", manifest)
        lines = ["# CAB macOS ARM spectral MAC diagnostic", "", f"Result: **{manifest['result']}**",
            "", f"Fixed base: {BASE_SHA}; harness: {args.harness_sha}.",
            "A is untouched 8097. B is the exact separately recorded two-file ARM spectral MAC overlay; both retain low worker priority.",
            "This comparison never replaces the failed 8097 acceptance result.", "",
            "| Run | Complete | Exit | Strict p99 | Misses |", "|---|---|---:|---:|---:|"]
        for run in [*manifest["measurements"], *manifest["profiles"]]:
            evidence = run.get("validation", {})
            lines.append(f"| {run.get('name', 'profile-' + run['variant'])} | {run['complete']} | "
                f"{run.get('command', {}).get('returncode', 'unavailable')} | "
                f"{evidence.get('strict_p99_passes', 'unavailable')}/30 | {evidence.get('misses', 'unavailable')} |")
        lines += ["", "## Interpretation limits", "", *("- " + item for item in manifest["caveats"])]
        if manifest.get("error"):
            lines += ["", "Driver error: " + manifest["error"]]
        summary = "\n".join(lines) + "\n"
        (output / "summary.md").write_text(summary, encoding="utf-8")
        if os.environ.get("GITHUB_STEP_SUMMARY"):
            with Path(os.environ["GITHUB_STEP_SUMMARY"]).open("a", encoding="utf-8") as stream:
                stream.write(summary)
        v.write_json(output / "SHA256SUMS.json", [{"path": p.relative_to(output).as_posix(),
            "bytes": p.stat().st_size, "sha256": v.digest(p)}
            for p in sorted(output.rglob("*")) if p.is_file() and p.name != "SHA256SUMS.json"])
    return 0 if manifest["result"] == "DIAGNOSTIC_COMPLETE_ALL_GATES_PASSED" else 1


if __name__ == "__main__":
    raise SystemExit(main())
