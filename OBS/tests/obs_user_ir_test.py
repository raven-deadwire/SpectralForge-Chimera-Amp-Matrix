#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""A user IR in the Chimera filter's state, and a damaged one, in the real OBS Studio app (plan stage C).

Chimera keeps a loaded user IR inside its own state (USER_IRS, the WAV bytes in
JUCE's base64 form), not as a path to the file, so an OBS scene carries the IR
with it. Installs the filter as obs_app_test.py does (same layouts, same
guards), then:

1. Builds five Chimera states from one saved by another host
   (chimera-obs-state-tool): "Filters" uses the cabinet filters on all three
   cabinet lanes; "User IR" selects the user IR on all lanes and carries a
   400 Hz low-pass IR; "Damaged IR" selects the user IR but its IR data is
   unreadable. Two more damaged cases carry a negative size prefix and a valid
   JUCE encoding of invalid WAV bytes. Five media sources playing the same DI loop each get a Chimera
   filter with one of them, and each is recorded on its own (the others
   muted). User IR must remove nearly all energy above 500 Hz that Filters
   keeps. Damaged IR must sound like Filters: Chimera's documented fallback
   ("Saved IR is damaged. Filters only.").
2. The scene file must hold the IR itself in User IR's filter state. OBS
   starts again, with no IR file anywhere on disk: User IR must still sound
   low-passed, and Damaged IR must still sound like Filters.

Each OBS run must exit cleanly.
"""
import argparse
import base64
import io
import json
import math
import os
import re
import shutil
import struct
import subprocess
import sys
import time
import wave
import xml.etree.ElementTree as ET
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import obs_app_test as app  # noqa: E402

FILTERS, USER_IR, DAMAGED = "Filters", "User IR", "Damaged IR"
DAMAGED_SIZE, DAMAGED_WAV = "Damaged size", "Damaged WAV"
DAMAGED_SOURCES = [DAMAGED, DAMAGED_SIZE, DAMAGED_WAV]
SOURCES = [FILTERS, USER_IR, *DAMAGED_SOURCES]
IR_NAME = "Chimera OBS test low-pass"
CUTOFF = 400.0  # Hz
DEFAULT_OUTPUT_GAIN = 0.625  # Chimera's default (see obs_app_test.PRESET_PARAMETER)
USER_IR_CHOICE, FILTERS_CHOICE = 3, 0  # cabtype choices: cabinet filters, ..., user IR


def low_pass_ir(rate: int = app.RATE) -> bytes:
    """A 255-tap windowed-sinc low-pass as a 16-bit mono WAV file."""
    taps, fc = 255, CUTOFF / rate
    centre = taps // 2
    h = [(2 * fc if n == centre else math.sin(2 * math.pi * fc * (n - centre)) / (math.pi * (n - centre)))
         * (0.54 - 0.46 * math.cos(2 * math.pi * n / (taps - 1))) for n in range(taps)]
    out = io.BytesIO()
    with wave.open(out, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, 4 * v)) * 32767)) for v in h))
    return out.getvalue()


def juce_base64(data: bytes) -> str:
    """juce::MemoryBlock::toBase64Encoding: "<size>.<6-bit groups, least significant bits first>"."""
    table = ".ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+"
    value = int.from_bytes(data, "little")
    return f"{len(data)}." + "".join(table[(value >> (6 * i)) & 63] for i in range((len(data) * 8 + 5) // 6))


def state_xml(component: str) -> tuple[bytes, str, bytes]:
    """Splits a Chimera component state (JUCE's "VC2!" XML block plus the VST3 wrapper's tail)."""
    raw = base64.b64decode(component)
    if raw[:4] != b"VC2!":
        raise RuntimeError("the Chimera state is not a JUCE XML block")
    size = struct.unpack("<I", raw[4:8])[0]
    return raw[:4], raw[8:8 + size].rstrip(b"\x00").decode("utf-8"), raw[8 + size:]


def with_cabinet(component: str, choice: int, ir_data: str | None) -> str:
    """The same state with every cabinet lane on `choice` and, if given, that IR data on each lane."""
    magic, xml, tail = state_xml(component)
    for lane in (1, 2, 3):  # the first PARAMS block is the live sound; COMPARISONS follows it
        xml, count = re.subn(rf'(<PARAM id="cabtype{lane}" value=")[^"]*(")', rf"\g<1>{choice}.0\2", xml, count=1)
        if count != 1:
            raise RuntimeError(f"the Chimera state has no cabtype{lane} parameter")
    if ir_data is not None:
        irs = "".join(f'<IR lane="{lane}" slot="0" name="{IR_NAME}" data="{ir_data}"/>' for lane in range(3))
        if "<USER_IRS/>" not in xml:
            raise RuntimeError("the Chimera state has no empty USER_IRS block")
        xml = xml.replace("<USER_IRS/>", f"<USER_IRS>{irs}</USER_IRS>", 1)
    body = xml.encode("utf-8") + b"\x00"
    return base64.b64encode(magic + struct.pack("<I", len(body)) + body + tail).decode()


def high_share(x: list[float]) -> float:
    """Share of the energy above 500 Hz (octave bands from 504 Hz up)."""
    return sum(app.band_profile(x)[3:])


def check_embedded_ir(component: str, expected: str) -> None:
    """Check original WAV bytes and live lane bindings in inline and pooled states."""
    root = ET.fromstring(state_xml(component)[1])
    assets = {a.get("sha256"): a.get("data") for a in root.findall("./IR_ASSETS/ASSET")}
    irs = root.findall("./USER_IRS/IR")
    app.check(len(irs) == 3 and {(i.get("lane"), i.get("slot", "0")) for i in irs}
              == {("0", "0"), ("1", "0"), ("2", "0")}, "the saved IR keeps all three live lane bindings")
    app.check(all(i.get("name") == IR_NAME and i.get("data", assets.get(i.get("asset"))) == expected for i in irs),
              "the saved state holds the exact original WAV bytes on every live lane")


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
    logs = layout.config / "logs"
    scene_file = layout.config / "basic" / "scenes" / "Chimera.json"
    check = app.check

    base = json.loads(subprocess.run([args.state_tool, str(Path(args.vst3).resolve()), app.PRESET_PARAMETER,
                                      str(DEFAULT_OUTPUT_GAIN)], check=True, capture_output=True, text=True).stdout)
    encoded_ir = juce_base64(low_pass_ir())
    states = {FILTERS: with_cabinet(base["component"], FILTERS_CHOICE, None),
              USER_IR: with_cabinet(base["component"], USER_IR_CHOICE, encoded_ir),
              DAMAGED: with_cabinet(base["component"], USER_IR_CHOICE, "@@ not JUCE base64 @@"),
              DAMAGED_SIZE: with_cabinet(base["component"], USER_IR_CHOICE, "-1.A"),
              DAMAGED_WAV: with_cabinet(base["component"], USER_IR_CHOICE, juce_base64(b"A" * 64))}

    def log_text() -> str:
        return "".join(p.read_text(errors="replace") for p in sorted(logs.glob("*.txt")))

    def solo(obs: app.Obs, name: str) -> list[float]:
        for source in SOURCES:
            obs.request("SetInputMute", {"inputName": source, "inputMuted": source != name})
        time.sleep(1)
        x = app.decode(obs.record(name, args.seconds))
        print(f"     {name:11} RMS {app.rms(x):.4f}  above 500 Hz {high_share(x):.4f}  "
              f"bands {[round(v, 3) for v in app.band_profile(x)]}")
        check(app.rms(x) > 0.001, f"{name} recording has sound")
        return x

    def judge(take: dict[str, list[float]], when: str) -> None:
        filters, user = take[FILTERS], take[USER_IR]
        check(high_share(user) < high_share(filters) / 5,
              f"{when}: the user IR low-passes the sound (above 500 Hz {high_share(user):.4f}, "
              f"cabinet filters {high_share(filters):.4f})")
        for source in DAMAGED_SOURCES:
            damaged = take[source]
            shape = app.distance(app.band_profile(damaged), app.band_profile(filters))
            level = 20 * math.log10(app.rms(damaged) / app.rms(filters))
            check(shape < 0.02 and abs(level) < 1.0,
                  f"{when}: {source} falls back to the cabinet filters (distance {shape:.3f}, {level:+.1f} dB)")

    print("--- first OBS run: cabinet filters, a user IR in the state, a damaged user IR", flush=True)
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
            obs.request("CreateSourceFilter", {"sourceName": source, "filterName": f"Chimera {source}",
                                               "filterKind": app.FILTER_KIND,
                                               "filterSettings": {"vst3_component_state": states[source],
                                                                  "vst3_controller_state": base["controller"]}})
        app.wait_until(lambda: all(f"[chimera-obs] 'Chimera {s}': loaded " in log_text() for s in SOURCES),
                       "all filters found Chimera and loaded it")
        first = {source: solo(obs, source) for source in SOURCES}
    finally:
        obs.quit()
    judge(first, "first run")

    scene = json.loads(scene_file.read_text(encoding="utf-8"))
    saved = {s["name"]: [f for f in s.get("filters", []) if f["id"] == app.FILTER_KIND] for s in scene["sources"]}
    check(all(len(saved.get(s, [])) == 1 for s in SOURCES), "the scene file stores one Chimera filter per source")
    check_embedded_ir(saved[USER_IR][0]["settings"]["vst3_component_state"], encoded_ir)
    check([p for p in work.rglob("*.wav") if p != di] == [], "no IR file exists on disk (the IR was never a file)")

    print("--- second OBS run: the scene restores the user IR from its own state", flush=True)
    obs = app.Obs(layout, port, work / "obs-run-2.txt")
    try:
        app.wait_until(lambda: all(log_text().count(f"[chimera-obs] 'Chimera {s}': loaded ") >= 2 for s in SOURCES),
                       "all restored filters loaded Chimera")
        restored = obs.request("GetSourceFilter", {"sourceName": USER_IR, "filterName": f"Chimera {USER_IR}"})
        check_embedded_ir(restored["filterSettings"]["vst3_component_state"], encoded_ir)
        second = {source: solo(obs, source) for source in SOURCES}
    finally:
        obs.quit()
    judge(second, "after the restart")
    user_level = 20 * math.log10(app.rms(second[USER_IR]) / app.rms(first[USER_IR]))
    user_shape = app.distance(app.band_profile(second[USER_IR]), app.band_profile(first[USER_IR]))
    check(abs(user_level) < 1.0 and user_shape < 0.02,
          f"after the restart the user IR sounds as before ({user_level:+.1f} dB, distance {user_shape:.3f})")

    log = log_text()
    check(log.count("[chimera-obs] module ") == 2, "both runs log the Chimera module")
    unloads = log.count("[chimera-obs] unloaded Chimera filter instance")
    expected_unloads = 2 * len(SOURCES)
    check(unloads == expected_unloads, f"every filter instance is unloaded ({unloads} of {expected_unloads})")
    print(f"PASS real OBS Studio {version['obsVersion']}: a user IR travels in the scene and is restored without its file, "
          "a damaged user IR falls back to the cabinet filters, clean shutdowns")


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
        print(f"FAIL {app.failure_text(e)}", file=sys.stderr)
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
