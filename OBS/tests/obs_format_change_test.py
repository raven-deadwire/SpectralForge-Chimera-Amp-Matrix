#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""A saved Chimera filter in the real OBS Studio app restarted with another sample rate and speaker layout (plan stage D).

OBS fixes its sample rate and channel layout at startup (Settings > Audio, applied on
restart), so a format change reaches the filter as an OBS restart with the same scene and
a new profile. This test installs the filter as obs_app_test.py does and runs OBS three
times with one scene:

1. 48 kHz 5.1: the filter is added and a preset is pushed into it;
2. 44.1 kHz 7.1: the restored filter, at another rate and with two more channels;
3. 48 kHz 5.1 again.

Each run plays a media file at that run's format (no resampling, no remixing), with the
notes of obs_vst3_match_test.py: one note on front left and centre, another on front
right and rear left. The centre and rear left pass through the filter, so each lossless
recording holds the exact audio the filter received. In each run:

- OBS's log shows the format it opened, and the filter loads Chimera, is prepared for that
  format, processes every block and unloads;
- the recorded front pair matches the same Chimera VST3 outside OBS (render-tool.cpp) at
  that run's sample rate, with the preset's state, sample-aligned;
- against the default state's reference it differs clearly: the preset survived the restart.
"""
import argparse
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import obs_app_test as app  # noqa: E402
import obs_vst3_match_test as match  # noqa: E402

FORMATS = (
    {"name": "48 kHz 5.1", "rate": 48000, "setup": "5.1", "speakers": 6, "layout": "5.1"},
    {"name": "44.1 kHz 7.1", "rate": 44100, "setup": "7.1", "speakers": 8, "layout": "7.1"},
    {"name": "48 kHz 5.1 again", "rate": 48000, "setup": "5.1", "speakers": 6, "layout": "5.1"},
)
CENTRE, REAR_LEFT = 2, 4  # the same positions in OBS's 5.1 and 7.1 layouts (FL FR FC LFE RL RR [SL SR])
# OBS against Chimera outside OBS: the 16-bit conversion (up to a step from nearest on the
# macOS CI runner) plus the float difference between the filter and Chimera outside OBS
# (about 0.1 step on Linux; see obs_resume_test.py).
MATCH_STEPS = 1.25
CONTROL_DB = -20  # the preset's recording against the default state's reference: at least this residual


def write_media(path: Path, fmt: dict) -> None:
    """The notes at the format's rate and layout, 16-bit: note A on FL and FC, note B on FR and RL."""
    a, b = match.note(110.0), match.note(164.81)
    exprs = [a, b, a, "0", b] + ["0"] * (fmt["speakers"] - 5)
    subprocess.run([app.FFMPEG, "-v", "error", "-y", "-f", "lavfi", "-i",
                    f"aevalsrc=exprs='{'|'.join(exprs)}':c={fmt['layout']}:s={fmt['rate']}:d={match.MEDIA_SECONDS}",
                    "-c:a", "pcm_s16le", str(path)], check=True)


def channels(path: Path, count: int, rate: int) -> list[list[float]]:
    raw = subprocess.run([app.FFMPEG, "-v", "error", "-i", str(path), "-map", "0:a:0", "-ac", str(count),
                          "-ar", str(rate), "-f", "f32le", "-"], check=True, capture_output=True).stdout
    samples = struct.unpack(f"<{len(raw) // 4}f", raw)
    return [list(samples[c::count]) for c in range(count)]


