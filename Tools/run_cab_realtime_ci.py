#!/usr/bin/env python3
"""Run unchanged CAB deadline gates and retain source-bound comparison evidence.

The baseline and candidate are built separately on one runner and measured in
sequence. A failing baseline remains a failed reference; it never determines
candidate acceptance. The instrumented executable is a separate diagnostic.
Its own exit status is retained, but cannot replace the normal CTest verdict.
"""

from __future__ import annotations

import argparse
import csv
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import re
import shutil
import signal
import subprocess
import sys
import time
import xml.etree.ElementTree as ET


DEFAULT_TESTS = {
    "ChimeraOriginalCabIntegrationTests": ("original-v1", (64, 256)),
    "ChimeraCabExpansionIntegrationTests": ("expanded-v2", (64, 256)),
    "ChimeraCabLayoutIntegrationTests": ("array-v3-6x10", (64, 256)),
}
EXTENDED_TEST = "ChimeraCabExtendedTimingTests"
REALTIME_TEST = "ChimeraOriginalCabRealtimeTests"
PROFILE_STAGES = {
    "parameters", "kernel_swap", "filters", "active_convolution",
    "fading_convolution", "mic_post", "fft_forward", "fft_inverse", "spectral_mac",
}
WORKER_STAGES = {
    "worker_build", "response_generate", "kernel_prepare", "worker_publish", "worker_collect",
}
RATES = (44100, 48000, 96000)
PHASE_BLOCKS = {"worker_and_automation": 1200, "forced_six_slot_publication": 400}
ROW = re.compile(r"([A-Za-z_]\w*)=([^\s]+)")


def utc_now():
    return datetime.now(timezone.utc).isoformat()


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False, allow_nan=False) + "\n", encoding="utf-8")


def digest(path):
    sha = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            sha.update(block)
    return sha.hexdigest()


def capture(command, cwd=None):
    result = subprocess.run(command, cwd=cwd, text=True, encoding="utf-8", errors="replace",
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=60)
    if result.returncode:
        raise RuntimeError(f"{command[0]} returned {result.returncode}: {result.stdout[-3000:]}")
    return result.stdout.strip()


def source_snapshot(source, expected_sha):
    if not re.fullmatch(r"[0-9a-f]{40}", expected_sha):
        raise ValueError("Source SHA must be a complete lowercase 40-character Git SHA")
    head = capture(["git", "rev-parse", "HEAD"], source)
    status = capture(["git", "status", "--porcelain", "--untracked-files=all"], source)
    if head != expected_sha or status:
        raise RuntimeError(f"Source mismatch or dirty checkout at {source}: expected={expected_sha}, actual={head}, status={status}")
    return {"sha": head, "tree": capture(["git", "rev-parse", "HEAD^{tree}"], source),
            "clean": True, "path": str(source)}


def run_logged(command, cwd, log, timeout=1800):
    """Keep complete output on disk; show concise progress and exact error tails."""
    log.parent.mkdir(parents=True, exist_ok=True)
    print(f"Running {' '.join(str(arg) for arg in command)}", flush=True)
    started = time.monotonic()
    # The CTest entries also unset tracing. Apply the same environment to the
    # diagnostic executable; no timer thresholds or worker load are changed.
    env = os.environ.copy()
    env.pop("CHIMERA_LIFECYCLE_TRACE", None)
    with log.open("w", encoding="utf-8") as output:
        group = {"creationflags": subprocess.CREATE_NEW_PROCESS_GROUP} if os.name == "nt" else {"start_new_session": True}
        process = subprocess.Popen([str(arg) for arg in command], cwd=cwd, env=env,
                                   stdout=output, stderr=subprocess.STDOUT, **group)
        try:
            code = process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            # Do not leave compiler/test children consuming CPU when collecting
            # later evidence after a timeout on the same hosted runner.
            if os.name == "nt":
                subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"],
                               stdout=output, stderr=subprocess.STDOUT, timeout=30, check=False)
            else:
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
            if process.poll() is None:
                process.kill()
            process.wait(timeout=30)
            output.write(f"\nDRIVER ERROR: command timed out after {timeout} seconds\n")
            code = 124
    duration = time.monotonic() - started
    print(f"Exit {code}, {duration:.1f}s; complete output: {log}", flush=True)
    if code:
        print("\n".join(log.read_text(encoding="utf-8", errors="replace").splitlines()[-35:]), flush=True)
    return {"command": [str(arg) for arg in command], "cwd": str(cwd), "returncode": code,
            "seconds": duration, "log": str(log)}


