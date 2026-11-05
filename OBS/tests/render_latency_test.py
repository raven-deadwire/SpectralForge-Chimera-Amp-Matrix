#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Chimera's own processing delay, outside OBS and without 16-bit rounding (plan stage D).

obs_latency_test.py measures the delay in a 16-bit OBS recording, where output below
half a step is lost and, on the macOS CI runner, single steps appear up to a block before
a burst. This test runs the same Chimera VST3 through render-tool.cpp (the filter's VST3
hosting, no OBS) on a note after two seconds of digital silence, in float, with blocks of
1024 (OBS's tick), 480 and 64 frames and the note at the start and in the middle of a
block, and reports:

- where the output first exceeds 1e-6 (-120 dBFS) after the note: Chimera's delay, which
  must not be shorter than the latency Chimera reports and must not depend on the block
  size or on where in a block the note starts;
- the largest output before the note: what Chimera puts out ahead of the note within the
  block that holds it (seen at about 1e-8 on Linux), printed, not checked;
- where the output passes 16 16-bit steps: the note's rise, which Chimera's processing
  shapes and which is not delay.
"""
import argparse
import math
import re
import struct
import subprocess
import sys
from pathlib import Path

RATE = 48000
SILENCE = 2 * RATE
BLOCKS = (1024, 480, 64)
FLOAT_THRESHOLD = 1e-6
STEP = 1 / 32768


def note(frames: int) -> list[float]:
    return [0.3 * math.exp(-3 * n / RATE) * sum(math.sin(2 * math.pi * 110 * k * n / RATE) / k for k in range(1, 5))
            for n in range(frames)]


def render(tool: str, vst3: str, block: int, onset: int, work: Path) -> tuple[list[float], int]:
    x = [0.0] * onset + note(RATE // 2) + [0.0] * RATE
    source, target = work / "input.f32", work / "output.f32"
    source.write_bytes(struct.pack(f"<{2 * len(x)}f", *(v for s in x for v in (s, s))))
    result = subprocess.run([tool, vst3, str(RATE), "2", str(block), str(source), str(target)],
                            capture_output=True, text=True, check=False)
    if result.returncode:
        raise RuntimeError(f"render tool failed: {result.stderr.strip() or result.stdout.strip()}")
    raw = target.read_bytes()
    return list(struct.unpack(f"<{len(raw) // 4}f", raw)[0::2]), int(re.search(r"latency (\d+)", result.stdout).group(1))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--vst3", required=True, help="SpectralForge Chimera.vst3 bundle")
    parser.add_argument("--render-tool", required=True, help="chimera-obs-render-tool executable")
    parser.add_argument("--work", required=True, help="scratch directory")
    args = parser.parse_args()
    work = Path(args.work).resolve()
    work.mkdir(parents=True, exist_ok=True)
    delays, failures = set(), []
    for block in BLOCKS:
        # The note at a block start, and half a block in (OBS's ticks do not line up with the audio).
        for onset in (SILENCE - SILENCE % block, SILENCE - SILENCE % block + block // 2):
            y, latency = render(args.render_tool, str(Path(args.vst3).resolve()), block, onset, work)
            first = next(n - onset for n in range(onset, len(y)) if abs(y[n]) > FLOAT_THRESHOLD)
            rise = next(n - onset for n in range(onset, len(y)) if abs(y[n]) > 16 * STEP)
            ahead = max(abs(v) for v in y[onset - block:onset])
            print(f"     block {block:4}, note {onset % block:3} into a block: output above {FLOAT_THRESHOLD:g} at "
                  f"{first:+d} samples, 16 steps at {rise:+d}, largest output before the note {ahead:.2g} "
                  f"({f'{20 * math.log10(ahead):.0f}' if ahead > 1e-12 else 'below -240'} dBFS); reported latency {latency}")
            delays.add(first)
            if first < latency:
                failures.append(f"block {block}: output {first} samples after the note, before the reported {latency}")
    if len(delays) != 1:
        failures.append(f"the delay depends on the block size or on where the note starts: {sorted(delays)}")
    if failures:
        print("FAIL " + "; ".join(failures), file=sys.stderr)
        return 1
    print(f"PASS Chimera outside OBS: the output follows a note by {delays.pop()} samples (above {FLOAT_THRESHOLD:g}), "
          f"never sooner than its reported latency, with every block size and note position")
    return 0


if __name__ == "__main__":
    sys.exit(main())
