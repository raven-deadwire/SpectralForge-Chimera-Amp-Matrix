#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""An edit made in Chimera's editor window, opened from the filter in the real OBS Studio app (plan stage C).

Linux only: the edit is a mouse drag sent with xdotool to the editor's X11 window (the
test runs under Xvfb). Installs the filter as obs_app_test.py does, then:

1. Adds the DI loop with a Chimera filter in its default state and records it.
2. Starts OBS again with obs_editor_test.py's Lua script in the scene collection and opens
   the editor from the filter's button on the OBS UI thread. In the editor's EASY VIEW the
   test drags the OUTPUT knob down (Chimera's final level, -6 dB by default). The filter
   must write the new state into its OBS settings (what OBS saves in the scene), and a
   recording must be clearly quieter. OBS quits with the editor still open.
3. A third OBS run restores the filter: the saved scene holds the edited state and the
   recording sounds like the edited one.

A screenshot of the editor before and after the drag is kept in the work directory.
"""
import argparse
import json
import math
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import obs_app_test as app  # noqa: E402
import obs_editor_test as editor  # noqa: E402

# Chimera's EASY VIEW, the view the editor opens in: the OUTPUT knob's centre, in window pixels.
EDITOR_SIZE = (1180, 780)
OUTPUT_KNOB = (1065, 585)
DRAG_PIXELS = 150  # downward; Chimera's knobs turn down when dragged down


def editor_window() -> str | None:
    ids = subprocess.run(["xdotool", "search", "--name", f"^{app.FILTER}$"], capture_output=True, text=True).stdout.split()
    return ids[-1] if ids else None


def window_size(window: str) -> tuple[int, int]:
    shape = subprocess.run(["xdotool", "getwindowgeometry", "--shell", window], check=True, capture_output=True,
                           text=True).stdout
    values = dict(line.split("=", 1) for line in shape.split())
    return int(values["WIDTH"]), int(values["HEIGHT"])


def screenshot(window: str, path: Path) -> None:
    subprocess.run(["import", "-window", window, str(path)], check=False, capture_output=True)


def drag_down(window: str, x: int, y: int, pixels: int) -> None:
    subprocess.run(["xdotool", "mousemove", "--window", window, str(x), str(y), "mousedown", "1"], check=True)
    for _ in range(pixels // 10):
        subprocess.run(["xdotool", "mousemove_relative", "--", "0", "10"], check=True)
        time.sleep(0.03)
    subprocess.run(["xdotool", "mouseup", "1"], check=True)


def run(args: argparse.Namespace) -> None:
    work = Path(args.work).resolve()
    if work.exists():
        shutil.rmtree(work)
    recordings = work / "recordings"
    recordings.mkdir(parents=True)
    layout = args.layout
    layout.install(Path(args.module).resolve(), Path(args.vst3).resolve())
    port = app.free_port()
    app.configure(layout.config, port, recordings)
    di = work / "di-loop.wav"
    app.write_di(di)
    script = work / "chimera-editor-test.lua"
    script.write_text(editor.LUA, encoding="utf-8")
    logs = layout.config / "logs"
    scene_file = layout.config / "basic" / "scenes" / "Chimera.json"
    check = app.check
    filter_ref = {"sourceName": app.SOURCE, "filterName": app.FILTER}

    def log_text() -> str:
        return "".join(p.read_text(encoding="utf-8", errors="replace") for p in sorted(logs.glob("*.txt")))

    def count(line: str) -> int:
        return log_text().count(line)

    def level(obs: app.Obs, name: str) -> float:
        time.sleep(1)
        x = app.decode(obs.record(name, args.seconds))
        print(f"     {name:14} RMS {app.rms(x):.4f}")
        check(app.rms(x) > 0.0001, f"{name} recording has sound")
        return app.rms(x)

    def component(obs: app.Obs) -> str:
        return obs.request("GetSourceFilter", filter_ref)["filterSettings"].get("vst3_component_state", "")

    def saved_component() -> str:
        scene = json.loads(scene_file.read_text(encoding="utf-8"))
        for source in scene.get("sources", []):
            for f in source.get("filters", []):
                if f.get("name") == app.FILTER:
                    return f.get("settings", {}).get("vst3_component_state", "")
        return ""

    print("--- first OBS run: the filter in its default state", flush=True)
    obs = app.Obs(layout, port, work / "obs-run-1.txt")
    try:
        version = obs.request("GetVersion")
        print(f"     OBS {version['obsVersion']}, obs-websocket {version['obsWebSocketVersion']}")
        obs.request("CreateScene", {"sceneName": app.SCENE})
        obs.request("SetCurrentProgramScene", {"sceneName": app.SCENE})
        obs.request("CreateInput", {"sceneName": app.SCENE, "inputName": app.SOURCE, "inputKind": "ffmpeg_source",
                                    "inputSettings": {"is_local_file": True, "local_file": str(di), "looping": True,
                                                      "restart_on_activate": False}})
        obs.request("CreateSourceFilter", {**filter_ref, "filterKind": app.FILTER_KIND, "filterSettings": {}})
        app.wait_until(lambda: count(f"[chimera-obs] '{app.FILTER}': loaded ") >= 1, "the filter loaded Chimera")
        default = level(obs, "default")
    finally:
        obs.quit()

    scene = json.loads(scene_file.read_text(encoding="utf-8"))
    scene.setdefault("modules", {})["scripts-tool"] = [{"path": script.as_posix(), "settings": {}}]
    scene_file.write_text(json.dumps(scene, indent=4), encoding="utf-8")

    print("--- second OBS run: open the editor, drag its OUTPUT knob down, record, quit with it open", flush=True)
    obs = app.Obs(layout, port, work / "obs-run-2.txt")
    try:
        app.wait_until(lambda: count(f"[chimera-obs] '{app.FILTER}': loaded ") >= 2, "the restored filter loaded Chimera")
        before = component(obs)
        check(before != "", "the filter's settings hold Chimera's state")
        obs.request("SetStudioModeEnabled", {"studioModeEnabled": True})
        app.wait_until(lambda: count(f"[chimera-obs] '{app.FILTER}': editor shown") >= 1, "the filter showed Chimera's editor", 30)
        app.wait_until(lambda: editor_window() is not None, "the editor's X11 window", 15)
        window = editor_window()
        time.sleep(2)  # the editor draws its first frame
        screenshot(window, work / "editor-before.png")
        size = window_size(window)
        check(size == EDITOR_SIZE, f"the editor opened at its EASY VIEW size {EDITOR_SIZE[0]}x{EDITOR_SIZE[1]} "
                                   f"({size[0]}x{size[1]}), where the test knows the OUTPUT knob")
        drag_down(window, *OUTPUT_KNOB, DRAG_PIXELS)
        app.wait_until(lambda: component(obs) != before, "the edit in the editor reached the filter's OBS settings", 15)
        time.sleep(1)
        screenshot(window, work / "editor-after.png")
        edited_state = component(obs)
        edited = level(obs, "edited")
        check("editor:" not in "\n".join(l for l in log_text().splitlines() if "[chimera-obs]" in l),
              "the filter logged no editor error")
    finally:
        obs.quit()  # with the editor open
    edited_db = 20 * math.log10(edited / default)
    print(f"     the OUTPUT knob dragged down {DRAG_PIXELS} pixels: {edited_db:+.1f} dB against the default state")
    check(edited_db < -6, f"the edit made in the editor changes the sound ({edited_db:+.1f} dB)")
    check(saved_component() == edited_state, "OBS saved the edited state in the scene")
    check(count(f"[chimera-obs] '{app.FILTER}': editor closed with the filter") == 1,
          "quitting OBS closed the open editor with the filter")

    print("--- third OBS run: the edited state is restored", flush=True)
    obs = app.Obs(layout, port, work / "obs-run-3.txt")
    try:
        app.wait_until(lambda: count(f"[chimera-obs] '{app.FILTER}': loaded ") >= 3, "the restored filter loaded Chimera")
        restored = level(obs, "restored")
    finally:
        obs.quit()
    restored_db = 20 * math.log10(restored / edited)
    check(abs(restored_db) < 1.0, f"the restored filter sounds like the edited one ({restored_db:+.1f} dB)")

    log = log_text()
    check(log.count("[chimera-obs] module ") == 3, "all three runs log the Chimera module")
    unloads = log.count("[chimera-obs] unloaded Chimera filter instance")
    check(unloads == 3, f"every filter instance is unloaded ({unloads} of 3)")
    print(f"PASS real OBS Studio {version['obsVersion']}: an edit made with the mouse in Chimera's editor window reaches "
          f"the filter's settings, changes the sound ({edited_db:+.1f} dB), is saved in the scene and restored after a restart")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--module", required=True, help="staged chimera-obs plugin directory")
    parser.add_argument("--vst3", required=True, help="SpectralForge Chimera.vst3 bundle")
    parser.add_argument("--work", required=True, help="scratch directory (deleted first)")
    parser.add_argument("--obs", default="obs", help="OBS Studio executable (as for obs_app_test.py)")
    parser.add_argument("--ffmpeg", default="ffmpeg", help="ffmpeg executable for the analysis")
    parser.add_argument("--seconds", type=float, default=4.0, help="length of each recording")
    args = parser.parse_args()
    if app.WINDOWS or app.MACOS:
        print("FAIL this test drives the editor with xdotool under X11 (Linux only)", file=sys.stderr)
        return 2
    if not os.environ.get("DISPLAY"):
        print("FAIL no X display (run under xvfb-run)", file=sys.stderr)
        return 2
    if shutil.which("xdotool") is None or shutil.which("import") is None:
        print("FAIL xdotool and ImageMagick's import are needed", file=sys.stderr)
        return 2
    app.FFMPEG = args.ffmpeg
    args.layout = app.Layout(args.obs, Path(args.work).resolve())
    passed = False
    try:
        run(args)
        passed = True
    except Exception as e:  # noqa: BLE001 - report every failure the same way
        print(f"FAIL {app.failure_text(e)}", file=sys.stderr)
        work = Path(args.work).resolve()
        for log in [*sorted(work.glob("obs-run*.txt")), *sorted((args.layout.config / "logs").glob("*.txt"))[-1:]]:
            print(f"--- {log.name} (last 60 lines)", file=sys.stderr)
            print("\n".join(log.read_text(encoding="utf-8", errors="replace").splitlines()[-60:]), file=sys.stderr)
        return 1
    finally:
        try:
            if (args.layout.config / "logs").is_dir():
                shutil.copytree(args.layout.config / "logs", Path(args.work).resolve() / "obs-logs", dirs_exist_ok=True)
        except OSError as e:  # must not keep the filter installed
            print(f"note: could not keep the OBS logs: {e}", file=sys.stderr)
        args.layout.uninstall(passed)
    return 0


if __name__ == "__main__":
    sys.exit(main())
