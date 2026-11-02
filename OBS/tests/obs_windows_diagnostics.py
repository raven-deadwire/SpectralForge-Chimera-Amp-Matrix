#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Capture an OBS thread minidump in a disposable, time-bounded child process.

The caller must impose a subprocess timeout: DbgHelp is external code and must
never hold up test cleanup. No full-memory dump or credentials are requested.
"""
import argparse
import ctypes
from ctypes import wintypes
from pathlib import Path
import sys


def write_minidump(pid: int, path: Path) -> None:
    if sys.platform != "win32":
        raise RuntimeError("Windows process dumps require native Windows")
    import msvcrt

    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    dbghelp = ctypes.WinDLL("dbghelp", use_last_error=True)
    kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel.OpenProcess.restype = wintypes.HANDLE
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel.CloseHandle.restype = wintypes.BOOL
    dbghelp.MiniDumpWriteDump.argtypes = [
        wintypes.HANDLE, wintypes.DWORD, wintypes.HANDLE, wintypes.DWORD,
        ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p,
    ]
    dbghelp.MiniDumpWriteDump.restype = wintypes.BOOL
    # QUERY_INFORMATION | VM_READ; do not suspend or terminate the target.
    process = kernel.OpenProcess(0x0400 | 0x0010, False, pid)
    if not process:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        # Normal stacks, thread info, unloaded modules, memory-map metadata.
        # MiniDumpWithFullMemory (0x2) is deliberately absent.
        flags = 0x1000 | 0x0020 | 0x0800
        with path.open("xb") as output:
            handle = msvcrt.get_osfhandle(output.fileno())
            if not dbghelp.MiniDumpWriteDump(process, pid, handle, flags, None, None, None):
                raise ctypes.WinError(ctypes.get_last_error())
    finally:
        kernel.CloseHandle(process)
    if path.stat().st_size < 32:
        raise RuntimeError("DbgHelp produced an incomplete minidump")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        write_minidump(args.pid, args.output)
    except Exception as error:
        print(f"minidump unavailable: {type(error).__name__}: {error}", file=sys.stderr)
        return 1
    print(f"saved thread minidump: {args.output} ({args.output.stat().st_size} bytes)", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
