"""One original/U-candidate compatibility pair; no retry or release certification."""
from pathlib import Path
import difflib
import hashlib
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
import time

BASE = "3171605606e224c1ca63fc0f3827e992a19b481f"
CANDIDATE = "c291d7132ba33561db922e7d5a992ab40212345d"
CANDIDATE_BLOB = "bf059e183bba35b944a804d534185b056881c270"
RELATIVE = Path("diagnostics/macos-queue-compatibility")
WORK = Path.cwd()
SOURCE, CANDIDATE_SOURCE, CONTROLLER = (WORK / name for name in ("source", "candidate-source", "controller"))
BUILD, EVIDENCE = WORK / "build-compatibility", WORK / "compatibility-evidence"
ORIGINAL_SHA = "6c06b177982ccbece3e278f5734c701bf0fd67288ffb6402583f65a79fa758b6"
CANDIDATE_SHA = "23ed1c047145f68f592840facd68eb628fb4cfe591757c00a23836eaa58f08e2"
HEADER_SHA = "3768899735474cf425a96592adfc7fb6319a6a41e7a8a9403741f4131b045b38"
JUCE_FILES = {
    "juce_events/native/juce_MessageManager_mac.mm": "05989ffd8f3c81f7bc45716fd6912ad683715519948fd30b7dbb46618f439803",
    "juce_events/native/juce_MessageQueue_mac.h": "34b360544f8e565e188ebbf89d9669013c7be7d746fdbe2531f7b2be9707a649",
}
CONTROLLER_FILES = {".github/workflows/build.yml"} | {
    str(RELATIVE / name) for name in ("QueueCompatibilityProbe.mm", "targets.cmake", "diagnostic.py", "README.md")
}


def git(path, *args):
    return subprocess.check_output(["git", "-C", str(path), *args], text=True).strip()


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def save(name, value):
    (EVIDENCE / name).write_text(json.dumps(value, indent=2) + "\n")


def prepare():
    EVIDENCE.mkdir(exist_ok=False)
    assert git(SOURCE, "rev-parse", "HEAD") == BASE
    assert git(CANDIDATE_SOURCE, "rev-parse", "HEAD") == CANDIDATE
    assert not git(SOURCE, "status", "--porcelain")
    assert not git(CANDIDATE_SOURCE, "status", "--porcelain")
    controller = git(CONTROLLER, "rev-parse", "HEAD")
    assert controller == os.environ["GITHUB_SHA"]
    assert git(CONTROLLER, "rev-parse", "HEAD^") == BASE
    assert set(git(CONTROLLER, "diff", "--name-only", BASE, "HEAD").splitlines()) == CONTROLLER_FILES
    original = SOURCE / "Tests/NativeMessageLoop.mm"
    candidate_input = CANDIDATE_SOURCE / "Tests/NativeMessageLoop.mm"
    header = SOURCE / "Tests/NativeMessageLoop.h"
    assert sha(original) == ORIGINAL_SHA and sha(candidate_input) == CANDIDATE_SHA
    assert git(CANDIDATE_SOURCE, "rev-parse", "HEAD:Tests/NativeMessageLoop.mm") == CANDIDATE_BLOB
    assert sha(header) == sha(CANDIDATE_SOURCE / "Tests/NativeMessageLoop.h") == HEADER_SHA
    assert "GIT_TAG 8.0.8)" in (SOURCE / "CMakeLists.txt").read_text()
    old = "            CFRunLoopRunInMode(kCFRunLoopDefaultMode,juce::jmin(.01,remaining*.001),true);"
    new = "\n".join((
        "            // Test-only fairness: JUCE serves four messages per CF source callback.",
        "            // Keep servicing ready sources for this requested slice before AppKit",
        "            // polling, whose internal native work may outlast its event deadline.",
        old.replace(",true);", ",false);")))
    assert original.read_text().count(old) == 1
    assert original.read_text().replace(old, new) == candidate_input.read_text()
    target = SOURCE / RELATIVE
    target.mkdir(parents=True, exist_ok=False)
    for name in ("QueueCompatibilityProbe.mm", "targets.cmake"):
        shutil.copy2(CONTROLLER / RELATIVE / name, target / name)
    candidate = target / "candidate/NativeMessageLoop.mm"
    candidate.parent.mkdir()
    shutil.copy2(candidate_input, candidate)
    (EVIDENCE / "candidate-helper.patch").write_text("".join(difflib.unified_diff(
        original.read_text().splitlines(True), candidate.read_text().splitlines(True),
        fromfile="S/Tests/NativeMessageLoop.mm", tofile="U/Tests/NativeMessageLoop.mm")))
    for label, path in (("original-helper.mm", original), ("candidate-helper.mm", candidate),
                        ("NativeMessageLoop.h", header), ("QueueCompatibilityProbe.mm", target / "QueueCompatibilityProbe.mm"),
                        ("targets.cmake", target / "targets.cmake"), ("original-CMakeLists.txt", SOURCE / "CMakeLists.txt")):
        shutil.copy2(path, EVIDENCE / label)
    for relative in CONTROLLER_FILES:
        output = EVIDENCE / "controller" / relative
        output.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(CONTROLLER / relative, output)
    cmake = SOURCE / "CMakeLists.txt"
    cmake.write_bytes(cmake.read_bytes() + b'\ninclude("${CMAKE_CURRENT_SOURCE_DIR}/diagnostics/macos-queue-compatibility/targets.cmake")\n')
    assert git(SOURCE, "diff", "--name-only") == "CMakeLists.txt"
    save("provenance.json", dict(original_source_sha=BASE, candidate_source_sha=CANDIDATE,
        candidate_helper_git_blob=CANDIDATE_BLOB, controller_sha=controller,
        run_id=os.environ["GITHUB_RUN_ID"], run_attempt=os.environ["GITHUB_RUN_ATTEMPT"],
        workflow_ref=os.environ["GITHUB_WORKFLOW_REF"], runner_image=os.environ.get("ImageOS"),
        runner_image_version=os.environ.get("ImageVersion"),
        helper_sha256=dict(original=sha(original), candidate=sha(candidate), header=sha(header)),
        probe_sha256=sha(target / "QueueCompatibilityProbe.mm"), release_acceptance=False,
        original_gui_failure_reproduced=False, hardware_input_test=False,
        purpose="Actual application-defined AppKit queue, JUCE FIFO and explicit stop compatibility"))


