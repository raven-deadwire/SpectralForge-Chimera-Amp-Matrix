#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Compare native Windows OBS startup with Chimera absent or registered, without filters.

Uses a new OBS profile for every case and the app test's original startup RPC
sequence. Each failure remains a failure; subsequent cases are observations,
not retries. Refuses existing work, OBS configuration and Chimera installation.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import traceback
import uuid

import obs_app_test as app

OWNER = ".chimera-startup-probe-owner"


def write_json(path: Path, value) -> None:
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def hash_path(path: Path) -> dict:
    """Hash actual files, with relative names so two bundles can be compared."""
    files = [path] if path.is_file() else sorted(p for p in path.rglob("*") if p.is_file())
    if not files:
        raise RuntimeError(f"no files to hash: {path}")
    result = {}
    for file in files:
        digest = hashlib.sha256()
        with file.open("rb") as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(chunk)
        name = file.name if path.is_file() else file.relative_to(path).as_posix()
        result[name] = {"sha256": digest.hexdigest(), "size": file.stat().st_size}
    return {"path": str(path), "files": result}


def environment() -> dict:
    keys = ("ImageOS", "ImageVersion", "RUNNER_OS", "RUNNER_ARCH", "RUNNER_NAME",
            "GITHUB_SHA", "GITHUB_REF", "GITHUB_HEAD_REF", "GITHUB_RUN_ID", "GITHUB_RUN_ATTEMPT")
    try:
        source = subprocess.run(["git", "rev-parse", "HEAD"], cwd=Path(__file__).resolve().parent,
                                capture_output=True, text=True, check=True, timeout=5).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        source = None
    return {"platform": platform.platform(), "python": sys.version, "source_sha": source,
            "ci": {key: os.environ.get(key) for key in keys},
            "harness": hash_path(Path(app.__file__).resolve()),
            "probe": hash_path(Path(__file__).resolve())}


def case_order(repeats: int) -> list[dict]:
    return [{"repeat": repeat, "mode": mode}
            for repeat in range(1, repeats + 1)
            for mode in (("absent", "module") if repeat % 2 else ("module", "absent"))]


def refuse_existing(path: Path) -> None:
    if path.exists() or path.is_symlink():
        raise RuntimeError(f"refusing existing path: {path}")


def initial_sequence(obs: app.Obs, stop_after: str, di: Path) -> dict:
    """Do not insert probes, sleeps or extra readiness requests into this sequence."""
    obs.phase("probe-version")
    version = obs.request("GetVersion")
    if stop_after in ("scene", "input"):
        obs.phase("probe-scene")
        obs.request("CreateScene", {"sceneName": app.SCENE})
        obs.request("SetCurrentProgramScene", {"sceneName": app.SCENE})
    if stop_after == "input":
        obs.phase("probe-input")
        obs.request("CreateInput", {
            "sceneName": app.SCENE, "inputName": app.SOURCE, "inputKind": "ffmpeg_source",
            "inputSettings": {"is_local_file": True, "local_file": str(di), "looping": True,
                              "restart_on_activate": False}})
    return version


def verify_control(obs: app.Obs, module: bool, stop_after: str = "input") -> dict:
    obs.phase("probe-control-checks")
    kinds = obs.request("GetSourceFilterKindList")["sourceFilterKinds"]
    registered = app.FILTER_KIND in kinds
    if registered != module:
        raise RuntimeError(f"invalid {'module' if module else 'absent'} control: "
                           f"Chimera filter kind registered={registered}")
    if stop_after == "version":
        # Keep this variant out of the scene/input UI paths. Module registration
        # is queried, while the full log checks for unexpected instance activity.
        return {"chimera_registered": registered, "source_filters_queried": False}
    scenes = obs.request("GetSceneList")["scenes"]
    inputs = obs.request("GetInputList")["inputs"]
    sources = {scene["sceneName"] for scene in scenes} | {item["inputName"] for item in inputs}
    for source in sorted(sources):
        filters = obs.request("GetSourceFilterList", {"sourceName": source})["filters"]
        if filters:
            raise RuntimeError(f"unexpected filters on source {source!r}; zero-instance control invalid")
    return {"chimera_registered": registered, "source_filters_queried": True,
            "sources_without_filters": sorted(sources)}