def environment_snapshot(output):
    info = {"recorded_at": utc_now(), "uname": platform.uname()._asdict(),
            "cpu_count": os.cpu_count(), "python": sys.version,
            "runner": {key: os.environ.get(key) for key in (
                "RUNNER_OS", "RUNNER_ARCH", "RUNNER_ENVIRONMENT", "ImageOS", "ImageVersion",
                "GITHUB_REPOSITORY", "GITHUB_RUN_ID", "GITHUB_RUN_ATTEMPT", "GITHUB_JOB")}}
    for name in ("git", "cmake", "ctest"):
        info[name] = capture([name, "--version"])
    cpu_command = {"Linux": ["lscpu"], "Darwin": ["system_profiler", "SPHardwareDataType"],
                   "Windows": ["powershell", "-NoProfile", "-Command",
                               "Get-CimInstance Win32_Processor | Select-Object Name,NumberOfCores,NumberOfLogicalProcessors | ConvertTo-Json"]}.get(platform.system())
    if cpu_command:
        try:
            cpu_text = capture(cpu_command)
            # system_profiler also prints hardware serials/UUIDs. Keep CPU and
            # memory fields only; they are sufficient performance provenance.
            if platform.system() == "Darwin":
                cpu_text = "\n".join(line for line in cpu_text.splitlines() if any(
                    key in line for key in ("Model Name:", "Model Identifier:", "Chip:",
                                            "Processor Name:", "Processor Speed:", "Total Number of Cores:", "Memory:")))
            info["cpu_details"] = cpu_text
        except (OSError, subprocess.SubprocessError, RuntimeError) as error:
            info["cpu_details_unavailable"] = str(error)
    write_json(output / "environment.json", info)
    return info


def parse_rows(text):
    parsed = {"protocol": [], "timing": [], "phases": [], "profile": [], "worker_profile": []}
    context = {}
    for line in text.splitlines():
        if line.startswith("PHASE_CONFIG "):
            context = dict(ROW.findall(line))
        for prefix, kind in (("TIMING_PROTOCOL ", "protocol"), ("TIMING ", "timing"),
                             ("PHASE ", "phases"), ("PROFILE ", "profile"),
                             ("WORKER_PROFILE ", "worker_profile")):
            if line.startswith(prefix):
                row = (context.copy() if kind == "phases" else {})
                row.update(ROW.findall(line))
                parsed[kind].append(row)
                break
    return parsed


def route_key(row):
    return row["engine"], int(float(row["sr"])), int(row["block"]), int(row["channels"])


def validate_matrix(parsed, engine, buffers):
    """Check complete evidence without replacing the executable's p99 gate."""
    required = {(engine, sr, block, channels) for sr in RATES for block in buffers for channels in (1, 2)}
    keys = [route_key(row) for row in parsed["timing"]]
    if len(keys) != len(required) or set(keys) != required:
        raise RuntimeError(f"Missing or duplicate timing routes: wanted {len(required)}, got {len(keys)}; missing={sorted(required-set(keys))}")
    protocol = parsed["protocol"]
    if len(protocol) != 1 or any(protocol[0].get(key) != value for key, value in (
        ("lifecycle_trace", "disabled"), ("measured_blocks", "1600"),
        ("worker_blocks", "1200"), ("forced_publication_blocks", "400"))):
        raise RuntimeError("Missing or changed 1600-callback timing protocol")
    phases = [(route_key(row), row.get("name")) for row in parsed["phases"]]
    expected_phases = {(key, phase) for key in required for phase in PHASE_BLOCKS}
    if len(phases) != len(expected_phases) or set(phases) != expected_phases:
        raise RuntimeError("Incomplete worker/automation or forced-publication phase evidence")
    for row in parsed["phases"]:
        if int(row["blocks"]) != PHASE_BLOCKS[row["name"]]:
            raise RuntimeError("Changed phase callback count")
    for row in parsed["timing"]:
        for key in ("p50_us", "p99_us", "max_us", "kernel_residual", "peak", "max_step"):
            value = float(row[key])
            if not math.isfinite(value) or value < 0:
                raise RuntimeError(f"Invalid measured {key}: {row[key]}")
        misses, callbacks = map(int, row["misses"].split("/"))
        if callbacks != 1600 or not 0 <= misses <= callbacks:
            raise RuntimeError("Incomplete measured callback population")
        row["deadline_us"] = 1e6 * int(row["block"]) / float(row["sr"])
        row["p99_within_deadline"] = float(row["p99_us"]) < row["deadline_us"]


