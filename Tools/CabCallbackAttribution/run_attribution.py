#!/usr/bin/env python3
"""Prespecified callback-attribution experiment; never a product performance gate.

A = same harness with source hooks compiled out; B = hooks compiled in and on.
This A/B intervention measures observer sensitivity, NOT the historical QoS
intervention and NOT a proposed DSP fix. Both inherited QoS and original gates
remain unchanged. Every allocation uses fresh build and evidence directories.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import shutil
import sys
import traceback

sys.dont_write_bytecode = True

FROZEN_SHA = "7b20cd9d1f90e3e2969c250712b199e893cf50cd"
JUCE_SHA = "d6181bde38d858c283c3b7bf699ce6340c050b5d"
TESTS = ("ChimeraOriginalCabIntegrationTests", "ChimeraOriginalCabRealtimeTests",
         "ChimeraCabExpansionIntegrationTests", "ChimeraCabExtendedTimingTests",
         "ChimeraCabLayoutIntegrationTests")
ENGINES = ("original-v1", "expanded-v2", "array-v3-6x10")
ORDERS = {"ABBA": ("A", "B", "B", "A"), "BAAB": ("B", "A", "A", "B")}
CONTROL_TARGET = "ChimeraCabCallbackProbeControl"
TRACE_TARGET = "ChimeraCabCallbackProbe"
SEMANTIC_TARGET = "ChimeraCabAttributionRealtimeTests"
TRACE_CONTRACT_TARGET = "ChimeraCabCallbackTraceTests"


def write_json(path, obj):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(obj, indent=2, ensure_ascii=False, allow_nan=False) + "\n")


def sha256(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for data in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(data)
    return result.hexdigest()


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def executable(build, target):
    suffix = ".exe" if os.name == "nt" else ""
    return build / f"{target}_artefacts/Release/{target}{suffix}"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--source-sha", required=True)
    parser.add_argument("--frozen-source", type=Path, required=True)
    parser.add_argument("--build-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--order", choices=tuple(ORDERS), required=True)
    parser.add_argument("--environment", choices=("native-macos", "portable-validation"),
                        default="native-macos")
    parser.add_argument("--juce-source", type=Path,
                        help="Optional already available JUCE checkout; SHA is verified")
    args = parser.parse_args()
    source, frozen, builds, output = (p.resolve() for p in
        (args.source, args.frozen_source, args.build_root, args.output))
    if args.environment == "native-macos" and platform.system() != "Darwin":
        raise RuntimeError("Native macOS evidence requires Darwin; choose portable-validation for other hosts")
    if output.exists() or builds.exists():
        raise RuntimeError("Preserve prior attempts: output and build-root must both be new")
    output.mkdir(parents=True)
    driver = verifier = None
    controller = source / "Tools/CabCallbackAttribution"
    report = {
        "schema_version": 1, "diagnostic_only": True,
        "source_sha": args.source_sha, "frozen_sha": FROZEN_SHA,
        "order": args.order, "planned_arms": list(ORDERS[args.order]),
        "A": "same harness, CHIMERA_CAB_ATTRIBUTION compiled out, trace off",
        "B": "same harness, CHIMERA_CAB_ATTRIBUTION compiled in, stages on",
        "qos_intervention": False, "environment_kind": args.environment,
        "scope": "console CAB-only intervals; no audio device, AU/VST3 or DAW",
        "audio_host_validation": "not_executed", "product_fix_demonstrated": False,
        "product_acceptance": False, "complete": False,
        "gates": [], "trials": [], "commands": [],
        "resource_order": "before arms" if args.order == "BAAB" else "after arms",
        "limitations": [
            "A/B quantify instrumentation sensitivity, not a proposed product performance improvement.",
            "Two binary hashes differ; source/harness parity is not binary identity.",
            "Worker-present actual publications are asynchronous, not assumed exposure-matched.",
            "CPU and wall clock envelopes differ; their subtraction is not exact scheduler time.",
            "Logical memory bytes and resource counter deltas do not measure cache miss latency.",
            "Resource-enabled populations are separately instrumented, never pooled with A/B.",
            "A complete console experiment does not verify physical-device or DAW playback.",
        ],
    }
    failure = None
    codes = []

    def logged(command, cwd, log, *, measured=False, timeout=1800):
        receipt = driver.run_logged(command, cwd, log, measured=measured, timeout=timeout)
        receipt["log"] = str(Path(receipt["log"]).relative_to(output))
        report["commands"].append(receipt)
        write_json(output / "report.partial.json", report)
        return receipt

    def configure(source_dir, build):
        command = ["cmake", "-S", source_dir, "-B", build,
                   "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"]
        if args.juce_source:
            command.append(f"-DFETCHCONTENT_SOURCE_DIR_JUCE={args.juce_source.resolve()}")
        if platform.system() == "Darwin":
            command += ["-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0",
                        f"-DCMAKE_OSX_ARCHITECTURES={platform.machine()}"]
        record = logged(command, source, output / "build-logs" / f"{build.name}-configure.log")
        if record["returncode"]:
            raise RuntimeError(f"{build.name} configure failed before measurement")

    def build_targets(build, targets):
        record = logged(["cmake", "--build", build, "--config", "Release", "--target",
                         *targets, "--parallel", "2"], source,
                        output / "build-logs" / f"{build.name}-build.log")
        if record["returncode"]:
            raise RuntimeError(f"{build.name} build failed before measurement")

    def gate(name, build):
        destination = output / "gates" / name
        destination.mkdir(parents=True)
        regex = "^(" + "|".join(TESTS) + ")$"
        inventory = json.loads(driver.capture(["ctest", "--test-dir", build, "-C", "Release",
                                               "-R", regex, "--show-only=json-v1"]))
        write_json(destination / "inventory.json", inventory)
        if sorted(t["name"] for t in inventory["tests"]) != sorted(TESTS):
            raise RuntimeError("The original five-test inventory changed")
        result = logged(["ctest", "--test-dir", build, "-C", "Release", "-R", regex,
                         "--parallel", "1", "--verbose", "--no-tests=error", "--timeout", "600",
                         "--test-output-size-passed", "10485760", "--test-output-size-failed", "10485760",
                         "--output-junit", destination / "ctest-results.xml"], source,
                        destination / "ctest-console.log", measured=True)
        # Preserve partial CSVs before parsing; failed/incomplete gates never disappear.
        if (build / "cab-timing-diagnostics").exists():
            shutil.copytree(build / "cab-timing-diagnostics", destination / "cab-timing-diagnostics")
        last_log = build / "Testing/Temporary/LastTest.log"
        if last_log.exists():
            shutil.copy2(last_log, destination / "LastTest.log")
        specs = dict(driver.DEFAULT_TESTS)
        specs[driver.EXTENDED_TEST] = ("expanded-v2", (32, 64, 128, 256, 512))
        evidence = driver.test_evidence(destination / "ctest-results.xml", specs, True)
        evidence["raw_files"] = driver.validate_timing_samples(destination, evidence["timing"])
        if len(evidence["timing"]) != 66:
            raise RuntimeError("Original acceptance population must contain all 66 routes")
        codes.append(result["returncode"])
        codes.append(0 if all(t["passed"] for t in evidence["tests"]) else 1)
        report["gates"].append({"name": name, "command": result, "evidence": evidence})
        write_json(destination / "evidence.json", evidence)

    def population(label, arm, level, resource=False):
        probe = executable(trace_build, TRACE_TARGET if arm == "B" else CONTROL_TARGET)
        entry = {"label": label, "arm": arm, "trace_level": level,
                 "resource_population": resource, "binary_sha256": sha256(probe), "modes": []}
        report["trials"].append(entry)
        for mode in ("cab", "arithmetic", "memory", "clock"):
            # Sentinels preserve rate, buffer, counts and pacing but have their
            # own workload and exposure. They are not CPU-time subtraction terms.
            engines = ENGINES if mode == "cab" else ("original-v1",)
            directory = output / "trials" / label / mode
            command = [probe, "--mode", mode, "--trace-level", level,
                       "--engine", "all" if mode == "cab" else "original-v1",
                       "--channels", "all", "--output-dir", directory]
            result = logged(command, source, output / "trials" / label / f"{mode}.log",
                            measured=True, timeout=900)
            codes.append(result["returncode"])
            mode_entry = {"mode": mode, "command": result}
            entry["modes"].append(mode_entry)
            try:
                checked = verifier.verify_directory(directory, expected_mode=mode,
                    expected_trace_level=level, expected_engines=engines, expected_channels=(1, 2))
                mode_entry["integrity"] = checked
                if checked.get("complete") is not True or checked.get("exit_code", 2) != 0:
                    codes.append(checked.get("exit_code", 2) or 2)
                write_json(output / "trials" / label / f"{mode}-verification.json", checked)
            except Exception as error:
                # Continue the registered arms even after a bad one. Missing or
                # corrupt data remains a failed trial; no replacement rerun.
                mode_entry["integrity_error"] = repr(error)
                codes.append(2)
            write_json(output / "report.partial.json", report)

    try:
        driver = load_module("frozen_attribution_gate_driver", frozen / "Tools/run_cab_realtime_ci.py")
        verifier = load_module("callback_attribution_verifier", source / "Tools/CabCallbackAttribution/verify_attribution.py")
        report["source_before"] = driver.source_snapshot(source, args.source_sha)
        report["frozen_before"] = driver.source_snapshot(frozen, FROZEN_SHA)
        report["environment_before"] = driver.environment_snapshot(output / "environment-before")
        report["controller_files"] = {p.name: sha256(p) for p in controller.iterdir() if p.is_file()}
        frozen_build, trace_build = builds / "frozen", builds / "attribution"
        configure(frozen, frozen_build)
        configure(controller, trace_build)
        build_targets(frozen_build, ("ChimeraOriginalCabIntegrationTests", "ChimeraOriginalCabRealtimeTests"))
        build_targets(trace_build, (CONTROL_TARGET, TRACE_TARGET, SEMANTIC_TARGET, TRACE_CONTRACT_TARGET,
                                   "ChimeraOriginalCabIntegrationTests", "ChimeraOriginalCabRealtimeTests"))
        for name, build in (("frozen", frozen_build), ("attribution", trace_build)):
            metadata = output / "build-metadata" / name
            metadata.mkdir(parents=True)
            for path in (build / "CMakeCache.txt", build / "compile_commands.json", build / "attribution-source.txt",
                         *build.glob("CMakeFiles/*/CMakeCXXCompiler.cmake"),
                         *build.glob("CMakeFiles/*/CMakeOBJCXXCompiler.cmake")):
                if path.exists():
                    shutil.copy2(path, metadata / path.name)
            juce = args.juce_source.resolve() if args.juce_source else build / "_deps/juce-src"
            revision = driver.capture(["git", "rev-parse", "HEAD"], juce)
            if revision != JUCE_SHA:
                raise RuntimeError(f"{name} JUCE dependency changed: {revision}")
            write_json(metadata / "juce.json", {"sha": revision,
                "fft_source_sha256": sha256(juce / "modules/juce_dsp/frequency/juce_FFT.cpp")})
        report["binaries"] = {name: sha256(executable(trace_build, name))
                              for name in (CONTROL_TARGET, TRACE_TARGET, SEMANTIC_TARGET)}
        contract_executable = trace_build / (TRACE_CONTRACT_TARGET + (".exe" if os.name == "nt" else ""))
        report["binaries"][TRACE_CONTRACT_TARGET] = sha256(contract_executable)
        contract = logged([contract_executable], source, output / "gates/trace-contract.log", timeout=60)
        report["trace_contract"] = contract
        codes.append(contract["returncode"])
        # Finish every compilation before the first measured population.
        gate("frozen-original", frozen_build)
        gate("current-hooks-off", trace_build / "product")
        semantic = logged([executable(trace_build, SEMANTIC_TARGET)], source,
                          output / "gates/enabled-semantics.log", measured=True, timeout=600)
        report["enabled_semantics"] = semantic
        codes.append(semantic["returncode"])
        if args.order == "BAAB":
            population("resources-before", "B", "resources", True)
        for number, arm in enumerate(ORDERS[args.order], 1):
            population(f"{number:02d}-{arm}", arm, "off" if arm == "A" else "stages")
        if args.order == "ABBA":
            population("resources-after", "B", "resources", True)
        report["source_after"] = driver.source_snapshot(source, args.source_sha)
        report["frozen_after"] = driver.source_snapshot(frozen, FROZEN_SHA)
        report["environment_after"] = driver.environment_snapshot(output / "environment-after")
        report["complete"] = (len(report["trials"]) == 5 and
                              all(len(t["modes"]) == 4 for t in report["trials"]) and
                              all(m.get("integrity", {}).get("complete") is True
                                  for t in report["trials"] for m in t["modes"]))
        report["all_required_commands_passed"] = all(code == 0 for code in codes)
    except BaseException as error:
        failure = error
        report["error"] = repr(error)
        (output / "driver-error.log").write_text(traceback.format_exc())
    finally:
        report["finished_utc"] = datetime.now(timezone.utc).isoformat()
        report["recorded_returncodes"] = codes
        write_json(output / "report.json", report)
        write_json(output / "SHA256SUMS.json", {str(p.relative_to(output)): sha256(p)
            for p in sorted(output.rglob("*")) if p.is_file() and p.name != "SHA256SUMS.json"})
    if failure:
        raise failure
    return 0 if report["complete"] and all(code == 0 for code in codes) else 1


if __name__ == "__main__":
    sys.exit(main())
