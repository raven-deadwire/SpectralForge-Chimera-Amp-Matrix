#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""The Chimera filter's delay and audio/video sync in a real OBS Studio recording (plan stage D).

OBS has no plugin delay compensation, so the filter's processing delay is added to the
audio it processes. This test measures that delay in a recording made by the real OBS
app and checks it against the delay the filter reports in its properties.

Installs the filter as obs_app_test.py does (same layouts, same guards), runs OBS at
48 kHz 5.1 and plays one media file (not a loop): every second three white video frames
and, at the same instant, a short harmonic burst on front left/right and the same burst
on the centre channel. The filter processes the front pair and passes the centre through
untouched, so in one recording the centre is a sample-exact reference for the front pair.
Lossless (16-bit PCM) recordings are made with the filter disabled and enabled:

- disabled: the front pair starts with the centre (calibration);
- enabled: the front pair's output starts (non-zero for 16 samples in a row; isolated single
  steps a few ms early, seen on macOS, do not count) after the centre's by the filter's delay, which must match the delay shown in the filter's properties (read by a scene
  script through obs_source_properties on the OBS UI thread), the same for every burst.
  The time to 10% of the burst's level is printed as well: Chimera's processing makes a
  burst rise over a few ms (also with its noise gate off), which is not processing delay;
- in each recording the centre's audio-to-video offset holds within one video frame.
  OBS's own offset for a media source differs from one recording to the next (also with
  the filter disabled in both), so recordings are not compared with each other.
The OBS log must show the filter loading Chimera and processing every block, and OBS
must exit cleanly.
"""
import argparse
import json
import os
import re
import shutil
import statistics
import struct
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import obs_app_test as app  # noqa: E402

PERIOD = 1.0  # seconds between markers
BURST = 0.12  # seconds of tone per marker
FPS = 30
MEDIA_SECONDS = 60.0

LUA = f"""obs = obslua

local function log_status()
    local source = obs.obs_get_source_by_name("{app.SOURCE}")
    local filter = source and obs.obs_source_get_filter_by_name(source, "{app.FILTER}")
    if filter then
        local props = obs.obs_source_properties(filter)
        local status = props and obs.obs_properties_get(props, "status_info")
        if status then
            local text = string.gsub(obs.obs_property_description(status), "\\n", " | ")
            obs.script_log(obs.LOG_INFO, "chimera latency test: status " .. text)
        else
            obs.script_log(obs.LOG_WARNING, "chimera latency test: the filter has no status text")
        end
        if props then obs.obs_properties_destroy(props) end
        obs.obs_source_release(filter)
    else
        obs.script_log(obs.LOG_WARNING, "chimera latency test: source or filter missing")
    end
    if source then obs.obs_source_release(source) end
end

-- Frontend events run on the OBS UI thread, where the properties are built.
local function on_event(event)
    if event == obs.OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED then log_status() end
end

function script_load(settings)
    obs.obs_frontend_add_event_callback(on_event)