def test_evidence(junit, specs, include_realtime):
    root = ET.parse(junit).getroot()
    tests = root.findall("testcase")
    expected = set(specs) | ({REALTIME_TEST} if include_realtime else set())
    names = [test.get("name") for test in tests]
    if len(names) != len(expected) or set(names) != expected:
        raise RuntimeError(f"Unexpected CTest coverage: expected={sorted(expected)}, actual={names}")
    result = {"tests": [], "timing": [], "phases": []}
    for test in tests:
        name = test.get("name")
        text = test.findtext("system-out", "")
        if "This part of the test output was removed" in text:
            raise RuntimeError(f"CTest output was truncated for {name}")
        passed = test.get("status") == "run" and test.find("failure") is None and test.find("skipped") is None
        result["tests"].append({"name": name, "passed": passed, "seconds": float(test.get("time", "0"))})
        if name in specs:
            parsed = parse_rows(text)
            validate_matrix(parsed, *specs[name])
            for kind in ("timing", "phases"):
                result[kind].extend(dict(row, test=name) for row in parsed[kind])
    return result


def preserve_build_metadata(build, output):
    files = [build / "CMakeCache.txt", *build.glob("CMakeFiles/*/CMakeCXXCompiler.cmake")]
    for path in files:
        if path.is_file():
            destination = output / "build-metadata" / path.relative_to(build)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(path, destination)
    juce_source = build / "_deps" / "juce-src"
    if (juce_source / ".git").exists():
        write_json(output / "juce.json", {"sha": capture(["git", "rev-parse", "HEAD"], juce_source)})


def preserve_timing_samples(build, output, suites):
    """Retain complete or partial raw evidence, including when a gate fails."""
    root = build / "cab-timing-diagnostics"
    result = []
    for suite in suites:
        for path in sorted((root / suite).glob("*/*.csv")):
            destination = output / "cab-timing-diagnostics" / path.relative_to(root)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(path, destination)
            result.append({"path": destination.relative_to(output).as_posix(),
                           "bytes": destination.stat().st_size, "sha256": digest(destination)})
    return result


