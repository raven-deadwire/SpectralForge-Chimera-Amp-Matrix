#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Chimera's own editor window opened from the filter in the real OBS Studio app (plan stages C, F).

Installs the filter as obs_app_test.py does (same layouts, same guards), then:

1. Adds the DI loop with a Chimera filter carrying a quieter preset from another
   host (chimera-obs-state-tool) and records it with the editor closed.
2. Starts OBS again with a Lua script in the scene collection. The script
   presses the filter's "Open Chimera editor" button through OBS's own
   obs_property_button_clicked(), the call the button in the filter's
   properties makes, from an OBS frontend event: those run on the OBS UI
   thread, where the editor must open. The test fires the event by turning
   studio mode on over obs-websocket. The filter must log that it showed the
   editor, the window must exist (Windows: a top-level OBS window with the
   filter's name; Linux: an X11 window with that name), and a recording with
   the editor open must sound like the one without it. Studio mode off presses
   the button again: the same window is raised, not a second one. OBS then
   quits with the editor still open: it must exit cleanly, closing the editor
   with the filter.
3. A third OBS run restores the filter with the same sound: opening and closing
   the editor did not change the saved state.

Editing Chimera inside the editor window needs a person (or UI automation) and
stays a manual check.
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

LUA = f"""obs = obslua

local function press_open_editor()
    local source = obs.obs_get_source_by_name("{app.SOURCE}")
    local filter = source and obs.obs_source_get_filter_by_name(source, "{app.FILTER}")
    if filter then
        local props = obs.obs_source_properties(filter)
        local button = props and obs.obs_properties_get(props, "open_editor")
        if button then
            obs.obs_property_button_clicked(button, filter)
            obs.script_log(obs.LOG_INFO, "chimera editor test: pressed the open editor button")
        else
            obs.script_log(obs.LOG_WARNING, "chimera editor test: the filter has no open editor button")
        end
        if props then obs.obs_properties_destroy(props) end
        obs.obs_source_release(filter)
    else
        obs.script_log(obs.LOG_WARNING, "chimera editor test: source or filter missing")
    end
    if source then obs.obs_source_release(source) end
end

-- Frontend events run on the OBS UI thread, as the properties button does.
local function on_event(event)
    if event == obs.OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED or event == obs.OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED then
        press_open_editor()
    end
end

function script_load(settings)
    obs.obs_frontend_add_event_callback(on_event)
end
"""


def editor_windows(pid: int) -> int | None:
    """How many top-level windows carry the filter's name, or None where the test cannot look (macOS)."""
    if app.WINDOWS:
        return sum(1 for title in app.window_titles(pid) if title == app.FILTER)
    if app.MACOS:
        return None
    tree = subprocess.run(["xwininfo", "-root", "-tree"], check=True, capture_output=True, text=True).stdout
    return tree.count(f'"{app.FILTER}"')


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
    script.write_text(LUA, encoding="utf-8")
    logs = layout.config / "logs"
    scene_file = layout.config / "basic" / "scenes" / "Chimera.json"
    check = app.check

    preset = json.loads(subprocess.run([args.state_tool, str(Path(args.vst3).resolve()), app.PRESET_PARAMETER,
                                        str(app.PRESET_VALUE)], check=True, capture_output=True, text=True).stdout)

    def log_text() -> str:
        return "".join(p.read_text(encoding="utf-8", errors="replace") for p in sorted(logs.glob("*.txt")))

    def count(line: str) -> int:
        return log_text().count(line)

    shown = f"[chimera-obs] '{app.FILTER}': editor shown"
    pressed = "chimera editor test: pressed the open editor button"

    def level(obs: app.Obs, name: str) -> float:
        time.sleep(1)
        x = app.decode(obs.record(name, args.seconds))
        print(f"     {name:14} RMS {app.rms(x):.4f}")
        check(app.rms(x) > 0.001, f"{name} recording has sound")
        return app.rms(x)

    print("--- first OBS run: the filter with a preset, editor closed", flush=True)
    obs = app.Obs(layout, port, work / "obs-run-1.txt")
    try:
        version = obs.request("GetVersion")
        print(f"     OBS {version['obsVersion']}, obs-websocket {version['obsWebSocketVersion']}")
        obs.request("CreateScene", {"sceneName": app.SCENE})
        obs.request("SetCurrentProgramScene", {"sceneName": app.SCENE})
        obs.request("CreateInput", {"sceneName": app.SCENE, "inputName": app.SOURCE, "inputKind": "ffmpeg_source",
                                    "inputSettings": {"is_local_file": True, "local_file": str(di), "looping": True,
                                                      "restart_on_activate": False}})
        obs.request("CreateSourceFilter", {"sourceName": app.SOURCE, "filterName": app.FILTER,
                                           "filterKind": app.FILTER_KIND,
                                           "filterSettings": {"vst3_component_state": preset["component"],
                                                              "vst3_controller_state": preset["controller"]}})
        app.wait_until(lambda: count(f"[chimera-obs] '{app.FILTER}': loaded ") >= 1, "the filter loaded Chimera")
        closed = level(obs, "editor closed")
    finally:
        obs.quit()

    scene = json.loads(scene_file.read_text(encoding="utf-8"))
    scene.setdefault("modules", {})["scripts-tool"] = [{"path": script.as_posix(), "settings": {}}]
    scene_file.write_text(json.dumps(scene, indent=4), encoding="utf-8")

    print("--- second OBS run: open the editor from the filter's button, record, quit with it open", flush=True)
    obs = app.Obs(layout, port, work / "obs-run-2.txt")
    try:
        app.wait_until(lambda: count(f"[chimera-obs] '{app.FILTER}': loaded ") >= 2, "the restored filter loaded Chimera")
        check(editor_windows(obs.process.pid) in (0, None), "no editor window before the button")
        obs.request("SetStudioModeEnabled", {"studioModeEnabled": True})
        app.wait_until(lambda: count(pressed) >= 1, "the script pressed the open editor button on the UI thread", 30)
        app.wait_until(lambda: count(shown) >= 1, "the filter showed Chimera's editor", 30)
        windows = editor_windows(obs.process.pid)
        if windows is not None:
            app.wait_until(lambda: editor_windows(obs.process.pid) == 1, "one editor window with the filter's name", 15)
        else:
            print("     (macOS: the window itself is not inspected; the filter's log line is the evidence)")
        opened = level(obs, "editor open")
        obs.request("SetStudioModeEnabled", {"studioModeEnabled": False})
        app.wait_until(lambda: count(pressed) >= 2 and count(shown) >= 2, "pressing the button again raises the editor", 30)
        if windows is not None:
            time.sleep(1)
            check(editor_windows(obs.process.pid) == 1, "still one editor window, not a second one")
        check("editor:" not in "\n".join(l for l in log_text().splitlines() if "[chimera-obs]" in l),
              "the filter logged no editor error")
    finally:
        obs.quit()  # with the editor open
    open_db = 20 * math.log10(opened / closed)
    check(abs(open_db) < 0.5, f"the open editor leaves the sound as it was ({open_db:+.1f} dB)")
    check(count(f"[chimera-obs] '{app.FILTER}': editor closed with the filter") == 1,
          "quitting OBS closed the open editor with the filter")

    print("--- third OBS run: the saved state is unchanged", flush=True)
    obs = app.Obs(layout, port, work / "obs-run-3.txt")
    try:
        app.wait_until(lambda: count(f"[chimera-obs] '{app.FILTER}': loaded ") >= 3, "the restored filter loaded Chimera")
        restored = level(obs, "restored")
    finally:
        obs.quit()
    restored_db = 20 * math.log10(restored / closed)
    check(abs(restored_db) < 1.0, f"after the editor was open the restored filter sounds the same ({restored_db:+.1f} dB)")

    log = log_text()
    check(log.count("[chimera-obs] module ") == 3, "all three runs log the Chimera module")
    unloads = log.count("[chimera-obs] unloaded Chimera filter instance")
    check(unloads == 3, f"every filter instance is unloaded ({unloads} of 3)")
    print(f"PASS real OBS Studio {version['obsVersion']}: the filter's button opens Chimera's editor on the OBS UI thread, "
          "the sound and saved state stay the same, OBS quits cleanly with the editor open")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--module", required=True, help="staged chimera-obs plugin directory (macOS: chimera-obs.plugin)")
    parser.add_argument("--vst3", required=True, help="SpectralForge Chimera.vst3 bundle")
    parser.add_argument("--state-tool", required=True, help="chimera-obs-state-tool executable")
    parser.add_argument("--work", required=True, help="scratch directory (deleted first)")
    parser.add_argument("--obs", default="obs", help="OBS Studio executable (as for obs_app_test.py)")
    parser.add_argument("--ffmpeg", default="ffmpeg", help="ffmpeg executable for decoding the recordings")
    parser.add_argument("--seconds", type=float, default=5.0, help="length of each recording")
    args = parser.parse_args()
    if not app.WINDOWS and not app.MACOS and not os.environ.get("DISPLAY"):
        print("FAIL no X display (run under xvfb-run)", file=sys.stderr)
        return 2
    app.FFMPEG = args.ffmpeg
    args.layout = app.Layout(args.obs, Path(args.work).resolve())
    passed = False
    try:
        run(args)
        passed = True
    except Exception as e:  # noqa: BLE001 - report every failure the same way
        print(f"FAIL {e}", file=sys.stderr)
        work = Path(args.work).resolve()
        for log in [*sorted(work.glob("obs-run-*.txt")), *sorted((args.layout.config / "logs").glob("*.txt"))[-1:]]:
            print(f"--- {log.name} (last 60 lines)", file=sys.stderr)
            print("\n".join(log.read_text(encoding="utf-8", errors="replace").splitlines()[-60:]), file=sys.stderr)
        return 1
    finally:
        # Kept for the CI artifact: on Windows and macOS a passing run removes
        # the OBS configuration, logs included.
        try:
            if (args.layout.config / "logs").is_dir():
                shutil.copytree(args.layout.config / "logs", Path(args.work).resolve() / "obs-logs", dirs_exist_ok=True)
        except OSError as e:  # must not keep the filter installed
            print(f"note: could not keep the OBS logs: {e}", file=sys.stderr)
        args.layout.uninstall(passed)
    return 0


if __name__ == "__main__":
    sys.exit(main())
