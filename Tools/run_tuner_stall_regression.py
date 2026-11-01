#!/usr/bin/env python3
"""Compare exact historical tuner fixtures and diagnostic-only delayed copies.

Linux/GCC + Unix Makefiles helper; first build ChimeraPrecisionTunerIntegrationTests.
Never writes a tracked fixture, production header, approval, or digest pin. Results
are scheduling experiments, not a substitute for the native platform CI gates.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shlex
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = "Tests/PrecisionTunerIntegrationTests.cpp"
VARIANTS = {
    "short": ("9163ec6aa2d9591f592c6b4a8765b70bcc6f07bd",
              "1371469c87ec73b0c2edd24d3daebc5342d026d2bdbdcf456be28581bc1ecd60"),
    "long": ("f0ba8e9c9f9eca16553802e50f612d53c2ef51e9",
             "8f71de6fc444877dd306d4c9d2483ed5262557d513fdb1c32a489e629298c7fc"),
}


def run(command, **kwargs):
    return subprocess.run(command, check=True, **kwargs)


def blob(ref, path):
    return subprocess.check_output(["git", "show", f"{ref}:{path}"], cwd=ROOT)


def replace_once(data, old, new):
    if data.count(old) != 1:
        raise RuntimeError("Instrumentation anchor changed; inspect before running: " + repr(old))
    return data.replace(old, new, 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repetitions", type=int, default=3)
    args = parser.parse_args()
    if sys.platform != "linux" or args.repetitions < 1:
        parser.error("Linux and a positive repetition count are required")
    build, output = args.build_dir.resolve(), args.output.resolve()
    if output == ROOT or ROOT in output.parents:
        parser.error("Use an output directory outside the repository")
    output.mkdir(parents=True, exist_ok=True)
    target = build / "CMakeFiles/ChimeraPrecisionTunerIntegrationTests.dir"
    flags = {}
    for line in (target / "flags.make").read_text().splitlines():
        if line.startswith("CXX_") and " = " in line:
            key, value = line.split(" = ", 1)
            flags[key] = shlex.split(value)
    link = shlex.split((target / "link.txt").read_text())
    source_object = "CMakeFiles/ChimeraPrecisionTunerIntegrationTests.dir/Tests/PrecisionTunerIntegrationTests.cpp.o"
    if link.count(source_object) != 1 or link.count("-o") != 1:
        raise RuntimeError("Unexpected CMake link command")
    protected = [ROOT / FIXTURE, ROOT / "Source/GlobalDSP.h", ROOT / "Tools/preset_maintenance.py"]
    before = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in protected}
    metadata = {"schema": "chimera.tuner-stall-scheduling-experiment.v1",
                "source_sha": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
                "platform": platform.platform(), "compiler": subprocess.check_output([link[0], "--version"], text=True).splitlines()[0],
                "protected_before": before, "variants": {}, "results": [],
                "owner_approval_granted": False, "native_macos_windows_reproduced": False}
    helper = (b"#include <cstdlib>\n#include <chrono>\n#include <thread>\n"
              b"inline int schedulingDelay(const char* key) {const char* v=std::getenv(key);return v?std::atoi(v):0;}\n")
    global_source = (ROOT / "Source/GlobalDSP.h").read_bytes()
    delayed_global = replace_once(global_source, b"                testWorkerPaused.store(false);",
        b"                testWorkerPaused.store(false);\n"
        b"                std::this_thread::sleep_for(std::chrono::milliseconds(schedulingDelay(\"CHIMERA_DIAG_RESUME_MS\")));")
    binaries = {}
    for name, (ref, digest) in VARIANTS.items():
        original = blob(ref, FIXTURE)
        if hashlib.sha256(original).hexdigest() != digest:
            raise RuntimeError("Historical fixture identity mismatch: " + name)
        metadata["variants"][name] = {"fixture_ref": ref, "fixture_sha256": digest}
        for instrumented in (False, True):
            key = name + ("-delayed" if instrumented else "-exact")
            folder = output / key
            folder.mkdir(exist_ok=True)
            source = original
            if instrumented:
                source = replace_once(source,
                    b"            tuner.push(audio,true);\r\n        }\r\n        std::this_thread::sleep_for(std::chrono::milliseconds(280));",
                    b"            tuner.push(audio,true);\n"
                    b"            std::this_thread::sleep_for(std::chrono::milliseconds(schedulingDelay(\"CHIMERA_DIAG_PUSH_MS\")));\n"
                    b"        }\n"
                    b"        std::this_thread::sleep_for(std::chrono::milliseconds(280+schedulingDelay(\"CHIMERA_DIAG_STALE_MS\")));")
                source = helper + source
                (folder / "GlobalDSP.h").write_bytes(delayed_global)
            cpp, obj, executable = folder / "fixture.cpp", folder / "fixture.o", folder / "probe"
            cpp.write_bytes(source)
            command = [link[0], *flags["CXX_DEFINES"], "-I" + str(folder), "-I" + str(ROOT / "Tests"),
                       *flags["CXX_INCLUDES"], *flags["CXX_FLAGS"], "-c", str(cpp), "-o", str(obj)]
            run(command, cwd=build)
            command = [str(obj) if item == source_object else item for item in link]
            command[command.index("-o") + 1] = str(executable)
            run(command, cwd=build)
            binaries[key] = executable
            metadata["variants"][name][key + "_sha256"] = hashlib.sha256(source).hexdigest()
    profiles = [
        ("baseline", {}, {"short": True, "long": True}),
        ("producer-8ms-per-block", {"CHIMERA_DIAG_PUSH_MS": "8"}, {"short": False, "long": True}),
        ("stale-sleep-270ms-overshoot", {"CHIMERA_DIAG_STALE_MS": "270"}, {"short": False, "long": True}),
        ("worker-resume-400ms-delay", {"CHIMERA_DIAG_RESUME_MS": "400"}, {"short": False, "long": True}),
        ("beyond-long-pause-budget", {"CHIMERA_DIAG_STALE_MS": "1400"}, {"short": False, "long": False}),
    ]
    unexpected = False
    for name in VARIANTS:
        for profile, injected, expected in profiles:
            for repeat in range(args.repetitions):
                key = name + ("-exact" if profile == "baseline" else "-delayed")
                env = {k: v for k, v in os.environ.items() if not k.startswith("CHIMERA_DIAG_")}
                env.update(injected)
                start = time.monotonic()
                result = subprocess.run([str(binaries[key]), "--stale-only"], env=env,
                                        capture_output=True, text=True, timeout=30)
                log = f"{name}-{profile}-{repeat + 1}.log"
                (output / log).write_text(result.stdout + result.stderr)
                expected_failure = ("Worker did not resume after the pause" if "RESUME" in " ".join(injected)
                                    else "Worker resumed before stale observation check")
                matches = (result.returncode == 0 if expected[name] else
                           result.returncode != 0 and expected_failure in result.stderr)
                unexpected |= not matches
                row = {"variant": name, "profile": profile, "repeat": repeat + 1,
                       "exit_code": result.returncode, "seconds": round(time.monotonic()-start, 3),
                       "expected_pass": expected[name], "matches_expectation": matches, "log": log}
                metadata["results"].append(row)
                print(json.dumps(row), flush=True)
    metadata["protected_after"] = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in protected}
    if before != metadata["protected_after"]:
        raise RuntimeError("Protected worktree content changed during experiment")
    metadata["matches_all_expectations"] = not unexpected
    (output / "results.json").write_text(json.dumps(metadata, indent=2) + "\n")
    return 1 if unexpected else 0


if __name__ == "__main__":
    raise SystemExit(main())
