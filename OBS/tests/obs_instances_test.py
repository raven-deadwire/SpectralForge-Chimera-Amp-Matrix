#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Several Chimera filters, and copies of one, in the real OBS Studio app (plan stage C).

Installs the filter as obs_app_test.py does (same layouts, same guards), then:

1. Adds four media sources playing the same DI loop: "Guitar A" with a Chimera
   filter in its default state, "Guitar B" with a Chimera filter carrying a
   quieter preset from another host (lower output gain, written by
   chimera-obs-state-tool), and "Guitar C" and "Guitar D" with no filter. Each
   source is recorded on its own (the others muted; OBS still runs their
   filters): B must be clearly quieter than A, so the two instances process
   side by side with their own state.
2. Quits OBS normally and starts it again with a Lua script in the scene
   collection. A and B must come back sounding as before. Over obs-websocket
   the test then presses the script's hotkey, which calls OBS's own
   obs_source_copy_filters() from B to C and obs_source_copy_single_filter()
   with B's filter on D: the functions behind the Paste Filters and Paste
   Filter menu items. Both copies must load Chimera, carry B's state in their
   settings and sound like B, while A still sounds like A. A's state pushed
   to D's filter must then make D sound like A and leave B as it was. (OBS's Duplicate
   does not copy filters for media or audio capture sources: it adds another
   reference to the same source, so the copy paths above are the ones that
   create new filter instances on audio sources. obs-filter-tests covers
   obs_source_duplicate() with a source OBS does duplicate.)
3. Quits and starts OBS a third time: C's filter must be restored from the
   scene file with B's state and sound like B, D's with A's state and sound like A.

Each OBS run must exit cleanly, and the log must show every filter instance
unloaded.
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

A, B, C, D = "Guitar A", "Guitar B", "Guitar C", "Guitar D"
SOURCES = [A, B, C, D]
FILTER_A, FILTER_B = "Chimera A", "Chimera B"
HOTKEY = "chimera_instances_test_paste"

