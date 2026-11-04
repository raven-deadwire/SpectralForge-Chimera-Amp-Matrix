#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""The Chimera filter in the real OBS Studio app set to other audio formats (plan stages D, F).

OBS fixes its sample rate and channel layout at startup (Settings > Audio, applied on
restart), so each format is its own OBS run with its own profile. The filter, installed
as obs_app_test.py does (same layouts, same guards), prepares Chimera for that format:

1. 44.1 kHz mono: a stereo DI loop, which OBS mixes down. A recording with the filter
   enabled must sound different from one with it disabled.
2. 48 kHz 5.1: a six-channel loop with the DI on front left/right and a different tone
   on each of the other four channels. Chimera processes the first two channels and the
   filter passes the others through: with the filter enabled the front pair must change,
   and centre, LFE and both rear channels must keep their level and spectrum.

In each run OBS's log must show the format it opened, and the filter must load Chimera,
process every block (no unprocessed blocks in its session statistics) and unload, and
OBS must exit cleanly.
"""
import argparse
import math
import os
import re
import shutil
import struct
import subprocess
import sys
import time
import wave
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import obs_app_test as app  # noqa: E402

# Passed-through tones on centre, LFE (low, as an LFE channel would carry) and the rear pair.
SURROUND_TONES = (440.0, 55.0, 660.0, 880.0)
FORMATS = (
    {"name": "44.1 kHz mono", "rate": 44100, "setup": "Mono", "speakers": 1},
    {"name": "48 kHz 5.1", "rate": 48000, "setup": "5.1", "speakers": 6},
)


def write_surround(path: Path, seconds: float = 2.0) -> None:
    """Six channels at 48 kHz (FL FR FC LFE BL BR): the DI on the front pair, one sine each elsewhere.
    Every tone fits a whole number of periods in the loop, so it repeats seamlessly."""
    rate = app.RATE
    frames = bytearray()
    for n in range(int(rate * seconds)):
        t = n / rate
        di = sum(math.sin(2 * math.pi * 110.0 * k * t) / k for k in range(1, 9)) * 0.2
        v = int(max(-1.0, min(1.0, di)) * 32767)
        rest = [int(0.2 * math.sin(2 * math.pi * f * t) * 32767) for f in SURROUND_TONES]
        frames += struct.pack("<6h", v, v, *rest)
    with wave.open(str(path), "wb") as w:
        w.setnchannels(6)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(bytes(frames))


def decode_channels(path: Path, channels: int, skip: float = 1.0, seconds: float = 3.0) -> list[list[float]]:
    """Each channel of a recording, resampled to 48 kHz for the band analysis."""
    raw = subprocess.run(
        [app.FFMPEG, "-v", "error", "-ss", str(skip), "-t", str(seconds), "-i", str(path), "-vn", "-ac",
         str(channels), "-ar", str(app.RATE), "-f", "f32le", "-"], check=True, capture_output=True).stdout
    samples = struct.unpack(f"<{len(raw) // 4}f", raw)
    return [list(samples[c::channels]) for c in range(channels)]


def peak_hz(x: list[float]) -> float:
    """The strongest frequency in the first 8192 samples (about 6 Hz resolution at 48 kHz)."""
    size = 8192
    spectrum = app.fft([v * (0.5 - 0.5 * math.cos(2 * math.pi * n / size)) for n, v in enumerate(x[:size])])
    k = max(range(1, size // 2), key=lambda i: abs(spectrum[i]))
    return k * app.RATE / size


def run_format(args: argparse.Namespace, fmt: dict, port: int) -> None:
    layout = args.layout
    work = Path(args.work).resolve()
    tag = fmt["setup"].replace(".", "")
    recordings = work / f"recordings-{tag}"
    recordings.mkdir(parents=True)
    # A fresh profile and scene for each format: OBS reads both at startup.
    # OBS falls back to Chimera.json.bak when Chimera.json is missing, so both go.
    for scene_file in (layout.config / "basic" / "scenes").glob("Chimera.json*"):
        scene_file.unlink()
    app.configure(layout.config, port, recordings, fmt["rate"], fmt["setup"])
    # Lossless recordings (PCM audio): AAC at OBS's bitrate for six channels changes the
    # rear channels' level by more than a dB from one recording to the next.
    app.lossless_recordings(layout.config)
    if fmt["speakers"] == 6:
        loop = work / "surround-loop.wav"
        write_surround(loop)
    else:
        loop = work / "di-loop.wav"
        app.write_di(loop)
    check = app.check

    print(f"--- OBS at {fmt['name']}", flush=True)
    obs = app.Obs(layout, port, work / f"obs-run-{tag}.txt")
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
              f"OBS opened its audio at {fmt['rate']} Hz with {fmt['speakers']} speaker(s)")
        obs.request("CreateScene", {"sceneName": app.SCENE})
        obs.request("SetCurrentProgramScene", {"sceneName": app.SCENE})
        obs.request("CreateInput", {"sceneName": app.SCENE, "inputName": app.SOURCE, "inputKind": "ffmpeg_source",
                                    "inputSettings": {"is_local_file": True, "local_file": str(loop), "looping": True,
                                                      "restart_on_activate": False}})
        obs.request("CreateSourceFilter", {"sourceName": app.SOURCE, "filterName": app.FILTER,
                                           "filterKind": app.FILTER_KIND, "filterSettings": {}})
        app.wait_until(lambda: f"[chimera-obs] '{app.FILTER}': loaded " in log_text(),
                       "the filter found Chimera and loaded it")
        check("could not be prepared" not in log_text(), f"Chimera was prepared for {fmt['name']}")

        obs.request("SetSourceFilterEnabled", {"sourceName": app.SOURCE, "filterName": app.FILTER,
                                               "filterEnabled": False})
        time.sleep(1)
        dry = decode_channels(obs.record("dry", args.seconds), fmt["speakers"])
        obs.request("SetSourceFilterEnabled", {"sourceName": app.SOURCE, "filterName": app.FILTER,
                                               "filterEnabled": True})
        time.sleep(1)
        wet = decode_channels(obs.record("filtered", args.seconds), fmt["speakers"])
    finally:
        obs.quit()

    names = ["mono"] if fmt["speakers"] == 1 else ["front L", "front R", "centre", "LFE", "rear L", "rear R"]
    for i, name in enumerate(names):
        a, b = app.rms(dry[i]), app.rms(wet[i])
        print(f"     {name:8} dry RMS {a:.4f}  filtered RMS {b:.4f}  "
              f"bands dry {[round(v, 3) for v in app.band_profile(dry[i])]} "
              f"filtered {[round(v, 3) for v in app.band_profile(wet[i])]}")
        check(a > 0.001 and b > 0.001, f"{name} carries sound with the filter disabled and enabled")
    processed = 1 if fmt["speakers"] == 1 else 2
    for i in range(processed):
        shape = app.distance(app.band_profile(wet[i]), app.band_profile(dry[i]))
        level = 20 * math.log10(app.rms(wet[i]) / app.rms(dry[i]))
        check(shape > 0.1 or abs(level) > 1.0,
              f"Chimera changes {names[i]} (spectral distance from dry {shape:.3f}, level {level:+.1f} dB)")
    for i in range(processed, fmt["speakers"]):
        shape = app.distance(app.band_profile(wet[i]), app.band_profile(dry[i]))
        level = 20 * math.log10(app.rms(wet[i]) / app.rms(dry[i]))
        check(shape < 0.01 and abs(level) < 0.2,
              f"{names[i]} passes through unchanged (spectral distance {shape:.3f}, level {level:+.1f} dB)")
    if fmt["speakers"] == 6:
        # Each passed-through channel keeps its own tone: none is swapped or mixed into another.
        dry_peaks = [peak_hz(dry[i]) for i in range(2, 6)]
        wet_peaks = [peak_hz(wet[i]) for i in range(2, 6)]
        print(f"     tones dry {[round(f) for f in dry_peaks]} Hz, filtered {[round(f) for f in wet_peaks]} Hz")
        check(len({round(f / 20) for f in dry_peaks}) == 4
              and all(abs(d - w) < 12 for d, w in zip(dry_peaks, wet_peaks))
              and all(min(abs(d - t) for t in SURROUND_TONES) < 12 for d in dry_peaks),
              "centre, LFE and the rear pair keep their own tones with the filter enabled")

    log = log_text()
    check(log.count("[chimera-obs] module ") == 1, "the run logs the Chimera module")
    totals = re.findall(rf"\[chimera-obs\] '{re.escape(app.FILTER)}': session total: .*unprocessed blocks (\d+)", log)
    check(len(totals) == 1 and totals[0] == "0", f"the filter processed every block (unprocessed: {totals})")
    check(log.count("[chimera-obs] unloaded Chimera filter instance") == 1, "the filter instance is unloaded")


def run(args: argparse.Namespace) -> None:
    work = Path(args.work).resolve()
    if work.exists():
        shutil.rmtree(work)
    work.mkdir(parents=True)
    args.layout.install(Path(args.module).resolve(), Path(args.vst3).resolve())
    port = app.free_port()
    for fmt in FORMATS:
        run_format(args, fmt, port)
    print("PASS real OBS Studio at 44.1 kHz mono and 48 kHz 5.1: the filter loads and processes Chimera in each "
          "format, the front pair is processed and the other 5.1 channels pass through unchanged, clean shutdowns")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--module", required=True, help="staged chimera-obs plugin directory (macOS: chimera-obs.plugin)")
    parser.add_argument("--vst3", required=True, help="SpectralForge Chimera.vst3 bundle")
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
