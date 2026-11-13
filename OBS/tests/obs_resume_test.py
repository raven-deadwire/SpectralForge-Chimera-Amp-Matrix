#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""The Chimera filter after an interruption in a real OBS Studio recording (plan stage D).

OBS does not run a disabled filter, and a paused media source sends no audio, so Chimera
keeps whatever it held when its input stopped. The filter drops those tails when it is
re-enabled and when its input resumes after a second or more (README: "꼬리를 지운다").
This test checks both in one lossless recording of the real OBS app at 48 kHz 5.1, with
the media file of obs_vst3_match_test.py (a note on front left and one on front right,
1 s of note and 2 s of silence, the same notes on the centre and rear left, which the
filter passes through) plus a steady pilot tone on the LFE channel, which shows where
the media was paused:

- filter re-enabled: the filter is disabled half a second into a note (its tail still
  ringing in Chimera) and enabled again in the silence after the note. From the end of
  that note to the next one the front pair must stay silent (within one 16-bit step):
  no tail from before the filter was disabled;
- input resumed: the media is paused just after a note ends, while Chimera's tail rings,
  and played again 1.5 s later. From the resume to the next note the front pair must stay
  silent, and the filter's session total must count one restart;
- control: Chimera outside OBS (render-tool.cpp) given the same input without the
  interrupted span and without a reset plays a clear tail at the same place, so the
  checks above would see a tail that was not dropped; with a reset there, it is silent;
- the notes after each interruption match Chimera outside OBS as in obs_vst3_match_test.py
  (the filter keeps processing normally afterwards).

The OBS log must show the filter loading Chimera and processing every block, and OBS
must exit cleanly.
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
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import obs_app_test as app  # noqa: E402
import obs_vst3_match_test as match  # noqa: E402

PERIOD_MS = int(match.PERIOD * 1000)
NOTE_MS = int(match.NOTE * 1000)
STEP = 1 / 32768
MEDIA_SECONDS = 60.0
PAUSE_SECONDS = 1.5
TAIL_STEPS = 16  # the control's tail must reach this, so the test can see a tail that was kept
# The notes after the interruptions against Chimera outside OBS: the 16-bit conversion (up to
# a step from nearest on the macOS CI runner) plus the float difference between the filter and
# Chimera outside OBS, seen at about 0.1 step on Linux before any interruption.
MATCH_STEPS = 1.25


def write_media(path: Path) -> None:
    """5.1 at 48 kHz, 16-bit: note A on FL and FC, note B on FR and BL, a 50 Hz pilot on LFE."""
    a, b = match.note(110.0), match.note(164.81)
    pilot = "0.05*sin(2*PI*50*t)+0.02"
    subprocess.run([app.FFMPEG, "-v", "error", "-y", "-f", "lavfi", "-i",
                    f"aevalsrc=exprs='{a}|{b}|{a}|{pilot}|{b}|0':c=5.1:s={app.RATE}:d={MEDIA_SECONDS}",
                    "-c:a", "pcm_s16le", str(path)], check=True)


def zero_runs(x: list[float], shortest: int) -> list[tuple[int, int]]:
    """Spans of at least `shortest` samples that are exactly zero."""
    runs, start = [], None
    for n, v in enumerate(x + [1.0]):
        if v == 0.0 and start is None:
            start = n
        elif v != 0.0 and start is not None:
            if n - start >= shortest:
                runs.append((start, n))
            start = None
    return runs


def dry_runs(front: list[float], centre: list[float], shortest: int) -> list[tuple[int, int]]:
    """Spans where the front channel is the untouched input and a note sounds in them: the filter
    was disabled. (A span runs on through the silence after the note, where a filter that drops
    its tails puts out silence too.)"""
    runs, start = [], None
    for n, (f, c) in enumerate(zip(front + [1.0], centre + [0.0])):
        if f == c and start is None:
            start = n
        elif f != c and start is not None:
            if n - start >= shortest and max(abs(v) for v in centre[start:n]) > 0.01:
                runs.append((start, n))
            start = None
    return runs


