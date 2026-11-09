#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""What the Chimera filter in the real OBS Studio app sends when Chimera cannot process (plan stage D).

The filter's "When Chimera cannot process" setting is Mute by default (an unprocessed
microphone or guitar must never reach the stream unnoticed) or Pass unprocessed audio.
This test installs the filter as obs_app_test.py does and runs OBS at 48 kHz 5.1 with
the media file of obs_vst3_match_test.py (one note on front left and centre, another on
front right and rear left; centre and rear left pass through the filter, so each lossless
recording also holds the filter's input). With the filter pointed at a Chimera bundle that
does not exist:

1. the default policy: the filter's output is silent, every sample of every channel zero
   (Mute sends nothing, not even the channels it would pass through); OBS's log names the
   bundle that failed;
2. Pass unprocessed audio: the front pair is the filter's input, sample for sample, and
   every channel carries its notes;
3. the bundle path cleared (the standard install location): the filter loads Chimera and
   the front pair is processed;
4. the missing bundle chosen again, with Mute: silent again.

The filter's session total counts unprocessed blocks from the first block Chimera
processed (realtime-stats.hpp), so it must count those of step 4; the filter must unload
and OBS exit cleanly.
"""
import argparse
import os
import re
import shutil
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import obs_app_test as app  # noqa: E402
import obs_vst3_match_test as match  # noqa: E402

STEP = 1 / 32768
POLICY_DRY = 1


def steps(x: list[float]) -> float:
    return max((abs(v) for v in x), default=0.0) / STEP


def run(args: argparse.Namespace) -> None:
    work = Path(args.work).resolve()
    if work.exists():
        shutil.rmtree(work)
    recordings = work / "recordings"
    recordings.mkdir(parents=True)
    layout = args.layout
    layout.install(Path(args.module).resolve(), Path(args.vst3).resolve())
    port = app.free_port()
    app.configure(layout.config, port, recordings, app.RATE, "5.1")
    app.lossless_recordings(layout.config)
    media = work / "notes.wav"
    match.write_media(media)
    missing = work / "not installed" / "SpectralForge Chimera.vst3"
    check = app.check
    filter_ref = {"sourceName": app.SOURCE, "filterName": app.FILTER}

    logs = layout.config / "logs"
    earlier = set(logs.glob("*.txt"))

    def log_text() -> str:
        return "".join(p.read_text(encoding="utf-8", errors="replace") for p in sorted(set(logs.glob("*.txt")) - earlier))

    obs = app.Obs(layout, port, work / "obs-run.txt")
    try:
        version = obs.request("GetVersion")
        print(f"     OBS {version['obsVersion']}, obs-websocket {version['obsWebSocketVersion']}")
        check(re.search(r"speakers:\s*6\b", log_text()) is not None, "OBS opened its audio in 5.1")
        obs.request("CreateScene", {"sceneName": app.SCENE})
        obs.request("SetCurrentProgramScene", {"sceneName": app.SCENE})
        obs.request("CreateInput", {"sceneName": app.SCENE, "inputName": app.SOURCE, "inputKind": "ffmpeg_source",
                                    "inputSettings": {"is_local_file": True, "local_file": str(media), "looping": False,
                                                      "restart_on_activate": False}})
        obs.request("CreateSourceFilter", {**filter_ref, "filterKind": app.FILTER_KIND,
                                           "filterSettings": {"bundle_path": str(missing)}})
        app.wait_until(lambda: f"[chimera-obs] '{app.FILTER}': " in log_text() and str(missing) in log_text(),
                       "the filter reported the bundle it could not load")
        settings = obs.request("GetSourceFilter", filter_ref)["filterSettings"]
        check("failure_policy" not in settings or settings["failure_policy"] == 0,
              f"the new filter uses the default policy (settings: failure_policy {settings.get('failure_policy', 'default')})")
        time.sleep(1)
        muted = obs.record("Mute", args.seconds)
        obs.request("SetSourceFilterSettings", {**filter_ref, "overlay": True, "filterSettings": {"failure_policy": POLICY_DRY}})
        time.sleep(1)
        passed = obs.record("Pass unprocessed", args.seconds)
        obs.request("SetSourceFilterSettings", {**filter_ref, "overlay": True, "filterSettings": {"bundle_path": ""}})
        app.wait_until(lambda: f"[chimera-obs] '{app.FILTER}': loaded " in log_text(),
                       "with the bundle path cleared the filter loaded Chimera from the standard location")
        time.sleep(2)
        processed = obs.record("Chimera loaded", args.seconds)
        obs.request("SetSourceFilterSettings", {**filter_ref, "overlay": True,
                                                "filterSettings": {"bundle_path": str(missing), "failure_policy": 0}})
        app.wait_until(lambda: log_text().count(str(missing)) >= 4, "the filter reported the missing bundle again")
        time.sleep(1)
        muted_again = obs.record("Mute after Chimera ran", args.seconds)
    finally:
        obs.quit()

    log = log_text()
    for line in log.splitlines():
        if "[chimera-obs]" in line and ("session total" in line or str(missing) in line):
            print(f"     log: {line.strip()}")

    # The first second of each recording can hold the tick in which the setting changed.
    skip = app.RATE
    for name, path in (("Mute", muted), ("Mute after Chimera ran", muted_again)):
        ch = match.channels6(path)
        loudest = max(steps(c[skip:]) for c in ch)
        print(f"     {name}: largest sample in any of the six channels {loudest:.1f} 16-bit steps")
        check(len(ch[0]) > app.RATE * 2 and loudest == 0,
              f"{name}: Chimera not loaded and the filter's output is silent in every channel ({loudest:.1f} steps)")

    ch = match.channels6(passed)
    differs = max(steps([a - b for a, b in zip(ch[0][skip:], ch[2][skip:])]),
                  steps([a - b for a, b in zip(ch[1][skip:], ch[4][skip:])]))
    level = min(steps(c[skip:]) for i, c in enumerate(ch) if i not in (3, 5))
    print(f"     Pass unprocessed: quietest channel with notes {level:.0f} steps, front pair's largest difference "
          f"from the filter's input {differs:.1f} steps")
    check(level > 1000 and differs == 0,
          f"Pass unprocessed: the front pair is the filter's input, sample for sample ({differs:.1f} steps apart)")

    ch = match.channels6(processed)
    differs = max(steps([a - b for a, b in zip(ch[0][skip:], ch[2][skip:])]),
                  steps([a - b for a, b in zip(ch[1][skip:], ch[4][skip:])]))
    level = min(steps(ch[0][skip:]), steps(ch[1][skip:]))
    print(f"     Chimera loaded: front pair {level:.0f} steps, {differs:.0f} steps from the filter's input")
    check(level > 10 and differs > 1000, f"Chimera loaded: the front pair is processed ({differs:.0f} steps from the input)")

    check(log.count(str(missing)) >= 1 and re.search(rf"\[chimera-obs\] '{re.escape(app.FILTER)}': .*{re.escape(str(missing))}", log),
          "OBS's log names the Chimera bundle the filter could not load")
    totals = re.findall(rf"\[chimera-obs\] '{re.escape(app.FILTER)}': session total: .*unprocessed blocks (\d+)", log)
    check(len(totals) == 1 and int(totals[0]) > 0,
          f"the session total counts the blocks after Chimera ran that it did not process (unprocessed: {totals})")
    check(log.count("[chimera-obs] unloaded Chimera filter instance") == 1, "the filter instance is unloaded")
    print(f"PASS real OBS Studio {version['obsVersion']}: with Chimera not loaded the filter is silent by default, passes "
          f"its input unchanged when set to, processes once Chimera loads, is silent again when the bundle goes missing, "
          f"and counts those unprocessed blocks ({totals[0]})")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--module", required=True, help="staged chimera-obs plugin directory (macOS: chimera-obs.plugin)")
    parser.add_argument("--vst3", required=True, help="SpectralForge Chimera.vst3 bundle")
    parser.add_argument("--work", required=True, help="scratch directory (deleted first)")
    parser.add_argument("--obs", default="obs", help="OBS Studio executable (as for obs_app_test.py)")
    parser.add_argument("--ffmpeg", default="ffmpeg", help="ffmpeg executable for the media file and the analysis")
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