def verify_logs(config: Path, module: bool, observations: dict) -> None:
    paths = sorted((config / "logs").glob("*.txt"))
    observations.update(verified=False, full_logs=[path.name for path in paths])
    if not paths:
        raise RuntimeError("no full OBS log available to verify the control")
    lines = "\n".join(path.read_text(errors="replace") for path in paths)
    registered = f"[chimera-obs] registered {app.FILTER_KIND}" in lines
    loaded = "[chimera-obs] module " in lines
    instances = sum("[chimera-obs] '" in line or "unloaded Chimera filter instance" in line
                    for line in lines.splitlines())
    observations.update(module_registered_in_log=registered, module_loaded_in_log=loaded,
                        instance_log_entries=instances)
    if registered != module or loaded != module:
        raise RuntimeError("OBS module log does not match the requested absent/module control")
    # Per-instance errors also invalidate the control: a failed engine load is
    # still an attempted filter instance, even if no audio was processed.
    if instances:
        raise RuntimeError("Chimera instance activity in a zero-instance probe")
    observations["verified"] = True


def require_owned(path: Path, token: str) -> None:
    if path.is_symlink() or (path / OWNER).read_text(encoding="utf-8") != token:
        raise RuntimeError(f"ownership check failed; leaving {path}")


def run_case(args, case: dict, folder: Path, metadata: dict) -> dict:
    folder.mkdir()
    result = {**case, "started_utc": datetime.now(timezone.utc).isoformat(),
              "stop_after": args.stop_after, "status": "failed", "safe_to_continue": True,
              "startup_gate": "frontend-finished-observer" if getattr(args, "startup_observer", None) else "websocket-ready",
              "environment": metadata, "errors": []}
    layout = app.Layout(args.obs, folder)
    token = uuid.uuid4().hex
    config_owned = False
    obs = None

    def failure(phase: str, error: Exception) -> None:
        result["errors"].append({"phase": phase, "type": type(error).__name__, "message": str(error),
                                 "traceback": "".join(traceback.format_exception(error))})
        print(f"FAIL probe {folder.name} {phase}: {error}", file=sys.stderr, flush=True)

    try:
        # Uses the real APPDATA/ProgramData layout, without safe mode or APPDATA
        # overrides (Windows known-folder APIs need not honour such overrides).
        refuse_existing(layout.config)
        refuse_existing(layout.plugin)
        refuse_existing(layout.startup_observer)
        if case["mode"] == "module":
            layout.install(Path(args.module), Path(args.vst3))
            (layout.plugin / OWNER).write_text(token, encoding="utf-8")
        if getattr(args, "startup_observer", None):
            layout.install_startup_observer(Path(args.startup_observer))
            (layout.startup_observer / OWNER).write_text(token, encoding="utf-8")
        layout.config.mkdir(parents=True, exist_ok=False)
        (layout.config / OWNER).write_text(token, encoding="utf-8")
        config_owned = True
        recordings = folder / "recordings"
        recordings.mkdir()
        port = app.free_port()
        app.configure(layout.config, port, recordings)
        di = folder / "di-loop.wav"
        app.write_di(di)
        # Retain the process handle even if __init__ raises during _connect.
        # Cleanup may remove an owned profile only after this process is dead.
        obs = app.Obs.__new__(app.Obs)
        app.Obs.__init__(obs, layout, port, folder / "obs-run-1.txt")
        result["pid"] = obs.process.pid
        try:
            result["obs_version"] = initial_sequence(obs, args.stop_after, di)
            result["control"] = verify_control(obs, case["mode"] == "module", args.stop_after)
        finally:
            obs.quit()  # preserves an active primary error if shutdown also fails
    except Exception as error:
        failure("test", error)
    finally:
        process = getattr(obs, "process", None)
        stopped = process is None or process.poll() is not None
        result["process_returncode"] = process.returncode if process is not None else None
        # A spawn error can occur after opening stdout but before Popen returns.
        if process is None and getattr(obs, "output", None) is not None:
            obs.output.close()
        if not stopped:
            result["safe_to_continue"] = False
            failure("cleanup", RuntimeError("OBS process is not confirmed stopped; leaving all owned paths"))
        if config_owned and stopped:
            # Early request failures still need the absent/module attribution.
            # Keep those observations and any log-check error independently of
            # the original request exception, before archiving/removing files.
            result["log_control"] = {}
            try:
                verify_logs(layout.config, case["mode"] == "module", result["log_control"])
            except Exception as error:
                failure("log-control", error)
        try:
            # Copy full logs/config on PASS as well as FAIL. Do this before
            # deleting a profile, and keep the live copy if archiving fails.
            if config_owned:
                require_owned(layout.config, token)
                shutil.copytree(layout.config, folder / "obs-config", symlinks=True)
            if stopped:
                if layout.installed:
                    require_owned(layout.plugin, token)
                if layout.startup_observer_installed:
                    require_owned(layout.startup_observer, token)
                if layout.installed or layout.startup_observer_installed:
                    layout.uninstall(False)
                    refuse_existing(layout.plugin)
                    refuse_existing(layout.startup_observer)
                elif layout.plugin.exists() or layout.plugin.is_symlink():
                    raise RuntimeError("unowned or partially installed Chimera module remains; stopping probe")
                elif layout.startup_observer.exists() or layout.startup_observer.is_symlink():
                    raise RuntimeError("unowned or partially installed startup observer remains; stopping probe")
                if config_owned:
                    require_owned(layout.config, token)
                    shutil.rmtree(layout.config)
                refuse_existing(layout.config)
        except Exception as error:
            result["safe_to_continue"] = False
            failure("archive-cleanup", error)
        if not result["errors"]:
            result["status"] = "passed"
        result["finished_utc"] = datetime.now(timezone.utc).isoformat()
        write_json(folder / "result.json", result)
    return result