def inspect():
    juce = BUILD / "_deps/juce-src"
    assert Path(git(juce, "rev-parse", "--show-toplevel")).resolve() == juce.resolve()
    head = git(juce, "rev-parse", "HEAD")
    assert head == git(juce, "rev-parse", "8.0.8^{commit}")
    for relative, digest in JUCE_FILES.items():
        data = (juce / "modules" / relative).read_bytes().replace(b"\r\n", b"\n")
        assert hashlib.sha256(data).hexdigest() == digest, relative
        (EVIDENCE / Path(relative).name).write_bytes(data)
    manager = (juce / "modules/juce_events/native/juce_MessageManager_mac.mm").read_text()
    assert manager.count("CheckEventBlockedByModalComps isEventBlockedByModalComps = nullptr;") == 1
    commands = json.loads((BUILD / "compile_commands.json").read_text())
    selected = [entry for entry in commands if any("ChimeraQueueCompatibility" in str(value)
                                                 for value in entry.values())]
    assert selected
    allowed = {(SOURCE / "Tests/NativeMessageLoop.mm").resolve(),
               (SOURCE / RELATIVE / "candidate/NativeMessageLoop.mm").resolve(),
               (SOURCE / RELATIVE / "QueueCompatibilityProbe.mm").resolve()}
    for entry in selected:
        path = Path(entry["file"]).resolve()
        assert path in allowed or any((juce / "modules" / module).resolve() in path.parents
                                      for module in ("juce_core", "juce_events")), path
        flags = entry.get("arguments") or shlex.split(entry["command"])
        assert "-O3" in flags and "-DNDEBUG" in flags and "-g" in flags, flags
        assert not any("sanitize" in flag or flag == "-O0" for flag in flags), flags
    save("probe-compile-commands.json", selected)
    save("juce-source.json", dict(tag="8.0.8", commit=head, normalized_sha256=JUCE_FILES,
        modal_filter_symbol="original juce_events definition; no replacement",
        optimization="normal Release -O3 -DNDEBUG, debug symbols added, no sanitizer"))


def executables():
    paths = (BUILD / "compatibility-executables-Release.txt").read_text().splitlines()
    assert len(paths) == 2 and len(set(paths)) == 2
    result = list(zip(("Original", "Candidate"), map(Path, paths)))
    for variant, binary in result:
        assert binary.is_file() and binary.name == "ChimeraQueueCompatibility" + variant
    return result


def symbols():
    records = []
    directory = EVIDENCE / "binaries"
    directory.mkdir(exist_ok=False)
    for variant, binary in executables():
        saved_binary = directory / binary.name
        shutil.copy2(binary, saved_binary)
        dsym = directory / (binary.name + ".dSYM")
        subprocess.run(["dsymutil", str(binary), "-o", str(dsym)], check=True)
        binary_uuid = subprocess.check_output(["dwarfdump", "--uuid", str(binary)], text=True)
        symbol_uuid = subprocess.check_output(["dwarfdump", "--uuid", str(dsym)], text=True)
        pattern = r"UUID: ([A-Fa-f0-9-]+) \(([^)]+)\)"
        assert re.findall(pattern, binary_uuid)
        assert sorted(re.findall(pattern, binary_uuid)) == sorted(re.findall(pattern, symbol_uuid))
        assert sha(binary) == sha(saved_binary)
        records.append(dict(variant=variant, binary_sha256=sha(binary), binary=str(binary),
            retained_binary=str(saved_binary), dsym=str(dsym), binary_uuid=binary_uuid,
            symbol_uuid=symbol_uuid, architecture=subprocess.check_output(["lipo", "-archs", str(binary)], text=True).strip()))
    save("binary-manifest.json", records)


