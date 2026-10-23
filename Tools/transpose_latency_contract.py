#!/usr/bin/env python3
"""Check a reported Transpose latency against when notes actually arrive.

A host compensates a plug-in's reported latency L by shifting its output
earlier by L. For a pitch shifter that is only right if new notes arrive
L after the input. This measures, per note event, the arrival delay minus
the delay the engine reports (ChimeraTransposeRender prints it; -1 means
none, scored against REFERENCE_MS):

  picked   attacks of the protocol material, harder material and palm-muted
           sixteenths: low-band (below 150 Hz) first-crossing delay
           (transpose_synthetic_evidence.onset_latency_ms) per pick
  legato   a sustained E2 / A2 note that changes pitch with no new pick:
           pure (phase-continuous, no level change) and hammer-on (+6 dB,
           2 ms bump). Arrival = when the new note's level first exceeds the
           old note's: summed centred 120 ms Hann demodulation of harmonics
           1-10 (x ratio) that are > 18 Hz from every harmonic of the other
           note (a semitone at the 2nd harmonic is only ~10 Hz apart, and a
           new note's 8th harmonic can sit on the old note's 7th), output
           minus input. The centred window biases input and output alike.

Reports each event's error re the reported latency. Synthetic only.

  python Tools/transpose_latency_contract.py --render build/.../ChimeraTransposeRender --out DIR
"""
import argparse, json, sys, tempfile
from pathlib import Path
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import transpose_synthetic_evidence as ev
import transpose_realistic_probe as rp

SHIFTS = [-12, -7, -5, -2, 2, 5, 7, 12]
MODES = [(2, "TIGHT"), (37, "AL10RA-800"), (39, "AL10RA-800-L10")]
REFERENCE_MS = 10.0
PICKED = ['bass_B0', 'bass_E1', 'guitar_E2', 'lead_A3', 'power_chord_E2', 'bass_E1_repick', 'guitar_E2_repick']
LEGATO_AT = [1.0, 1.6, 2.2]   # seconds after the lead-in

def legato(f0, steps, rate, hammer):
    """One string note changing pitch at LEGATO_AT by `steps` semitones each."""
    t = np.arange(int(rate * 3.0)) / rate
    cents = np.zeros_like(t)
    for k, at in enumerate(LEGATO_AT):
        cents[t >= at] = 100 * sum(steps[:k + 1])
    x, _ = rp.string(f0, rate, 1e-4, seconds=3.0, decay=4.0, cents=cents)
    if hammer:
        for at in LEGATO_AT:
            i = int(at * rate); n = int(0.002 * rate)
            env = np.ones_like(t); env[i:i + 4 * n] += np.concatenate([np.linspace(0, 1, n), np.linspace(1, 0, 3 * n)])
            x *= env
    return rp.normalise(rp.lead_in(x)), [f0 * 2 ** (sum(steps[:k]) / 12) for k in range(len(steps) + 1)]

def demod(y, f, rate, window_s=0.12):
    t = np.arange(len(y)) / rate
    z = np.asarray(y, float) * np.exp(-2j * np.pi * f * t)
    w = np.hanning(int(window_s * rate)); w /= w.sum()
    return 2 * np.abs(np.convolve(z, w, mode='same'))

def note_level(y, f, other, ratio, rate):
    """Summed demodulated level of harmonics 1-10 of f (x ratio) that are more
    than 18 Hz from every harmonic of `other`: the 120 ms Hann main lobe is
    +-17 Hz, so closer pairs leak into each other."""
    total = 0
    for k in range(1, 11):
        if min(abs(k * f - j * other) for j in range(1, 16)) > 18:
            total = total + demod(y, k * f * ratio, rate)
    return total

def crossings(y, notes, ratio, rate):
    """Times (s) at which note k+1's level first exceeds note k's, near each change."""
    out = []
    for k, at in enumerate(LEGATO_AT):
        old = note_level(y, notes[k], notes[k + 1], ratio, rate)
        new = note_level(y, notes[k + 1], notes[k], ratio, rate)
        a, b = int((ev.PRE_S + at - 0.08) * rate), int((ev.PRE_S + at + 0.15) * rate)
        if np.isscalar(old) or np.isscalar(new): out.append(None); continue
        idx = np.nonzero(new[a:b] > old[a:b])[0]
        out.append((a + idx[0]) / rate if len(idx) and idx[0] > 0 else None)
    return out

def run(probe, out, rate=48000, block=128, modes=MODES, shifts=SHIFTS):
    rows = []
    mat = ev.material(rate)
    with tempfile.TemporaryDirectory(prefix='transpose-contract-') as tmp:
        picked = {n: (mat[n][0], ev.events(n)) for n in PICKED}
        x, _, picks = rp.fixed_materials(rate)['E2_palm16']
        picked['E2_palm16'] = (x, picks)
        for name, (x, picks) in picked.items():
            lx = ev.lowpass_lr4(x, rate)
            for shift in shifts:
                for mode, label in modes:
                    y, latency = ev.render(probe, rate, block, mode, shift, x, tmp)
                    reported = 1000 * latency / rate if latency >= 0 else REFERENCE_MS
                    ly = ev.lowpass_lr4(y, rate)
                    for p in picks:
                        d = ev.onset_latency_ms(lx, ly, rate, [p])
                        rows.append({'kind': 'picked', 'material': name, 'shift_semitones': shift, 'mode': label,
                                     'event_s': round(p, 3), 'reported_ms': round(reported, 3),
                                     'arrival_ms': d, 'error_ms': None if d is None else round(d - reported, 2)})
        for name, f0, steps in [('E2_legato_up', 82.41, [2, 1, 2]), ('E2_legato_down', 82.41, [-1, -2, -2]),
                                ('A2_legato_up', 110.0, [2, 2, 1])]:
            for hammer in (False, True):
                x, notes = legato(f0, steps, rate, hammer)
                cin = crossings(x, notes, 1.0, rate)
                for shift in shifts:
                    ratio = 2 ** (shift / 12)
                    for mode, label in modes:
                        y, latency = ev.render(probe, rate, block, mode, shift, x, tmp)
                        reported = 1000 * latency / rate if latency >= 0 else REFERENCE_MS
                        cout = crossings(y, notes, ratio, rate)
                        for at, i, o in zip(LEGATO_AT, cin, cout):
                            d = None if i is None or o is None else round(1000 * (o - i), 2)
                            rows.append({'kind': 'hammer' if hammer else 'legato', 'material': name, 'shift_semitones': shift,
                                         'mode': label, 'event_s': at, 'reported_ms': round(reported, 3),
                                         'arrival_ms': d, 'error_ms': None if d is None else round(d - reported, 2)})
    out = Path(out); out.mkdir(parents=True, exist_ok=True)
    (out / 'contract.json').write_text(json.dumps(rows, indent=1) + '\n', encoding='utf-8')
    return rows

def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--render', required=True, help='built ChimeraTransposeRender')
    ap.add_argument('--out', required=True)
    args = ap.parse_args(argv)
    rows = run(args.render, args.out)
    print(f'{len(rows)} events -> {Path(args.out) / "contract.json"}')
    return 0

if __name__ == '__main__':
    sys.exit(main())