LUA = f"""obs = obslua

local function press(pressed)
    if not pressed then return end
    local b = obs.obs_get_source_by_name("{B}")
    local c = obs.obs_get_source_by_name("{C}")
    local d = obs.obs_get_source_by_name("{D}")
    local filter = obs.obs_source_get_filter_by_name(b, "{FILTER_B}")
    obs.obs_source_copy_filters(c, b)
    obs.obs_source_copy_single_filter(d, filter)
    obs.script_log(obs.LOG_INFO, "chimera instances test: pasted the filters of '{B}' on '{C}' ("
                   .. obs.obs_source_filter_count(c) .. ") and its Chimera filter on '{D}' ("
                   .. obs.obs_source_filter_count(d) .. ")")
    obs.obs_source_release(filter)
    obs.obs_source_release(d)
    obs.obs_source_release(c)
    obs.obs_source_release(b)
end

function script_load(settings)
    obs.obs_hotkey_register_frontend("{HOTKEY}", "Chimera instances test: paste filters", press)
end
"""


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
    script = work / "chimera-instances-test.lua"
    script.write_text(LUA, encoding="utf-8")
    logs = layout.config / "logs"
    scene_file = layout.config / "basic" / "scenes" / "Chimera.json"

    preset = json.loads(subprocess.run([args.state_tool, str(Path(args.vst3).resolve()), app.PRESET_PARAMETER,
                                        str(app.PRESET_VALUE)], check=True, capture_output=True, text=True).stdout)

    def log_text() -> str:
        return "".join(p.read_text(errors="replace") for p in sorted(logs.glob("*.txt")))

    def loads(name: str) -> int:
        return log_text().count(f"[chimera-obs] '{name}': loaded ")

    def solo(obs: app.Obs, name: str, sources: list[str]) -> float:
        """RMS of a recording of `name` alone; the other sources keep playing, muted."""
        for source in sources:
            obs.request("SetInputMute", {"inputName": source, "inputMuted": source != name})
        time.sleep(1)
        level = app.rms(app.decode(obs.record(name, args.seconds)))
        print(f"     {name:14} RMS {level:.4f}")
        check(level > 0.001, f"{name} recording has sound")
        return level

    def state(obs: app.Obs, source: str, filter_name: str) -> str:
        return obs.request("GetSourceFilter", {"sourceName": source, "filterName": filter_name})[
            "filterSettings"].get("vst3_component_state", "")

    def db(level: float, reference: float) -> float:
        return 20 * math.log10(level / reference)

    check = app.check

    print("--- first OBS run: two Chimera filters with their own state", flush=True)
    obs = app.Obs(layout, port, work / "obs-run-1.txt")
    try:
        version = obs.request("GetVersion")
        print(f"     OBS {version['obsVersion']}, obs-websocket {version['obsWebSocketVersion']}")
        obs.request("CreateScene", {"sceneName": app.SCENE})
        obs.request("SetCurrentProgramScene", {"sceneName": app.SCENE})
        for source in SOURCES:
            obs.request("CreateInput", {"sceneName": app.SCENE, "inputName": source, "inputKind": "ffmpeg_source",
                                        "inputSettings": {"is_local_file": True, "local_file": str(di),
                                                          "looping": True, "restart_on_activate": False}})
        obs.request("CreateSourceFilter", {"sourceName": A, "filterName": FILTER_A, "filterKind": app.FILTER_KIND,
                                           "filterSettings": {}})
        obs.request("CreateSourceFilter", {"sourceName": B, "filterName": FILTER_B, "filterKind": app.FILTER_KIND,
                                           "filterSettings": {"vst3_component_state": preset["component"],
                                                              "vst3_controller_state": preset["controller"]}})
        app.wait_until(lambda: loads(FILTER_A) >= 1 and loads(FILTER_B) >= 1, "both filters found Chimera and loaded it")
        level_a = solo(obs, A, SOURCES)
        level_b = solo(obs, B, SOURCES)
    finally:
        obs.quit()
    preset_db = db(level_b, level_a)
    check(preset_db < -3.0, f"each instance keeps its own state: B {preset_db:+.1f} dB against A, both running")

    scene = json.loads(scene_file.read_text(encoding="utf-8"))
    scene.setdefault("modules", {})["scripts-tool"] = [{"path": script.as_posix(), "settings": {}}]
    scene_file.write_text(json.dumps(scene, indent=4), encoding="utf-8")

    print("--- second OBS run: restore, then paste B's filters on C and its Chimera filter on D", flush=True)
    obs = app.Obs(layout, port, work / "obs-run-2.txt")
    try:
        app.wait_until(lambda: loads(FILTER_A) >= 2 and loads(FILTER_B) >= 2, "both restored filters loaded Chimera")
        check(state(obs, B, FILTER_B) == preset["component"], "B's filter carries its saved state")
        check(state(obs, A, FILTER_A) != preset["component"], "A's filter does not carry B's state")
        restored_a = solo(obs, A, SOURCES)
        restored_b = solo(obs, B, SOURCES)
        check(abs(db(restored_a, level_a)) < 1.0 and abs(db(restored_b, level_b)) < 1.0,
              f"after the restart A and B sound as before (A {db(restored_a, level_a):+.1f} dB, "
              f"B {db(restored_b, level_b):+.1f} dB)")

        # obs-websocket refuses a hotkey name nobody registered: this also checks the script loaded.
        obs.request("TriggerHotkeyByName", {"hotkeyName": HOTKEY})
        app.wait_until(lambda: "chimera instances test: pasted" in log_text(),
                       "the script pasted B's filters on C and its Chimera filter on D", 30)
        app.wait_until(lambda: loads(FILTER_B) >= 4, "both pasted filters loaded Chimera")
        check(state(obs, C, FILTER_B) == preset["component"], "the filter pasted with Paste Filters carries B's state")
        check(state(obs, D, FILTER_B) == preset["component"], "the filter pasted on its own carries B's state")
        pasted_c = solo(obs, C, SOURCES)
        pasted_d = solo(obs, D, SOURCES)
        after_a = solo(obs, A, SOURCES)
        check(abs(db(pasted_c, level_b)) < 1.0 and abs(db(pasted_d, level_b)) < 1.0,
              f"the pasted filters sound like B (C {db(pasted_c, level_b):+.1f} dB, D {db(pasted_d, level_b):+.1f} dB)")
        check(abs(db(after_a, level_a)) < 1.0, f"A is unchanged by the copies ({db(after_a, level_a):+.1f} dB)")

        # A pasted filter is its own instance: A's state pushed to D leaves B alone.
        settings_a = obs.request("GetSourceFilter", {"sourceName": A, "filterName": FILTER_A})["filterSettings"]
        state_a = settings_a["vst3_component_state"]
        obs.request("SetSourceFilterSettings", {"sourceName": D, "filterName": FILTER_B, "overlay": True,
                                                "filterSettings": {
                                                    "vst3_component_state": state_a,
                                                    "vst3_controller_state": settings_a.get("vst3_controller_state", "")}})
        changed_d = solo(obs, D, SOURCES)
        still_b = solo(obs, B, SOURCES)
        check(state(obs, B, FILTER_B) == preset["component"], "B keeps its state when D's changes")
    finally:
        obs.quit()
    check(abs(db(changed_d, level_a)) < 1.0 and abs(db(still_b, level_b)) < 1.0,
          f"D now sounds like A and B is unchanged (D {db(changed_d, level_a):+.1f} dB against A, "
          f"B {db(still_b, level_b):+.1f} dB)")

    scene = json.loads(scene_file.read_text(encoding="utf-8"))
    saved = {s["name"]: [f for f in s.get("filters", []) if f["id"] == app.FILTER_KIND] for s in scene["sources"]}
    check(all(len(saved.get(s, [])) == 1 for s in SOURCES), "the scene file stores one Chimera filter per source")
    stored = {s: saved[s][0]["settings"].get("vst3_component_state") for s in SOURCES}
    check(stored[B] == preset["component"] and stored[C] == preset["component"],
          "the scene file stores B's state for B and the filters pasted on C")
    check(stored[D] == state_a, "the scene file stores A's state for D")

    print("--- third OBS run: the copies are restored", flush=True)
    obs = app.Obs(layout, port, work / "obs-run-3.txt")
    try:
        app.wait_until(lambda: loads(FILTER_A) >= 3 and loads(FILTER_B) >= 7, "all four restored filters loaded Chimera")
        check(state(obs, C, FILTER_B) == preset["component"], "the restored filter on C carries B's state")
        check(state(obs, D, FILTER_B) == state_a, "the restored filter on D carries A's state")
        restored_c = solo(obs, C, SOURCES)
        restored_d = solo(obs, D, SOURCES)
    finally:
        obs.quit()
    check(abs(db(restored_c, level_b)) < 1.0 and abs(db(restored_d, level_a)) < 1.0,
          f"after the restart C still sounds like B and D like A (C {db(restored_c, level_b):+.1f} dB, "
          f"D {db(restored_d, level_a):+.1f} dB)")

    log = log_text()
    check(log.count("[chimera-obs] module ") == 3, "all three runs log the Chimera module")
    unloads = log.count("[chimera-obs] unloaded Chimera filter instance")
    check(unloads == 2 + 4 + 4, f"every filter instance is unloaded ({unloads} of 10)")
    print(f"PASS real OBS Studio {version['obsVersion']}: two Chimera filters keep their own state side by side, "
          "pasted filters carry the state, stay independent and keep it through a restart, clean shutdowns")


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
            print("\n".join(log.read_text(errors="replace").splitlines()[-60:]), file=sys.stderr)
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