def validate_timing_samples(output, timing, profile=False):
    """Match CSV callbacks to the unchanged executable wall-time verdict.

    Clock values remain paired by callback index. Cycle deltas are never
    converted to time, and unavailable clocks do not invent a zero CPU cost.
    """
    files = []
    for summary in timing:
        engine, rate, block, channels = route_key(summary)
        suite = "profile" if profile else "extended" if summary["test"] == EXTENDED_TEST else "integration"
        path = output / "cab-timing-diagnostics" / suite / engine / f"{rate}-{block}-{channels}.csv"
        with path.open(newline="", encoding="utf-8") as stream:
            rows = list(csv.DictReader(stream))
        if len(rows) != 1600:
            raise RuntimeError(f"Incomplete raw callback evidence: {path} has {len(rows)} rows")
        walls, misses, valid_cpu, valid_cycles = [], 0, 0, 0
        previous_cpu_end, previous_cycles_end = -1.0, -1
        budget = 1e6 * block / rate
        for index, row in enumerate(rows):
            expected = {"schema_version": "1", "engine": engine, "suite": suite,
                        "sample_rate": str(rate), "block_size": str(block), "channels": str(channels),
                        "callback_index": str(index), "phase_index": str(index if index < 1200 else index - 1200),
                        "phase": "worker_and_automation" if index < 1200 else "forced_six_slot_publication",
                        "automation_requested": "1" if index < 80 else "0"}
            if any(row.get(key) != value for key, value in expected.items()):
                raise RuntimeError(f"Raw callback identity/phase mismatch at {path}:{index}")
            wall, recorded_budget = float(row["wall_us"]), float(row["budget_us"])
            if not math.isfinite(wall) or wall < 0 or not math.isclose(recorded_budget, budget, rel_tol=1e-12):
                raise RuntimeError(f"Invalid raw wall time or changed deadline at {path}:{index}")
            miss = wall > budget
            if row["deadline_miss"] != str(int(miss)):
                raise RuntimeError(f"Raw deadline miss mismatch at {path}:{index}")
            for key in ("thread_cpu_valid", "thread_cycles_valid"):
                if row[key] not in ("0", "1"):
                    raise RuntimeError(f"Invalid raw clock validity at {path}:{index}")
            cpu = float(row["thread_cpu_us"])
            if row["thread_cpu_valid"] == "1":
                first, last = float(row["thread_cpu_start_us"]), float(row["thread_cpu_end_us"])
                if (not all(math.isfinite(value) for value in (first, last, cpu))
                        or first < 0 or first < previous_cpu_end or last < first
                        or not math.isclose(cpu, last - first, rel_tol=1e-12, abs_tol=1e-8)):
                    raise RuntimeError(f"Unpaired thread CPU sample at {path}:{index}")
                valid_cpu += 1
                previous_cpu_end = last
            elif cpu != -1:
                raise RuntimeError(f"Unavailable thread CPU must remain -1 at {path}:{index}")
            if row["thread_cycles_valid"] == "1":
                first, last = int(row["thread_cycles_start"]), int(row["thread_cycles_end"])
                if first < 0 or first < previous_cycles_end or last < first or int(row["thread_cycles"]) != last - first:
                    raise RuntimeError(f"Unpaired thread cycle sample at {path}:{index}")
                valid_cycles += 1
                previous_cycles_end = last
            elif int(row["thread_cycles"]) != 0:
                raise RuntimeError(f"Invalid thread cycle sentinel at {path}:{index}")
            walls.append(wall)
            misses += miss
        walls.sort()
        for key, value in (("p50_us", walls[800]), ("p99_us", walls[1584]), ("max_us", walls[-1])):
            if not math.isclose(float(summary[key]), value, rel_tol=5e-11, abs_tol=1e-8):
                raise RuntimeError(f"Raw callbacks disagree with executable {key} at {path}")
        if summary["misses"] != f"{misses}/1600":
            raise RuntimeError(f"Raw callbacks disagree with executable deadline misses at {path}")
        files.append({"path": path.relative_to(output).as_posix(), "samples": len(rows),
                      "thread_cpu_valid_samples": valid_cpu, "thread_cycles_valid_samples": valid_cycles})
    if len({entry["path"] for entry in files}) != len(files):
        raise RuntimeError("Duplicate raw callback route references")
    return files


def executable(build, target):
    path = build / f"{target}_artefacts" / "Release" / (target + (".exe" if os.name == "nt" else ""))
    if not path.is_file():
        raise RuntimeError(f"Expected Release executable was not produced: {path}")
    return path