def run_format(args: argparse.Namespace, index: int, fmt: dict, port: int, preset: dict, preset_file: Path) -> dict:
    layout, work, check = args.layout, Path(args.work).resolve(), app.check
    recordings = work / f"recordings-{index + 1}"
    recordings.mkdir(parents=True)
    # The same scene each run, a new profile: OBS reads both at startup.
    app.configure(layout.config, port, recordings, fmt["rate"], fmt["setup"])
    app.lossless_recordings(layout.config)
    media = work / "notes.wav"  # the scene's media source plays this file
    write_media(media, fmt)

    print(f"--- OBS run {index + 1}: {fmt['name']}", flush=True)
    obs = app.Obs(layout, port, work / f"obs-run-{index + 1}.txt")
    new_logs = []

    def log_text() -> str:
        new_logs[:] = sorted(set(obs.logs.glob("*.txt")) - obs.earlier_logs)
        return "".join(p.read_text(encoding="utf-8", errors="replace") for p in new_logs)

    try:
        version = obs.request("GetVersion")
        print(f"     OBS {version['obsVersion']}, obs-websocket {version['obsWebSocketVersion']}")
        log = log_text()
        check(re.search(rf"samples per sec:\s*{fmt['rate']}\b", log) is not None
              and re.search(rf"speakers:\s*{fmt['speakers']}\b", log) is not None,
              f"OBS opened its audio at {fmt['rate']} Hz with {fmt['speakers']} speakers")
        if index == 0:
            obs.request("CreateScene", {"sceneName": app.SCENE})
            obs.request("SetCurrentProgramScene", {"sceneName": app.SCENE})
            obs.request("CreateInput", {"sceneName": app.SCENE, "inputName": app.SOURCE, "inputKind": "ffmpeg_source",
                                        "inputSettings": {"is_local_file": True, "local_file": str(media),
                                                          "looping": False, "restart_on_activate": False}})
            obs.request("CreateSourceFilter", {"sourceName": app.SOURCE, "filterName": app.FILTER,
                                               "filterKind": app.FILTER_KIND, "filterSettings": {}})
        app.wait_until(lambda: f"[chimera-obs] '{app.FILTER}': loaded " in log_text(),
                       "the filter loaded Chimera" if index == 0 else "the restored filter loaded Chimera")
        if index == 0:
            obs.request("SetSourceFilterSettings", {
                "sourceName": app.SOURCE, "filterName": app.FILTER, "overlay": True,
                "filterSettings": {"vst3_component_state": preset["component"],
                                   "vst3_controller_state": preset["controller"]}})
        time.sleep(1)
        recording = obs.record(f"run {index + 1}", args.seconds)
    finally:
        obs.quit()

    log = log_text()
    for line in log.splitlines():
        if "session total" in line or "audio buffering" in line:
            print(f"     log: {line.strip()}")
    check("could not be prepared" not in log, f"Chimera was prepared for {fmt['name']}")
    check(log.count("[chimera-obs] module ") == 1, "the run logs the Chimera module")
    totals = re.findall(rf"\[chimera-obs\] '{re.escape(app.FILTER)}': session total: .*unprocessed blocks (\d+)", log)
    check(totals == ["0"], f"the filter processed every block (unprocessed: {totals})")
    check(log.count("[chimera-obs] unloaded Chimera filter instance") == 1, "the filter instance is unloaded")

    # The analysis helpers of obs_vst3_match_test.py read the sample rate from obs_app_test.
    app.RATE = fmt["rate"]
    ch = channels(recording, fmt["speakers"], fmt["rate"])
    source = channels(media, fmt["speakers"], fmt["rate"])
    skip = []
    for label, c in (("centre", CENTRE), ("rear left", REAR_LEFT)):
        offsets, spans = match.input_breaks(ch[c], source[c])
        skip += spans
        print(f"     the recorded {label} (the filter's input) per note, offset into the media file: {offsets}"
              f"{'' if not spans else f'; not an unbroken run around {[round(a / app.RATE + match.PERIOD, 2) for a, _ in spans]} s'}")
        check(sum(o is not None for o in offsets) >= 2,
              f"the recorded {label} matches the media file sample for sample in at least two notes")
    start = int(app.RATE * args.warmup)
    check(len(ch[0]) > app.RATE * (args.warmup + 2 * match.PERIOD), f"the recording is long enough ({len(ch[0])} frames)")
    reference, latency = match.render(args, ch[CENTRE], ch[REAR_LEFT], preset_file, work, f"run-{index + 1}")
    default, _ = match.render(args, ch[CENTRE], ch[REAR_LEFT], None, work, f"run-{index + 1}-default")
    result = {"name": fmt["name"], "latency": latency}
    for i, (side, input_c) in enumerate((("left", CENTRE), ("right", REAR_LEFT))):
        notes = [n for n in match.onsets(ch[input_c], start) if not any(a <= n < b for a, b in skip)]
        check(len(notes) >= 2, f"{side}: at least two notes to compare after {args.warmup:g} s ({len(notes)})")
        shift = match.best_shift(ch[i], reference[i], notes[0])
        r = match.compare(ch[i], reference[i], shift, start, notes, skip)
        control = match.compare(ch[i], default[i], shift, start, notes, skip)
        print(f"     front {side:5}: OBS after the reference by {shift:+d} samples, processed RMS {r['signal']:.4f}, "
              f"largest difference {r['max_steps']:.2f} 16-bit steps at {r['worst_at']:.2f} s "
              f"({r['ahead_steps']:.2f} in the tick before a note); against the default state {control['db']:+.1f} dB; "
              f"Chimera latency {latency}")
        check(shift == 0, f"front {side}: OBS's processed audio lines up with Chimera outside OBS at {fmt['name']} ({shift:+d} samples)")
        check(r["signal"] > 0.001, f"front {side}: the processed audio carries sound (RMS {r['signal']:.4f})")
        check(r["max_steps"] <= MATCH_STEPS and r["ahead_steps"] <= args.max_ahead_steps,
              f"front {side}: OBS's output matches Chimera outside OBS at {fmt['name']} with the preset "
              f"({r['max_steps']:.2f} steps, allowed {MATCH_STEPS:g}; {r['ahead_steps']:.2f} before a note, "
              f"allowed {args.max_ahead_steps:g})")
        check(control["db"] > CONTROL_DB,
              f"front {side}: the filter still holds the preset, not the default state ({control['db']:+.1f} dB against the default)")
        result[side] = r["max_steps"]
    result["version"] = version["obsVersion"]
    return result


