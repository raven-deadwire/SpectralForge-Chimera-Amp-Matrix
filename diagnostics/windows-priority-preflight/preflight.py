"""One Windows launch preflight; no DSP workload, retries or release acceptance."""
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import time

SOURCE_SHA = "781d03dd49bba51d6cfa7b1c6a0ec8d9e75a0333"
SOURCE_TREE = "50be25e58674f35bbe1eaf560d3dc0e66cfc60bf"
BASE_DRIVER_SHA256 = "cd68cf57c43e7cc74290040d29f25878c267cdac4e624f1860dcb25aa7501901"
DRIVER_SHA256 = "ececc24808409e63c838ab3fbfea6a5887a84f5a5d89dda8f6332b8a79daba6b"
RELATIVE = Path("diagnostics/windows-priority-preflight")
WORK = Path.cwd()
SOURCE, CONTROLLER = WORK / "source", WORK / "controller"
EVIDENCE = WORK / "priority-preflight-evidence"
INJECTED_ERROR = "INTENTIONAL_PREFLIGHT_PRIORITY_QUERY_FAILURE_AFTER_REAL_QUERY"
ALLOWED = {".github/workflows/build.yml"} | {
    (RELATIVE / name).as_posix() for name in
    ("preflight.py", "frozen_run_cab_realtime_ci.py", "README.md")
}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def save(path, value):
    temporary = path.with_name(path.name + ".part")
    temporary.write_text(json.dumps(value, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    temporary.replace(path)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def git(path, *args):
    return subprocess.check_output(["git", "-C", str(path), *args], timeout=10,
                                   text=True, encoding="utf-8").strip()


def self_priority():
    import ctypes
    from ctypes import wintypes
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.GetCurrentProcess.argtypes = []
    kernel.GetCurrentProcess.restype = wintypes.HANDLE
    kernel.GetPriorityClass.argtypes = [wintypes.HANDLE]
    kernel.GetPriorityClass.restype = wintypes.DWORD
    result = int(kernel.GetPriorityClass(kernel.GetCurrentProcess()))
    if not result:
        raise OSError(ctypes.get_last_error(), "GetPriorityClass(self) failed")
    return result


def child(mode, report_path):
    # No descendants or external work. The five-second wait is a liveness
    # window for the real retained-handle query and the intentional cleanup.
    record = {"mode": mode, "pid": os.getpid(), "parent_pid": os.getppid(),
              "priority_class": self_priority(), "python": sys.version,
              "self_expiry_seconds": 5, "descendants_created": False}
    print("PREFLIGHT_CHILD " + json.dumps(record, sort_keys=True), flush=True)
    # Publish readiness only after complete JSON and flushed child stdout.
    save(report_path, record)
    time.sleep(5)
    return 0


def source_identity():
    require(git(SOURCE, "rev-parse", "HEAD") == SOURCE_SHA, "Wrong immutable source")
    require(git(SOURCE, "rev-parse", "HEAD^{tree}") == SOURCE_TREE, "Wrong source tree")
    require(not git(SOURCE, "status", "--porcelain", "--untracked-files=all"), "Source is dirty")
    require(sha(SOURCE / "Tools/run_cab_realtime_ci.py") == BASE_DRIVER_SHA256,
            "Original source launcher bytes changed")


def prepare(record):
    require(os.name == "nt" and sys.version_info[:2] == (3, 12), "Windows Python 3.12 required")
    source_identity()
    controller_sha = git(CONTROLLER, "rev-parse", "HEAD")
    require(controller_sha == os.environ["GITHUB_SHA"], "Wrong controller SHA")
    require(git(CONTROLLER, "rev-parse", "HEAD^") == SOURCE_SHA, "Wrong controller parent")
    require(not git(CONTROLLER, "status", "--porcelain", "--untracked-files=all"), "Controller is dirty")
    require(set(git(CONTROLLER, "diff", "--name-only", SOURCE_SHA, "HEAD").splitlines()) == ALLOWED,
            "Controller changed paths outside the four allowed files")
    frozen = CONTROLLER / RELATIVE / "frozen_run_cab_realtime_ci.py"
    require(sha(frozen) == DRIVER_SHA256, "Frozen V2 launcher hash mismatch")
    copied = EVIDENCE / frozen.name
    shutil.copyfile(frozen, copied)
    require(sha(copied) == DRIVER_SHA256, "Imported V2 launcher hash mismatch")
    record["provenance"] = {"source_sha": SOURCE_SHA, "source_tree": SOURCE_TREE,
        "controller_sha": controller_sha, "controller_parent": SOURCE_SHA,
        "controller_files_sha256": {name: sha(CONTROLLER / name) for name in sorted(ALLOWED)},
        "base_driver_sha256": BASE_DRIVER_SHA256, "imported_driver_sha256": sha(copied),
        "python": sys.version, "python_executable": sys.executable,
        "python_executable_sha256": sha(Path(sys.executable)), "platform": platform.platform(),
        "run_id": os.environ["GITHUB_RUN_ID"], "run_attempt": os.environ["GITHUB_RUN_ATTEMPT"],
        "workflow_ref": os.environ["GITHUB_WORKFLOW_REF"],
        "runner": {key: os.environ.get(key) for key in
                   ("RUNNER_OS", "RUNNER_ARCH", "ImageOS", "ImageVersion")}}
    spec = importlib.util.spec_from_file_location("frozen_priority_driver", copied)
    driver = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(driver)
    return driver


def run_controls(driver, record):
    parent_initial = self_priority()
    record["parent_priority_class_initial"] = parent_initial
    real_query = driver.windows_child_priority_class
    owned_failure = {}
    for mode, measured in (("ordinary-default", False), ("measured-normal", True),
                           ("intentional-query-failure", True)):
        report_path = EVIDENCE / (mode + "-child.json")
        log_path = EVIDENCE / (mode + ".log")
        control = {"mode": mode, "attempts": 1, "measured": measured,
                   "parent_class_before": self_priority()}
        record["controls"].append(control)

        def injected_query(process):
            # Keep Popen, taskkill and all waits real and unwrapped. Only this
            # one query result is replaced by a clearly labeled setup failure.
            owned_failure["process"] = process
            observed = real_query(process)
            control["real_retained_handle_query"] = {"pid": process.pid,
                "handle_present": getattr(process, "_handle", None) is not None,
                "observed_class": observed}
            require(observed == subprocess.NORMAL_PRIORITY_CLASS, "Failure-control child is not NORMAL")
            deadline = time.monotonic() + 2
            while not report_path.is_file() and time.monotonic() < deadline and process.poll() is None:
                time.sleep(0.01)
            require(report_path.is_file(), "Failure-control child did not report readiness")
            ready = json.loads(report_path.read_text(encoding="utf-8"))
            require(ready["pid"] == process.pid and ready["priority_class"] == observed,
                    "Failure-control child identity disagrees with real query")
            control["owned_child_live_before_injection"] = process.poll() is None
            require(control["owned_child_live_before_injection"], "Failure-control child already exited")
            control["fault_injected"] = True
            raise RuntimeError(INJECTED_ERROR)

        if mode == "intentional-query-failure":
            driver.windows_child_priority_class = injected_query
        try:
            receipt = driver.run_logged([sys.executable, "-u", str(Path(__file__).resolve()),
                "--child", mode, str(report_path)], WORK, log_path, timeout=10, measured=measured)
            control["driver_receipt"] = receipt
        finally:
            driver.windows_child_priority_class = real_query
            control["parent_class_after"] = self_priority()
            process = owned_failure.get("process") if mode == "intentional-query-failure" else None
            if process is not None:
                control["owned_child_exit_at_driver_return"] = process.poll()
                if process.poll() is None:
                    # Emergency ownership cleanup cannot make the control pass.
                    control["controller_emergency_cleanup_needed"] = True
                    try:
                        process.kill()
                        process.wait(timeout=10)
                    except (OSError, subprocess.TimeoutExpired) as error:
                        control["controller_emergency_cleanup_error"] = str(error)

        require(control["parent_class_before"] == control["parent_class_after"] == parent_initial,
                "Parent priority changed")
        own = json.loads(report_path.read_text(encoding="utf-8"))
        control["child_self_report"] = own
        require(own["mode"] == mode and own["parent_pid"] == os.getpid(), "Child identity mismatch")
        output = log_path.read_text(encoding="utf-8")
        child_lines = [line for line in output.splitlines() if line.startswith("PREFLIGHT_CHILD ")]
        require(len(child_lines) == 1 and json.loads(child_lines[0].split(" ", 1)[1]) == own,
                "Child stdout and own-priority report disagree")
        priority = receipt.get("measured_process_priority")
        if not measured:
            require(receipt["returncode"] == 0 and priority is None, "Ordinary default control failed")
            require(own["priority_class"] > 0, "Ordinary child's effective class unavailable")
        else:
            require(priority["pid"] == own["pid"] and own["priority_class"] == subprocess.NORMAL_PRIORITY_CLASS,
                    "Measured receipt and child self-query disagree")
            require(priority["creationflags"] == (subprocess.CREATE_NEW_PROCESS_GROUP | subprocess.NORMAL_PRIORITY_CLASS),
                    "Measured launch flags lost the process-group flag")
            lines = [line for line in output.splitlines() if line.startswith("MEASURED_PROCESS_PRIORITY ")]
            require(len(lines) == 1 and json.loads(lines[0].split(" ", 1)[1]) == priority,
                    "Returned priority receipt and appended log metadata disagree")
            if mode == "measured-normal":
                require(receipt["returncode"] == 0 and priority["verified"] is True
                        and priority["observed_class"] == subprocess.NORMAL_PRIORITY_CLASS,
                        "Real measured NORMAL precondition failed")
            else:
                cleanup = priority.get("cleanup", {})
                require(control.get("fault_injected") is True and priority.get("error") == INJECTED_ERROR
                        and priority["verified"] is False and receipt["returncode"] == 1,
                        "Intentional query failure was not latched")
                require(cleanup.get("taskkill_returncode") == 0 and cleanup.get("direct_child_reaped") is True
                        and control.get("owned_child_exit_at_driver_return") not in (None, 0)
                        and not control.get("controller_emergency_cleanup_needed"),
                        "Real owned-child termination and reap were not established")
        control["status"] = "PASS"
    record["parent_priority_class_final"] = self_priority()
    require(record["parent_priority_class_final"] == parent_initial, "Final parent priority changed")


def main():
    EVIDENCE.mkdir(exist_ok=False)
    record = {"status": "FAILED_OR_INCOMPLETE", "controls": [], "attempts_per_control": 1,
        "created_at": datetime.now(timezone.utc).isoformat(), "native_windows_execution": True,
        "product_build": False, "timing_workload": False, "release_acceptance": False,
        "original_stall_reproduced": False, "proves_original_stalls_cured": False,
        "limits": ["Child workload self-expires after a five-second wait; run_logged timeout is ten seconds.",
            "Frozen V2 taskkill and cleanup waits retain their original thirty-second ceilings.",
            "Process-group evidence is the exact flag forwarded by the frozen driver, not a kernel group-membership query.",
            "Only the failure control injects a query exception after observing the real live owned child's NORMAL class.",
            "All Popen, subprocess.run, taskkill and wait operations are real and unwrapped.",
            "Ordinary control records its effective inherited/default class without requiring NORMAL.",
            "No child creates descendants; direct-child termination is independently checked."]}
    try:
        driver = prepare(record)
        run_controls(driver, record)
        source_identity()
        require(not git(CONTROLLER, "status", "--porcelain", "--untracked-files=all"), "Controller was modified")
        require(sha(EVIDENCE / "frozen_run_cab_realtime_ci.py") == DRIVER_SHA256, "Imported driver was modified")
        record["source_unchanged_after"] = True
        record["status"] = "PASS_NATIVE_LAUNCH_PREFLIGHT_ONLY"
    except Exception as error:
        record["error"] = type(error).__name__ + ": " + str(error)
    finally:
        save(EVIDENCE / "verdict.json", record)
    print(json.dumps(record, indent=2))
    return 0 if record["status"] == "PASS_NATIVE_LAUNCH_PREFLIGHT_ONLY" else 1


if __name__ == "__main__":
    sys.dont_write_bytecode = True
    if len(sys.argv) == 4 and sys.argv[1] == "--child":
        raise SystemExit(child(sys.argv[2], Path(sys.argv[3])))
    require(len(sys.argv) == 1, "No user parameters are accepted")
    raise SystemExit(main())