def run_version(label, source, expected_sha, build, output, candidate):
    output.mkdir(parents=True, exist_ok=True)
    receipt = {"label": label, "started_at": utc_now(), "complete": False,
               "configuration": "Release", "runner_architecture": platform.machine(),
               "commands": [], "source": None, "ctest_returncode": None, "passed": False}
    try:
        receipt["source"] = source_snapshot(source, expected_sha)
        write_json(output / "source-before.json", receipt["source"])
        configure = ["cmake", "-S", source, "-B", build, "-DCMAKE_BUILD_TYPE=Release"]
        if os.name == "nt":
            configure += ["-A", "x64"]
        elif platform.system() == "Darwin":
            configure += ["-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0",
                          f"-DCMAKE_OSX_ARCHITECTURES={platform.machine()}"]
        # Native macOS architecture isolates the DSP measurement from universal
        # packaging. Product CI continues to build its unchanged universal target.
        for command, log in ((configure, "configure.log"),
                             (["cmake", "--build", build, "--config", "Release", "--target",
                               "ChimeraOriginalCabIntegrationTests",
                               *(["ChimeraOriginalCabRealtimeTests"] if candidate else []),
                               "--parallel", "2"], "build.log")):
            record = run_logged(command, source, output / log)
            receipt["commands"].append(record)
            if record["returncode"]:
                raise RuntimeError(f"{label} {log} failed with exit {record['returncode']}")
        preserve_build_metadata(build, output)
        receipt["binaries"] = [{"target": target, "path": str(executable(build, target)),
                                 "sha256": digest(executable(build, target))} for target in
                                ["ChimeraOriginalCabIntegrationTests", *(["ChimeraOriginalCabRealtimeTests"] if candidate else [])]]
        specs = dict(DEFAULT_TESTS)
        if candidate:
            specs[EXTENDED_TEST] = ("expanded-v2", (32, 64, 128, 256, 512))
        names = set(specs) | ({REALTIME_TEST} if candidate else set())
        regex = "^(" + "|".join(sorted(names)) + ")$"
        inventory = capture(["ctest", "--test-dir", str(build), "-C", "Release", "-R", regex, "--show-only=json-v1"])
        write_json(output / "ctest-inventory.json", json.loads(inventory))
        listed = [test["name"] for test in json.loads(inventory)["tests"]]
        if len(listed) != len(names) or set(listed) != names:
            raise RuntimeError(f"Required CTest entries missing before execution: {sorted(names-set(listed))}")
        source_snapshot(source, expected_sha)
        junit = output / "ctest-results.xml"
        command = ["ctest", "--test-dir", build, "-C", "Release", "-R", regex,
                   "--parallel", "1", "--verbose", "--no-tests=error", "--timeout", "600",
                   "--test-output-size-passed", "10485760", "--test-output-size-failed", "10485760",
                   "--output-junit", junit]
        record = run_logged(command, source, output / "ctest-console.log")
        receipt["commands"].append(record)
        receipt["ctest_returncode"] = record["returncode"]
        # Copy before another CTest invocation could replace the full raw log.
        last_test = build / "Testing" / "Temporary" / "LastTest.log"
        if not last_test.is_file():
            raise RuntimeError(f"Full CTest LastTest.log is missing for {label}")
        shutil.copyfile(last_test, output / "LastTest.log")
        evidence = test_evidence(junit, specs, candidate)
        receipt.update(evidence)
        if candidate:
            preserve_timing_samples(build, output, ("integration", "extended"))
            receipt["timing_sample_validation"] = validate_timing_samples(output, evidence["timing"])
        receipt["source_after"] = source_snapshot(source, expected_sha)
        if receipt["source_after"] != receipt["source"]:
            raise RuntimeError(f"{label} source changed during execution")
        receipt["passed"] = record["returncode"] == 0 and all(test["passed"] for test in evidence["tests"]) and all(row["p99_within_deadline"] for row in evidence["timing"])
        receipt["complete"] = True
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError, ET.ParseError) as error:
        receipt["error"] = str(error)
        print(f"{label} evidence error: {error}", flush=True)
    finally:
        try:
            preserve_build_metadata(build, output)
            receipt["timing_sample_files"] = preserve_timing_samples(build, output, ("integration", "extended"))
        except (OSError, RuntimeError, subprocess.SubprocessError) as error:
            receipt["metadata_error"] = str(error)
            receipt["complete"] = False
            receipt["passed"] = False
        receipt["finished_at"] = utc_now()
        write_json(output / "receipt.json", receipt)
    return receipt