def run(args: argparse.Namespace) -> None:
    work = Path(args.work).resolve()
    if work.exists():
        shutil.rmtree(work)
    work.mkdir(parents=True)
    layout = args.layout
    layout.install(Path(args.module).resolve(), Path(args.vst3).resolve())
    for scene_file in (layout.config / "basic" / "scenes").glob("Chimera.json*"):
        scene_file.unlink()
    preset = json.loads(subprocess.run([args.state_tool, str(Path(args.vst3).resolve()), app.PRESET_PARAMETER,
                                        str(app.PRESET_VALUE)], check=True, capture_output=True, text=True).stdout)
    preset_file = work / "preset.json"
    preset_file.write_text(json.dumps(preset), encoding="utf-8")
    port = app.free_port()
    results = [run_format(args, i, fmt, port, preset, preset_file) for i, fmt in enumerate(FORMATS)]
    worst = max(max(r["left"], r["right"]) for r in results)
    print(f"PASS real OBS Studio {results[0]['version']}: one saved filter restarted at "
          f"{' -> '.join(r['name'] for r in results)} keeps its preset and matches the same Chimera VST3 outside OBS "
          f"at each format, sample-aligned, within {worst:.2f} 16-bit steps; every block processed")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--module", required=True, help="staged chimera-obs plugin directory (macOS: chimera-obs.plugin)")
    parser.add_argument("--vst3", required=True, help="SpectralForge Chimera.vst3 bundle")
    parser.add_argument("--state-tool", required=True, help="chimera-obs-state-tool executable")
    parser.add_argument("--render-tool", required=True, help="chimera-obs-render-tool executable")
    parser.add_argument("--work", required=True, help="scratch directory (deleted first)")
    parser.add_argument("--obs", default="obs", help="OBS Studio executable (as for obs_app_test.py)")
    parser.add_argument("--ffmpeg", default="ffmpeg", help="ffmpeg executable for the media file and the analysis")
    parser.add_argument("--seconds", type=float, default=15.0, help="length of each recording")
    parser.add_argument("--warmup", type=float, default=6.0,
                        help="seconds of each recording left out of the comparison (Chimera's tails from before it)")
    parser.add_argument("--max-ahead-steps", type=float, default=2.0,
                        help="largest difference allowed in the OBS tick before a note (as obs_vst3_match_test.py)")
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
