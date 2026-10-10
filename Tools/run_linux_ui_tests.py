#!/usr/bin/env python3
"""Run native UI tests once on an isolated Xvfb desktop managed by Openbox.

Requires Xvfb, openbox, and x11-utils. Readiness checks only inspect the real
desktop; they neither create missing WM atoms nor suppress X errors. The test
command's exit status survives cleanup of the two processes owned here.
"""

from __future__ import annotations

import os
import re
import selectors
import shutil
import signal
import subprocess
import sys
import time


class DesktopError(RuntimeError):
    pass


class Interrupted(RuntimeError):
    def __init__(self, signum: int):
        self.signum = signum


def stop_owned(process: subprocess.Popen | None) -> None:
    """Bound cleanup without changing a completed test's result."""
    if process is None or process.poll() is not None:
        return
    try:
        os.killpg(process.pid, signal.SIGTERM)
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait(timeout=3)
    except ProcessLookupError:
        pass
    except (OSError, subprocess.TimeoutExpired) as error:
        print(f"Native desktop cleanup: {error}", file=sys.stderr, flush=True)


def wait_display(process: subprocess.Popen, descriptor: int) -> str:
    # -displayfd allocates an unused display and reports it only after Xvfb
    # can accept connections. No guessed display number or fixed startup sleep.
    deadline = time.monotonic() + 15
    received = b""
    with selectors.DefaultSelector() as ready:
        ready.register(descriptor, selectors.EVENT_READ)
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise DesktopError(f"Xvfb exited during startup ({process.returncode})")
            if not ready.select(timeout=min(0.2, max(0, deadline - time.monotonic()))):
                continue
            part = os.read(descriptor, 64)
            if not part:
                raise DesktopError("Xvfb closed its readiness pipe without a display")
            received += part
            if b"\n" in received:
                number = received.split(b"\n", 1)[0]
                if not number.isdigit():
                    raise DesktopError(f"Invalid Xvfb display response: {number!r}")
                return ":" + number.decode("ascii")
    raise DesktopError("Xvfb did not become ready within 15 seconds")


def inspect(command: list[str], environment: dict[str, str]) -> str:
    result = subprocess.run(command, env=environment, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=1)
    if result.returncode:
        raise DesktopError(result.stdout.strip() or f"Desktop probe failed: {command[0]}")
    return result.stdout


def wait_window_manager(xvfb: subprocess.Popen, manager: subprocess.Popen,
                        environment: dict[str, str], xprop: str, xlsatoms: str) -> None:
    deadline = time.monotonic() + 15
    last_problem = "The supporting window manager property is absent"
    while time.monotonic() < deadline:
        if xvfb.poll() is not None or manager.poll() is not None:
            raise DesktopError("Xvfb or Openbox exited before native desktop readiness")
        try:
            root = inspect([xprop, "-root", "_NET_SUPPORTING_WM_CHECK"], environment)
            match = re.search(r"window id # (0x[0-9a-fA-F]+)", root)
            if match and int(match[1], 16):
                window = match[1]
                support = inspect([xprop, "-id", window, "_NET_SUPPORTING_WM_CHECK"], environment)
                self_check = re.search(r"window id # (0x[0-9a-fA-F]+)", support)
                protocols = inspect([xlsatoms, "-name", "WM_PROTOCOLS"], environment)
                if (self_check and int(self_check[1], 16) == int(window, 16)
                        and re.search(r"\b[1-9][0-9]*\s+WM_PROTOCOLS\b", protocols)):
                    print(f"Native desktop ready: DISPLAY={environment['DISPLAY']}, "
                          f"Openbox supporting window={window}, WM_PROTOCOLS present", flush=True)
                    return
                last_problem = "Openbox support-window/WM_PROTOCOLS validation is incomplete"
            else:
                last_problem = root.strip()
        except (DesktopError, subprocess.TimeoutExpired) as error:
            last_problem = str(error)
        time.sleep(0.1)
    raise DesktopError(f"Openbox did not become ready within 15 seconds: {last_problem}")


def main(arguments: list[str]) -> int:
    command = arguments[1:] if arguments[:1] == ["--"] else arguments
    if not command or sys.platform != "linux":
        print("Usage on Linux: run_linux_ui_tests.py -- COMMAND [ARG ...]", file=sys.stderr)
        return 2
    programs = {name: shutil.which(name) for name in ("Xvfb", "openbox", "xprop", "xlsatoms")}
    missing = [name for name, path in programs.items() if path is None]
    if missing:
        print("Missing native desktop tools: " + ", ".join(missing), file=sys.stderr)
        return 2

    xvfb = manager = child = None
    read_descriptor = write_descriptor = None
    old_handlers = {}

    def interrupted(signum, _frame):
        raise Interrupted(signum)

    try:
        for signum in (signal.SIGINT, signal.SIGTERM):
            old_handlers[signum] = signal.signal(signum, interrupted)
        read_descriptor, write_descriptor = os.pipe()
        xvfb = subprocess.Popen(
            [programs["Xvfb"], "-displayfd", str(write_descriptor), "-screen", "0",
             "1920x1080x24", "-nolisten", "tcp", "-noreset"],
            pass_fds=(write_descriptor,), stdin=subprocess.DEVNULL, start_new_session=True)
        os.close(write_descriptor)
        write_descriptor = None
        environment = os.environ.copy()
        environment["DISPLAY"] = wait_display(xvfb, read_descriptor)
        os.close(read_descriptor)
        read_descriptor = None
        manager = subprocess.Popen([programs["openbox"], "--sm-disable"], env=environment,
                                   stdin=subprocess.DEVNULL, start_new_session=True)
        wait_window_manager(xvfb, manager, environment, programs["xprop"], programs["xlsatoms"])
        # Keep argv and all native stdout/stderr intact. No test retry, filter,
        # error handler, timing threshold, or assertion is changed here.
        child = subprocess.Popen(command, env=environment, start_new_session=True)
        result = child.wait()
        return result if result >= 0 else 128 - result
    except Interrupted as error:
        return 128 + error.signum
    except (DesktopError, OSError) as error:
        print(f"Native desktop setup failed: {error}", file=sys.stderr, flush=True)
        return 2
    finally:
        for signum in old_handlers:
            signal.signal(signum, signal.SIG_IGN)
        for process in (child, manager, xvfb):
            stop_owned(process)
        for descriptor in (read_descriptor, write_descriptor):
            if descriptor is not None:
                os.close(descriptor)
        for signum, handler in old_handlers.items():
            signal.signal(signum, handler)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