def run_profile(source, expected_sha, build, output, target):
    output.mkdir(parents=True, exist_ok=True)
    receipt = {"diagnostic_only": True, "complete": False, "commands": []}
    try:
        receipt["source"] = source_snapshot(source, expected_sha)
        build_record = run_logged(["cmake", "--build", build, "--config", "Release",
                                   "--target", target, "--parallel", "2"], source, output / "build.log")
        receipt["commands"].append(build_record)
        if build_record["returncode"]:
            raise RuntimeError("Diagnostic profiling target did not build")
        binary = executable(build, target)
        receipt["binary"] = {"path": str(binary), "sha256": digest(binary)}
        # Diagnostics write beside CTest evidence, never into the clean source.
        record = run_logged([binary, "--expanded"], build, output / "profile.log", timeout=600)
        receipt["commands"].append(record)
        receipt["returncode"] = record["returncode"]
        parsed = parse_rows((output / "profile.log").read_text(encoding="utf-8", errors="replace"))
        validate_matrix(parsed, "expanded-v2", (64, 256))
        if not PROFILE_STAGES.issubset({row.get("stage") for row in parsed["profile"]}):
            raise RuntimeError("Required callback stage profile rows are missing")
        if not WORKER_STAGES.issubset({row.get("stage") for row in parsed["worker_profile"]}):
            raise RuntimeError("Required IR worker profile rows are missing")
        receipt.update(parsed)
        preserve_timing_samples(build, output, ("profile",))
        receipt["timing_sample_validation"] = validate_timing_samples(output, parsed["timing"], profile=True)
        receipt["source_after"] = source_snapshot(source, expected_sha)
        receipt["complete"] = True
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError) as error:
        receipt["error"] = str(error)
        print(f"Diagnostic evidence error: {error}", flush=True)
    try:
        receipt["timing_sample_files"] = preserve_timing_samples(build, output, ("profile",))
    except OSError as error:
        receipt["sample_preservation_error"] = str(error)
        receipt["complete"] = False
    write_json(output / "receipt.json", receipt)
    return receipt


def comparison_rows(baseline, candidate):
    reference = {(row["test"], route_key(row)): row for row in baseline.get("timing", [])}
    result = []
    for row in candidate.get("timing", []):
        previous = reference.get((row["test"], route_key(row)))
        if previous is None:
            continue  # New extended-buffer routes have no baseline test equivalent.
        before, after = float(previous["p99_us"]), float(row["p99_us"])
        result.append({"test": row["test"], "engine": row["engine"], "sr": int(float(row["sr"])),
                       "block": int(row["block"]), "channels": int(row["channels"]),
                       "deadline_us": row["deadline_us"], "baseline_p99_us": before,
                       "candidate_p99_us": after, "p99_reduction_percent": 100 * (before-after) / before if before else None,
                       "baseline_within_deadline": previous["p99_within_deadline"],
                       "candidate_within_deadline": row["p99_within_deadline"],
                       "baseline_misses": previous["misses"], "candidate_misses": row["misses"]})
    return result


