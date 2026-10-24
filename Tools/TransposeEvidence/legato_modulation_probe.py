#!/usr/bin/env python3
"""Do legato re-anchors under vibrato and fast bends hurt the output?

The legato detector (Tools/TransposeNext/LegatoDetector.h) also fires on
wide vibrato (+-100 cents at 7 Hz: 10 events per 3 s in section 7.4, 14-29 on
the material here) and fast bends
(docs/TRANSPOSE_HYBRID_LATENCY_CONTRACT.md section 7.4). This compares the
research hybrid without legato re-anchoring (probe mode 39) against legato
re-anchoring without (mode 41) and with the product's 10 ms steady guard
(mode 45; 44/46: 6/15 ms) and TIGHT (mode 2) on a held, slowly decaying
inharmonic string (E2, A2) whose pitch follows a known cents track.

Per output fundamental cycle (zero crossings of the output low-passed at
1.4 x the highest expected fundamental), the cycle's frequency is compared
with the input track x ratio at the cycle's centre minus a lag (the lag in
0-45 ms that minimises the median error). Reported over the held part
(0.3 s after the onset to 0.2 s before the end):

  median / p99 |cents|, share of cycles off by > 50 cents (a mid-cycle
  read-head jump leaves one long or short cycle), worst |cents|, and the
  legato event count from the probe.

Synthetic only; whether repeated re-anchoring on extreme vibrato is audible
needs listening.

  python Tools/TransposeEvidence/legato_modulation_probe.py --render build/.../ChimeraTransposeRender --out FILE.json
"""
import argparse, json, os, subprocess, sys, tempfile
from pathlib import Path
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import transpose_synthetic_evidence as ev
import transpose_realistic_probe as rp
import transpose_latency_contract as lc

RATE, LENGTH_S = 48000, 3.0
SHIFTS = [-12, -5, 2, 12]
MODES = [(2, 'TIGHT'), (39, 'L10 no legato'), (41, 'legato, no guard')]
LABELS = dict(MODES + [(44, 'legato guard 6 ms'), (45, 'guard 10 ms (product)'), (46, 'legato guard 15 ms')])

def tracks():
    """name -> (f0, cents per sample)."""
    t = np.arange(int(RATE * LENGTH_S)) / RATE
    on = t > 0.3
    out = {}
    for f0, note in [(82.41, 'E2'), (110.0, 'A2')]:
        for depth, hz in [(20, 5.5), (50, 6.0), (100, 7.0)]:
            out[f'{note}_vib{depth}c_{hz:g}Hz'] = (f0, np.where(on, depth * np.sin(2 * np.pi * hz * (t - 0.3)), 0.0))
    for cents, ms in [(200, 100), (200, 50), (100, 30)]:
        out[f'E2_bend{cents}c_{ms}ms'] = (82.41, cents * np.clip((t - 0.8) / (ms / 1000), 0, 1))
        # Bend up and release, twice (a common lead gesture).
        up = np.clip((t - 0.8) / (ms / 1000), 0, 1) - np.clip((t - 1.3) / (ms / 1000), 0, 1) \
            + np.clip((t - 1.8) / (ms / 1000), 0, 1) - np.clip((t - 2.3) / (ms / 1000), 0, 1)
        out[f'E2_bendrelease{cents}c_{ms}ms'] = (82.41, cents * up)
    return out

def render(probe, mode, shift, x, tmp):
    src, dst = os.path.join(tmp, 'in.f32'), os.path.join(tmp, 'out.f32')
    np.asarray(x, dtype='<f4').tofile(src)
    p = subprocess.run([str(probe), str(RATE), '128', str(mode), str(shift), src, dst, 'stats'],
                       check=True, capture_output=True, text=True)
    events = int(p.stderr.split('legato_events=')[1].split()[0]) if 'legato_events=' in p.stderr else None
    return np.fromfile(dst, dtype='<f4').astype(float), events

