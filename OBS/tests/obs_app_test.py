#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
r"""Runs the real OBS Studio app with the installed Chimera filter (plan stages B, C, F).

OBS never touches the user's configuration:
- Linux: OBS runs under the current X display (CI: xvfb-run) with its own HOME.
  The module goes to ~/.config/obs-studio/plugins and Chimera to ~/.vst3.
- Windows: OBS runs in portable mode (configuration next to its bin folder).
  The module goes to %ProgramData%\obs-studio\plugins, where Chimera Setup
  installs it, and finds Chimera through default-vst3-path.txt, which Setup
  writes. An existing installation there is never replaced.

The test drives OBS through its bundled obs-websocket:

1. Adds a media source playing a generated DI loop and the Chimera filter to it.
   The filter must find Chimera and load it.
2. Records with the filter disabled, then enabled. Both recordings must carry
   sound, and the filtered one must differ from the dry one.
3. Pushes a Chimera state saved by another host (lower output gain, written
   by chimera-obs-state-tool) through obs-websocket, as a preset script would.
   The next recording must be clearly quieter.
4. Quits OBS normally (SIGINT on Linux, closing the main window on Windows). The scene file must hold exactly
   that state. OBS starts again: the filter must come back with it and sound
   like the preset recording, not like the default.

The OBS log must show the module, the filter load and the session statistics,
and each OBS run must exit cleanly.
"""
import argparse
import base64
import cmath
import configparser
import json
import math
import os
import shutil
import signal
import socket
import struct
import subprocess
import sys
import time
import wave
from pathlib import Path

FILTER_KIND = "spectralforge_chimera_filter"
SCENE = "Chimera OBS test"
SOURCE = "DI loop"
FILTER = "SpectralForge Chimera"
RATE = 48000
WINDOWS = sys.platform == "win32"
PRESET_PARAMETER = "Output gain"  # default 0.625: the preset is clearly quieter
PRESET_VALUE = 0.3


