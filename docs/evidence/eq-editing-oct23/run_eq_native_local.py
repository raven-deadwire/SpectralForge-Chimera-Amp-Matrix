#!/usr/bin/env python3
"""Run unchanged native tests with authenticated local TCP X11 transport.

This container permits local TCP listeners but has no UNIX-domain X11 sockets.
Use the repository's actual Openbox readiness checks and preserve child status.
The X authority cookie is private, temporary, and never written into evidence.
"""
import importlib.util
import os
from pathlib import Path
import secrets
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parent
helper_path = ROOT / "chimera-eq/Tools/run_linux_ui_tests.py"
spec = importlib.util.spec_from_file_location("native_desktop", helper_path)
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


def main():
    command = sys.argv[2:] if sys.argv[1:2] == ["--"] else sys.argv[1:]
    if not command:
        raise RuntimeError("Supply an unchanged test command")
    server = manager = child = None
    with tempfile.TemporaryDirectory(prefix="chimera-eq-desktop-") as temp:
        auth = Path(temp) / "authority"
        auth.touch(mode=0o600)
        display = "127.0.0.1:193"
        subprocess.run(["xauth", "-f", str(auth), "add", display,
                        "MIT-MAGIC-COOKIE-1", secrets.token_hex(16)], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        environment = os.environ.copy()
        environment.update(DISPLAY=display, XAUTHORITY=str(auth))
        try:
            server = subprocess.Popen(
                ["Xvfb", ":193", "-screen", "0", "1920x1080x24", "-nolisten", "local",
                 "-nolisten", "unix", "-listen", "tcp", "-auth", str(auth), "-noreset"],
                env=environment, stdin=subprocess.DEVNULL, start_new_session=True)
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                if server.poll() is not None:
                    raise RuntimeError("Xvfb exited before readiness")
                probe = subprocess.run(["xprop", "-root"], env=environment, timeout=1,
                                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                if probe.returncode == 0:
                    break
                time.sleep(.1)
            else:
                raise RuntimeError("Authenticated TCP Xvfb did not become ready")
            manager = subprocess.Popen(["openbox", "--sm-disable"], env=environment,
                                       stdin=subprocess.DEVNULL, start_new_session=True)
            native.wait_window_manager(server, manager, environment, "xprop", "xlsatoms")
            print("Transport: authenticated local TCP X11; test assertions unchanged", flush=True)
            child = subprocess.Popen(command, env=environment, start_new_session=True)
            result = child.wait()
            return result if result >= 0 else 128 - result
        finally:
            for process in (child, manager, server):
                native.stop_owned(process)


if __name__ == "__main__":
    sys.exit(main())