def valid(record, variant):
    result = record.get("result", {})
    yes = ("initial_zero_budget_continues", "native_subclass_verified", "main_thread_verified",
           "native_dequeue_unmodified", "native_event_created", "untouched_before_pump",
           "callback_fifo", "fence_delivered", "progress_pump_continues", "progress_passed",
           "stop_phase_executed", "stop_posted", "stop_callback_called", "stop_callback_returned",
           "stop_flag", "stop_passed", "outcome_frozen_before_cleanup")
    no = ("native_wrong_thread", "callback_wrong_thread", "stop_before_stop_phase", "stop_wrong_thread",
          "stop_dispatch_return", "zero_budget_after_stop_return", "poll_substitution", "sleep_injection",
          "hardware_input_test", "gui_failure_reproduced", "release_acceptance")
    return (record.get("process_exit") == result.get("exit_code") == 0
        and result.get("marker") == "PASS_APPLICATION_EVENT_FIFO_AND_STOP"
        and result.get("variant") == variant and result.get("original_source_sha") == BASE
        and result.get("candidate_source_sha") == CANDIDATE
        and result.get("compiled_helper_source_sha") == (BASE if variant == "Original" else CANDIDATE)
        and result.get("native_event_type") == "NSEventTypeApplicationDefined"
        and result.get("native_event_posts") == result.get("native_deliveries") == result.get("native_super_returns") == 1
        and result.get("callback_posts") == 13 and result.get("callback_count") == 12
        and result.get("callback_order") == list(range(12)) and result.get("slice_ms") == 20
        and result.get("delivery_limit_ms") == 2000 and result.get("process_attempts") == 1
        and 0 <= result.get("fence_ms", -1) <= 2000 and 0 <= result.get("native_delivery_ms", -1) <= 2000
        and all(result.get(key) is True for key in yes) and all(result.get(key) is False for key in no))


def run_pair():
    manifest = json.loads((EVIDENCE / "binary-manifest.json").read_text())
    records = []
    for variant, binary in executables():
        expected = next(entry for entry in manifest if entry["variant"] == variant)
        assert sha(binary) == expected["binary_sha256"]
        result_path = EVIDENCE / (variant.lower() + "-result.json")
        assert not result_path.exists()
        record = dict(variant=variant, binary_sha256=sha(binary), attempts=1)
        started = time.monotonic()
        with (EVIDENCE / (variant.lower() + "-process.log")).open("wb") as log:
            try:
                record["process_exit"] = subprocess.run([str(binary), str(result_path)],
                    stdout=log, stderr=subprocess.STDOUT, timeout=30, check=False).returncode
            except subprocess.TimeoutExpired:
                record["process_exit"] = None
                record["timeout_seconds"] = 30
        record["wall_seconds"] = time.monotonic() - started
        if result_path.exists():
            try:
                record["result"] = json.loads(result_path.read_text())
            except (ValueError, OSError) as error:
                record["result_error"] = str(error)
        records.append(record)
        save(variant.lower() + "-execution.json", record)
    original_ok, candidate_ok = (valid(record, variant) for record, variant in zip(records, ("Original", "Candidate")))
    verdict = dict(status="BOTH_COMPATIBLE" if original_ok and candidate_ok else "COMPATIBILITY_NOT_ESTABLISHED",
        original_compatible=original_ok, candidate_compatible=candidate_ok, records=records,
        release_acceptance=False, original_gui_failure_reproduced=False, hardware_input_test=False,
        scope="Application-defined AppKit queue, JUCE FIFO and explicit stop return only", retries=0)
    save("verdict.json", verdict)
    print(json.dumps(verdict, indent=2))
    return 0 if original_ok and candidate_ok else 1


if __name__ == "__main__":
    if len(sys.argv) != 2 or sys.argv[1] not in ("prepare", "inspect", "symbols", "run"):
        raise SystemExit("Usage: diagnostic.py prepare|inspect|symbols|run")
    if sys.argv[1] == "prepare": prepare()
    elif sys.argv[1] == "inspect": inspect()
    elif sys.argv[1] == "symbols": symbols()
    else: raise SystemExit(run_pair())