def write_summary(output, manifest):
    baseline, candidate = manifest.get("baseline", {}), manifest.get("candidate", {})
    lines = ["# CAB realtime performance", "", f"Result: **{manifest['result']}**", "",
             f"Candidate: `{manifest['candidate_sha']}`", f"Baseline: `{manifest['baseline_sha']}`", "",
             "Both revisions run sequentially on this runner, in separate Release builds.",
             "The baseline is a measured reference. Candidate CTest and its unchanged p99 deadlines determine acceptance.", "",
             "| Evidence | Complete | CTest exit | Deadline routes passing |", "|---|---|---:|---:|"]
    for name, data in (("Baseline", baseline), ("Candidate", candidate)):
        rows = data.get("timing", [])
        lines.append(f"| {name} | {data.get('complete', False)} | {data.get('ctest_returncode', 'unavailable')} | {sum(row['p99_within_deadline'] for row in rows)}/{len(rows)} |")
    profile = manifest.get("profile", {})
    lines += ["", f"Instrumented diagnostic: complete={profile.get('complete', False)}, exit={profile.get('returncode', 'unavailable')}. Its timing does not replace CTest acceptance.", "",
              "## Same-runner 96 kHz / 64-sample comparison", "",
              "| Engine | Channels | Baseline p99 µs | Candidate p99 µs | Deadline µs | Baseline / candidate misses |", "|---|---:|---:|---:|---:|---|"]
    for row in manifest.get("comparison", []):
        if row["sr"] == 96000 and row["block"] == 64:
            lines.append(f"| {row['engine']} | {row['channels']} | {row['baseline_p99_us']:.3f} | {row['candidate_p99_us']:.3f} | {row['deadline_us']:.3f} | {row['baseline_misses']} / {row['candidate_misses']} |")
    for name, data in (("Baseline", baseline), ("Candidate", candidate), ("Diagnostic", profile)):
        if data.get("error"):
            lines += ["", f"{name} error: `{data['error']}`"]
    if manifest.get("error"):
        lines += ["", f"Driver error: `{manifest['error']}`"]
    lines += ["", "Complete route/phase/profile tables, all candidate and diagnostic callback CSVs, full logs, compiler configuration, source/tree SHAs and file hashes are in the artifact.",
              "Raw wall time and CPU clocks are paired by callback index. Windows cycles remain cycles; they are not elapsed microseconds or a scheduling-only diagnosis.", ""]
    summary = "\n".join(lines)
    (output / "summary.md").write_text(summary, encoding="utf-8")
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with Path(os.environ["GITHUB_STEP_SUMMARY"]).open("a", encoding="utf-8") as stream:
            stream.write(summary)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate-source", type=Path, required=True)
    parser.add_argument("--candidate-sha", required=True)
    parser.add_argument("--baseline-source", type=Path, required=True)
    parser.add_argument("--baseline-sha", required=True)
    parser.add_argument("--build-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--profile-target", help="Optional diagnostic executable, built after the blocking tests")
    args = parser.parse_args()
    for key in ("candidate_source", "baseline_source", "build_root", "output"):
        setattr(args, key, getattr(args, key).resolve())
    args.output.mkdir(parents=True, exist_ok=True)
    manifest = {"schema": 1, "started_at": utc_now(), "candidate_sha": args.candidate_sha,
                "baseline_sha": args.baseline_sha, "order": ["baseline", "candidate", "diagnostic"],
                "result": "FAIL", "baseline_failure_is_candidate_failure": False}
    try:
        manifest["environment"] = environment_snapshot(args.output)
        # All measurement suites are sequential, including across revisions.
        baseline = run_version("baseline", args.baseline_source, args.baseline_sha,
                               args.build_root / "baseline", args.output / "baseline", False)
        manifest["baseline"] = baseline
        if not baseline["complete"]:
            raise RuntimeError("Baseline infrastructure or evidence failed; candidate measurements were not started")
        candidate = run_version("candidate", args.candidate_source, args.candidate_sha,
                                args.build_root / "candidate", args.output / "candidate", True)
        manifest.update(baseline=baseline, candidate=candidate)
        if args.profile_target and candidate.get("complete"):
            manifest["profile"] = run_profile(args.candidate_source, args.candidate_sha,
                                              args.build_root / "candidate", args.output / "diagnostic", args.profile_target)
        manifest["comparison"] = comparison_rows(baseline, candidate)
        # Recheck both sources after every executable, including diagnostics.
        source_snapshot(args.baseline_source, args.baseline_sha)
        source_snapshot(args.candidate_source, args.candidate_sha)
        complete = baseline["complete"] and candidate["complete"] and len(manifest["comparison"]) == 36
        if args.profile_target:
            complete = complete and manifest.get("profile", {}).get("complete", False)
        manifest["evidence_complete"] = complete
        if complete and candidate["passed"]:
            manifest["result"] = "PASS"
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError) as error:
        manifest["error"] = str(error)
        print(f"Driver error: {error}", flush=True)
    manifest["finished_at"] = utc_now()
    write_json(args.output / "manifest.json", manifest)
    write_json(args.output / "comparison.json", manifest.get("comparison", []))
    write_summary(args.output, manifest)
    # Hash only evidence files, never arbitrary local paths. This index is not
    # self-hashed; GitHub supplies the enclosing ZIP artifact digest as well.
    write_json(args.output / "SHA256SUMS.json", [{"path": str(path.relative_to(args.output)).replace("\\", "/"),
                                                "bytes": path.stat().st_size, "sha256": digest(path)}
                                               for path in sorted(args.output.rglob("*"))
                                               if path.is_file() and path.name != "SHA256SUMS.json"])
    print(f"CAB realtime result: {manifest['result']}; evidence: {args.output}", flush=True)
    return 0 if manifest["result"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