def run(args) -> dict:
    if not app.WINDOWS:
        raise RuntimeError("this incident probe requires native Windows OBS")
    requested_work = Path(args.work).absolute()
    refuse_existing(requested_work)  # including a dangling symlink
    work = requested_work.resolve()
    refuse_existing(work)
    args.obs = str(Path(args.obs).resolve())
    args.module = str(Path(args.module).resolve())
    if getattr(args, "startup_observer", None):
        args.startup_observer = str(Path(args.startup_observer).resolve())
    args.vst3 = str(Path(args.vst3).resolve()) if args.vst3 else str(work / "unavailable-chimera.vst3")
    if not Path(args.obs).is_file() or not Path(args.module).is_dir():
        raise RuntimeError("--obs must be an executable file and --module a staged module directory")
    layout = app.Layout(args.obs, work)
    refuse_existing(layout.config)
    refuse_existing(layout.plugin)
    refuse_existing(layout.startup_observer)
    metadata = environment()
    metadata["startup_gate"] = "frontend-finished-observer" if getattr(args, "startup_observer", None) else "websocket-ready"
    metadata["binaries"] = {"obs_executable": hash_path(Path(args.obs)),
                            "staged_module": hash_path(Path(args.module))}
    vst3 = Path(args.vst3)
    metadata["binaries"]["vst3"] = (hash_path(vst3) if vst3.exists() else
                                    {"path": str(vst3), "present": False, "loaded_by_probe": False})
    if getattr(args, "startup_observer", None):
        metadata["binaries"]["startup_observer"] = hash_path(Path(args.startup_observer))
    work.mkdir(parents=True, exist_ok=False)
    planned = case_order(args.repeats)
    summary = {"planned": planned, "cases": [], "aborted": False, "passed": False}
    for index, case in enumerate(planned, 1):
        result = run_case(args, case, work / f"{index:02d}-{case['mode']}", metadata)
        summary["cases"].append(result)
        summary["aborted"] = not result["safe_to_continue"]
        write_json(work / "summary.json", summary)
        if summary["aborted"]:
            break
    summary["not_run"] = planned[len(summary["cases"]):]
    summary["passed"] = (not summary["not_run"] and
                         all(case["status"] == "passed" for case in summary["cases"]))
    write_json(work / "summary.json", summary)
    return summary


def repeat_count(value: str) -> int:
    number = int(value)
    if not 1 <= number <= 10:
        raise argparse.ArgumentTypeError("--repeats must be between 1 and 10")
    return number


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--obs", required=True, help="native Windows bin/64bit/obs64.exe")
    parser.add_argument("--module", required=True, help="staged chimera-obs directory (bin/, data/)")
    parser.add_argument("--vst3", help="optional bundle path; default points to an unavailable bundle (no filter is created)")
    parser.add_argument("--startup-observer", help="optional staged test-only observer; gate both controls on frontend FINISHED_LOADING")
    parser.add_argument("--work", required=True, help="new evidence directory; refuses an existing path")
    parser.add_argument("--repeats", type=repeat_count, default=3, help="paired observations, 1..10 (not retries)")
    parser.add_argument("--stop-after", choices=("version", "scene", "input"), default="input")
    args = parser.parse_args()
    try:
        result = run(args)
    except Exception:
        traceback.print_exc()
        return 1
    print(f"{'PASS' if result['passed'] else 'FAIL'} startup probe: "
          f"{sum(case['status'] == 'passed' for case in result['cases'])}/{len(result['planned'])} cases; "
          f"aborted={result['aborted']}", flush=True)
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
