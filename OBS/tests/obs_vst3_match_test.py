#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""The Chimera filter in a real OBS Studio recording against the same Chimera VST3 outside OBS (plan stage B).

Installs the filter as obs_app_test.py does (same layouts, same guards), runs OBS at
48 kHz 5.1 and plays one media file (not a loop): two different plucked notes, one on
front left and one on front right, each repeated with silence in between. The same two
notes are on the centre and rear left channels, which the filter passes through
untouched, so each lossless (16-bit PCM) recording also holds the exact audio the filter
received. The test feeds that audio to the same Chimera VST3 outside OBS (render-tool.cpp:
the same VST3 hosting, without OBS, its buffers or the filter's bridge) in OBS-sized
blocks, with Chimera's default state and then with a pushed preset, and compares:

- the delay between OBS's processed front pair and the processed reference (expected 0);
- the difference (OBS minus reference) over the recording after Chimera's tails from before
  the recording started have died away: in each channel, with the default state and with
  the preset, no sample may differ by more than one step of the 16-bit recording (the
  reference is not rounded to 16 bits: half a step where OBS rounds to the nearest step, as
  on Linux and Windows; on the macOS CI runner OBS's 16-bit conversion differs from the
  nearest step by up to one, deterministically, also where Chimera's output is far below a
  step);
- that the recorded copy of the filter's input is the media file, sample for sample, note by
  note: where OBS drops, repeats or shifts samples in the recording, the recorded input is
  not what the filter received; a recording with such a break is made once more (seen on the
  macOS CI runner), and if the second one breaks too, the notes around the break are reported
  and left out, and the result is inconclusive;
  in the OBS tick (1024 samples) before each note the difference is checked on its own,
  against a looser bound: on the macOS CI runner single steps leave the filter there,
  up to a tick ahead of the note, which Chimera outside OBS on the same runner does not
  put out (cause not determined; Linux and Windows match there too);
- the preset recording against the reference rendered with the default state: the
  residual must be large there, so the comparison tells Chimera states apart.

The OBS log must show the filter loading Chimera and processing every block, and OBS
must exit cleanly.
"""
import argparse
import array
import json
import math
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

PERIOD = 3.0  # seconds from one note to the next
NOTE = 1.0  # seconds a note sounds; the rest of each period is silence
MEDIA_SECONDS = 90.0
BLOCK = 1024  # frames per OBS audio tick (AUDIO_OUTPUT_FRAMES)
SHIFT = 200  # samples searched either way for the delay between OBS and the reference


def note(freq: float) -> str:
    """A plucked string: four harmonics, decaying, for NOTE seconds of every PERIOD."""
    phase = f"mod(t,{PERIOD})"
    tone = "+".join(f"sin(2*PI*{freq * k}*t)/{k}" for k in range(1, 5))
    return f"if(lt({phase},{NOTE}),0.3*exp(-3*{phase})*({tone}),0)"


def write_media(path: Path) -> None:
    """5.1 at 48 kHz, 16-bit (FL FR FC LFE BL BR): note A on FL and FC, note B on FR and BL."""
    a, b = note(110.0), note(164.81)
    subprocess.run([app.FFMPEG, "-v", "error", "-y", "-f", "lavfi", "-i",
                    f"aevalsrc=exprs='{a}|{b}|{a}|0|{b}|0':c=5.1:s={app.RATE}:d={MEDIA_SECONDS}",
                    "-c:a", "pcm_s16le", str(path)], check=True)


def channels6(path: Path) -> list[list[float]]:
    raw = subprocess.run([app.FFMPEG, "-v", "error", "-i", str(path), "-map", "0:a:0", "-ac", "6",
                          "-ar", str(app.RATE), "-f", "f32le", "-"], check=True, capture_output=True).stdout
    samples = struct.unpack(f"<{len(raw) // 4}f", raw)
    return [list(samples[c::6]) for c in range(6)]


def render(args: argparse.Namespace, left: list[float], right: list[float], state: Path | None,
           work: Path, name: str, resets: list[int] = ()) -> tuple[list[list[float]], int]:
    """The reference: the same input through Chimera outside OBS, in OBS-sized blocks, with
    Chimera's tails dropped before each frame in `resets` (as the filter drops them)."""
    source, target = work / f"{name}-input.f32", work / f"{name}-reference.f32"
    source.write_bytes(array.array("f", (v for pair in zip(left, right) for v in pair)).tobytes())
    command = [args.render_tool, str(Path(args.vst3).resolve()), str(app.RATE), "2", str(BLOCK), str(source), str(target)]
    if state is not None:
        command.append(str(state))
    if resets:
        command.append("--reset-at=" + ",".join(str(n) for n in resets))
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    if result.returncode:
        raise RuntimeError(f"render tool failed: {result.stderr.strip() or result.stdout.strip()}")
    latency = int(re.search(r"latency (\d+)", result.stdout).group(1))
    raw = target.read_bytes()
    samples = struct.unpack(f"<{len(raw) // 4}f", raw)
    return [list(samples[0::2]), list(samples[1::2])], latency


def onsets(x: list[float], start: int) -> list[int]:
    """Where each note starts (first sample above 1% of full scale after 0.5 s of quiet)."""
    found, quiet = [], 0
    for n in range(start, len(x)):
        if abs(x[n]) > 0.01:
            if quiet >= app.RATE // 2:
                found.append(n)
            quiet = 0
        else:
            quiet += 1
    return found


def input_breaks(recorded: list[float], media: list[float]) -> tuple[list[int], list[tuple[int, int]]]:
    """Where the recording's copy of the filter's input stops being an unbroken run of the media
    file (OBS dropped or repeated samples, or shifted the audio): per recorded note, its offset
    into the media, and the recorded spans (two notes either side of a break) to leave out of
    the comparison, since there the input the filter received and the recorded one differ."""
    period = int(PERIOD * app.RATE)
    media_notes = onsets(media, 0)
    offsets, spans = [], []
    for r in onsets(recorded, 0):
        exact = [m - r for m in media_notes
                 if recorded[r:r + period] == media[m:m + min(period, len(recorded) - r)]]
        # Every note in the media file is the same, so keep the offset of the note before when it fits.
        previous = offsets[-1] if offsets else None
        offsets.append(previous if previous in exact else exact[0] if exact else None)
        if not exact or (len(offsets) > 1 and offsets[-1] != offsets[-2]):
            spans.append((r - period, r + period))
    return offsets, spans


def best_shift(obs: list[float], ref: list[float], at: int) -> int:
    """The shift (samples, positive: OBS later) that best lines the reference up with OBS
    over 0.2 s from a note's start."""
    window = range(at, at + app.RATE // 5)

    def error(s: int) -> float:
        return sum((obs[n] - ref[n - s]) ** 2 for n in window)

    return min(range(-SHIFT, SHIFT + 1), key=error)


def alignment_notes(notes: list[int], skip: list[tuple[int, int]]) -> list[int]:
    """Use a note only when its entire alignment window is retained."""
    return [n for n in notes
            if not any(a < n + app.RATE // 5 and b > n for a, b in skip)]


def require_complete_comparison(results: dict) -> None:
    """A broken recording cannot establish full output equivalence."""
    if any(r["skipped_samples"] for k, r in results.items() if k != "control"):
        raise RuntimeError("INCONCLUSIVE: recorded input is discontinuous; retained spans were analysed, "
                           "but this recording cannot certify output equivalence (see diagnostics.json)")


def compare(obs: list[float], ref: list[float], shift: int, start: int, notes: list[int] = (),
            skip: list[tuple[int, int]] = ()) -> dict:
    """The difference over the recording from `start`. The OBS tick before each note (the
    1024 samples where, on the macOS CI runner, single 16-bit steps leave the filter ahead of
    the note but not Chimera outside OBS) is reported separately as `ahead_steps`."""
    stop = len(obs) - SHIFT
    ahead = {n for at in notes for n in range(at - BLOCK, at)}
    # Spans where the recorded input is not what the filter received are left out (see input_breaks).
    raw = [obs[n] - ref[n - shift] for n in range(start, stop)]
    excluded = [any(a <= n < b for a, b in skip) for n in range(start, stop)]
    diff = [0.0 if omit else d for d, omit in zip(raw, excluded)]
    count = max(1, len(raw) - sum(excluded))
    signal = math.sqrt(sum(obs[n] ** 2 for n, omit in zip(range(start, stop), excluded) if not omit) / count)
    residual = math.sqrt(sum(d * d for d in diff) / count)
    worst = max(range(len(raw)), key=lambda k: abs(raw[k]))
    sample = start + worst
    return {"signal": signal, "residual": residual,
            "skipped_samples": sum(excluded), "compared_samples": len(raw) - sum(excluded),
            "raw_max_steps": abs(raw[worst]) * 32768, "raw_worst_sample": sample,
            "raw_worst_seconds": sample / app.RATE, "raw_worst_excluded": excluded[worst],
            "raw_worst_obs": obs[sample], "raw_worst_reference": ref[sample - shift],
            # This phase is relative to the reference file, not an observed OBS block boundary.
            "reference_block_phase": (sample - shift) % BLOCK,
            "db": 20 * math.log10(max(residual, 1e-12) / max(signal, 1e-12)),
            "max_steps": max(abs(d) for n, d in zip(range(start, stop), diff) if n not in ahead) * 32768,
            # Where the largest difference is (seconds into the recording), and the largest per second.
            "worst_at": (start + max((k for k, n in enumerate(range(start, stop)) if n not in ahead),
                                     key=lambda k: abs(diff[k]))) / app.RATE,
            "per_second": [round(max(abs(d) for d in diff[k:k + app.RATE]) * 32768, 1)
                           for k in range(0, len(diff), app.RATE)],
            "ahead_steps": max((abs(d) for n, d in zip(range(start, stop), diff) if n in ahead), default=0.0) * 32768}


def first_outputs(x: list[float], notes: list[int], rounded: bool) -> list[int]:
    """Per note: the first sample, from one OBS tick before the note, that a 16-bit recording
    holds as non-zero (the reference is rounded as OBS rounds its recording), relative to the note."""
    found = []
    for at in notes:
        hit = next((n for n in range(at - BLOCK, min(len(x), at + 400))
                    if (round(x[n] * 32768) if rounded else x[n] * 32768) != 0), None)
        found.append(hit - at if hit is not None else None)
    return found


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
    write_media(media)
    preset = json.loads(subprocess.run([args.state_tool, str(Path(args.vst3).resolve()), app.PRESET_PARAMETER,
                                        str(app.PRESET_VALUE)], check=True, capture_output=True, text=True).stdout)
    preset_file = work / "preset.json"
    preset_file.write_text(json.dumps(preset), encoding="utf-8")
    source = channels6(media)
    check = app.check

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
        obs.request("CreateSourceFilter", {"sourceName": app.SOURCE, "filterName": app.FILTER,
                                           "filterKind": app.FILTER_KIND, "filterSettings": {}})
        app.wait_until(lambda: f"[chimera-obs] '{app.FILTER}': loaded " in log_text(),
                       "the filter found Chimera and loaded it")
        time.sleep(1)

        def record(name: str) -> Path:
            path = obs.record(name, args.seconds)
            ch = channels6(path)
            spans = [span for c in (2, 4) for span in input_breaks(ch[c], source[c])[1]]
            if spans:
                print(f"     {name}: OBS broke the recorded input around "
                      f"{[round(a / app.RATE + PERIOD, 2) for a, _ in spans]} s; recording again", flush=True)
                path = obs.record(f"{name} (again)", args.seconds)
            return path

        default = record("default")
        obs.request("SetSourceFilterSettings", {
            "sourceName": app.SOURCE, "filterName": app.FILTER, "overlay": True,
            "filterSettings": {"vst3_component_state": preset["component"],
                               "vst3_controller_state": preset["controller"]}})
        time.sleep(1)
        pushed = record("preset")
    finally:
        obs.quit()

    # OBS's own account of the run: the filter's input gaps and OBS's audio buffering changes,
    # either of which makes the recorded input differ from what the filter received in time.
    for line in log_text().splitlines():
        if "session total" in line or "audio buffering" in line or "Max audio buffering" in line:
            print(f"     log: {line.strip()}")
    names = ("left", "right")
    results = {}
    diagnostics = {"obs_version": version["obsVersion"], "rate": app.RATE, "block": BLOCK,
                   "warmup_seconds": args.warmup, "max_steps": args.max_steps,
                   "max_ahead_steps": args.max_ahead_steps, "channels": []}
    for name, path, state in (("default", default, None), ("preset", pushed, preset_file)):
        ch = channels6(path)
        skip = []
        for label, rec_c, media_c in (("centre", 2, 2), ("rear left", 4, 4)):
            offsets, spans = input_breaks(ch[rec_c], source[media_c])
            skip += spans
            print(f"     {name}: the recorded {label} (the filter's input) per note, offset into the media file: "
                  f"{offsets}{'' if not spans else f'; not an unbroken run around {[round(a / app.RATE + PERIOD, 2) for a, _ in spans]} s'}")
            check(sum(o is not None for o in offsets) >= 2,
                  f"{name}: the recorded {label} matches the media file sample for sample in at least two notes")
        if skip:
            print(f"     {name}: left out of the comparison (OBS broke the recorded input there): "
                  f"{[(round(a / app.RATE, 2), round(b / app.RATE, 2)) for a, b in skip]} s")
        kept = [n for n in onsets(ch[2], int(app.RATE * args.warmup)) if not any(a <= n < b for a, b in skip)]
        check(len(kept) >= 2, f"{name}: at least two notes left to compare after {args.warmup:g} s ({len(kept)})")
        check(len(ch[0]) > app.RATE * (args.warmup + 2 * PERIOD), f"{name}: the recording is long enough ({len(ch[0])} frames)")
        reference, latency = render(args, ch[2], ch[4], state, work, name)
        start = int(app.RATE * args.warmup)
        for i, (obs_x, ref_x, input_x) in enumerate(zip((ch[0], ch[1]), reference, (ch[2], ch[4]))):
            notes = onsets(input_x, start)
            check(len(notes) >= 2, f"{name} {names[i]}: notes found in the recording after {args.warmup:g} s ({len(notes)})")
            usable = alignment_notes(notes, skip)
            check(len(usable) >= 2, f"{name} {names[i]}: at least two retained alignment windows ({len(usable)})")
            shift = best_shift(obs_x, ref_x, usable[0])
            r = compare(obs_x, ref_x, shift, start, notes, skip)
            results[(name, i)] = dict(r, shift=shift, latency=latency)
            diagnostics["channels"].append(dict(r, recording=name, path=str(path), channel=names[i],
                                               shift=shift, latency=latency, alignment_sample=usable[0],
                                               notes=notes, excluded_spans=skip))
            (work / "diagnostics.json").write_text(json.dumps(diagnostics, indent=2), encoding="utf-8")
            print(f"     {name} {names[i]}: raw peak {r['raw_max_steps']:.5f} steps at sample "
                  f"{r['raw_worst_sample']} ({r['raw_worst_seconds']:.6f} s), "
                  f"excluded={r['raw_worst_excluded']}; {r['compared_samples']} compared, "
                  f"{r['skipped_samples']} excluded samples")
            # Where the output starts around each note, in OBS's recording and outside OBS: the
            # same positions mean that whatever comes before a note comes from Chimera itself.
            print(f"     {name:7} {names[i]:5}: first non-zero 16-bit sample per note: OBS "
                  f"{first_outputs(obs_x, notes, False)}, reference {first_outputs(ref_x, notes, True)}")
            print(f"     {name:7} {names[i]:5}: OBS after the reference by {shift:+d} samples, processed RMS "
                  f"{r['signal']:.4f}, residual {r['db']:+.1f} dB (largest {r['max_steps']:.2f} 16-bit steps, "
                  f"{r['ahead_steps']:.2f} in the OBS tick before a note), "
                  f"Chimera latency {latency}")
            print(f"     {name:7} {names[i]:5}: largest difference at {r['worst_at']:.3f} s; largest per second from "
                  f"{start / app.RATE:g} s, in 16-bit steps: {r['per_second']}")
        if name == "preset":
            # The same comparison against the default state's reference must fail clearly.
            wrong, _ = render(args, ch[2], ch[4], None, work, "preset-against-default")
            results["control"] = compare(ch[0], wrong[0], results[("preset", 0)]["shift"], start)
            print(f"     control: preset recording against the default state's reference: residual "
                  f"{results['control']['db']:+.1f} dB")

    for (name, i), r in sorted((k, v) for k, v in results.items() if k != "control"):
        check(r["shift"] == 0, f"{name} {names[i]}: OBS's processed audio lines up with the reference ({r['shift']:+d} samples)")
        check(r["signal"] > 0.001, f"{name} {names[i]}: the processed audio carries sound (RMS {r['signal']:.4f})")
        check(r["max_steps"] <= args.max_steps,
              f"{name} {names[i]}: OBS's output matches Chimera outside OBS (largest difference "
              f"{r['max_steps']:.2f} 16-bit steps outside the tick before each note, allowed {args.max_steps:g}; "
              f"residual {r['db']:+.1f} dB)")
        check(r["ahead_steps"] <= args.max_ahead_steps,
              f"{name} {names[i]}: in the OBS tick before each note OBS differs from Chimera outside OBS by at most "
              f"{r['ahead_steps']:.2f} 16-bit steps (allowed {args.max_ahead_steps:g})")
    check(results["control"]["db"] > -20,
          f"the comparison tells Chimera states apart (preset against default: {results['control']['db']:+.1f} dB)")

    log = log_text()
    check(log.count("[chimera-obs] module ") == 1, "the run logs the Chimera module")
    totals = re.findall(rf"\[chimera-obs\] '{re.escape(app.FILTER)}': session total: .*unprocessed blocks (\d+)", log)
    check(totals == ["0"], f"the filter processed every block (unprocessed: {totals})")
    check(log.count("[chimera-obs] unloaded Chimera filter instance") == 1, "the filter instance is unloaded")
    require_complete_comparison(results)
    worst = max(r["max_steps"] for k, r in results.items() if k != "control")
    print(f"PASS real OBS Studio {version['obsVersion']}: the filter's output matches the same Chimera VST3 outside "
          f"OBS with the default state and a pushed preset, sample-aligned, within {worst:.2f} 16-bit steps in each "
          f"channel; a different state differs by {results['control']['db']:+.1f} dB")


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
    parser.add_argument("--max-steps", type=float, default=1.01,
                        help="largest difference allowed from the reference, in steps of the 16-bit recording")
    parser.add_argument("--max-ahead-steps", type=float, default=2.0,
                        help="largest difference allowed in the OBS tick before a note (single steps the filter "
                             "puts out there on the macOS CI runner, not Chimera outside OBS; cause not determined)")
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

