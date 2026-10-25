#!/usr/bin/env python3
"""Fixed-PDC study for a variable-delay Transpose candidate (T6 prep).

The candidate's read delay varies (about 2-40 ms), so no fixed latency makes
its output line up exactly. This measures, per event, the added delay of the
1-8 kHz attack peak and of the 120 Hz low-band energy centroid (output minus
input, the T1/T2/T3 metrics), then evaluates fixed reported latencies L: the
residual a host leaves after compensating L is (added delay - L).

  run       render and store per-event added delays (results.json)
  evaluate  residual statistics for a grid of L and the rule's choice

Synthetic inputs only; see docs/TRANSPOSE_T6_PDC_CONTRACT.md.
"""
import argparse, json, subprocess
from pathlib import Path
import numpy as np
from evaluate import attacks
from legacy_metrics import sha
from latency_targets import inputs
from weak_attack_holdout import lowband_extra

def cmd_run(a):
    a.out.mkdir(parents=True, exist_ok=True); rows = []
    for rate in map(int, a.rates.split(',')):
        for key, x, meta, roles in inputs(rate, tuple(map(int, a.holdout_seeds.split(','))), a.long_seed):
            if not roles & {'attack', 'short', 'long'}: continue
            ip = a.out/'in.f32'; x.tofile(ip)
            before = attacks(x, meta) if 'attack' in roles else None
            for shift in map(int, a.shifts.split(',')):
                op = a.out/'out.f32'
                subprocess.run([str(a.renderer), str(ip), str(op), str(rate), '128', str(shift), a.engine, str(a.out/'cb.csv')], check=True, capture_output=True)
                y = np.fromfile(op, dtype='<f4').reshape(-1, 2); row = {'input': key, 'rate': rate, 'shift': shift, 'roles': sorted(roles)}
                if before is not None and meta['name'] != 'riff50':  # riff50's search window overlaps (as latency_targets)
                    lin, lout = before['attack_lags_ms'], attacks(y, meta)['attack_lags_ms']
                    # Per event: only events whose own input peak sits within 10 ms.
                    row['attack_added_ms'] = [o-i for i, o in zip(lin, lout) if i <= 10]
                if roles & {'short', 'long'}: row['lowband_added_ms'] = lowband_extra(x, y, meta['onsets'], rate)
                rows.append(row)
            print(rate, key, flush=True)
    rep = {'engine': a.engine, 'renderer_sha256': sha(a.renderer), 'holdout_seeds': a.holdout_seeds, 'long_seed': a.long_seed}
    (a.out/'results.json').write_text(json.dumps({'report': rep, 'rows': rows}, indent=1)+'\n')
    for f in ('in.f32', 'out.f32', 'cb.csv'): (a.out/f).unlink(missing_ok=True)
    return 0

def residuals(values, L):
    r = np.asarray(values)-L
    return {'max_abs_ms': round(float(np.abs(r).max()), 3), 'p5_ms': round(float(np.percentile(r, 5)), 3), 'p95_ms': round(float(np.percentile(r, 95)), 3),
            'min_ms': round(float(r.min()), 3), 'max_ms': round(float(r.max()), 3)}

def choose(values):
    """Rule: the L (0.25 ms grid) minimising the largest |attack residual|."""
    grid = np.arange(0, 20.001, .25); worst = [np.abs(np.asarray(values)-L).max() for L in grid]
    return float(grid[int(np.argmin(worst))])

def analyse(rows):
    out = {}
    for rate in sorted({r['rate'] for r in rows}):
        sel = [r for r in rows if r['rate'] == rate]
        att = [v for r in sel for v in r.get('attack_added_ms', [])]
        L = choose(att); per_shift = {}
        for sh in sorted({r['shift'] for r in sel}):
            s = [v for r in sel if r['shift'] == sh for v in r.get('attack_added_ms', [])]
            low = [v for r in sel if r['shift'] == sh for v in r.get('lowband_added_ms', [])]
            per_shift[str(sh)] = {'attack_events': len(s), 'attack_added_range_ms': [round(min(s), 3), round(max(s), 3)], 'attack_residual_at_L': residuals(s, L),
                                  'lowband_added_range_ms': [round(min(low), 3), round(max(low), 3)], 'lowband_residual_at_L': residuals(low, L)}
        out[str(rate)] = {'attack_events': len(att), 'attack_added_ms': {'min': round(min(att), 3), 'median': round(float(np.median(att)), 3), 'max': round(max(att), 3)},
                          'L_ms': L, 'L_samples': int(round(L*rate/1000)), 'attack_residual_at_L': residuals(att, L),
                          'attack_residual_at_0': residuals(att, 0.), 'per_shift': per_shift}
    return out

def cmd_evaluate(a):
    d = json.loads((a.out/'results.json').read_text()); res = analyse(d['rows'])
    (a.out/'pdc_summary.json').write_text(json.dumps({'source': d['report'], 'by_rate': res}, indent=1)+'\n')
    for rate, v in res.items():
        print(rate, 'L', v['L_ms'], 'ms =', v['L_samples'], 'samples; attack added', v['attack_added_ms'], 'residual', v['attack_residual_at_L'])
    return 0

def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter); s = p.add_subparsers(dest='cmd', required=True)
    q = s.add_parser('run'); q.add_argument('--renderer', type=Path, required=True); q.add_argument('--out', type=Path, required=True)
    q.add_argument('--engine', default='corr40-split-v3'); q.add_argument('--rates', default='44100,48000,96000'); q.add_argument('--shifts', default='-2,-1,1,2')
    q.add_argument('--holdout-seeds', default='8139,20261024'); q.add_argument('--long-seed', type=int, default=1); q.set_defaults(f=cmd_run)
    q = s.add_parser('evaluate'); q.add_argument('--out', type=Path, required=True); q.set_defaults(f=cmd_evaluate)
    a = p.parse_args(); raise SystemExit(a.f(a))

if __name__ == '__main__':
    main()