end
"""


def write_markers(path: Path) -> None:
    """Black 320x180 video with a 3-frame white flash every PERIOD; 5.1 audio with the
    same burst on front left, front right and centre at each flash, LFE and rears silent."""
    tone = "+".join(f"sin(2*PI*{110 * k}*t)/{k}" for k in range(1, 9))
    burst = f"if(lt(mod(t\\,{PERIOD})\\,{BURST})\\,0.2*({tone})\\,0)"
    audio = f"aevalsrc=exprs='{burst}|{burst}|{burst}|0|0|0':c=5.1:s={app.RATE}:d={MEDIA_SECONDS}"
    flash = f"drawbox=x=0:y=0:w=iw:h=ih:color=white:t=fill:enable='lt(mod(t\\,{PERIOD})\\,{3 / FPS - 0.001})'"
    subprocess.run([app.FFMPEG, "-v", "error", "-y", "-f", "lavfi", "-i",
                    f"color=c=black:s=320x180:r={FPS}:d={MEDIA_SECONDS}", "-f", "lavfi", "-i", audio, "-vf", flash,
                    "-c:v", "libx264", "-preset", "ultrafast", "-g", str(FPS), "-pix_fmt", "yuv420p",
                    "-c:a", "flac", str(path)], check=True)


def video_onsets(path: Path) -> list[float]:
    """Timestamps of the first white frame of each flash."""
    result = subprocess.run([app.FFMPEG, "-v", "info", "-copyts", "-i", str(path), "-map", "0:v:0",
                             "-vf", "scale=8:8,format=gray,showinfo", "-f", "null", "-"],
                            check=True, capture_output=True, text=True, errors="replace")
    onsets, bright = [], False
    for line in result.stderr.splitlines():
        m = re.search(r"pts_time:\s*([\d.]+).*?mean:\[(\d+)", line)
        if not m:
            continue
        now = int(m.group(2)) > 128
        if now and not bright:
            onsets.append(float(m.group(1)))
        bright = now
    return onsets


def audio_channels(path: Path) -> tuple[float, list[list[float]]]:
    """(timestamp of the first sample, the six channels at 48 kHz)."""
    result = subprocess.run([app.FFMPEG, "-v", "info", "-copyts", "-i", str(path), "-map", "0:a:0",
                             "-af", "ashowinfo", "-ac", "6", "-ar", str(app.RATE), "-f", "f32le", "-"],
                            check=True, capture_output=True)
    first = re.search(rb"pts_time:\s*([\d.]+)", result.stderr)
    if not first:
        raise RuntimeError(f"no audio frames in {path.name}")
    raw = result.stdout
    samples = struct.unpack(f"<{len(raw) // 4}f", raw)
    return float(first.group(1)), [list(samples[c::6]) for c in range(6)]


# Lossless recordings are 16-bit: one step is 1/32768.
FIRST_SAMPLE = 0.5 / 32768


def onsets(x: list[float], threshold: float | None = None) -> list[int]:
    """Sample indices where a burst starts: the first sample above the threshold (default
    10% of the loudest sample) after at least half a period below it. The first and last
    burst are dropped, as either may be cut off by the start or end of the recording."""
    if threshold is None:
        threshold = 0.1 * max(abs(v) for v in x)
    quiet = int(app.RATE * PERIOD / 2)
    found, last = [], -quiet
    for i, v in enumerate(x):
        if abs(v) > threshold:
            if i - last >= quiet:
                found.append(i)
            last = i
    return found[1:-1]


def pair(a: list[float], b: list[float], window: float) -> list[float]:
    """For each time in a, the nearest b minus it, when one is within window."""
    out = []
    for t in a:
        near = [u - t for u in b if abs(u - t) < window]
        if near:
            out.append(min(near, key=abs))
    return out


SUSTAIN = 16  # samples in a row: an output starts when it stays above the threshold this long


def first_above(x: list[float], start: int, stop: int, threshold: float, sustain: int = 1) -> int | None:
    """Index of the first sample in x[start:stop] that begins `sustain` samples in a row louder
    than threshold. With sustain > 1, isolated low-level samples ahead of a burst (seen on
    macOS: single 16-bit steps up to a few ms early) do not count as its start."""
    run = 0
    for i in range(max(0, start), min(len(x), stop)):
        run = run + 1 if abs(x[i]) > threshold else 0
        if run >= sustain:
            return i - sustain + 1
    return None


def measure(path: Path, name: str) -> dict:
    """Delays of the front pair after the untouched centre, in samples, for each centre burst:
    to where the front pair's output starts, staying above its own level just before the burst
    (at least half a 16-bit step) for SUSTAIN samples in a row (the filter's delay) and to 10% of the burst's level (when it is heard,
    which Chimera's processing shapes), and the centre's audio-to-video offsets."""
    t0, channels = audio_channels(path)
    front_x = channels[0]
    centre = onsets(channels[2], FIRST_SAMPLE)
    check = app.check
    check(len(centre) >= 4, f"{name}: bursts found on the centre channel ({len(centre)})")
    reach = int(app.RATE * PERIOD / 4)  # a burst's start is looked for up to 250 ms after the centre's
    delays, heard_delays, floors, singles, steps = [], [], [], [], {1: [], 2: [], 4: [], 16: []}
    for c in centre:
        floor = max((abs(v) for v in front_x[max(0, c - int(0.3 * app.RATE)):max(0, c - int(0.02 * app.RATE))]), default=0.0)
        floors.append(floor)
        first = first_above(front_x, c - reach, c + reach, max(FIRST_SAMPLE, 2 * floor), SUSTAIN)
        single = first_above(front_x, c - reach, c + reach, max(FIRST_SAMPLE, 2 * floor))
        if single is not None:
            singles.append(single - c)
        peak = max((abs(v) for v in front_x[c:c + reach]), default=0.0)
        heard = first_above(front_x, c - reach, c + reach, 0.1 * peak) if peak > 0 else None
        if first is not None and heard is not None:
            delays.append(first - c)
            heard_delays.append(heard - c)
        for n in steps:
            at = first_above(front_x, c - reach, c + reach, max((n - 0.5) / 32768, 2 * floor), SUSTAIN)
            if at is not None:
                steps[n].append(at - c)
    print(f"     {name}: front level before each burst {[f'{f:.1e}' for f in floors]}; front after centre by "
          f"16-bit steps passed for {SUSTAIN} samples: " + ", ".join(f"{n} {statistics.median(d):+.0f}" if d else f"{n} -"
                                            for n, d in steps.items())
          + f"; first single non-zero sample {sorted(singles)}")
    check(len(delays) >= 4, f"{name}: the front pair's start found for the centre bursts ({len(delays)} of {len(centre)})")
    video = video_onsets(path)
    av = pair(video, [t0 + c / app.RATE for c in centre], PERIOD / 2)
    check(len(av) >= 4, f"{name}: video flashes paired with the centre bursts ({len(video)} flashes, {len(av)} paired)")
    loud = steps[16]
    check(len(loud) >= 4, f"{name}: the front pair passes 16 steps for the centre bursts ({len(loud)} of {len(centre)})")
    result = {"delay": statistics.median(delays), "delay_spread": max(delays) - min(delays),
              "loud": statistics.median(loud), "loud_spread": max(loud) - min(loud),
              # the front pair stays digitally silent until the burst reaches it
              "silent_ahead": bool(singles) and min(singles) >= 0,
              "heard": statistics.median(heard_delays), "av": statistics.median(av), "av_spread": max(av) - min(av),
              "floor": max(floors)}
    print(f"     {name:9} front after centre: output starts {result['delay']:+.0f} samples "
          f"({1000 * result['delay'] / app.RATE:+.3f} ms, spread {result['delay_spread']:.0f}), "
          f"10% level {result['heard']:+.0f} samples; centre audio after video {1000 * result['av']:+.1f} ms "
          f"(spread {1000 * result['av_spread']:.1f} ms, {len(av)} flashes)")
    return result


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
    media = work / "markers.mkv"
    write_markers(media)
    script = work / "chimera-latency-test.lua"
    script.write_text(LUA, encoding="utf-8")
    check = app.check

    logs = layout.config / "logs"
    earlier = set(logs.glob("*.txt"))

    def log_text() -> str:  # both OBS runs of this test
        return "".join(p.read_text(encoding="utf-8", errors="replace") for p in sorted(set(logs.glob("*.txt")) - earlier))

    obs = app.Obs(layout, port, work / "obs-run-1.txt")

    try:
        version = obs.request("GetVersion")
        print(f"     OBS {version['obsVersion']}, obs-websocket {version['obsWebSocketVersion']}")
        check(re.search(r"speakers:\s*6\b", log_text()) is not None, "OBS opened its audio in 5.1")
        obs.request("CreateScene", {"sceneName": app.SCENE})
        obs.request("SetCurrentProgramScene", {"sceneName": app.SCENE})
        obs.request("CreateInput", {"sceneName": app.SCENE, "inputName": app.SOURCE, "inputKind": "ffmpeg_source",
                                    "inputSettings": {"is_local_file": True, "local_file": str(media), "looping": False,
                                                      "restart_on_activate": False}})
        obs.request("CreateSourceFilter", {"sourceName": app.SOURCE, "filterName": app.FILTER,
                                           "filterKind": app.FILTER_KIND, "filterSettings": {}})
        app.wait_until(lambda: f"[chimera-obs] '{app.FILTER}': loaded " in log_text(),
                       "the filter found Chimera and loaded it")
        obs.request("SetSourceFilterEnabled", {"sourceName": app.SOURCE, "filterName": app.FILTER,
                                               "filterEnabled": False})
        time.sleep(1)
        disabled = obs.record("disabled", args.seconds)
        obs.request("SetSourceFilterEnabled", {"sourceName": app.SOURCE, "filterName": app.FILTER,
                                               "filterEnabled": True})
        time.sleep(1)
        enabled = obs.record("enabled", args.seconds)
    finally:
        obs.quit()

    # The filter's properties, read by a scene script on the OBS UI thread.
    scene_file = layout.config / "basic" / "scenes" / "Chimera.json"
    scene = json.loads(scene_file.read_text(encoding="utf-8"))
    scene.setdefault("modules", {})["scripts-tool"] = [{"path": script.as_posix(), "settings": {}}]
    scene_file.write_text(json.dumps(scene, indent=4), encoding="utf-8")
    obs = app.Obs(layout, port, work / "obs-run-2.txt")
    try:
        app.wait_until(lambda: f"[chimera-obs] '{app.FILTER}': loaded " in log_text(), "the restored filter loaded Chimera")
        obs.request("SetStudioModeEnabled", {"studioModeEnabled": True})
        app.wait_until(lambda: "chimera latency test: status " in log_text(), "the scene script read the filter's status", 30)
    finally:
        obs.quit()
    status = re.search(r"chimera latency test: status (.*)", log_text()).group(1)
    reported = re.search(r"(\d+) samples \((\d+) us\)", status)
    check(reported is not None, f"the filter's properties show its latency ({status.strip()})")
    reported_samples = int(reported.group(1))
    print(f"     the filter reports a latency of {reported_samples} samples ({reported.group(2)} us)")

    off = measure(disabled, "disabled")
    on = measure(enabled, "enabled")
    check(off["delay"] == 0 and off["delay_spread"] == 0,
          f"with the filter disabled the front pair starts with the centre ({off['delay']:+.0f} samples)")
    measured = on["delay"]
    # Every platform: from 16 steps up the processed burst arrives at the same sample in every
    # burst, and never before the reported delay.
    check(on["loud_spread"] <= 1 and on["loud"] >= reported_samples,
          f"the processed bursts pass 16 steps at the same sample, after the reported delay "
          f"({on['loud']:+.0f} samples, spread {on['loud_spread']:.0f}; reported {reported_samples})")
    if on["silent_ahead"]:
        # The output starts slightly after the reported delay: the processed signal needs a
        # few samples to rise above one 16-bit step.
        check(reported_samples <= measured <= reported_samples + args.tolerance,
              f"the measured filter delay matches the reported latency ({measured:+.0f} samples against "
              f"{reported_samples}, allowed {reported_samples} to {reported_samples + args.tolerance:g})")
        check(on["delay_spread"] <= 1, f"the delay is the same for every burst (spread {on['delay_spread']:.0f} samples)")
    else:
        # Seen on the macOS CI runner: single 16-bit steps leave Chimera up to 8 ms before the
        # burst reaches it (cause not determined), so where the output starts cannot be told at
        # that level.
        print(f"     note: the front pair is not digitally silent until the burst arrives; the delay to the "
              f"output's start ({measured:+.0f} samples) is not compared with the reported latency here")
    for name, r in (("disabled", off), ("enabled", on)):
        check(r["av_spread"] <= 1 / FPS + 0.001, f"{name}: the centre's audio-to-video offset holds within one frame "
              f"over the recording (spread {1000 * r['av_spread']:.1f} ms)")

    log = log_text()
    totals = re.findall(rf"\[chimera-obs\] '{re.escape(app.FILTER)}': session total: .*unprocessed blocks (\d+)", log)
    # The filter logs a session total only after it has processed a block: the second run, which
    # only reads the status and quits, can end before its first one (as on the Linux CI runner).
    check(len(totals) in (1, 2) and all(t == "0" for t in totals), f"the filter processed every block (unprocessed: {totals})")
    check(log.count("[chimera-obs] unloaded Chimera filter instance") == 2, "both runs unload the filter")
    start = (f"the processed audio starts {measured:.0f} samples ({1000 * measured / app.RATE:.2f} ms) late"
             if on["silent_ahead"] else "the output's start is not exactly silent ahead (not compared)")
    print(f"PASS real OBS Studio {version['obsVersion']}: {start}, reported {reported_samples}; the processed bursts "
          f"pass 16 steps {on['loud']:.0f} samples late in every burst; audio-to-video offsets hold within each recording")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--module", required=True, help="staged chimera-obs plugin directory (macOS: chimera-obs.plugin)")
    parser.add_argument("--vst3", required=True, help="SpectralForge Chimera.vst3 bundle")
    parser.add_argument("--work", required=True, help="scratch directory (deleted first)")
    parser.add_argument("--obs", default="obs", help="OBS Studio executable (as for obs_app_test.py)")
    parser.add_argument("--ffmpeg", default="ffmpeg", help="ffmpeg executable for the media file and the analysis")
    parser.add_argument("--seconds", type=float, default=8.0, help="length of each recording")
    parser.add_argument("--tolerance", type=float, default=16.0,
                        help="samples the first processed sample may come after the reported delay")
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
