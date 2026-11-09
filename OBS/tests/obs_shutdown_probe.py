#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Diagnostic: which sources outlive clearing the scene when the real OBS Studio app quits.

On the macOS CI runner OBS 31.1.2 sometimes logs "Not all sources were cleared" for the
media source at quit, then destroys it after the modules and the audio output are gone
and crashes (SIGSEGV in obs_source_output_audio or in obs-websocket's signal handler).
This probe starts OBS several times with the same media source in four variants and
prints, for each run, the exit code, what OBS's log says about clearing the scene, how
many sources were left at "Freeing OBS context data", and where the Chimera filter's
teardown falls in that order:

- media: the media source alone (no Chimera filter);
- media+filter: with a Chimera filter;
- media+filter, stopped: the media stopped over obs-websocket before quitting;
- media+filter, removed: the input removed over obs-websocket before quitting;
- media+filter, recorded: a short recording made before quitting, as the app tests do;
- restored, recorded (with and without the filter): a second OBS run loads the saved scene
  and records, as the app tests' later runs do; the run that saved the scene is not reported.

The first four variants (12 runs) showed no source left on the macOS CI runner, so the
later ones add what the app tests that crashed have in common: a recording, a saved scene.

It never fails on what it observes (exit 0); only a broken harness exits non-zero.
"""
import argparse
import re
import shutil
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import obs_app_test as app  # noqa: E402

# name, Chimera filter, action before quitting, record before quitting, report a second run on the saved scene
VARIANTS = [("media", False, None, False, False), ("media+filter", True, None, False, False),
            ("media+filter, stopped", True, "stop", False, False),
            ("media+filter, removed", True, "remove", False, False),
            ("media+filter, recorded", True, None, True, False), ("restored, recorded", True, None, True, True),
            ("restored, no filter, rec.", False, None, True, True)]


def summary(log: str) -> dict:
    lines = log.splitlines()

    def at(pattern: str, start: int = 0) -> int:
        return next((i for i, line in enumerate(lines) if i >= start and pattern in line), -1)

    remaining = [l.split(":", 1)[-1].strip() for l in lines if "source(s) were remaining" in l]
    left = []
    # OBS also clears the scene data when it loads the scene collection: look after the shutdown only.
    shutdown = max(at("==== Shutting down"), 0)
    start = at("Not all sources were cleared", shutdown)
    if start >= 0:
        for line in lines[start + 1:start + 12]:
            m = re.search(r"- (.+)$", line)
            if m:
                left.append(m.group(1).strip())
    cleared = at("All scene data cleared", shutdown)
    return {"not cleared": left if start >= 0 else None, "all cleared": cleared >= 0,
            "remaining": remaining, "shutdown": at("==== Shutting down"), "clear": max(start, cleared),
            "unloaded": at("[chimera-obs] unloaded Chimera filter instance"),
            "websocket unloaded": at("[obs-websocket] [obs_module_unload] Finished shutting down"),
            "freeing": at("Freeing OBS context data")}


def build_scene(obs: app.Obs, di: Path, with_filter: bool) -> None:
    obs.request("CreateScene", {"sceneName": app.SCENE})
    obs.request("SetCurrentProgramScene", {"sceneName": app.SCENE})
    obs.request("CreateInput", {"sceneName": app.SCENE, "inputName": app.SOURCE, "inputKind": "ffmpeg_source",
                                "inputSettings": {"is_local_file": True, "local_file": str(di), "looping": True,
                                                  "restart_on_activate": False}})
    if with_filter:
        obs.request("CreateSourceFilter", {"sourceName": app.SOURCE, "filterName": app.FILTER,
                                           "filterKind": app.FILTER_KIND, "filterSettings": {}})


def run(args: argparse.Namespace) -> None:
    work = Path(args.work).resolve()
    if work.exists():
        shutil.rmtree(work)
    recordings = work / "recordings"
    recordings.mkdir(parents=True)
    layout = args.layout
    layout.install(Path(args.module).resolve(), Path(args.vst3).resolve())
    di = work / "di-loop.wav"
    app.write_di(di)
    logs = layout.config / "logs"
    print(f"{'variant':24} {'run':>3} {'exit':>5}  not cleared / all cleared / remaining at free / order", flush=True)
    for name, with_filter, before_quit, record, restored in VARIANTS:
        for attempt in range(1, args.runs + 1):
            shutil.rmtree(layout.config / "basic" / "scenes", ignore_errors=True)
            if restored:  # a first run builds and saves the scene; only the second run is reported
                port = app.free_port()
                app.configure(layout.config, port, recordings)
                setup = app.Obs(layout, port, work / f"obs-setup-{attempt}.txt")
                try:
                    build_scene(setup, di, with_filter)
                    time.sleep(1)
                finally:
                    setup.stop_media = lambda: None
                    try:
                        setup.quit()
                    except RuntimeError as e:
                        print(f"     (the run that saved the scene: {e})", flush=True)
            port = app.free_port()
            app.configure(layout.config, port, recordings)
            earlier = set(logs.glob("*.txt")) if logs.is_dir() else set()
            obs = app.Obs(layout, port, work / f"obs-{len(earlier)}.txt")
            code = "?"
            try:
                if restored:
                    app.wait_until(lambda: app.SOURCE in [i["inputName"] for i in
                                                          obs.request("GetInputList").get("inputs", [])],
                                   "the saved scene restored the media source")
                else:
                    build_scene(obs, di, with_filter)
                time.sleep(args.seconds)
                if record:
                    obs.record(name, 3.0)
                if before_quit == "stop":
                    obs.request("TriggerMediaInputAction", {"inputName": app.SOURCE,
                                                            "mediaAction": "OBS_WEBSOCKET_MEDIA_INPUT_ACTION_STOP"})
                elif before_quit == "remove":
                    obs.request("RemoveInput", {"inputName": app.SOURCE})
                time.sleep(1)
            finally:
                obs.stop_media = lambda: None  # quit exactly as the variant left OBS
                try:
                    obs.quit()
                    code = 0
                except RuntimeError as e:
                    m = re.search(r"code (-?\d+)", str(e))
                    code = m.group(1) if m else str(e)
            new = sorted(set(logs.glob("*.txt")) - earlier)
            text = "".join(p.read_text(encoding="utf-8", errors="replace") for p in new)
            s = summary(text)
            order = " < ".join(k for k, _ in sorted(((k, s[k]) for k in ("shutdown", "unloaded", "clear",
                                                                          "websocket unloaded", "freeing")
                                                      if s[k] >= 0), key=lambda kv: kv[1]))
            print(f"{name:24} {attempt:>3} {code!s:>5}  {s['not cleared']} / {s['all cleared']} / "
                  f"{s['remaining']} / {order}", flush=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--module", required=True, help="staged chimera-obs plugin directory (macOS: chimera-obs.plugin)")
    parser.add_argument("--vst3", required=True, help="SpectralForge Chimera.vst3 bundle")
    parser.add_argument("--work", required=True, help="scratch directory (deleted first)")
    parser.add_argument("--obs", default="obs", help="OBS Studio executable (as for obs_app_test.py)")
    parser.add_argument("--runs", type=int, default=3, help="OBS runs per variant")
    parser.add_argument("--seconds", type=float, default=3.0, help="how long the media plays before quitting")
    args = parser.parse_args()
    args.layout = app.Layout(args.obs, Path(args.work).resolve())
    try:
        run(args)
    except Exception as e:  # noqa: BLE001 - the probe reports, it does not judge
        print(f"probe harness error: {app.failure_text(e)}", file=sys.stderr)
        return 1
    finally:
        args.layout.uninstall(True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