def steps(x: list[float], lo: int, hi: int) -> float:
    return max((abs(v) for v in x[max(0, lo):max(lo, hi)]), default=0.0) / STEP


def wait_phase(obs: app.Obs, lo: int, hi: int, after_ms: int = 0) -> int:
    """Wait until the media cursor is lo..hi ms into a note period (and past after_ms); returns the cursor."""
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        cursor = obs.request("GetMediaInputStatus", {"inputName": app.SOURCE}).get("mediaCursor") or 0
        if cursor >= after_ms and lo <= cursor % PERIOD_MS <= hi:
            return cursor
        time.sleep(0.01)
    raise RuntimeError(f"the media cursor did not reach {lo}-{hi} ms into a note")


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
    check = app.check

    logs = layout.config / "logs"
    earlier = set(logs.glob("*.txt"))

    def log_text() -> str:
        return "".join(p.read_text(encoding="utf-8", errors="replace") for p in sorted(set(logs.glob("*.txt")) - earlier))

    filter_ref = {"sourceName": app.SOURCE, "filterName": app.FILTER}
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
        obs.request("CreateSourceFilter", {**filter_ref, "filterKind": app.FILTER_KIND, "filterSettings": {}})
        app.wait_until(lambda: f"[chimera-obs] '{app.FILTER}': loaded " in log_text(),
                       "the filter found Chimera and loaded it")
        time.sleep(1)
        obs.request("StartRecord")
        app.wait_until(lambda: obs.request("GetRecordStatus")["outputActive"], "recording started", 15)
        # Chimera settles on the recorded input first (obs_vst3_match_test.py's warm-up).
        start = wait_phase(obs, 0, 100, (obs.request("GetMediaInputStatus", {"inputName": app.SOURCE})
                                         .get("mediaCursor") or 0) + int(args.warmup * 1000))
        wait_phase(obs, 450, 600, start)
        obs.request("SetSourceFilterEnabled", {**filter_ref, "filterEnabled": False})
        disabled_at = time.monotonic()
        wait_phase(obs, NOTE_MS + 300, NOTE_MS + 500, start)
        obs.request("SetSourceFilterEnabled", {**filter_ref, "filterEnabled": True})
        disabled_for = time.monotonic() - disabled_at
        print(f"     filter disabled for {disabled_for:.2f} s, from about 0.5 s into a note to 0.3 s after it")
        second = wait_phase(obs, 0, 100, start + PERIOD_MS)
        # Past the note by more than the media cursor leads the recorded audio (60 ms or more
        # on the macOS CI runner), while Chimera's tail is still well above a step.
        wait_phase(obs, NOTE_MS + 100, NOTE_MS + 180, second + PERIOD_MS)
        obs.request("TriggerMediaInputAction", {"inputName": app.SOURCE,
                                                "mediaAction": "OBS_WEBSOCKET_MEDIA_INPUT_ACTION_PAUSE"})
        time.sleep(PAUSE_SECONDS)
        obs.request("TriggerMediaInputAction", {"inputName": app.SOURCE,
                                                "mediaAction": "OBS_WEBSOCKET_MEDIA_INPUT_ACTION_PLAY"})
        print(f"     media paused for {PAUSE_SECONDS:g} s, just after a note ended")
        time.sleep(match.PERIOD * 2 + 0.5)
        path = Path(obs.request("StopRecord")["outputPath"])
        app.wait_until(lambda: not obs.request("GetRecordStatus")["outputActive"], "recording stopped", 15)
        check(path.is_file() and path.stat().st_size > 0, f"recording written ({path.name})")
    finally:
        obs.quit()

    ch = match.channels6(path)
    front, centre, lfe = ch[0], ch[2], ch[3]
    rate = app.RATE
    notes = match.onsets(centre, 0)
    print(f"     notes in the recording at {[round(n / rate, 2) for n in notes]} s")

    # Filter re-enabled: the dry stretch shows the filter was off during a note.
    dry = dry_runs(front, centre, rate // 10)
    check(len(dry) == 1, f"the filter was disabled during one note (front pair dry for {[(round(a / rate, 2), round(b / rate, 2)) for a, b in dry]} s)")
    off_at = dry[0][0]
    toggled = max(n for n in notes if n <= off_at)
    note_end = toggled + int(match.NOTE * rate)
    after = min(n for n in notes if n > note_end)
    kept = steps(front, note_end, after - match.BLOCK)
    print(f"     filter re-enabled: front left from the end of that note to the next, largest {kept:.2f} 16-bit steps")
    check(kept <= 1.01, f"after the filter is re-enabled Chimera's tail from before is gone (largest {kept:.2f} steps)")

    # Input resumed: the pilot's silent stretch is where the media was paused.
    paused = [(a, b) for a, b in zero_runs(lfe, rate // 5)]
    check(len(paused) == 1, f"the media was paused once (pilot silent for {[(round(a / rate, 2), round((b - a) / rate, 2)) for a, b in paused]} s)")
    pause_at, resume_at = paused[0]
    check(resume_at - pause_at >= rate, f"the pause lasted a second or more ({(resume_at - pause_at) / rate:.2f} s)")
    before = max(n for n in notes if n < pause_at)
    next_note = min(n for n in notes if n > resume_at)
    left = steps(front, resume_at, next_note - match.BLOCK)
    tail_at_pause = steps(front, pause_at - rate // 50, pause_at)
    print(f"     input resumed: front left in the last 20 ms before the pause {tail_at_pause:.1f} steps (Chimera's tail), "
          f"from the resume to the next note largest {left:.2f} steps")
    # The note had ended in the recording: the filter's input (the centre copy) is silent from the
    # note's end (NOTE after its first sample; the note stops there, not fading out) to the pause.
    # The pause may land only a few ms after the note: the recorded audio trails the media cursor
    # by a varying amount (on the macOS CI runner once 1 ms after the note's end).
    input_end = before + int(match.NOTE * rate)
    input_before = steps(centre, input_end, pause_at) if pause_at > input_end else None
    check(input_before == 0 and tail_at_pause > TAIL_STEPS,
          f"the media was paused after a note ended ({(pause_at - before) / rate:.3f} s after it started, input from "
          f"the note's end to the pause {'-' if input_before is None else f'{input_before:.1f}'} steps), "
          f"while Chimera's tail rang ({tail_at_pause:.1f} steps)")
    check(left <= 1.01, f"after the input resumes Chimera's tail from before the pause is gone (largest {left:.2f} steps)")

    # Control: the same input without the interrupted spans, through Chimera outside OBS.
    on_at = off_at + int(disabled_for * rate)
    for name, cut_from, cut_to in (("re-enabled", off_at, on_at), ("resumed", pause_at, resume_at)):
        left_in = centre[:cut_from] + centre[cut_to:]
        right_in = ch[4][:cut_from] + ch[4][cut_to:]
        kept_tail, _ = match.render(args, left_in, right_in, None, work, f"{name}-no-reset")
        dropped, _ = match.render(args, left_in, right_in, None, work, f"{name}-reset", [cut_from])
        without = steps(kept_tail[0], cut_from, cut_from + rate // 5)
        with_reset = steps(dropped[0], cut_from, cut_from + rate // 5)
        print(f"     control ({name}): Chimera outside OBS on the same input, 0.2 s after the cut: "
              f"{without:.1f} steps without a reset, {with_reset:.2f} with one")
        check(without > TAIL_STEPS and with_reset <= 1.01,
              f"control ({name}): a kept tail would show ({without:.1f} steps), a dropped one does not ({with_reset:.2f})")

    # The notes after the interruptions: as obs_vst3_match_test.py, against Chimera outside OBS,
    # with its tails dropped where the filter dropped them: when it was re-enabled (to within
    # an OBS tick, in the silence after the note) and at the first sample after the pause.
    reference, _ = match.render(args, centre, ch[4], None, work, "reference", [on_at, resume_at])
    start_at = after - rate // 5
    # Where OBS dropped, repeated or shifted samples in the recording (as obs_vst3_match_test.py
    # finds them), the recorded input is not what the filter received: left out, as is the pause.
    skip = [(pause_at - int(match.PERIOD * rate), next_note + int(match.NOTE * rate))]
    source = match.channels6(media)
    for label, c in (("centre", 2), ("rear left", 4)):
        offsets, spans = match.input_breaks(ch[c], source[c])
        print(f"     the recorded {label} (the filter's input) per note, offset into the media file: {offsets}")
        skip += spans
    if len(skip) > 1:
        print(f"     left out besides the pause (OBS broke the recorded input there): "
              f"{[(round(a / rate, 2), round(b / rate, 2)) for a, b in skip[1:]]} s")
    compared = [n for n in notes if n >= after and not any(a <= n < b for a, b in skip)]
    check(after in compared, f"the first note after the re-enable is compared (notes compared at "
          f"{[round(n / rate, 2) for n in compared]} s)")
    shift = match.best_shift(front, reference[0], after)
    r = match.compare(front, reference[0], shift, start_at, [n for n in notes if n >= after], skip)
    print(f"     notes after the re-enable (not around the pause): OBS after the reference by {shift:+d}, largest "
          f"difference {r['max_steps']:.2f} steps at {r['worst_at']:.2f} s, {r['ahead_steps']:.2f} in the tick before a note; "
          f"largest per second from {start_at / rate:.1f} s: {r['per_second']}")
    check(shift == 0 and r["max_steps"] <= MATCH_STEPS and r["ahead_steps"] <= 2,
          f"after the interruption the filter's output matches Chimera outside OBS ({r['max_steps']:.2f} steps, "
          f"allowed {MATCH_STEPS:g})")

    log = log_text()
    for line in log.splitlines():
        if "session total" in line:
            print(f"     log: {line.strip()}")
    restarts = re.findall(rf"\[chimera-obs\] '{re.escape(app.FILTER)}': session total: .*resumed after silence (\d+)", log)
    check(restarts == ["1"], f"the filter counted one restart after the pause (session total: {restarts})")
    totals = re.findall(rf"\[chimera-obs\] '{re.escape(app.FILTER)}': session total: .*unprocessed blocks (\d+)", log)
    check(totals == ["0"], f"the filter processed every block (unprocessed: {totals})")
    check(log.count("[chimera-obs] unloaded Chimera filter instance") == 1, "the filter instance is unloaded")
    print(f"PASS real OBS Studio {version['obsVersion']}: Chimera's tails are dropped when the filter is re-enabled "
          f"(largest {kept:.2f} steps after) and when the input resumes after a {PAUSE_SECONDS:g} s pause "
          f"(largest {left:.2f} steps after, one restart counted); the following notes match Chimera outside OBS")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--module", required=True, help="staged chimera-obs plugin directory (macOS: chimera-obs.plugin)")
    parser.add_argument("--vst3", required=True, help="SpectralForge Chimera.vst3 bundle")
    parser.add_argument("--render-tool", required=True, help="chimera-obs-render-tool executable")
    parser.add_argument("--work", required=True, help="scratch directory (deleted first)")
    parser.add_argument("--obs", default="obs", help="OBS Studio executable (as for obs_app_test.py)")
    parser.add_argument("--ffmpeg", default="ffmpeg", help="ffmpeg executable for the media file and the analysis")
    parser.add_argument("--warmup", type=float, default=6.0, help="seconds recorded before the first interruption")
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
