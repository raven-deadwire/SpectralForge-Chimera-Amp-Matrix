#!/usr/bin/env python3
"""Low-band delay bound: sustain lag spread against splice rate.

On a held note the hybrid's low band is read D(t) behind the input: D moves
at (1 - ratio) per sample between 2 ms and the bound (40 ms in the product)
and a correlation-aligned splice jumps it back. The upper band stays at the
reported 10 ms, so on sustain the low band trails or leads by D + tau - L.
A tighter bound narrows that spread but splices more often. Compares the
product (probe mode 45, 40 ms) with 30 / 25 ms (modes 47 / 48); below
~25 ms a +12 pick anchor (20.3 ms) no longer fits. Modes 49 / 50 / 51 keep
the 40 ms bound and instead try an early splice every 1 ms once a held
downshift's delay passes L - tau, taken when its cost is <= .05 / .1 / .2
(52 / 53 / 54: .01 / .003 / .001; CorrelationPitch::setSustainCentre).

  sustain      E2 and A2 notes, E2 power chord held 2.5 s: per shift the
               read delay's range over the sustain (0.3 s after the pick to
               the end), its mean |D + tau - L|, and splices per second
  contract     transpose_latency_contract picks / legato / hammer-on (read head)
  chords       transpose_chord_metric spurious / partial deviation (sustain)
  modulation   legato_modulation_probe cycle errors (vibrato, bends)

Synthetic only.

  python Tools/TransposeEvidence/bound_sweep_probe.py --render build/.../ChimeraTransposeRender --out DIR
"""
import argparse, json, sys, tempfile
from pathlib import Path
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import transpose_synthetic_evidence as ev
import transpose_latency_contract as lc
import transpose_chord_metric as cm
import legato_modulation_probe as lm

RATE = 48000
MODES = [(45, 'bound 40 ms (product)'), (47, 'bound 30 ms'), (48, 'bound 25 ms')]
LABELS = dict(MODES + [(49, 'centred c<=.05'), (50, 'centred c<=.1'), (51, 'centred c<=.2'), (52, 'centred c<=.01'), (53, 'centred c<=.003'), (54, 'centred c<=.001')])
SHIFTS = [-12, -7, -5, -2, 2, 5, 7, 12]

def sustain(probe, modes):
    rows = []
    materials = {'E2': [82.41], 'A2': [110.0], 'E2_power': [82.41, 123.47, 164.81]}
    with tempfile.TemporaryDirectory(prefix='bound-sweep-') as tmp:
        for name, notes in materials.items():
            x = cm.chord(notes, RATE)
            for shift in SHIFTS:
                ratio = 2 ** (shift / 12)
                for mode, label in modes:
                    y, latency, delays, tau = lc.render_with_delays(probe, RATE, 128, mode, shift, x, tmp)
                    a, b = int((ev.PRE_S + 0.3) * RATE), len(delays)
                    d = delays[a:b]
                    # A splice moves D against its drift direction; count each
                    # run of such samples (within 5 ms) once.
                    step = np.diff(d); drift = 1 - ratio
                    against = np.nonzero(np.sign(step) == -np.sign(drift))[0] if drift != 0 else np.array([], int)
                    splices = int(np.sum(np.diff(against) > int(0.005 * RATE)) + (1 if len(against) else 0))
                    lag = 1000 * (d + tau - latency) / RATE
                    rows.append({'material': name, 'shift': shift, 'mode': label,
                                 'delay_min_ms': round(1000 * float(d.min()) / RATE, 2),
                                 'delay_max_ms': round(1000 * float(d.max()) / RATE, 2),
                                 'mean_abs_lag_ms': round(float(np.mean(np.abs(lag))), 2),
                                 'max_abs_lag_ms': round(float(np.max(np.abs(lag))), 2),
                                 'splices_per_s': round(splices / ((b - a) / RATE), 2)})
    return rows

def summarise(out, modes):
    lines = []
    s = out['sustain']
    lines.append('sustain (worst over materials E2, A2, E2 power; per shift group):')
    for label in [l for _, l in modes]:
        for group, shifts in [('down', [-12, -7, -5, -2]), ('up', [2, 5, 7, 12])]:
            sel = [r for r in s if r['mode'] == label and r['shift'] in shifts]
            lines.append(f"  {label:22s} {group:4s} delay {min(r['delay_min_ms'] for r in sel):5.2f}..{max(r['delay_max_ms'] for r in sel):5.2f} ms"
                         f"  mean|lag| {max(r['mean_abs_lag_ms'] for r in sel):5.2f}  max|lag| {max(r['max_abs_lag_ms'] for r in sel):5.2f}"
                         f"  splices/s {min(r['splices_per_s'] for r in sel):5.2f}..{max(r['splices_per_s'] for r in sel):5.2f}")
    lines.append('contract (error re reported latency, ms; legato/hammer by read head):')
    for label in [l for _, l in modes]:
        for kind, key in [('picked', 'error_ms'), ('legato', 'read_head_error_ms'), ('hammer', 'read_head_error_ms')]:
            e = np.array([r[key] for r in out['contract'] if r['mode'] == label and r['kind'] == kind and r[key] is not None], float)
            lines.append(f"  {label:22s} {kind:6s} n {len(e):3d} min {e.min():6.2f} max {e.max():6.2f} |e|<=2 {np.mean(np.abs(e) <= 2):.2f} |e|<=5 {np.mean(np.abs(e) <= 5):.2f}")
    lines.append('chords (worst over shifts):')
    for name in cm.CHORDS:
        parts = []
        for label in [l for _, l in modes]:
            sel = [r for r in out['chords'] if r['mode'] == label and r['material'] == name]
            parts.append(f"{label.split(' (')[0]}: spurious {max(r['spurious_db'] for r in sel):6.1f} dB p90dev {max(r['partial_dev_p90_db'] for r in sel):5.2f} dB mistuned {max(r['mistuned'] for r in sel)}")
        lines.append(f"  {name:10s} " + ' | '.join(parts))
    lines.append('modulation (worst over shifts):')
    lines.append(lm.summary(out['modulation']))
    return '\n'.join(lines) + '\n'

def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--render', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--modes', help='comma-separated probe modes (default 45,47,48)')
    ap.add_argument('--name', default='bound_sweep_48k', help='output file stem')
    args = ap.parse_args(argv)
    out = Path(args.out); out.mkdir(parents=True, exist_ok=True)
    modes = [(int(m), LABELS.get(int(m), f'mode {m}')) for m in args.modes.split(',')] if args.modes else MODES
    result = {'sustain': sustain(args.render, modes)}
    with tempfile.TemporaryDirectory() as tmp:
        result['contract'] = lc.run(args.render, tmp, modes=modes)
        result['chords'] = cm.run(args.render, tmp, modes=modes)
    result['modulation'] = lm.run(args.render, modes)
    (out / f'{args.name}.json').write_text(json.dumps(result, indent=1) + '\n', encoding='utf-8')
    text = summarise(result, modes)
    (out / f'{args.name}.txt').write_text(text, encoding='utf-8')
    print(text)
    return 0

if __name__ == '__main__':
    sys.exit(main())
