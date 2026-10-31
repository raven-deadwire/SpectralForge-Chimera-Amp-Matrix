#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Records an hour through the Chimera filter in the real OBS Studio app (plan stage D).

Installs the filter as obs_app_test.py does (same layouts, same guards), then:

1. Plays one long media file, not a loop: every 2 s a 0.25 s harmonic burst with
   three white video frames at the same instant. A loop would restart its
   timestamps every pass; one file keeps the hour continuous.
2. Records for --minutes through the Chimera filter, polling OBS's own
   statistics (CPU, memory, skipped render and output frames) every minute.
3. Quits OBS normally and checks:
   - the filter's 10-minute realtime lines and its session total in the OBS log
     cover the hour with no unprocessed blocks (the test kit's PASS rule for
     one_hour_recording, parsed with the test kit's own pattern);
   - audio-to-video offset of the bursts in the first and the last minute of the
     recording differs by at most one video frame (start/end sync);
   - the last minute sounds like the first (level within 1 dB): no state build-up.
   Load, 20 ms windows over real time, input gaps, CPU, memory and skipped
   frames are printed for the record; a person judges those.
"""
import argparse
import importlib.util
import json
import math
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

spec = importlib.util.spec_from_file_location("obs_testkit", HERE.parents[1] / "Tools" / "obs_testkit.py")
testkit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(testkit)

PERIOD = 2.0  # seconds between markers
BURST = 0.25  # seconds of tone per marker
FPS = 30
SEGMENT = 60.0  # seconds analysed at the start and at the end


def write_markers(path: Path, seconds: float) -> None:
    """Black 320x180 video and silence, with a white flash and a tone burst every PERIOD."""
    tone = "+".join(f"sin(2*PI*{110 * k}*t)/{k}" for k in range(1, 9))
    audio = f"aevalsrc=exprs='if(lt(mod(t\\,{PERIOD})\\,{BURST})\\,0.2*({tone})\\,0)':s={app.RATE}:d={seconds}"
    flash = f"drawbox=x=0:y=0:w=iw:h=ih:color=white:t=fill:enable='lt(mod(t\\,{PERIOD})\\,{3 / FPS - 0.001})'"
    subprocess.run([app.FFMPEG, "-v", "error", "-y", "-f", "lavfi", "-i", f"color=c=black:s=320x180:r={FPS}:d={seconds}",
                    "-f", "lavfi", "-i", audio, "-vf", flash, "-ac", "2", "-c:v", "libx264", "-preset", "ultrafast",
                    "-g", str(int(PERIOD * FPS)), "-pix_fmt", "yuv420p", "-c:a", "flac", str(path)], check=True)


def video_onsets(path: Path, start: float, seconds: float) -> list[float]:
    """Times (original timestamps) of the first white frame of each flash."""
    result = subprocess.run([app.FFMPEG, "-v", "info", "-copyts", "-ss", str(start), "-t", str(seconds), "-i", str(path),
                             "-map", "0:v:0", "-vf", "scale=8:8,format=gray,showinfo", "-f", "null", "-"],
                            check=True, capture_output=True, text=True)
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


def audio_segment(path: Path, start: float, seconds: float) -> tuple[float, list[float]]:
    """(timestamp of the first sample, mono samples) for that part of the recording."""
    result = subprocess.run([app.FFMPEG, "-v", "info", "-copyts", "-ss", str(start), "-t", str(seconds), "-i", str(path),
                             "-map", "0:a:0", "-af", "ashowinfo", "-ac", "1", "-ar", str(app.RATE), "-f", "f32le", "-"],
                            check=True, capture_output=True)
    first = re.search(rb"pts_time:\s*([\d.]+)", result.stderr)
    if not first:
        raise RuntimeError(f"no audio frames in {path.name} at {start:.0f} s")
    raw = result.stdout
    return float(first.group(1)), list(struct.unpack(f"<{len(raw) // 4}f", raw))


def audio_onsets(t0: float, x: list[float]) -> list[float]:
    """Times where a burst starts: the first sample above 10% of the loudest one after
    at least half a period of quiet."""
    threshold = 0.1 * max(abs(v) for v in x)
    quiet = int(app.RATE * PERIOD / 2)
    onsets, last = [], -quiet
    for i, v in enumerate(x):
        if abs(v) > threshold:
            if i - last >= quiet:
                onsets.append(t0 + i / app.RATE)
            last = i
    return onsets


def sync(path: Path, start: float) -> tuple[float, float, int]:
    """(median audio-minus-video offset in seconds, spread, markers paired) in one segment."""
    video = video_onsets(path, start, SEGMENT)
    t0, samples = audio_segment(path, start, SEGMENT)
    audio = audio_onsets(t0, samples)
    offsets = []
    # A marker at either edge of the segment may be cut off in one of the two streams.
    for v in (v for v in video if start + PERIOD / 2 < v < start + SEGMENT - PERIOD / 2):
        near = [a - v for a in audio if abs(a - v) < PERIOD / 2]
        if near:
            offsets.append(min(near, key=abs))
    if len(offsets) < SEGMENT / PERIOD / 2:
        raise RuntimeError(f"only {len(offsets)} audio/video markers paired at {start:.0f} s "
                           f"({len(video)} flashes, {len(audio)} bursts)")
    return statistics.median(offsets), max(offsets) - min(offsets), len(offsets)


def burst_level(path: Path, start: float) -> float:
    _, x = audio_segment(path, start, SEGMENT)
    return app.rms(x)


def duration(path: Path) -> float:
    result = subprocess.run([app.FFMPEG, "-v", "info", "-i", str(path), "-f", "null", "-"],
                            check=True, capture_output=True, text=True)
    times = re.findall(r"time=(\d+):(\d\d):([\d.]+)", result.stderr)
    if not times:
        raise RuntimeError(f"could not read the length of {path.name}")
    h, m, s = times[-1]
    return int(h) * 3600 + int(m) * 60 + float(s)


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
    record_seconds = args.minutes * 60
    media = work / "markers.mkv"
    started = time.monotonic()
    write_markers(media, record_seconds + 180)
    print(f"     marker file {media.stat().st_size / 1e6:.1f} MB written in {time.monotonic() - started:.0f} s", flush=True)
    own = sync(media, 60)[0]
    app.check(abs(own) < 0.002, f"the marker file itself is in sync ({1000 * own:+.1f} ms)")
    logs = layout.config / "logs"

    def log_text() -> str:
        return "".join(p.read_text(errors="replace") for p in sorted(logs.glob("*.txt")))

    obs = app.Obs(layout, port, work / "obs-run.txt")
    samples = []
    try:
        version = obs.request("GetVersion")
        print(f"     OBS {version['obsVersion']}, obs-websocket {version['obsWebSocketVersion']}")
        obs.request("CreateScene", {"sceneName": app.SCENE})
        obs.request("SetCurrentProgramScene", {"sceneName": app.SCENE})
        obs.request("CreateInput", {"sceneName": app.SCENE, "inputName": app.SOURCE, "inputKind": "ffmpeg_source",
                                    "inputSettings": {"is_local_file": True, "local_file": str(media), "looping": False,
                                                      "restart_on_activate": False}})
        obs.request("CreateSourceFilter", {"sourceName": app.SOURCE, "filterName": app.FILTER,
                                           "filterKind": app.FILTER_KIND, "filterSettings": {}})
        app.wait_until(lambda: f"[chimera-obs] '{app.FILTER}': loaded " in log_text(), "the filter found Chimera and loaded it")
        obs.request("StartRecord")
        app.wait_until(lambda: obs.request("GetRecordStatus")["outputActive"], "recording started", 15)
        begin = time.monotonic()
        while (elapsed := time.monotonic() - begin) < record_seconds:
            time.sleep(min(60.0, record_seconds - elapsed))
            stats = obs.request("GetStats")
            samples.append((time.monotonic() - begin, stats))
            print(f"     {samples[-1][0] / 60:5.1f} min: CPU {stats['cpuUsage']:.1f}%, memory {stats['memoryUsage']:.0f} MB, "
                  f"skipped render {stats['renderSkippedFrames']}/{stats['renderTotalFrames']}, "
                  f"output {stats['outputSkippedFrames']}/{stats['outputTotalFrames']}", flush=True)
        recording = Path(obs.request("StopRecord")["outputPath"])
        app.wait_until(lambda: not obs.request("GetRecordStatus")["outputActive"], "recording stopped", 60)
    finally:
        obs.quit()

    length = duration(recording)
    app.check(length >= record_seconds - 5, f"the recording lasts {length / 60:.1f} min")

    lines = [m.groupdict() for m in map(testkit.REALTIME_LINE.search, log_text().splitlines())
             if m and m.group("filter") == app.FILTER]
    for line in lines:
        print(f"     {line['when']:13} {line['h']}:{line['m']}:{line['s']} audio, load {line['average']}% peak {line['peak']}%, "
              f"over real time {line['overruns']}, gaps {line['gaps']} (after silence {line['restarts']}), "
              f"unprocessed {line['unprocessed']}")
    realtime = [line for line in lines if line["when"] == "realtime"]
    total = [line for line in lines if line["when"] == "session total"]
    seconds = lambda line: int(line["h"]) * 3600 + int(line["m"]) * 60 + int(line["s"])  # noqa: E731
    app.check(len(realtime) >= args.minutes // 10, f"{len(realtime)} realtime lines, one per 10 minutes of audio")
    app.check(len(total) == 1 and seconds(total[0]) >= record_seconds - 60,
              f"the session total covers the recording ({total[0]['h']}:{total[0]['m']}:{total[0]['s']} of audio)"
              if total else "the session total is logged")
    app.check(all(line["unprocessed"] == "0" for line in lines), "no unprocessed blocks in any line")
    # RealtimeStats closes a window after a whole source block crosses 20 ms.
    # Large media-source blocks therefore make longer windows; audio seconds
    # divided by 20 ms is not the number of windows actually measured.
    print(f"     over-real-time windows (at least 20 ms of audio): {total[0]['overruns']}; "
          f"input gaps {total[0]['gaps']}, source blocks {total[0]['blocks']}")

    first_offset, first_spread, first_pairs = sync(recording, 30)
    last_start = length - SEGMENT - 5
    last_offset, last_spread, last_pairs = sync(recording, last_start)
    print(f"     sync at the start: audio {1000 * first_offset:+.1f} ms after video ({first_pairs} markers, spread {1000 * first_spread:.1f} ms)")
    print(f"     sync at the end:   audio {1000 * last_offset:+.1f} ms after video ({last_pairs} markers, spread {1000 * last_spread:.1f} ms)")
    app.check(abs(last_offset - first_offset) <= 1 / FPS + 0.001,
              f"start/end sync within one frame (drift {1000 * (last_offset - first_offset):+.1f} ms over {length / 60:.0f} min)")
    level_db = 20 * math.log10(burst_level(recording, last_start) / burst_level(recording, 30))
    app.check(abs(level_db) < 1.0, f"the last minute sounds like the first ({level_db:+.2f} dB)")

    after_start = [s for t, s in samples if t >= 5 * 60] or [s for _, s in samples]
    memory = [s["memoryUsage"] for s in after_start]
    print(f"     OBS memory from minute 5 to the end: {memory[0]:.0f} -> {memory[-1]:.0f} MB (max {max(memory):.0f}); "
          f"CPU average {statistics.mean(s['cpuUsage'] for _, s in samples):.1f}%")
    last = samples[-1][1]
    print(f"     skipped frames: render {last['renderSkippedFrames']}/{last['renderTotalFrames']}, "
          f"output {last['outputSkippedFrames']}/{last['outputTotalFrames']}")
    summary = {"obs_version": version["obsVersion"], "minutes": args.minutes, "recording_seconds": length,
               "realtime_lines": lines, "sync_start_ms": 1000 * first_offset, "sync_end_ms": 1000 * last_offset,
               "level_change_db": level_db, "obs_stats": [{"seconds": t, **s} for t, s in samples]}
    (work / "long-record-summary.json").write_text(json.dumps(summary, indent=1), encoding="utf-8")
    print(f"PASS real OBS Studio {version['obsVersion']}: {length / 60:.0f} min recorded through Chimera, "
          f"{seconds(total[0])} s of filter audio with no unprocessed blocks, start/end sync within one frame")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--module", required=True, help="staged chimera-obs plugin directory (bin/, data/)")
    parser.add_argument("--vst3", required=True, help="SpectralForge Chimera.vst3 bundle")
    parser.add_argument("--work", required=True, help="scratch directory (deleted first)")
    parser.add_argument("--obs", default="obs", help="OBS Studio executable")
    parser.add_argument("--ffmpeg", default="ffmpeg", help="ffmpeg executable for the marker file and the analysis")
    parser.add_argument("--minutes", type=int, default=61, help="length of the recording (61: the hour plus a margin)")
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
        for log in [*sorted(work.glob("obs-run*.txt")), *sorted((args.layout.config / "logs").glob("*.txt"))[-1:]]:
            print(f"--- {log.name} (last 60 lines)", file=sys.stderr)
            print("\n".join(log.read_text(errors="replace").splitlines()[-60:]), file=sys.stderr)
        return 1
    finally:
        # Kept for the CI artifact on every run: on Windows and macOS a passing
        # run removes the OBS configuration, logs included.
        try:
            if (args.layout.config / "logs").is_dir():
                shutil.copytree(args.layout.config / "logs", Path(args.work).resolve() / "obs-logs", dirs_exist_ok=True)
        except OSError as e:  # must not keep the filter installed
            print(f"note: could not keep the OBS logs: {e}", file=sys.stderr)
        args.layout.uninstall(passed)
    return 0


if __name__ == "__main__":
    sys.exit(main())