class WebSocket:
    """Just enough of RFC 6455 for obs-websocket's JSON text messages."""

    def __init__(self, port: int, timeout: float):
        self.sock = socket.create_connection(("127.0.0.1", port), timeout=timeout)
        key = base64.b64encode(os.urandom(16)).decode()
        self.sock.sendall(
            f"GET / HTTP/1.1\r\nHost: 127.0.0.1:{port}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n"
            "Sec-WebSocket-Protocol: obswebsocket.json\r\n\r\n".encode())
        response = b""
        while b"\r\n\r\n" not in response:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise RuntimeError("obs-websocket closed the handshake")
            response += chunk
        head, self.buffer = response.split(b"\r\n\r\n", 1)
        if b" 101 " not in head.split(b"\r\n", 1)[0]:
            raise RuntimeError(f"obs-websocket refused the handshake: {head[:200]!r}")

    def _read(self, n: int) -> bytes:
        while len(self.buffer) < n:
            chunk = self.sock.recv(65536)
            if not chunk:
                raise RuntimeError("obs-websocket closed the connection")
            self.buffer += chunk
        data, self.buffer = self.buffer[:n], self.buffer[n:]
        return data

    def send(self, message: dict) -> None:
        payload = json.dumps(message).encode()
        header = bytearray([0x81])
        if len(payload) < 126:
            header.append(0x80 | len(payload))
        elif len(payload) < 65536:
            header += bytes([0x80 | 126]) + struct.pack("!H", len(payload))
        else:
            header += bytes([0x80 | 127]) + struct.pack("!Q", len(payload))
        mask = os.urandom(4)
        # XOR as one big integer: no per-byte Python loop for ~700 KB Chimera states.
        key = (mask * (len(payload) // 4 + 1))[:len(payload)]
        masked = (int.from_bytes(payload, "big") ^ int.from_bytes(key, "big")).to_bytes(len(payload), "big")
        self.sock.sendall(bytes(header) + mask + masked)

    def receive(self) -> dict:
        message = b""
        while True:
            first, second = self._read(2)
            length = second & 0x7F
            if length == 126:
                length = struct.unpack("!H", self._read(2))[0]
            elif length == 127:
                length = struct.unpack("!Q", self._read(8))[0]
            mask = self._read(4) if second & 0x80 else None
            data = self._read(length)
            if mask:
                data = bytes(b ^ mask[i % 4] for i, b in enumerate(data))
            opcode = first & 0x0F
            if opcode == 0x8:
                raise RuntimeError("obs-websocket closed the connection")
            if opcode == 0x9:  # ping
                continue
            message += data
            if first & 0x80 and opcode in (0x0, 0x1):
                return json.loads(message)

    def close(self) -> None:
        self.sock.close()


class Layout:
    """Where this OBS reads its configuration and the Chimera module."""

    def __init__(self, obs: str, work: Path):
        self.obs = obs
        self.installed = False
        if WINDOWS:
            exe = Path(obs).resolve()
            self.cwd = exe.parent  # OBS on Windows expects to start in bin\64bit
            self.config = exe.parents[2] / "config" / "obs-studio"  # --portable
            self.plugin = Path(os.environ["ProgramData"]) / "obs-studio" / "plugins" / "chimera-obs"
            self.env = dict(os.environ)
            self.options = ["--portable"]
        else:
            home = work / "home"
            self.cwd = home
            self.config = home / ".config" / "obs-studio"
            self.plugin = self.config / "plugins" / "chimera-obs"
            self.env = dict(os.environ, HOME=str(home), XDG_CONFIG_HOME=str(home / ".config"))
            self.options = []

    def install(self, module: Path, vst3: Path) -> None:
        if self.plugin.exists():
            raise RuntimeError(f"{self.plugin} already exists; this test does not replace an installed filter")
        if self.config.exists():
            shutil.rmtree(self.config)
        self.cwd.mkdir(parents=True, exist_ok=True)
        shutil.copytree(module, self.plugin, symlinks=True)
        self.installed = True
        if WINDOWS:
            # What Chimera Setup writes when it installs the filter.
            (self.plugin / "data" / "default-vst3-path.txt").write_text(str(vst3), encoding="utf-8")
        else:
            (self.cwd / ".vst3").mkdir()
            (self.cwd / ".vst3" / vst3.name).symlink_to(vst3)

    def uninstall(self) -> None:
        if WINDOWS and self.installed:  # only what install() put there
            shutil.rmtree(self.plugin)


def close_windows(pid: int) -> int:
    """Posts WM_CLOSE to the process's visible top-level windows, as the close button does."""
    import ctypes
    from ctypes import wintypes
    user32 = ctypes.windll.user32
    found = []
    callback = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)

    def visit(hwnd, _):
        owner = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid and user32.IsWindowVisible(hwnd):
            found.append(hwnd)
        return True

    user32.EnumWindows(callback(visit), 0)
    for hwnd in found:
        user32.PostMessageW(hwnd, 0x0010, 0, 0)  # WM_CLOSE
    return len(found)


class Obs:
    """One OBS Studio process with its own configuration, driven over obs-websocket."""

    def __init__(self, layout: Layout, port: int, log: Path):
        self.port = port
        self.output = log.open("w")
        self.process = subprocess.Popen(
            [layout.obs, *layout.options, "--multi", "--disable-shutdown-check", "--disable-missing-files-check"],
            env=layout.env, cwd=layout.cwd, stdout=self.output, stderr=subprocess.STDOUT)
        self.ws = None
        self.next_id = 0
        try:
            self._connect(port, log)
        except Exception:
            # The caller never gets this object, so nothing else would stop OBS.
            if self.ws is not None:
                self.ws.close()
            self.process.kill()
            self.process.wait()
            self.output.close()
            raise

    def _connect(self, port: int, log: Path) -> None:
        deadline = time.monotonic() + 60
        while self.ws is None:
            if self.process.poll() is not None:
                raise RuntimeError(f"OBS exited during startup (code {self.process.returncode}); see {log}")
            try:
                self.ws = WebSocket(port, timeout=30)
            except OSError:
                if time.monotonic() > deadline:
                    raise RuntimeError("obs-websocket did not start within 60 s")
                time.sleep(0.5)
        hello = self.ws.receive()
        check(hello.get("op") == 0, "obs-websocket Hello")
        check("authentication" not in hello["d"], "obs-websocket without authentication")
        self.ws.send({"op": 1, "d": {"rpcVersion": 1, "eventSubscriptions": 0}})
        check(self.ws.receive().get("op") == 2, "obs-websocket Identified")
        # obs-websocket accepts connections before OBS has finished loading
        # (requests then fail with code 207, "not ready").
        wait_until(lambda: self._ready(), "OBS ready for requests", 60)

    def _ready(self) -> bool:
        self.next_id += 1
        request_id = str(self.next_id)
        self.ws.send({"op": 6, "d": {"requestType": "GetVersion", "requestId": request_id, "requestData": {}}})
        while True:
            message = self.ws.receive()
            if message.get("op") == 7 and message["d"].get("requestId") == request_id:
                return bool(message["d"]["requestStatus"].get("result"))

    def request(self, kind: str, data: dict | None = None, ok: bool = True) -> dict:
        self.next_id += 1
        request_id = str(self.next_id)
        self.ws.send({"op": 6, "d": {"requestType": kind, "requestId": request_id, "requestData": data or {}}})
        while True:
            message = self.ws.receive()
            if message.get("op") == 7 and message["d"].get("requestId") == request_id:
                status = message["d"]["requestStatus"]
                if ok and not status.get("result"):
                    raise RuntimeError(f"{kind} failed: {status}")
                return message["d"].get("responseData") or {}

    def record(self, name: str, seconds: float) -> Path:
        self.request("StartRecord")
        wait_until(lambda: self.request("GetRecordStatus")["outputActive"], f"{name} recording started", 15)
        time.sleep(seconds)
        path = Path(self.request("StopRecord")["outputPath"])
        wait_until(lambda: not self.request("GetRecordStatus")["outputActive"], f"{name} recording stopped", 15)
        check(path.is_file() and path.stat().st_size > 0, f"{name} recording written ({path.name})")
        return path

    def filter_settings(self) -> dict:
        return self.request("GetSourceFilter", {"sourceName": SOURCE, "filterName": FILTER})["filterSettings"]

    def quit(self) -> None:
        if self.ws is not None:
            self.ws.close()
        # OBS's normal shutdown: saves the scene, unloads modules.
        if WINDOWS:
            check(close_windows(self.process.pid) > 0, "OBS main window found to close")
        else:
            self.process.send_signal(signal.SIGINT)
        try:
            code = self.process.wait(timeout=60)
        except subprocess.TimeoutExpired:
            self.process.kill()
            raise RuntimeError("OBS did not shut down within 60 s of the close request")
        finally:
            self.output.close()
        check(code == 0, f"OBS exited cleanly (code {code})")


def check(ok: bool, what: str) -> None:
    if not ok:
        raise RuntimeError(what)
    print(f"ok   {what}", flush=True)


def wait_until(condition, what: str, timeout: float = 60) -> None:
    deadline = time.monotonic() + timeout
    while not condition():
        if time.monotonic() > deadline:
            raise RuntimeError(f"{what} (waited {timeout:.0f} s)")
        time.sleep(0.25)
    check(True, what)


def write_di(path: Path, seconds: float = 2.0) -> None:
    """A sustained 110 Hz string-like tone (8 harmonics). Steady, so a recording's
    level and spectrum do not depend on where in the loop it starts."""
    frames = bytearray()
    for n in range(int(RATE * seconds)):  # whole periods of 110 Hz: the loop is seamless
        t = n / RATE
        s = sum(math.sin(2 * math.pi * 110.0 * k * t) / k for k in range(1, 9)) * 0.2
        v = int(max(-1.0, min(1.0, s)) * 32767)
        frames += struct.pack("<hh", v, v)
    with wave.open(str(path), "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(bytes(frames))


FFMPEG = "ffmpeg"


def decode(path: Path, skip: float = 1.0, seconds: float = 3.0) -> list[float]:
    raw = subprocess.run(
        [FFMPEG, "-v", "error", "-ss", str(skip), "-t", str(seconds), "-i", str(path), "-vn", "-ac", "1",
         "-ar", str(RATE), "-f", "f32le", "-"], check=True, capture_output=True).stdout
    return list(struct.unpack(f"<{len(raw) // 4}f", raw))


def rms(x: list[float]) -> float:
    return math.sqrt(sum(v * v for v in x) / max(1, len(x)))


def band_profile(x: list[float]) -> list[float]:
    """Average energy share per octave band (63 Hz – 16 kHz) over 4096-sample frames."""
    size = 4096
    edges = [63 * 2 ** i for i in range(9)]
    totals = [0.0] * (len(edges) - 1)
    window = [0.5 - 0.5 * math.cos(2 * math.pi * n / size) for n in range(size)]
    for start in range(0, len(x) - size + 1, size):
        frame = [x[start + n] * window[n] for n in range(size)]
        spectrum = fft(frame)
        for i in range(len(totals)):
            lo, hi = int(edges[i] * size / RATE), int(edges[i + 1] * size / RATE)
            totals[i] += sum(abs(c) ** 2 for c in spectrum[lo:hi])
    s = sum(totals) or 1.0
    return [t / s for t in totals]


def fft(x: list[float]) -> list[complex]:
    n = len(x)
    if n == 1:
        return [complex(x[0])]
    even, odd = fft(x[0::2]), fft(x[1::2])
    twiddle = [cmath.exp(-2j * math.pi * k / n) * odd[k] for k in range(n // 2)]
    return [even[k] + twiddle[k] for k in range(n // 2)] + [even[k] - twiddle[k] for k in range(n // 2)]


def distance(a: list[float], b: list[float]) -> float:
    return sum(abs(p - q) for p, q in zip(a, b))


def configure(config: Path, port: int, recordings: Path) -> None:
    (config / "basic" / "profiles" / "Chimera").mkdir(parents=True, exist_ok=True)
    (config / "basic" / "scenes").mkdir(parents=True, exist_ok=True)
    glob = configparser.ConfigParser()
    glob.optionxform = str
    glob["General"] = {"FirstRun": "false", "LastVersion": "503316482", "EnableAutoUpdates": "false"}
    glob["Basic"] = {"Profile": "Chimera", "ProfileDir": "Chimera", "SceneCollection": "Chimera",
                     "SceneCollectionFile": "Chimera", "ConfigOnNewProfile": "false"}
    glob["BasicWindow"] = {"ConfirmOnExit": "false", "WarnBeforeStoppingRecord": "false",
                           "WarnBeforeStartingStream": "false", "WarnBeforeStoppingStream": "false"}
    glob["OBSWebSocket"] = {"FirstLoad": "false", "ServerEnabled": "true", "ServerPort": str(port),
                            "AlertsEnabled": "false", "AuthRequired": "false", "ServerPassword": ""}
    # OBS 31 keeps these in user.ini; OBS 30 in global.ini.
    for name in ("global.ini", "user.ini"):
        with (config / name).open("w") as f:
            glob.write(f, space_around_delimiters=False)
    profile = configparser.ConfigParser()
    profile.optionxform = str
    profile["General"] = {"Name": "Chimera"}
    profile["Video"] = {"BaseCX": "320", "BaseCY": "180", "OutputCX": "320", "OutputCY": "180",
                        "FPSType": "0", "FPSCommon": "30"}
    profile["Audio"] = {"SampleRate": str(RATE), "ChannelSetup": "Stereo"}
    profile["Output"] = {"Mode": "Simple"}
    profile["SimpleOutput"] = {"FilePath": str(recordings), "RecFormat2": "mkv", "RecQuality": "Stream",
                               "StreamEncoder": "x264", "VBitrate": "300", "ABitrate": "160"}
    with (config / "basic" / "profiles" / "Chimera" / "basic.ini").open("w") as f:
        profile.write(f, space_around_delimiters=False)
    # obs-websocket 5.5 (OBS 31) keeps its settings here instead of the ini files.
    websocket = config / "plugin_config" / "obs-websocket"
    websocket.mkdir(parents=True, exist_ok=True)
    (websocket / "config.json").write_text(json.dumps({
        "alerts_enabled": False, "auth_required": False, "first_load": False, "server_enabled": True,
        "server_password": "", "server_port": port}), encoding="utf-8")


def free_port() -> int:
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def run(args: argparse.Namespace) -> None:
    work = Path(args.work).resolve()
    if work.exists():
        shutil.rmtree(work)
    recordings = work / "recordings"
    recordings.mkdir(parents=True)
    layout = args.layout
    layout.install(Path(args.module).resolve(), Path(args.vst3).resolve())
    port = free_port()
    configure(layout.config, port, recordings)
    di = work / "di-loop.wav"
    write_di(di)
    logs = layout.config / "logs"

    preset = json.loads(subprocess.run([args.state_tool, str(Path(args.vst3).resolve()), PRESET_PARAMETER,
                                        str(PRESET_VALUE)], check=True, capture_output=True, text=True).stdout)

    def log_text() -> str:
        return "".join(p.read_text(errors="replace") for p in sorted(logs.glob("*.txt")))

    print("--- first OBS run: add the filter, record dry, default and preset", flush=True)
    obs = Obs(layout, port, work / "obs-run-1.txt")
    try:
        version = obs.request("GetVersion")
        print(f"     OBS {version['obsVersion']}, obs-websocket {version['obsWebSocketVersion']}")
        obs.request("CreateScene", {"sceneName": SCENE})
        obs.request("SetCurrentProgramScene", {"sceneName": SCENE})
        obs.request("CreateInput", {"sceneName": SCENE, "inputName": SOURCE, "inputKind": "ffmpeg_source",
                                    "inputSettings": {"is_local_file": True, "local_file": str(di), "looping": True,
                                                      "restart_on_activate": False}})
        obs.request("CreateSourceFilter", {"sourceName": SOURCE, "filterName": FILTER, "filterKind": FILTER_KIND,
                                           "filterSettings": {}})
        wait_until(lambda: f"[chimera-obs] '{FILTER}': loaded " in log_text(),
                   "the filter found Chimera and loaded it")

        obs.request("SetSourceFilterEnabled", {"sourceName": SOURCE, "filterName": FILTER, "filterEnabled": False})
        time.sleep(1)
        dry = decode(obs.record("dry", args.seconds))
        obs.request("SetSourceFilterEnabled", {"sourceName": SOURCE, "filterName": FILTER, "filterEnabled": True})
        time.sleep(1)
        default = decode(obs.record("default", args.seconds))
        obs.request("SetSourceFilterSettings", {
            "sourceName": SOURCE, "filterName": FILTER, "overlay": True,
            "filterSettings": {"vst3_component_state": preset["component"],
                               "vst3_controller_state": preset["controller"]}})
        time.sleep(1)
        pushed = decode(obs.record("preset", args.seconds))
    finally:
        obs.quit()

    levels = {name: rms(x) for name, x in (("dry", dry), ("default", default), ("preset", pushed))}
    profiles = {name: band_profile(x) for name, x in (("dry", dry), ("default", default), ("preset", pushed))}
    for name in levels:
        print(f"     {name:8} RMS {levels[name]:.4f}  bands {[round(v, 3) for v in profiles[name]]}")
    for name, level in levels.items():
        check(level > 0.001, f"{name} recording has sound")

    def db(a: str, b: str) -> float:
        return 20 * math.log10(levels[a] / levels[b])

    shape = distance(profiles["default"], profiles["dry"])
    check(shape > 0.1 or abs(db("default", "dry")) > 1.0,
          f"Chimera changes the sound (spectral distance from dry {shape:.3f}, level {db('default', 'dry'):+.1f} dB)")
    preset_db = db("preset", "default")
    check(preset_db < -3.0, f"the pushed state applies: {preset_db:+.1f} dB against the default state")

    scene = json.loads((layout.config / "basic" / "scenes" / "Chimera.json").read_text(encoding="utf-8"))
    saved = [f for s in scene["sources"] if s["name"] == SOURCE for f in s.get("filters", []) if f["id"] == FILTER_KIND]
    check(len(saved) == 1, "the scene file stores the Chimera filter")
    check(saved[0]["settings"].get("vst3_component_state") == preset["component"],
          f"the scene file stores exactly the pushed Chimera state ('{preset['title']}')")

    print("--- second OBS run: the scene restores the filter", flush=True)
    obs = Obs(layout, port, work / "obs-run-2.txt")
    try:
        wait_until(lambda: any(f["filterKind"] == FILTER_KIND for f in obs.request(
            "GetSourceFilterList", {"sourceName": SOURCE}, ok=False).get("filters", [])),
            "the restored scene has the Chimera filter")
        check(obs.filter_settings().get("vst3_component_state") == preset["component"],
              "the restored filter carries the saved Chimera state")
        wait_until(lambda: log_text().count(f"[chimera-obs] '{FILTER}': loaded ") >= 2,
                   "the restored filter loaded Chimera")
        time.sleep(1)
        restored = decode(obs.record("restored", args.seconds))
    finally:
        obs.quit()
    restored_db = 20 * math.log10(rms(restored) / levels["default"])
    print(f"     restored RMS {rms(restored):.4f}  bands {[round(v, 3) for v in band_profile(restored)]}")
    check(abs(restored_db - preset_db) < 1.0,
          f"after the restart the filter sounds like the preset ({restored_db:+.1f} dB against the default, "
          f"preset {preset_db:+.1f} dB)")

    log = log_text()
    check(log.count("[chimera-obs] module ") == 2, "both runs log the Chimera module")
    check(log.count(f"[chimera-obs] '{FILTER}': session total") == 2, "both runs log the filter's session statistics")
    check(log.count("[chimera-obs] unloaded Chimera filter instance") == 2, "both runs unload the filter")
    print(f"PASS real OBS Studio {version['obsVersion']}: Chimera found and loaded by the filter, filtered and preset recordings differ, "
          "the pushed state is saved in the scene and restored after a restart, clean shutdowns")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--module", required=True, help="staged chimera-obs plugin directory (bin/, data/)")
    parser.add_argument("--vst3", required=True, help="SpectralForge Chimera.vst3 bundle")
    parser.add_argument("--state-tool", required=True, help="chimera-obs-state-tool executable")
    parser.add_argument("--work", required=True, help="scratch directory (deleted first)")
    parser.add_argument("--obs", default="obs", help="OBS Studio executable (Windows: bin\\64bit\\obs64.exe "
                        "of an unpacked release)")
    parser.add_argument("--ffmpeg", default="ffmpeg", help="ffmpeg executable for decoding the recordings")
    parser.add_argument("--seconds", type=float, default=6.0, help="length of each recording")
    args = parser.parse_args()
    if not WINDOWS and not os.environ.get("DISPLAY"):
        print("FAIL no X display (run under xvfb-run)", file=sys.stderr)
        return 2
    global FFMPEG
    FFMPEG = args.ffmpeg
    args.layout = Layout(args.obs, Path(args.work).resolve())
    try:
        run(args)
    except Exception as e:  # noqa: BLE001 - report every failure the same way
        print(f"FAIL {e}", file=sys.stderr)
        work = Path(args.work).resolve()
        logs = sorted((args.layout.config / "logs").glob("*.txt"))[-2:]
        if not logs:
            print(f"--- no OBS log in {args.layout.config / 'logs'}; config holds: "
                  f"{sorted(str(p.relative_to(args.layout.config)) for p in args.layout.config.rglob('*'))[:40]}",
                  file=sys.stderr)
        for log in [*sorted(work.glob("obs-run-*.txt")), *logs]:
            print(f"--- {log.name} (last 60 lines)", file=sys.stderr)
            print("\n".join(log.read_text(errors="replace").splitlines()[-60:]), file=sys.stderr)
        return 1
    finally:
        args.layout.uninstall()
    return 0


if __name__ == "__main__":
    sys.exit(main())
