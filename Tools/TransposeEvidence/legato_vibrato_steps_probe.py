#!/usr/bin/env python3
"""Legato / hammer-on read-head arrival with vibrato on the held notes.

Same material as transpose_latency_contract.legato (E2 up/down, A2 up; three
changes each, pure and hammer-on, 8 shifts: 72 events per kind), plus the
same steps played with +-20 cents / 5.5 Hz and +-50 cents / 6 Hz vibrato.
Scored by the engine's own read head (read_head_arrivals): arrival minus the
reported latency, ms. Used to check that the legato steady guard (probe
modes 44-46) keeps real changes.

  python Tools/TransposeEvidence/legato_vibrato_steps_probe.py --render build/.../ChimeraTransposeRender --modes 41,45
"""
import argparse, sys, tempfile
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import numpy as np
import transpose_latency_contract as lc, transpose_synthetic_evidence as ev, transpose_realistic_probe as rp
RATE = 48000
def legato_vib(f0, steps, hammer, depth, hz):
    t = np.arange(int(RATE * 3.0)) / RATE
    cents = np.zeros_like(t)
    for k, at in enumerate(lc.LEGATO_AT): cents[t >= at] = 100 * sum(steps[:k + 1])
    cents += np.where(t > 0.3, depth * np.sin(2 * np.pi * hz * (t - 0.3)), 0)
    x, _ = rp.string(f0, RATE, 1e-4, seconds=3.0, decay=4.0, cents=cents)
    if hammer:
        for at in lc.LEGATO_AT:
            i = int(at * RATE); n = int(0.002 * RATE)
            env = np.ones_like(t); env[i:i + 4 * n] += np.concatenate([np.linspace(0, 1, n), np.linspace(1, 0, 3 * n)]); x *= env
    return rp.normalise(rp.lead_in(x))
def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--render', required=True)
    ap.add_argument('--modes', default='41,45')
    args = ap.parse_args(argv)
    PROBE, MODES = args.render, [int(m) for m in args.modes.split(',')]
    res = {}
    with tempfile.TemporaryDirectory() as tmp:
        for vib in [(0, 0), (20, 5.5), (50, 6.0)]:
            for name, f0, steps in [('E2_up', 82.41, [2, 1, 2]), ('E2_down', 82.41, [-1, -2, -2]), ('A2_up', 110.0, [2, 2, 1])]:
                for hammer in (False, True):
                    x = legato_vib(f0, steps, hammer, *vib) if vib[0] else lc.legato(f0, steps, RATE, hammer)[0]
                    for shift in lc.SHIFTS:
                        for mode in MODES:
                            y, latency, delays, tau = lc.render_with_delays(PROBE, RATE, 128, mode, shift, x, tmp)
                            heads = lc.read_head_arrivals(delays, tau, RATE)
                            rep = 1000 * latency / RATE
                            errs = [None if h is None else 1000 * h - 1000 * (ev.PRE_S + at) - rep for h, at in zip(heads, lc.LEGATO_AT)]
                            key = (vib, 'hammer' if hammer else 'legato', mode)
                            res.setdefault(key, []).extend(errs)
    for key, errs in res.items():
        e = np.array([np.nan if v is None else v for v in errs], float); ok = np.isfinite(e)
        print(f"vib {key[0][0]:3d}c {key[1]:6s} mode {key[2]}: n {len(e)} median {np.nanmedian(e):6.2f} min {np.nanmin(e):6.2f} max {np.nanmax(e):6.2f} |e|<=2 {np.mean(np.abs(e[ok])<=2):.2f} |e|<=5 {np.mean(np.abs(e[ok])<=5):.2f}")
    return 0

if __name__ == '__main__':
    sys.exit(main())
