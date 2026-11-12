#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""An edit made in Chimera's editor window, opened from the filter in the real OBS Studio app (plan stage C).

Linux and Windows: the edit is a mouse drag, sent with xdotool to the editor's X11 window
under Xvfb on Linux and with SendInput to the editor's top-level window on Windows (the
test moves the window so the knob is on the CI runner's small screen and raises it).
Installs the filter as obs_app_test.py does, then:

1. Adds the DI loop with a Chimera filter in its default state and records it.
2. Starts OBS again with obs_editor_test.py's Lua script in the scene collection and opens
   the editor from the filter's button on the OBS UI thread. In the editor's EASY VIEW the
   test drags the OUTPUT knob down (Chimera's final level, -6 dB by default). The filter
   must write the new state into its OBS settings (what OBS saves in the scene), and a
   recording must be clearly quieter. OBS quits with the editor still open.
3. A third OBS run restores the filter: the saved scene holds the edited state and the
   recording sounds like the edited one.

A screenshot of the editor before and after the drag is kept in the work directory.
"""
import argparse
import ctypes
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
import obs_editor_test as editor  # noqa: E402

# Chimera's EASY VIEW, the view the editor opens in: the OUTPUT knob's centre at 100 %, in window pixels.
EDITOR_SIZE = (1180, 780)
OUTPUT_KNOB = (1065, 585)
# Chimera fits its editor to the screen and scales the view to the window (Source/WindowFit.h):
# the scale is the window's width or height over EDITOR_SIZE, the smaller one, kept within these.
EDITOR_SCALES = (0.75, 1.5)
DRAG_PIXELS = 150  # downward; Chimera's knobs turn down when dragged down


def editor_window(pid: int) -> str | int | None:
    """The editor's window: an X11 window id on Linux, a top-level window handle on Windows."""
    if app.WINDOWS:
        handles = [hwnd for hwnd, title in app.top_level_windows(pid) if title == app.FILTER]
        return handles[-1] if handles else None
    ids = subprocess.run(["xdotool", "search", "--name", f"^{app.FILTER}$"], capture_output=True, text=True).stdout.split()
    return ids[-1] if ids else None


def editor_scale(size: tuple[int, int]) -> float:
    """The scale Chimera draws its editor at in a window of this content size (fittedEditorScale)."""
    return min(max(min(size[0] / EDITOR_SIZE[0], size[1] / EDITOR_SIZE[1]), EDITOR_SCALES[0]), EDITOR_SCALES[1])


def window_size(window: str | int) -> tuple[int, int]:
    """The size of the window's content, where Chimera's editor draws."""
    if app.WINDOWS:
        rect = win32.RECT()
        win32.user32.GetClientRect(window, ctypes.byref(rect))
        return rect.right - rect.left, rect.bottom - rect.top
    shape = subprocess.run(["xdotool", "getwindowgeometry", "--shell", window], check=True, capture_output=True,
                           text=True).stdout
    values = dict(line.split("=", 1) for line in shape.split())
    return int(values["WIDTH"]), int(values["HEIGHT"])


def screenshot(window: str | int, path: Path) -> None:
    if not app.WINDOWS:
        subprocess.run(["import", "-window", window, str(path)], check=False, capture_output=True)
        return
    left, top = win32.client_to_screen(window, 0, 0)
    width, height = window_size(window)
    script = ("Add-Type -AssemblyName System.Drawing;"
              f"$b=New-Object System.Drawing.Bitmap {width},{height};"
              "$g=[System.Drawing.Graphics]::FromImage($b);"
              f"$g.CopyFromScreen({left},{top},0,0,$b.Size);"
              f"$b.Save('{path}',[System.Drawing.Imaging.ImageFormat]::Png)")
    subprocess.run(["powershell", "-NoProfile", "-Command", script], check=False, capture_output=True, timeout=60)


def drag_down(window: str | int, x: int, y: int, pixels: int) -> None:
    if app.WINDOWS:
        win32.drag_down(window, x, y, pixels)
        return
    subprocess.run(["xdotool", "mousemove", "--window", window, str(x), str(y), "mousedown", "1"], check=True)
    for _ in range(pixels // 10):
        subprocess.run(["xdotool", "mousemove_relative", "--", "0", "10"], check=True)
        time.sleep(0.03)
    subprocess.run(["xdotool", "mouseup", "1"], check=True)


class Win32:
    """The few user32 calls the Windows drag needs, with typed prototypes (a window handle is 64-bit)."""

    def __init__(self) -> None:
        from ctypes import wintypes
        self.RECT = wintypes.RECT
        self.POINT = wintypes.POINT
        user32 = ctypes.WinDLL("user32", use_last_error=True)
        # Screen pixels as OBS (DPI aware) lays the editor out, not scaled ones.
        user32.SetProcessDpiAwarenessContext.argtypes = [ctypes.c_void_p]
        user32.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))  # PER_MONITOR_AWARE_V2
        HWND = wintypes.HWND
        user32.GetClientRect.argtypes = [HWND, ctypes.POINTER(wintypes.RECT)]
        user32.GetWindowRect.argtypes = [HWND, ctypes.POINTER(wintypes.RECT)]
        user32.ClientToScreen.argtypes = [HWND, ctypes.POINTER(wintypes.POINT)]
        user32.SetWindowPos.argtypes = [HWND, HWND, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, wintypes.UINT]
        user32.SetForegroundWindow.argtypes = [HWND]
        user32.WindowFromPoint.argtypes = [wintypes.POINT]
        user32.WindowFromPoint.restype = HWND
        user32.GetAncestor.argtypes = [HWND, wintypes.UINT]
        user32.GetAncestor.restype = HWND
        user32.GetSystemMetrics.argtypes = [ctypes.c_int]

        class MOUSEINPUT(ctypes.Structure):
            _fields_ = [("dx", wintypes.LONG), ("dy", wintypes.LONG), ("mouseData", wintypes.DWORD),
                        ("dwFlags", wintypes.DWORD), ("time", wintypes.DWORD), ("dwExtraInfo", ctypes.c_size_t)]

        class INPUT(ctypes.Structure):  # the union's largest member is MOUSEINPUT on 64-bit Windows
            _fields_ = [("type", wintypes.DWORD), ("mi", MOUSEINPUT)]

        self.INPUT, self.MOUSEINPUT = INPUT, MOUSEINPUT
        user32.SendInput.argtypes = [wintypes.UINT, ctypes.POINTER(INPUT), ctypes.c_int]
        self.user32 = user32

    def client_to_screen(self, hwnd: int, x: int, y: int) -> tuple[int, int]:
        point = self.POINT(x, y)
        self.user32.ClientToScreen(hwnd, ctypes.byref(point))
        return point.x, point.y

    def mouse(self, flags: int, x: int, y: int) -> None:
        # Absolute coordinates are 0..65535 across the primary screen.
        width, height = self.user32.GetSystemMetrics(0), self.user32.GetSystemMetrics(1)
        event = self.INPUT(0, self.MOUSEINPUT(round(x * 65535 / (width - 1)), round(y * 65535 / (height - 1)), 0,
                                              flags | 0x8000 | 0x0001, 0, 0))  # ABSOLUTE | MOVE
        sent = self.user32.SendInput(1, ctypes.byref(event), ctypes.sizeof(event))
        app.check(sent == 1, f"Windows accepted the mouse input (error {ctypes.get_last_error()})")

    def bring_into_view(self, hwnd: int, x: int, y: int, pixels: int) -> None:
        """Put the window's point (x, y) on screen with room for the drag below it, above OBS's own windows."""
        # The CI runner's screen can be smaller than the editor.
        width, height = self.user32.GetSystemMetrics(0), self.user32.GetSystemMetrics(1)
        window, (knob_x, knob_y) = self.RECT(), self.client_to_screen(hwnd, x, y)
        self.user32.GetWindowRect(hwnd, ctypes.byref(window))
        left = window.left + width // 2 - knob_x
        top = window.top + max(0, (height - pixels) // 3) - knob_y
        self.user32.SetWindowPos(hwnd, -1, left, top, 0, 0, 0x0001 | 0x0040)  # HWND_TOPMOST; NOSIZE | SHOWWINDOW
        self.user32.SetForegroundWindow(hwnd)
        time.sleep(1)  # the moved window repaints

    def drag_down(self, hwnd: int, x: int, y: int, pixels: int) -> None:
        width, height = self.user32.GetSystemMetrics(0), self.user32.GetSystemMetrics(1)
        knob_x, knob_y = self.client_to_screen(hwnd, x, y)
        print(f"     screen {width}x{height}, the OUTPUT knob at ({knob_x}, {knob_y})")
        under = self.user32.WindowFromPoint(self.POINT(knob_x, knob_y))
        app.check(under is not None and self.user32.GetAncestor(under, 2) == hwnd,  # GA_ROOT
                  "the editor is the window under the OUTPUT knob")
        self.mouse(0, knob_x, knob_y)
        time.sleep(0.1)
        self.mouse(0x0002, knob_x, knob_y)  # LEFTDOWN
        for step in range(1, pixels // 10 + 1):
            time.sleep(0.03)
            self.mouse(0, knob_x, knob_y + 10 * step)
        self.mouse(0x0004, knob_x, knob_y + pixels)  # LEFTUP
        self.user32.SetWindowPos(hwnd, -2, 0, 0, 0, 0, 0x0001 | 0x0002 | 0x0010)  # HWND_NOTOPMOST; NOSIZE|NOMOVE|NOACTIVATE


win32 = Win32() if app.WINDOWS else None


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
    script = work / "chimera-editor-test.lua"
    script.write_text(editor.LUA, encoding="utf-8")
    logs = layout.config / "logs"
    scene_file = layout.config / "basic" / "scenes" / "Chimera.json"
    check = app.check
    filter_ref = {"sourceName": app.SOURCE, "filterName": app.FILTER}

    def log_text() -> str:
        return "".join(p.read_text(encoding="utf-8", errors="replace") for p in sorted(logs.glob("*.txt")))

    def count(line: str) -> int:
        return log_text().count(line)

    def level(obs: app.Obs, name: str) -> float:
        time.sleep(1)
        x = app.decode(obs.record(name, args.seconds))
        print(f"     {name:14} RMS {app.rms(x):.4f}")
        check(app.rms(x) > 0.0001, f"{name} recording has sound")
        return app.rms(x)

    def component(obs: app.Obs) -> str:
        return obs.request("GetSourceFilter", filter_ref)["filterSettings"].get("vst3_component_state", "")

    def saved_component() -> str:
        scene = json.loads(scene_file.read_text(encoding="utf-8"))
        for source in scene.get("sources", []):
            for f in source.get("filters", []):
                if f.get("name") == app.FILTER:
                    return f.get("settings", {}).get("vst3_component_state", "")
        return ""

    print("--- first OBS run: the filter in its default state", flush=True)
    obs = app.Obs(layout, port, work / "obs-run-1.txt")
    try:
        version = obs.request("GetVersion")
        print(f"     OBS {version['obsVersion']}, obs-websocket {version['obsWebSocketVersion']}")
        obs.request("CreateScene", {"sceneName": app.SCENE})
        obs.request("SetCurrentProgramScene", {"sceneName": app.SCENE})
        obs.request("CreateInput", {"sceneName": app.SCENE, "inputName": app.SOURCE, "inputKind": "ffmpeg_source",
                                    "inputSettings": {"is_local_file": True, "local_file": str(di), "looping": True,
                                                      "restart_on_activate": False}})
        obs.request("CreateSourceFilter", {**filter_ref, "filterKind": app.FILTER_KIND, "filterSettings": {}})
        app.wait_until(lambda: count(f"[chimera-obs] '{app.FILTER}': loaded ") >= 1, "the filter loaded Chimera")
        default = level(obs, "default")
    finally:
        obs.quit()

    scene = json.loads(scene_file.read_text(encoding="utf-8"))
    scene.setdefault("modules", {})["scripts-tool"] = [{"path": script.as_posix(), "settings": {}}]
    scene_file.write_text(json.dumps(scene, indent=4), encoding="utf-8")

    print("--- second OBS run: open the editor, drag its OUTPUT knob down, record, quit with it open", flush=True)
    obs = app.Obs(layout, port, work / "obs-run-2.txt")
    try:
        app.wait_until(lambda: count(f"[chimera-obs] '{app.FILTER}': loaded ") >= 2, "the restored filter loaded Chimera")
        before = component(obs)
        check(before != "", "the filter's settings hold Chimera's state")
        obs.request("SetStudioModeEnabled", {"studioModeEnabled": True})
        app.wait_until(lambda: count(f"[chimera-obs] '{app.FILTER}': editor shown") >= 1, "the filter showed Chimera's editor", 30)
        app.wait_until(lambda: editor_window(obs.process.pid) is not None, "the editor's window", 15)
        window = editor_window(obs.process.pid)
        time.sleep(2)  # the editor fits itself to the screen and draws its first frame
        size = window_size(window)
        scale = editor_scale(size)
        # A window smaller than the whole scaled view scrolls it, and the test no longer knows where the knob is.
        check(round(EDITOR_SIZE[0] * scale) <= size[0] and round(EDITOR_SIZE[1] * scale) <= size[1],
              f"the editor shows its whole EASY VIEW ({size[0]}x{size[1]} at {scale:.0%})")
        knob = (round(OUTPUT_KNOB[0] * scale), round(OUTPUT_KNOB[1] * scale))
        print(f"     the editor at {size[0]}x{size[1]}, drawn at {scale:.0%}: the OUTPUT knob at {knob}")
        if app.WINDOWS:
            win32.bring_into_view(window, *knob, DRAG_PIXELS)
        screenshot(window, work / "editor-before.png")
        drag_down(window, *knob, DRAG_PIXELS)
        app.wait_until(lambda: component(obs) != before, "the edit in the editor reached the filter's OBS settings", 15)
        time.sleep(1)
        screenshot(window, work / "editor-after.png")
        edited_state = component(obs)
        edited = level(obs, "edited")
        check("editor:" not in "\n".join(l for l in log_text().splitlines() if "[chimera-obs]" in l),
              "the filter logged no editor error")
    finally:
        obs.quit()  # with the editor open
    edited_db = 20 * math.log10(edited / default)
    print(f"     the OUTPUT knob dragged down {DRAG_PIXELS} pixels: {edited_db:+.1f} dB against the default state")
    check(edited_db < -6, f"the edit made in the editor changes the sound ({edited_db:+.1f} dB)")
    check(saved_component() == edited_state, "OBS saved the edited state in the scene")
    check(count(f"[chimera-obs] '{app.FILTER}': editor closed with the filter") == 1,
          "quitting OBS closed the open editor with the filter")

    print("--- third OBS run: the edited state is restored", flush=True)
    obs = app.Obs(layout, port, work / "obs-run-3.txt")
    try:
        app.wait_until(lambda: count(f"[chimera-obs] '{app.FILTER}': loaded ") >= 3, "the restored filter loaded Chimera")
        restored = level(obs, "restored")
    finally:
        obs.quit()
    restored_db = 20 * math.log10(restored / edited)
    check(abs(restored_db) < 1.0, f"the restored filter sounds like the edited one ({restored_db:+.1f} dB)")

    log = log_text()
    check(log.count("[chimera-obs] module ") == 3, "all three runs log the Chimera module")
    unloads = log.count("[chimera-obs] unloaded Chimera filter instance")
    check(unloads == 3, f"every filter instance is unloaded ({unloads} of 3)")
    print(f"PASS real OBS Studio {version['obsVersion']}: an edit made with the mouse in Chimera's editor window reaches "
          f"the filter's settings, changes the sound ({edited_db:+.1f} dB), is saved in the scene and restored after a restart")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--module", required=True, help="staged chimera-obs plugin directory")
    parser.add_argument("--vst3", required=True, help="SpectralForge Chimera.vst3 bundle")
    parser.add_argument("--work", required=True, help="scratch directory (deleted first)")
    parser.add_argument("--obs", default="obs", help="OBS Studio executable (as for obs_app_test.py)")
    parser.add_argument("--ffmpeg", default="ffmpeg", help="ffmpeg executable for the analysis")
    parser.add_argument("--seconds", type=float, default=4.0, help="length of each recording")
    args = parser.parse_args()
    if app.MACOS:
        print("FAIL this test drives the editor with xdotool (Linux) or SendInput (Windows), not on macOS",
              file=sys.stderr)
        return 2
    if not app.WINDOWS and not os.environ.get("DISPLAY"):
        print("FAIL no X display (run under xvfb-run)", file=sys.stderr)
        return 2
    if not app.WINDOWS and (shutil.which("xdotool") is None or shutil.which("import") is None):
        print("FAIL xdotool and ImageMagick's import are needed", file=sys.stderr)
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
