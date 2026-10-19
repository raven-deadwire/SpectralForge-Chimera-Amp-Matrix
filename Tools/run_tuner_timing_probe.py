#!/usr/bin/env python3
"""Collect observational callback timings; exit success never means RT qualification."""
import datetime
import hashlib
import json
from pathlib import Path
import platform
import re
import subprocess
import sys


def main():
    root = Path(__file__).resolve().parents[1]
    build = Path(sys.argv[1]).resolve()
    folder = build / "tuner-timing"
    folder.mkdir(parents=True, exist_ok=True)
    name = "ChimeraUITests.exe" if sys.platform == "win32" else "ChimeraUITests"
    candidates = [build / "ChimeraUITests_artefacts" / "Release" / name,
                  build / "ChimeraUITests_artefacts" / name]
    executable = next((p for p in candidates if p.is_file()), None)
    if executable is None:
        raise FileNotFoundError("Build ChimeraUITests before collecting timings")
    command = [str(executable), str(folder), "--precision-tuner-timing"]
    if sys.platform.startswith("linux"):
        command = [sys.executable, str(root / "Tools/run_linux_ui_tests.py"), "--", *command]
    revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
    dirty = bool(subprocess.check_output(["git", "status", "--porcelain", "--untracked-files=no"], cwd=root, text=True).strip())
    provenance = {
        "source_sha": revision, "source_dirty": dirty,
        "platform": platform.platform(), "machine": platform.machine(),
        "created_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "scope": "normal-priority synthetic callback feeder; no audio driver or DAW",
        "qualification": "NOT_ASSESSED", "thresholds_changed": False,
        "warmup_retained": True, "same_callback_thread_clock": True,
        "scenarios": "two reversed-order repeats; 48k/256 and 96k/64; tuner ON/OFF; UI visible/closed",
        "binary_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
    }
    with (folder / "probe.log").open("w", encoding="utf-8") as log:
        result = subprocess.run(command, cwd=root, stdout=log, stderr=subprocess.STDOUT, check=False)
    output = (folder / "probe.log").read_text(encoding="utf-8", errors="replace")
    built = re.search(r"^PROVENANCE source_sha=([0-9a-f]{40})$", output, re.MULTILINE)
    provenance["binary_source_sha"] = built.group(1) if built else None
    provenance["binary_source_matches"] = provenance["binary_source_sha"] == revision
    (folder / "provenance.json").write_text(json.dumps(provenance, indent=2) + "\n", encoding="utf-8")
    print(output)
    if not provenance["binary_source_matches"]:
        raise RuntimeError("Timing binary does not identify the selected source; observations remain unqualified")
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
