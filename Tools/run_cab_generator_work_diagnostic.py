#!/usr/bin/env python3
"""Bounded native Windows CAB generator-work compile repair; never replaces PR42 acceptance.

A is untouched 8097. B has only the final root-reviewed source/test overlay.
The overlay pins stay fail-closed until that exact patch is reviewed. Both retain low worker priority and the existing
1600 callback / 80 automation / 1200 worker / 400 forced-publication protocol.
No caller priority, process priority, affinity, power setting, fade, load,
deadline or tolerance is changed. One ABBA over expanded30 and layouts12, then both separate profile suites per variant.
The B-only frozen-vs-prepared generator probe runs once after all CAB timings.
The exact patch, allowed paths and every resulting source SHA are pinned below.
The first Windows B build failed on the Windows far macro. Only that test
identifier is repaired here. The completed macOS timing failures are retained;
this Windows-only comparison does not repeat or supersede that macOS run.
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
import struct
import sys
import time
import xml.etree.ElementTree as ET

sys.dont_write_bytecode = True
BASE_SHA = "8097ec69594ff728164c77f59e9ea1e39e384643"
BASE_TREE = "f12b4312f559e86e07c37899691b62858f4a4c3c"
JUCE_SHA = "d6181bde38d858c283c3b7bf699ce6340c050b5d"
BRANCH = "refs/heads/diagnostic/cab-generator-win-macro-20261011"
BUFFERS = (32, 64, 128, 256, 512)
COMMAND_TIMEOUT = 600
PRESERVATION_SECONDS = 300
command_deadline = None
INTEGRATION = "ChimeraOriginalCabIntegrationTests"
PROFILE = "ChimeraCabProfiling"
GUARDS = ("ChimeraIRLibraryTests", "ChimeraOriginalCabRealtimeTests",
          "ChimeraOriginalCabModelTests", "ChimeraCabExpansionModelTests")
TARGETS = (INTEGRATION, PROFILE, *GUARDS)
ORDER = (("01-A-reference", "reference"), ("02-B-candidate", "candidate"),
         ("03-B-candidate", "candidate"), ("04-A-reference", "reference"))
# Generator-only c460b52d plus the test-only far->farResponse repair be2121ab.
# ARM, QoS and geometry probes are excluded. Production source is unchanged
# from the first generator diagnostic; this remains separate from acceptance.
OVERLAY_REVIEWED = True
REVIEWED_GENERATOR_COMMIT = "be2121ab0c4cf52212bc6f0526490e19c1745c68"
REVIEWED_GENERATOR_TREE = "66ae8de8b62b22b2d51a64d02d85655b989fa94f"
OVERLAY_PATHS = ("Source/CabExpansionModel.h", "Source/CabLayoutModel.h", "Source/OriginalCabModel.h",
                 "Tests/CabExpansionModelTests.cpp", "Tests/CabPreparedResponseReference.h")
OVERLAY_FILE = "Tools/cab_generator_work_candidate.diff"
OVERLAY_SHA256 = "313d5d821be296a230b18c9513d8d7fee2c7077c1997fee0cd47d232ae4e4c36"
OVERLAY_RESULT_SHA256 = {
    "Source/CabExpansionModel.h": "2e48808d2c48aa274eee458c8cfe5ddf3848affe6fe102bb667309720c2662c4",
    "Source/CabLayoutModel.h": "a2b10ec204ba75edbb92ed803aee9145881715a2109d6e5edc4a5e6daa46d24c",
    "Source/OriginalCabModel.h": "be1cee490481f29c2565dbe13e8dc85052c87aa1d73dbcbc05a1a274b54bf569",
    "Tests/CabExpansionModelTests.cpp": "43865deb23961f102f734d5844603eabdfb6a34573346c0a16c459b068ed6b56",
    "Tests/CabPreparedResponseReference.h": "77ca29a4e348e7c0b29fb9c7c6d8139c784c660a5034f44633767628583e1c0a"
}
OVERLAY_ORIGINAL_SHA256 = {
    "Source/CabExpansionModel.h": "98254fc51eb01bce251d58282c0c7a1474a4abfbee7ed0a4053d1380f3bb6a02",
    "Source/CabLayoutModel.h": "f7abd4300e3f8e4fae2a08f60f3dfeae0e919626646cbc721f72fd14557785ea",
    "Source/OriginalCabModel.h": "972dacf18e91b2585b3bcef71d5650135241d733737b888e651c709c0e3e2735",
    "Tests/CabExpansionModelTests.cpp": "39695636a49926ee226aee5f8dc03c8ef200616f9a4abad59b218ea1df7ebe50",
    "Tests/CabPreparedResponseReference.h": None,
}
CANDIDATE_GUARD_RECORDS = {"ChimeraCabExpansionModelTests": {"PASS prepared_response_reference": {
    "source": BASE_SHA, "response_cases": "819", "public_and_request_local": "exact_double_bits",
    "driver_transfer_cases": "247", "waveform_cases": "40", "waveform_samples": "244840",
    "float_bytes": "identical", "rates": "48000,96000",
}}}
GENERATOR_BENCHMARK_CONTRACT = {
    "families": "2", "order": "ABBA", "repeats": "3", "samples": "24",
    "samples_per_implementation_per_family": "6", "waveform_byte_checks": "24",
    "warmup_generations": "4", "timing_threshold": "none",
}
GENERATOR_FAMILIES = (("v2-failing-strike", "93666328612611"),
                      ("v3-failing-strike", "19093175716974339"))
GENERATOR_FIELDS = (
    "schema_version", "family", "model", "sample_rate", "repeat", "abba_index",
    "implementation", "samples", "wall_us", "thread_cpu_us", "thread_cpu_valid",
    "thread_cycles", "thread_cycles_valid", "clock_start_error", "clock_end_error",
    "thread_cpu_start_us", "thread_cpu_end_us", "thread_cycles_start", "thread_cycles_end",
    "fnv1a64", "float_bytes_identical", "waveform")
JOB_MINUTES = {"Windows": 45}
WORKLOADS = ("expanded", "layouts")
SUITE_SPECS = {
    "extended": ("expanded-v2", BUFFERS, "ChimeraCabExtendedTimingTests", False),
    "profile-extended": ("expanded-v2", BUFFERS, "ChimeraCabExtendedTimingTests", True),
    "integration": ("array-v3-6x10", (64, 256), "ChimeraCabLayoutIntegrationTests", False),
    "profile": ("array-v3-6x10", (64, 256), "ChimeraCabLayoutIntegrationTests", True),
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
    require(OVERLAY_REVIEWED, "Candidate overlay is not frozen/reviewed; refusing a no-op diagnostic")
    require(OVERLAY_PATHS and len(set(OVERLAY_PATHS)) == len(OVERLAY_PATHS)
            and set(OVERLAY_RESULT_SHA256) == set(OVERLAY_ORIGINAL_SHA256) == set(OVERLAY_PATHS),
            "Invalid exact overlay path/hash contract")
    require(all(not Path(name).is_absolute() and ".." not in Path(name).parts
                and name.startswith(("Source/", "Tests/")) for name in OVERLAY_PATHS),
            "Overlay is not limited to reviewed source/test paths")
    require(all(len(value) == 64 and all(c in "0123456789abcdef" for c in value)
                for value in OVERLAY_RESULT_SHA256.values()), "Invalid resulting source SHA contract")
    require(CANDIDATE_GUARD_RECORDS, "Missing reviewed candidate correctness evidence contract")
    for source in sources.values():
        v.source_snapshot(source, BASE_SHA)
    original = tracked_files(v, sources["reference"])
    require(original == tracked_files(v, sources["candidate"]), "A/B source bytes differ before overlay")
    patch = Path(__file__).resolve().parent.parent / OVERLAY_FILE
    require(len(OVERLAY_SHA256) == 64 and all(c in "0123456789abcdef" for c in OVERLAY_SHA256),
            "Invalid root-reviewed candidate patch SHA")
    require(patch.is_file() and v.digest(patch) == OVERLAY_SHA256, "Missing/changed root-reviewed candidate patch")
    shutil.copyfile(patch, output / "input-candidate-overlay.diff")
    # --index records explicitly added reference-test files in the same source
    # ledger. HEAD and its tree remain the fixed base; B is an uncommitted overlay.
    v.capture(["git", "apply", "--check", "--index", "--binary", str(patch)], sources["candidate"])
    v.capture(["git", "apply", "--index", "--binary", str(patch)], sources["candidate"])
    changed = set(v.capture(["git", "diff", "HEAD", "--name-only"], sources["candidate"]).splitlines())
    require(changed == set(OVERLAY_PATHS), f"Unexpected candidate patch paths: {sorted(changed)}")
    changes = []
    for name in OVERLAY_PATHS:
        original_path, candidate_path = (sources[label] / name for label in ("reference", "candidate"))
        old = original_path.read_bytes() if original_path.is_file() else None
        require((v.digest(original_path) if old is not None else None) == OVERLAY_ORIGINAL_SHA256[name],
                f"Untouched source bytes do not match reviewed baseline: {name}")
        require(candidate_path.is_file() and not candidate_path.is_symlink(), f"Missing/non-regular overlay file: {name}")
        new = candidate_path.read_bytes()
        require(old != new, f"Expected candidate patch did not change {name}")
        require(v.digest(sources["candidate"] / name) == OVERLAY_RESULT_SHA256[name],
                f"Applied candidate source differs from the root-reviewed bytes: {name}")
        for label, data in (("original", old), ("candidate", new)):
            if data is None:
                continue
            saved = output / "overlay" / label / name
            saved.parent.mkdir(parents=True, exist_ok=True)
            saved.write_bytes(data)
        changes.append({"path": name, "added": old is None,
                        "original_sha256": v.digest(original_path) if old is not None else None,
                        "candidate_sha256": v.digest(sources["candidate"] / name)})
    # Preserve the exact input patch and the independently generated applied diff.
    diff = subprocess.check_output(["git", "diff", "--binary", "HEAD", "--", *OVERLAY_PATHS],
                                   cwd=sources["candidate"], timeout=60)
    (output / "applied-candidate-overlay.diff").write_bytes(diff)
    v.write_json(output / "overlay.json", {"base_sha": BASE_SHA, "base_tree": BASE_TREE,
        "reviewed_generator_commit": REVIEWED_GENERATOR_COMMIT, "reviewed_generator_tree": REVIEWED_GENERATOR_TREE,
        "b_is_uncommitted_overlay_not_the_base_tree": True, "files": changes,
        "input_patch_sha256": OVERLAY_SHA256,
        "applied_diff_sha256": v.digest(output / "applied-candidate-overlay.diff"),
        "timing_gate_unchanged": True, "worker_priority_unchanged": True, "caller_priority_unchanged": True})
    expected = {"reference": original, "candidate": tracked_files(v, sources["candidate"])}
    ref_map = {row["path"]: row for row in original}
    candidate_map = {row["path"]: row for row in expected["candidate"]}
    require(set(ref_map) <= set(candidate_map) and {name for name in set(ref_map) | set(candidate_map)
                if ref_map.get(name) != candidate_map.get(name)}
            == set(OVERLAY_PATHS), "candidate patch changed unexpected source bytes")
    for label, source in sources.items():
        state = snapshot(v, source, expected[label], () if label == "reference" else OVERLAY_PATHS)
        v.write_json(output / label / "source-before.json", state)
    return expected


def binary(v, build, target):
    if target in ("ChimeraOriginalCabModelTests", "ChimeraCabExpansionModelTests"):
        if os.name == "nt":
            paths = [build / "Release" / (target + ".exe"), build / (target + ".exe")]
        else:
            paths = [build / target]
        available = [path for path in paths if path.is_file()]
        require(len(available) == 1, f"Missing/ambiguous Release model test executable: {paths}")
        return available[0]
    return v.executable(build, target)


def native_architecture(v, path):
    if platform.system() == "Darwin":
        architecture = v.capture(["lipo", "-archs", str(path)])
        require(architecture == "arm64", f"Non-native macOS executable: {path}: {architecture}")
        return architecture
    # Read-only PE header verification; no compiler/disassembler flags are changed.
    with path.open("rb") as stream:
        require(stream.read(2) == b"MZ", f"Missing PE executable header: {path}")
        stream.seek(0x3c)
        offset = struct.unpack("<I", stream.read(4))[0]
        stream.seek(offset)
        require(stream.read(4) == b"PE\0\0" and struct.unpack("<H", stream.read(2))[0] == 0x8664,
                f"Expected native x64 PE executable: {path}")
    return "x86_64"


def build_variant(v, source, build, output):
    receipt = {"complete": False, "commands": [], "binaries": []}
    try:
        configure = ["cmake", "-S", source, "-B", build, "-DCMAKE_BUILD_TYPE=Release"]
        if platform.system() == "Darwin":
            configure += ["-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0", "-DCMAKE_OSX_ARCHITECTURES=arm64"]
        else:
            configure += ["-A", "x64"]
        for command, log in (
            (configure, "configure.log"),
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
            architecture = native_architecture(v, path)
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
        receipt["candidate_reference_records"] = {}
        for test_name, contracts in CANDIDATE_GUARD_RECORDS.items():
            for prefix, expected in contracts.items():
                records = [dict(v.ROW.findall(line)) for line in texts[test_name].splitlines()
                           if line.startswith(prefix + " ")]
                receipt["candidate_reference_records"][prefix] = records
                if label == "candidate":
                    receipt["guard_evidence"][prefix] = len(records) == 1 and records[0] == expected
                else:
                    require(not records, f"Unexpected candidate reference in untouched A: {prefix}")
        receipt["complete"] = True
        receipt["passed"] = (receipt["command"]["returncode"] == 0
            and all(t["passed"] for t in receipt["tests"]) and all(receipt["guard_evidence"].values()))
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError, ET.ParseError) as error:
        receipt["error"] = str(error)
    v.write_json(output / "receipt.json", receipt)
    return receipt


def same(a, b):
    return math.isclose(float(a), float(b), rel_tol=5e-11, abs_tol=1e-8)


def validate_caller_context(row, native_system):
    if native_system == "Darwin":
        require(row.get("caller_context") == "macos_base_qos"
                and int(row["caller_thread_qos"]) > 0, "Invalid native caller QoS")
    else:
        require(row.get("caller_context") == "windows_native"
                and row.get("caller_priority_valid") == row.get("caller_affinity_valid") == "1"
                and int(row["caller_process_priority_class"]) > 0
                and int(row["caller_process_affinity_mask"]) > 0, "Invalid native Windows caller context")
        require(-15 <= int(row["caller_thread_priority"]) <= 15, "Invalid Windows caller thread priority")


def caller_tuple(row, native_system):
    fields = (("caller_context", "caller_thread_qos") if native_system == "Darwin" else
              ("caller_context", "caller_process_priority_class", "caller_thread_priority",
               "caller_priority_valid", "caller_process_affinity_mask", "caller_affinity_valid"))
    return {field: row[field] for field in fields}


def compare_caller_context(runs, native_system):
    """Observe equality only. Never change priority or affinity to obtain it."""
    observations, reference = [], None
    comparable = True
    for run in runs:
        validation = run.get("validation", {})
        context = validation.get("caller_context_comparison")
        if not context:
            observations.append({"run": run.get("name"), "available": False})
            comparable = False
            continue
        if reference is None:
            reference = context["entry"]
        matches = context["entry"] == reference and context["all_routes_match_entry"]
        comparable = comparable and matches
        observations.append({"run": run.get("name"), "available": True,
            "entry": context["entry"], "all_routes_match_entry": context["all_routes_match_entry"],
            "matches_first_reference_entry": matches})
    return {"native_system": native_system, "comparable": bool(reference) and comparable,
        "reference_first_entry": reference, "observations": observations,
        "meaning": "Non-equivalent native caller tuples make this diagnostic comparison incomplete; every run remains preserved."}


def validate_samples(v, output, text, suite, expected_priority, native_system):
    """Suite-aware extension of 8097's validator, including same-callback pairs.

    The profile-extended name is supported explicitly. CSV time values and the
    strict wall p99 rule are never normalized, renamed, rounded, or substituted.
    The unchanged 8097 raw validator also checks every uninstrumented run.
    """
    require(suite in SUITE_SPECS and native_system in JOB_MINUTES, "Unexpected diagnostic suite/platform")
    engine_name, buffers, test_name, is_profile = SUITE_SPECS[suite]
    conditions = len(v.RATES) * len(buffers) * 2
    mac = native_system == "Darwin"
    parsed = v.parse_rows(text)
    v.validate_matrix(parsed, engine_name, buffers)
    if not is_profile or suite == "profile":
        v.validate_timing_samples(output, [dict(row, test=test_name) for row in parsed["timing"]], profile=is_profile)
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
        require(len(rows) == conditions and {v.route_key(r) for r in rows} == expected_routes,
                f"Missing/duplicate {prefix} routes")
        require(all(r["suite"] == suite for r in rows), f"Changed {prefix} suite")
        maps[prefix] = {v.route_key(r): r for r in rows}
    pairs = groups["PAIRED_TIMING"]
    pair_map = {(v.route_key(r), r["phase"]): r for r in pairs}
    phase_names = ("all", "worker_and_automation", "forced_six_slot_publication")
    require(len(pairs) == len(pair_map) == conditions*3 and set(pair_map) ==
            {(key, phase) for key in expected_routes for phase in phase_names}, "Missing/duplicate paired records")
    require(all(r["suite"] == suite for r in pairs), "Changed paired suite")
    phase_map = {(v.route_key(r), r["name"]): r for r in parsed["phases"]}
    require(len(groups["CALLER_SCHEDULING"]) == 1 and groups["CALLER_SCHEDULING"][0].get("phase") == "entry",
            "Missing caller entry context")
    validate_caller_context(groups["CALLER_SCHEDULING"][0], native_system)
    require(len(groups["CALLBACK"]) == 1, "Missing callback allocation report")
    require(groups["CALLBACK"][0].get("callback_new") == groups["CALLBACK"][0].get("callback_delete") == "0",
            "Callback C++ new/delete observed")
    for row in groups["WORKER_SCHEDULING"]:
        require(all(row.get(k) == value for k, value in {
            "start_succeeded": "1", "entered_run": "1", "requested_juce_priority": str(expected_priority),
            "observed_juce_priority": str(expected_priority), "worker_priority_query": "native"}.items()),
            "Worker native priority contract failed")
        validate_caller_context(row, native_system)
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
        previous_cpu_end, previous_cycles_end = -1.0, -1
        for index, row in enumerate(samples):
            expected = {"schema_version": "1", "engine": engine, "suite": suite, "sample_rate": str(rate),
                "block_size": str(block), "channels": str(channels), "callback_index": str(index),
                "phase_index": str(index if index < 1200 else index - 1200),
                "phase": phase_names[1 if index < 1200 else 2],
                "automation_requested": "1" if index < 80 else "0"}
            require(all(row.get(k) == value for k, value in expected.items()), f"Raw callback identity mismatch: {relative}:{index}")
            wall, cpu, first, last = (float(row[k]) for k in
                ("wall_us", "thread_cpu_us", "thread_cpu_start_us", "thread_cpu_end_us"))
            require(all(math.isfinite(x) for x in (wall, cpu, first, last)) and wall >= 0,
                    "Invalid native wall/CPU time")
            require(math.isclose(float(row["budget_us"]), budget, rel_tol=1e-12)
                    and row["deadline_miss"] == str(int(wall > budget)), "Changed deadline or miss flag")
            require(row["clock_start_error"] == row["clock_end_error"] == "0", "Native thread clock read failed")
            if mac:
                require(row["thread_cpu_valid"] == "1" and cpu >= 0 and first >= 0
                        and first >= previous_cpu_end and last >= first
                        and math.isclose(cpu, last-first, rel_tol=1e-12, abs_tol=1e-8), "Unpaired CPU sample")
                require(row["thread_cycles_valid"] == "0" and all(row[k] == "0" for k in
                        ("thread_cycles", "thread_cycles_start", "thread_cycles_end")), "Unexpected macOS cycle status")
                previous_cpu_end = last
            else:
                require(row["thread_cpu_valid"] == "0" and cpu == first == last == -1,
                        "Unavailable Windows CPU time must remain -1")
                cycle_first, cycle_last, cycles = (int(row[k]) for k in
                    ("thread_cycles_start", "thread_cycles_end", "thread_cycles"))
                require(row["thread_cycles_valid"] == "1" and cycle_first >= 0
                        and cycle_first >= previous_cycles_end and cycle_last >= cycle_first
                        and cycles == cycle_last-cycle_first, "Unpaired native Windows cycle sample")
                previous_cycles_end = cycle_last
            row.update(wall_us=wall, thread_cpu_us=cpu)
        results = []
        for phase, population in (("all", samples), (phase_names[1], samples[:1200]), (phase_names[2], samples[1200:])):
            ordered = sorted(population, key=lambda r: (r["wall_us"], int(r["callback_index"])))
            p50, p99, maximum = ordered[len(ordered)//2], ordered[int(len(ordered)*.99)], ordered[-1]
            cpu_p99 = sorted(r["thread_cpu_us"] for r in population)[int(len(population)*.99)]
            misses = sum(r["wall_us"] > budget for r in population)
            paired = pair_map[key, phase]
            require(all(int(paired[k]) == value for k, value in {
                "samples": len(population), "thread_cpu_valid_samples": len(population) if mac else 0,
                "thread_cycles_valid_samples": 0 if mac else len(population), "misses": misses,
                "misses_with_thread_cycles": 0 if mac else misses}.items()),
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
                "wall_p99_paired_cycles": int(p99["thread_cycles"]),
                "wall_max_callback_index": int(maximum["callback_index"]), "wall_max_paired_cpu_us": maximum["thread_cpu_us"],
                "wall_max_paired_cycles": int(maximum["thread_cycles"])})
        cancelled = int(maps["WORKER_ACTIVITY"][key]["cancelled_builds"])
        require(cancelled >= 0, "Invalid worker cancellation count")
        routes.append({"engine": engine, "sr": rate, "block": block, "channels": channels,
            "budget_us": budget, "strict_p99_pass": results[0]["p99_us"] < budget, "phases": results,
            "cancelled_builds": cancelled, "raw_csv": relative, "sha256": v.digest(path)})
    actual = {p.relative_to(output).as_posix() for p in (output / "cab-timing-diagnostics").glob("*/*/*.csv")}
    require(paths == actual, "Unexpected/missing raw CSV files")
    if is_profile:
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
    entry_context = caller_tuple(groups["CALLER_SCHEDULING"][0], native_system)
    route_contexts = [caller_tuple(row, native_system) for row in groups["WORKER_SCHEDULING"]]
    return {"conditions": conditions, "callbacks": conditions*1600,
        "paired_records": conditions*3, "phase_records": conditions*2,
        "native_system": native_system, "native_cpu_valid_callbacks": conditions*1600 if mac else 0,
        "native_cycles_valid_callbacks": 0 if mac else conditions*1600,
        "cycles_available": not mac, "clock_errors": 0,
        "strict_p99_passes": sum(r["strict_p99_pass"] for r in routes),
        "misses": sum(r["phases"][0]["misses"] for r in routes),
        "misses_by_phase": {phase: sum(r["phases"][n]["misses"] for r in routes)
                           for n, phase in enumerate(phase_names) if n},
        "routes": routes, "worker_scheduling": groups["WORKER_SCHEDULING"],
        "caller_entry": groups["CALLER_SCHEDULING"][0], "callback_allocations": groups["CALLBACK"][0],
        "caller_context_comparison": {"entry": entry_context,
            "all_routes_match_entry": all(row == entry_context for row in route_contexts),
            "unique_route_contexts": [json.loads(row) for row in sorted({json.dumps(row, sort_keys=True)
                                                                        for row in route_contexts})]},
        "worker_fading_stage_omissions": [list(key) for key in sorted(expected_routes)
            if is_profile and not any(v.route_key(r) == key
                and r["phase"] == "worker_and_automation" and r["stage"] == "fading_convolution"
                for r in parsed["profile"])],
        "worker_fading_omission_meaning": "Pinned reportProfile omits zero-call stages; this is not evidence of worker CPU occupancy.",
        "profile": parsed["profile"], "worker_profile": parsed["worker_profile"]}


def measure(v, build, output, label, workload, profile=False):
    output.mkdir(parents=True, exist_ok=True)
    require(workload in WORKLOADS, "Unknown fixed workload")
    if workload == "expanded":
        suite = "profile-extended" if profile else "extended"
        arguments = ["--expanded", "--extended-buffers"]
    else:
        suite = "profile" if profile else "integration"
        arguments = ["--layouts"]
    target = PROFILE if profile else INTEGRATION
    receipt = {"variant": label, "workload": workload, "suite": suite,
               "diagnostic_only": True, "complete": False, "passed": False}
    try:
        path = binary(v, build, target)
        receipt["binary_before_sha256"] = v.digest(path)
        # Each invocation owns its cwd: no old CSV can satisfy a later run.
        require(not (output / "cab-timing-diagnostics").exists(), "Run output must be fresh")
        receipt["command"] = bounded_run(v, [path, *arguments], output,
                                          output / "console.log")
        require(not receipt["command"].get("not_started"), receipt["command"].get("reason", ""))
        text = (output / "console.log").read_text(encoding="utf-8", errors="replace")
        receipt["validation"] = validate_samples(v, output, text, suite, -1, platform.system())
        receipt["binary_after_sha256"] = v.digest(path)
        require(receipt["binary_before_sha256"] == receipt["binary_after_sha256"], "Measured binary changed")
        receipt["complete"] = True
        receipt["passed"] = (receipt["command"]["returncode"] == 0
            and receipt["validation"]["strict_p99_passes"] == receipt["validation"]["conditions"])
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError) as error:
        receipt["error"] = str(error)
    finally:
        receipt["raw_files"] = [{"path": p.relative_to(output).as_posix(), "sha256": v.digest(p),
            "bytes": p.stat().st_size} for p in sorted((output / "cab-timing-diagnostics").glob("*/*/*.csv"))]
        v.write_json(output / "receipt.json", receipt)
    return receipt


def validate_generator_probe(v, output, text, native_system):
    """Keep all 24 observations and native waveform equality; no time threshold."""
    require(native_system in JOB_MINUTES, "Unexpected generator probe platform")
    mac = native_system == "Darwin"
    rows_by_prefix = {}
    for prefix in ("GENERATOR_BENCHMARK", "GENERATOR_BENCHMARK_WARMUP",
                   "GENERATOR_BENCHMARK_SAMPLE", "PASS generator_prepared_benchmark"):
        rows_by_prefix[prefix] = [dict(v.ROW.findall(line)) for line in text.splitlines()
                                 if line.startswith(prefix + " ")]
    require(rows_by_prefix["PASS generator_prepared_benchmark"] == [GENERATOR_BENCHMARK_CONTRACT],
            "Missing/changed completed generator probe contract")
    configs = rows_by_prefix["GENERATOR_BENCHMARK"]
    require(len(configs) == 1 and all(configs[0].get(key) == value for key, value in {
        "source_reference": BASE_SHA, "order": "ABBA", "repeats": "3", "warmups_per_family": "2",
        "hash": "fnv1a64", "waveform_format": "f32le", "timing_threshold": "none"}.items()),
        "Changed generator protocol")
    validate_caller_context(configs[0], native_system)
    warmups = rows_by_prefix["GENERATOR_BENCHMARK_WARMUP"]
    require(len(warmups) == 2 and [row["family"] for row in warmups] == [x[0] for x in GENERATOR_FAMILIES],
            "Missing/duplicate generator warmup evidence")
    csv_path = output / "generator-benchmark.csv"
    with csv_path.open(newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        require(tuple(reader.fieldnames or ()) == GENERATOR_FIELDS, "Changed generator CSV schema")
        rows = list(reader)
    logged = rows_by_prefix["GENERATOR_BENCHMARK_SAMPLE"]
    require(len(rows) == len(logged) == 24, "Incomplete/duplicate generator population")
    waveforms, expected_paths, anchors = [], set(), {}
    previous_cpu, previous_cycles = -1.0, -1
    for index, row in enumerate(rows):
        family, model = GENERATOR_FAMILIES[index // 12]
        repeat, position = divmod(index % 12, 4)
        implementation = "frozen8097" if position in (0, 3) else "prepared"
        relative = f"waveforms/{family}-{repeat}-{position}-{implementation}.f32le"
        expected = {"schema_version": "1", "family": family, "model": model, "sample_rate": "96000",
            "repeat": str(repeat), "abba_index": str(position), "implementation": implementation,
            "samples": "8161", "float_bytes_identical": "1", "waveform": relative}
        require(all(row.get(key) == value for key, value in expected.items()), "Changed generator ABBA row identity")
        wall, cpu, first, last = (float(row[key]) for key in
            ("wall_us", "thread_cpu_us", "thread_cpu_start_us", "thread_cpu_end_us"))
        require(all(math.isfinite(value) for value in (wall, cpu, first, last)) and wall >= 0
                and row["clock_start_error"] == row["clock_end_error"] == "0", "Invalid generator clock evidence")
        if mac:
            require(row["thread_cpu_valid"] == "1" and first >= 0 and first >= previous_cpu and last >= first
                    and cpu >= 0 and math.isclose(cpu, last-first, rel_tol=1e-12, abs_tol=1e-8),
                    "Unpaired generator CPU sample")
            require(row["thread_cycles_valid"] == "0" and all(row[key] == "0" for key in
                ("thread_cycles", "thread_cycles_start", "thread_cycles_end")), "Unexpected generator macOS cycles")
            previous_cpu = last
        else:
            start_cycles, end_cycles, cycles = (int(row[key]) for key in
                ("thread_cycles_start", "thread_cycles_end", "thread_cycles"))
            require(row["thread_cpu_valid"] == "0" and cpu == first == last == -1
                    and row["thread_cycles_valid"] == "1" and start_cycles >= 0
                    and start_cycles >= previous_cycles and end_cycles >= start_cycles
                    and cycles == end_cycles-start_cycles, "Unpaired generator Windows cycle sample")
            previous_cycles = end_cycles
        same_strings = ("family", "repeat", "abba_index", "implementation", "samples",
                        "thread_cpu_valid", "thread_cycles", "thread_cycles_valid", "fnv1a64")
        require(all(logged[index].get(key) == row[key] for key in same_strings)
                and logged[index].get("float_bytes") == "identical"
                and same(logged[index]["wall_us"], wall) and same(logged[index]["thread_cpu_us"], cpu),
                "Generator stdout and raw row disagree")
        path = output / relative
        require(path.is_file() and not path.is_symlink(), "Missing generator waveform file")
        data = path.read_bytes()
        require(len(data) == 8161*4, "Truncated generator waveform")
        fingerprint = 14695981039346656037
        for value in data:
            fingerprint = ((fingerprint ^ value) * 1099511628211) & ((1 << 64)-1)
        require(str(fingerprint) == row["fnv1a64"], "Generator waveform fingerprint does not match row")
        if family not in anchors:
            require(implementation == "frozen8097", "Missing first frozen waveform anchor")
            anchors[family] = data
        require(data == anchors[family], "Generator waveform bytes differ from native frozen8097 observation")
        expected_paths.add(relative)
        waveforms.append({"path": relative, "bytes": len(data), "sha256": v.digest(path),
                          "fnv1a64_noncryptographic": str(fingerprint)})
    actual_paths = {path.relative_to(output).as_posix() for path in (output / "waveforms").rglob("*") if path.is_file()}
    require(actual_paths == expected_paths, "Unexpected/missing generator waveform files")
    for index, row in enumerate(warmups):
        require(row.get("generations") == "2" and row.get("samples") == "8161"
                and row.get("float_bytes") == "identical"
                and row.get("frozen_fnv1a64") == row.get("prepared_fnv1a64") == rows[index*12]["fnv1a64"],
                "Generator warmup equality evidence changed")
    return {"observations": 24, "warmup_generations": 4, "timing_threshold": None,
        "source_reference": BASE_SHA, "native_system": native_system,
        "caller_entry": caller_tuple(configs[0], native_system),
        "csv_sha256": v.digest(csv_path), "csv_bytes": csv_path.stat().st_size,
        "all_native_frozen_and_prepared_waveforms_identical": True, "waveforms": waveforms,
        "rows": rows, "warmups": warmups, "completion_record": GENERATOR_BENCHMARK_CONTRACT,
        "limits": "No local waveform hash is pinned across OSes. Same-job native bytes are compared directly. FNV is noncryptographic; SHA256 is retained separately. Clock brackets include uncalibrated overhead and cycles are never converted to time."}


def generator_probe(v, build, output):
    output.mkdir(parents=True, exist_ok=True)
    receipt = {"variant": "candidate", "diagnostic_only": True, "complete": False,
               "passed": False, "timing_threshold": None}
    try:
        require(not (output / "generator-benchmark.csv").exists() and not (output / "waveforms").exists(),
                "Generator probe must own a fresh output directory")
        path = binary(v, build, "ChimeraCabExpansionModelTests")
        receipt["binary_before_sha256"] = v.digest(path)
        receipt["command"] = bounded_run(v, [path, "--benchmark-prepared-generation", output],
                                          output, output / "console.log")
        require(not receipt["command"].get("not_started"), receipt["command"].get("reason", ""))
        receipt["validation"] = validate_generator_probe(v, output,
            (output / "console.log").read_text(encoding="utf-8", errors="replace"), platform.system())
        receipt["binary_after_sha256"] = v.digest(path)
        require(receipt["binary_before_sha256"] == receipt["binary_after_sha256"], "Generator probe binary changed")
        receipt["complete"] = True
        receipt["passed"] = receipt["command"]["returncode"] == 0
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError) as error:
        receipt["error"] = str(error)
    finally:
        receipt["preserved_files"] = [{"path": path.relative_to(output).as_posix(),
            "bytes": path.stat().st_size, "sha256": v.digest(path)}
            for path in sorted(output.rglob("*")) if path.is_file() and path.name != "receipt.json"]
        v.write_json(output / "receipt.json", receipt)
    return receipt


def disassemble(v, build, output):
    result = {"diagnostic_only": True, "available": False, "complete": False, "commands": []}
    if platform.system() != "Darwin":
        result["unavailable_reason"] = "ARM disassembly is only requested for native macOS; Windows PE architecture is recorded separately"
        v.write_json(output / "receipt.json", result)
        return result
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
    parser.add_argument("--candidate-source", type=Path, required=True)
    parser.add_argument("--build-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    sources = {"reference": args.reference_source.resolve(), "candidate": args.candidate_source.resolve()}
    build_root, output = args.build_root.resolve(), args.output.resolve()
    require(not output.exists() or not any(output.iterdir()), "Evidence output must be new or empty")
    output.mkdir(parents=True, exist_ok=True)
    v = load_driver(sources["reference"])
    native_system = platform.system()
    job_minutes = JOB_MINUTES.get(native_system, 0)
    manifest = {"schema": 1, "diagnostic_only": True, "base_sha": BASE_SHA, "base_tree": BASE_TREE,
        "harness_sha": args.harness_sha, "started_at": v.utc_now(), "result": "DIAGNOSTIC_INCOMPLETE",
        "acceptance_superseded": False, "order": [name for name, _ in ORDER],
        "prior_diagnostic": {"run_id": 38086565058,
            "harness_sha": "98614c53fcf9efffc6276c79023f9b874b1b123a",
            "generator_commit": "c460b52db5464b7641bc954cd95d900c40f54df7",
            "windows_candidate": "build_failed_far_macro_no_candidate_measurements",
            "macos_candidate": "both_normal_expanded_runs_failed_strict_p99_preserved_not_repeated",
            "repair": "test_identifier_far_to_farResponse_only_production_source_unchanged"},
        "worker_priority": "unchanged_low_for_both_variants", "overlay_sha256": OVERLAY_SHA256,
        "candidate_guard_reference_expected": CANDIDATE_GUARD_RECORDS,
        "workloads_per_abba_entry": list(WORKLOADS),
        "native_system": native_system,
        "profiles_after_abba": ["reference", "candidate"], "variants": {}, "measurements": [], "profiles": [],
        "generator_probe_after_all_profiles": {"variant": "candidate",
            "cli": ["--benchmark-prepared-generation", "<fresh-output>"],
            "contract": GENERATOR_BENCHMARK_CONTRACT, "timing_threshold": None},
        "expected_populations": {"normal_invocations": 8, "profile_invocations": 4,
            "normal_conditions": 168, "profile_conditions_separate": 84,
            "total_csv_files": 252, "total_callback_rows": 403200, "total_paired_records": 756},
        "limits": {"command_timeout_seconds": 600, "job_timeout_minutes": job_minutes, "build_parallel": 2,
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
            "Windows QueryThreadCycleTime cycles are retained with the same callback and never converted to time.",
            "Profile stage clocks and per-callback parameter refresh add overhead; nested stage totals are not additive.",
            "Native base QoS readback does not record effective priority, core placement, clock rate or prove a cause."]}
    v.write_json(output / "manifest.json", manifest)
    try:
        require(native_system == "Windows" and platform.machine().lower() in ("amd64", "x86_64"),
                "Compile-repair diagnostic requires native Windows x64")
        require(os.environ.get("GITHUB_REF") == BRANCH, "Diagnostic branch-only execution guard")
        require(os.environ.get("GITHUB_SHA") == args.harness_sha, "Harness argument differs from workflow head")
        job_start = float(os.environ["CAB_GENERATOR_JOB_STARTED_EPOCH"])
        elapsed = time.time() - job_start
        require(math.isfinite(job_start) and 0 <= elapsed < job_minutes*60, "Missing/invalid diagnostic budget origin")
        command_deadline = time.monotonic() + job_minutes*60 - PRESERVATION_SECONDS - elapsed
        manifest["budget_origin_epoch"] = job_start
        harness = Path(__file__).resolve().parent.parent
        manifest["harness_source"] = v.source_snapshot(harness, args.harness_sha)
        manifest["environment"] = v.environment_snapshot(output)
        expected = prepare_sources(v, sources, output)
        for label, source in sources.items():
            built = build_variant(v, source, build_root / label, output / label)
            manifest["variants"][label] = {"build": built}
            v.write_json(output / "manifest.json", manifest)
        dependency = {"available": False, "juce_working_tree_files_identical": False}
        if all(item["build"]["complete"] for item in manifest["variants"].values()):
            ledgers = {label: json.loads((output / label / "juce-source-files.json").read_text(encoding="utf-8"))
                       for label in sources}
            maps = {label: {row["path"]: row for row in rows} for label, rows in ledgers.items()}
            dependency = {"available": True, "juce_head_sha": JUCE_SHA,
                "juce_working_tree_files_identical": ledgers["reference"] == ledgers["candidate"],
                "file_counts": {label: len(rows) for label, rows in ledgers.items()},
                "different_paths": sorted(name for name in set(maps["reference"]) | set(maps["candidate"])
                    if maps["reference"].get(name) != maps["candidate"].get(name))}
        manifest["dependency_comparison"] = dependency
        v.write_json(output / "manifest.json", manifest)
        # Both builds finish before any tests. No test or profile overlaps another.
        for label in sources:
            if manifest["variants"][label]["build"]["complete"]:
                manifest["variants"][label]["guards"] = correctness(v, build_root / label, output / label / "guards", label)
                v.write_json(output / "manifest.json", manifest)
        for name, label in ORDER:
            for workload in WORKLOADS:
                if manifest["variants"][label]["build"]["complete"]:
                    run = measure(v, build_root / label, output / "runs" / name / workload, label, workload)
                else:
                    run = {"variant": label, "workload": workload, "complete": False,
                           "passed": False, "skipped": "variant build failed"}
                manifest["measurements"].append(dict(run, name=name + "/" + workload))
                v.write_json(output / "manifest.json", manifest)
        for label in sources:
            for workload in WORKLOADS:
                if manifest["variants"][label]["build"]["complete"]:
                    run = measure(v, build_root / label, output / "profiles" / label / workload,
                                  label, workload, profile=True)
                else:
                    run = {"variant": label, "workload": workload, "complete": False,
                           "passed": False, "skipped": "variant build failed"}
                manifest["profiles"].append(dict(run, name="profile-" + label + "/" + workload))
                v.write_json(output / "manifest.json", manifest)
        if manifest["variants"]["candidate"]["build"]["complete"]:
            manifest["generator_probe"] = generator_probe(v, build_root / "candidate", output / "generator-probe")
        else:
            manifest["generator_probe"] = {"variant": "candidate", "complete": False,
                "passed": False, "skipped": "candidate build failed", "timing_threshold": None}
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
        parts = [*manifest["measurements"], *manifest["profiles"], manifest["generator_probe"],
                 *(x.get("guards", {}) for x in manifest["variants"].values())]
        manifest["caller_context_comparison"] = compare_caller_context(
            [*manifest["measurements"], *manifest["profiles"]], native_system)
        context = manifest["caller_context_comparison"]
        probe_context = manifest["generator_probe"].get("validation", {}).get("caller_entry")
        context["generator_probe_entry"] = probe_context
        context["generator_probe_matches_timing_entry"] = (probe_context is not None
            and probe_context == context["reference_first_entry"])
        context["comparable"] = context["comparable"] and context["generator_probe_matches_timing_entry"]
        context["generator_probe_context_limit"] = "Generator context is observed at entry only; no per-observation scheduler trace is asserted."
        assembly_complete = all(not x.get("disassembly", {}).get("available")
            or x["disassembly"]["complete"] for x in manifest["variants"].values())
        complete = (len(manifest["measurements"]) == 8 and len(manifest["profiles"]) == 4
                    and all(x.get("complete") for x in parts) and assembly_complete
                    and manifest["caller_context_comparison"]["comparable"]
                    and manifest["dependency_comparison"]["juce_working_tree_files_identical"])
        manifest["evidence_complete"] = complete
        if complete:
            manifest["result"] = "DIAGNOSTIC_COMPLETE_ALL_GATES_PASSED" if all(x.get("passed") for x in parts) else "DIAGNOSTIC_COMPLETE_WITH_FAILURES"
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError) as error:
        manifest["error"] = str(error)
    finally:
        manifest["finished_at"] = v.utc_now()
        v.write_json(output / "manifest.json", manifest)
        lines = ["# CAB Windows/macOS generator-work diagnostic", "", f"Result: **{manifest['result']}**",
            "", f"Fixed base: {BASE_SHA}; harness: {args.harness_sha}.",
            "A is untouched 8097. B is the exact separately recorded source/test overlay; both retain low worker priority.",
            "This comparison never replaces the failed 8097 acceptance result.", "",
            "| Run | Complete | Exit | Strict p99 | Misses |", "|---|---|---:|---:|---:|"]
        for run in [*manifest["measurements"], *manifest["profiles"]]:
            evidence = run.get("validation", {})
            lines.append(f"| {run.get('name', 'profile-' + run['variant'])} | {run['complete']} | "
                f"{run.get('command', {}).get('returncode', 'unavailable')} | "
                f"{evidence.get('strict_p99_passes', 'unavailable')}/{evidence.get('conditions', 'unavailable')} | "
                f"{evidence.get('misses', 'unavailable')} |")
        probe = manifest.get("generator_probe", {})
        lines += ["", "Generator-only diagnostic after all timings: complete=" + str(probe.get("complete", False))
            + ", passed=" + str(probe.get("passed", False)) + ", observations="
            + str(probe.get("validation", {}).get("observations", "unavailable")) + "; no timing threshold.",
            "Native caller context comparable=" + str(manifest.get("caller_context_comparison", {}).get("comparable", False))
            + ". A mismatch leaves the comparison incomplete while preserving all observations.",
            "JUCE working-tree file ledgers identical="
            + str(manifest.get("dependency_comparison", {}).get("juce_working_tree_files_identical", False)) + "."]
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
