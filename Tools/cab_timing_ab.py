#!/usr/bin/env python3
"""Runs the CAB extended timing test from two builds alternately (A B B A per round)
and tabulates p99 and misses per condition. Temporary investigation of PR #253."""
import argparse
import os
import re
import statistics
import subprocess
import sys
from pathlib import Path

LINE = re.compile(r"^TIMING engine=(\S+) sr=(\d+) block=(\d+) channels=(\d+) three_cabs_six_mics p50_us=([\d.]+) "
                  r"p99_us=([\d.]+) thread_cpu_p99_us=([\d.]+) max_us=([\d.]+) misses=(\d+)/(\d+)")


def run(exe: str, workdir: Path) -> tuple[int, dict]:
    workdir.mkdir(parents=True, exist_ok=True)
    env = {k: v for k, v in os.environ.items() if k != "CHIMERA_LIFECYCLE_TRACE"}
    proc = subprocess.run([exe, "--expanded", "--extended-buffers"], cwd=workdir, env=env,
                          capture_output=True, text=True)
    (workdir / "stdout.txt").write_text(proc.stdout + proc.stderr)
    rows = {}
    for line in proc.stdout.splitlines():
        m = LINE.match(line)
        if m:
            key = (int(m[2]), int(m[3]), int(m[4]))
            rows[key] = {"p99": float(m[6]), "cpu99": float(m[7]), "max": float(m[8]), "misses": int(m[9])}
    return proc.returncode, rows


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--a", required=True)
    ap.add_argument("--b", required=True)
    ap.add_argument("--rounds", type=int, default=3)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()
    out = Path(args.out)
    runs = []  # (label, round, index, code, rows)
    order = ["a", "b", "b", "a"]
    n = 0
    for r in range(args.rounds):
        for side in order:
            n += 1
            code, rows = run(getattr(args, side), out / f"run{n:02d}-{side}")
            failing = [k for k, v in rows.items() if v["p99"] >= 1e6 * k[1] / k[0]]
            print(f"run {n:2d} round {r + 1} {side.upper()}: exit {code}, conditions {len(rows)}, "
                  f"p99 over budget {failing or 'none'}, misses {sum(v['misses'] for v in rows.values())}", flush=True)
            runs.append((side, r, n, code, rows))
    keys = sorted({k for *_, rows in runs for k in rows})
    lines = ["sr block ch budget_us | " + " | ".join(f"{s.upper()}{n}" for s, _, n, _, _ in runs)]
    for k in keys:
        budget = 1e6 * k[1] / k[0]
        cells = []
        for *_, rows in runs:
            v = rows.get(k)
            cells.append("-" if v is None else f"{v['p99']:.0f}/{v['misses']}")
        lines.append(f"{k[0]} {k[1]} {k[2]} {budget:.0f} | " + " | ".join(cells))
    table = "\n".join(lines)
    print("\np99 us / misses per run\n" + table)
    summary = ["", "per side: failing runs, total misses, worst p99/budget"]
    for side in "ab":
        mine = [x for x in runs if x[0] == side]
        fails = sum(1 for x in mine if x[3] != 0)
        misses = sum(v["misses"] for x in mine for v in x[4].values())
        worst = max((v["p99"] / (1e6 * k[1] / k[0]), k) for x in mine for k, v in x[4].items())
        med = {k: statistics.median(x[4][k]["p99"] for x in mine if k in x[4]) for k in keys}
        summary.append(f"{side.upper()}: {fails}/{len(mine)} runs failed, misses {misses}, worst p99 {worst[0]:.2f}x budget at {worst[1]}")
        summary.append(f"{side.upper()} median p99 96k/64/2ch: {med.get((96000, 64, 2), float('nan')):.1f} us")
    print("\n".join(summary))
    (out / "table.txt").write_text(table + "\n" + "\n".join(summary) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