def cycle_errors(y, track_hz, ratio):
    """|cents| per output fundamental cycle at the best lag, or None."""
    s = lc.lowpass(y, 1.4 * track_hz.max() * ratio, RATE)
    z = np.nonzero((s[:-1] < 0) & (s[1:] >= 0))[0].astype(float)
    if len(z) < 20: return None
    # Sub-sample crossing positions.
    z = z + s[z.astype(int)] / (s[z.astype(int)] - s[z.astype(int) + 1])
    centres, freq = 0.5 * (z[1:] + z[:-1]), RATE / np.diff(z)
    a, b = (ev.PRE_S + 0.3) * RATE, (ev.PRE_S + LENGTH_S - 0.2) * RATE
    keep = (centres > a) & (centres < b)
    best = None
    for lag_ms in range(0, 46):
        idx = np.clip((centres[keep] - lag_ms * RATE / 1000).astype(int), 0, len(track_hz) - 1)
        err = np.abs(1200 * np.log2(freq[keep] / (track_hz[idx] * ratio)))
        if best is None or np.median(err) < np.median(best[1]): best = (lag_ms, err)
    return best

def run(probe, modes=MODES):
    rows = []
    with tempfile.TemporaryDirectory(prefix='legato-modulation-') as tmp:
        for name, (f0, cents) in tracks().items():
            x, _ = rp.string(f0, RATE, 1e-4, seconds=LENGTH_S, decay=4.0, cents=cents)
            x = rp.normalise(np.concatenate([np.zeros(int(ev.PRE_S * RATE)), x]))
            track = np.concatenate([np.full(int(ev.PRE_S * RATE), f0), f0 * 2 ** (cents / 1200)])
            for shift in SHIFTS:
                ratio = 2 ** (shift / 12)
                for mode, label in modes:
                    y, events = render(probe, mode, shift, x, tmp)
                    row = {'material': name, 'shift': shift, 'mode': label, 'legato_events': events}
                    best = cycle_errors(y, track, ratio)
                    if best is not None:
                        lag, err = best
                        row.update({'lag_ms': lag, 'median_cents': round(float(np.median(err)), 2),
                                    'p99_cents': round(float(np.percentile(err, 99)), 1),
                                    'off50_share': round(float(np.mean(err > 50)), 4),
                                    'worst_cents': round(float(err.max()), 1), 'cycles': int(len(err))})
                    rows.append(row)
    return rows

def summary(rows):
    lines = []
    for name in dict.fromkeys(r['material'] for r in rows):
        for mode in dict.fromkeys(r['mode'] for r in rows):
            sel = [r for r in rows if r['material'] == name and r['mode'] == mode and 'median_cents' in r]
            if not sel: continue
            ev_ = [r['legato_events'] for r in sel]
            lines.append(f"{name:26s} {mode:22s} events {str(ev_):16s} median {max(r['median_cents'] for r in sel):5.2f} "
                         f"p99 {max(r['p99_cents'] for r in sel):6.1f} off50 {max(r['off50_share'] for r in sel):.4f} "
                         f"worst {max(r['worst_cents'] for r in sel):7.1f}")
    return '\n'.join(lines)

def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--render', required=True)
    ap.add_argument('--out', required=True, help='JSON file; a .txt summary is written next to it')
    ap.add_argument('--modes', help='comma-separated probe modes (default 2,39,41)')
    args = ap.parse_args(argv)
    modes = [(int(m), LABELS.get(int(m), f'mode {m}')) for m in args.modes.split(',')] if args.modes else MODES
    rows = run(args.render, modes)
    out = Path(args.out); out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(rows, indent=1) + '\n', encoding='utf-8')
    text = ('worst over shifts ' + ','.join(f'{s:+d}' for s in SHIFTS) + ' (events per shift), 48 kHz, block 128\n'
            + summary(rows) + '\n')
    out.with_suffix('.txt').write_text(text, encoding='utf-8')
    print(text)
    return 0

if __name__ == '__main__':
    sys.exit(main())
