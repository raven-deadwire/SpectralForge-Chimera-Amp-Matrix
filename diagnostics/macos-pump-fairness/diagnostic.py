"""One controlled original/candidate pair; no retries or release certification."""
from pathlib import Path
import difflib
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time

BASE = "7c782fda9e2d7cb255e70b6d1d1857fc78300c46"
RELATIVE = Path("diagnostics/macos-pump-fairness")
WORK = Path.cwd()
SOURCE, CONTROLLER = WORK / "source", WORK / "controller"
BUILD, EVIDENCE = WORK / "build-fairness", WORK / "fairness-evidence"
HELPER_SHA = "6c06b177982ccbece3e278f5734c701bf0fd67288ffb6402583f65a79fa758b6"
HEADER_SHA = "3768899735474cf425a96592adfc7fb6319a6a41e7a8a9403741f4131b045b38"
JUCE_FILES = {
    "juce_events/native/juce_MessageManager_mac.mm": "05989ffd8f3c81f7bc45716fd6912ad683715519948fd30b7dbb46618f439803",
    "juce_events/native/juce_MessageQueue_mac.h": "34b360544f8e565e188ebbf89d9669013c7be7d746fdbe2531f7b2be9707a649",
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
    assert not git(SOURCE, "status", "--porcelain")
    controller = git(CONTROLLER, "rev-parse", "HEAD")
    assert controller == os.environ["GITHUB_SHA"]
    assert git(CONTROLLER, "rev-parse", "HEAD^") == BASE
    allowed = {".github/workflows/build.yml"} | {
        str(RELATIVE / name) for name in ("FairnessProbe.mm", "targets.cmake", "diagnostic.py", "README.md")
    }
    assert set(git(CONTROLLER, "diff", "--name-only", BASE, "HEAD").splitlines()) == allowed
    original, header = SOURCE / "Tests/NativeMessageLoop.mm", SOURCE / "Tests/NativeMessageLoop.h"
    assert sha(original) == HELPER_SHA and sha(header) == HEADER_SHA
    assert "GIT_TAG 8.0.8)" in (SOURCE / "CMakeLists.txt").read_text()
    target = SOURCE / RELATIVE
    target.mkdir(parents=True, exist_ok=False)
    for name in ("FairnessProbe.mm", "targets.cmake"):
        shutil.copy2(CONTROLLER / RELATIVE / name, target / name)
    old = b"CFRunLoopRunInMode(kCFRunLoopDefaultMode,juce::jmin(.01,remaining*.001),true);"
    new = old.replace(b",true);", b",false);")
    original_bytes = original.read_bytes()
    assert original_bytes.count(old) == 1
    candidate_bytes = original_bytes.replace(old, new)
    candidate = target / "candidate/NativeMessageLoop.mm"
    candidate.parent.mkdir()
    candidate.write_bytes(candidate_bytes)
    assert candidate_bytes.replace(new, old) == original_bytes
    (EVIDENCE / "candidate-helper.patch").write_text("".join(difflib.unified_diff(
        original_bytes.decode().splitlines(True), candidate_bytes.decode().splitlines(True),
        fromfile="original/Tests/NativeMessageLoop.mm", tofile="candidate/Tests/NativeMessageLoop.mm")))
    for label, path in (("original-helper.mm", original), ("candidate-helper.mm", candidate),
                        ("NativeMessageLoop.h", header), ("FairnessProbe.mm", target / "FairnessProbe.mm"),
                        ("targets.cmake", target / "targets.cmake"), ("original-CMakeLists.txt", SOURCE / "CMakeLists.txt"),
                        ("controller-workflow.yml", CONTROLLER / ".github/workflows/build.yml")):
        shutil.copy2(path, EVIDENCE / label)
    cmake = SOURCE / "CMakeLists.txt"
    cmake.write_bytes(cmake.read_bytes() + b'\ninclude("${CMAKE_CURRENT_SOURCE_DIR}/diagnostics/macos-pump-fairness/targets.cmake")\n')
    assert git(SOURCE, "diff", "--name-only") == "CMakeLists.txt"
    save("provenance.json", dict(source_sha=BASE, controller_sha=controller,
        run_id=os.environ["GITHUB_RUN_ID"], run_attempt=os.environ["GITHUB_RUN_ATTEMPT"],
        workflow_ref=os.environ["GITHUB_WORKFLOW_REF"], runner_image=os.environ.get("ImageOS"),
        runner_image_version=os.environ.get("ImageVersion"),
        helper_sha256=dict(original=sha(original), candidate=sha(candidate), header=sha(header)),
        probe_sha256=sha(target / "FairnessProbe.mm"), release_acceptance=False,
        original_gui_failure_reproduced=False, purpose="Controlled injected AppKit-poll scheduling workload"))


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
    selected = [entry for entry in commands if any("ChimeraPumpFairness" in str(value)
                                                 for value in entry.values())]
    assert selected
    allowed = {(SOURCE / "Tests/NativeMessageLoop.mm").resolve(),
               (SOURCE / RELATIVE / "candidate/NativeMessageLoop.mm").resolve(),
               (SOURCE / RELATIVE / "FairnessProbe.mm").resolve()}
    for entry in selected:
        path = Path(entry["file"]).resolve()
        assert path in allowed or any((juce / "modules" / module).resolve() in path.parents
                                      for module in ("juce_core", "juce_events")), path
    save("probe-compile-commands.json", selected)
    save("juce-source.json", dict(tag="8.0.8", commit=head, normalized_sha256=JUCE_FILES,
                                 modal_filter_symbol="defined in juce_events; no substitute"))


def run_pair():
    paths = (BUILD / "fairness-executables-Release.txt").read_text().splitlines()
    assert len(paths) == 2 and len(set(paths)) == 2
    records = []
    for variant, path in zip(("Original", "Candidate"), map(Path, paths)):
        assert path.is_file() and path.name == "ChimeraPumpFairness" + variant
        result_path = EVIDENCE / (variant.lower() + "-result.json")
        assert not result_path.exists()
        record = dict(variant=variant, binary_sha256=sha(path), attempts=1)
        started = time.monotonic()
        with (EVIDENCE / (variant.lower() + "-process.log")).open("wb") as log:
            try:
                record["process_exit"] = subprocess.run([str(path), str(result_path)],
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
    # Both processes have now run exactly once, regardless of the first exit.
    def common(record, variant):
        result = record.get("result", {})
        return (result.get("variant") == variant and result.get("source_sha") == BASE
                and result.get("posts_succeeded") == 13 and result.get("polls") == 1
                and result.get("budget_ms") == 10 and result.get("artificial_poll_ms") == 20
                and result.get("poll_block_ms", 0) >= 10 and result.get("fifo") is True
                and all(result.get(key) is True for key in ("untouched_at_arm", "pump_continues",
                    "native_subclass_verified", "main_thread_verified", "outcome_frozen_before_cleanup", "warmup_passed"))
                and all(result.get(key) is False for key in ("stop_sent", "wrong_thread", "release_acceptance")))
    original, candidate = records
    red, green = original.get("result", {}), candidate.get("result", {})
    controlled_red = (common(original, "Original") and original["process_exit"] == red.get("exit_code") == 1
        and red.get("marker") == "FAIL_FENCE_DEFERRED_BY_FIRST_APPKIT_POLL"
        and red.get("first_poll_delivered") == red.get("return_delivered") == 4
        and red.get("first_poll_order") == red.get("return_order") == list(range(4))
        and red.get("first_poll_fence") is False and red.get("return_fence") is False
        and red.get("fence_ms", 0) < 0)
    controlled_green = (common(candidate, "Candidate") and candidate["process_exit"] == green.get("exit_code") == 0
        and green.get("marker") == "PASS_FIFO_FENCE_BEFORE_FIRST_APPKIT_POLL"
        and green.get("first_poll_delivered") == green.get("return_delivered") == 12
        and green.get("first_poll_order") == green.get("return_order") == list(range(12))
        and green.get("first_poll_fence") is True and green.get("return_fence") is True
        and 0 <= green.get("fence_ms", -1) <= 10)
    verdict = dict(controlled_red=controlled_red, controlled_green=controlled_green, records=records,
        status="EXPECTED_RED_AND_GREEN" if controlled_red and controlled_green else "REQUIRED_PAIR_NOT_ESTABLISHED",
        release_acceptance=False, original_gui_failure_reproduced=False, retries=0)
    save("verdict.json", verdict)
    print(json.dumps(verdict, indent=2))
    return 0 if controlled_red and controlled_green else 1


if __name__ == "__main__":
    if len(sys.argv) != 2 or sys.argv[1] not in ("prepare", "inspect", "run"):
        raise SystemExit("Usage: diagnostic.py prepare|inspect|run")
    if sys.argv[1] == "prepare": prepare()
    elif sys.argv[1] == "inspect": inspect()
    else: raise SystemExit(run_pair())
