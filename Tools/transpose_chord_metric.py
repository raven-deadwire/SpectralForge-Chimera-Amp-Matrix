#!/usr/bin/env python3
"""Chord integrity of transposed synthetic material without a fundamental estimate.

The pitch metric in transpose_measure.py estimates one fundamental, which is
ambiguous for chords (a power chord reads 700 cents off even through CLASSIC
and TIGHT). Here the input is built from known notes, so every input partial
(harmonics 1-12 of each note) is known; after a shift by `ratio` each must
appear at partial * ratio. On the sustain (0.6-2.0 s after the onset, 1.4 s
Hann window, zero-padded to ~0.18 Hz bins), between 20 Hz and 2 kHz:

  spurious_db  : energy farther than max(30 cents, 2 Hz) from every expected
                 output partial, re the total, dB (lower is cleaner)
  partial_dev  : per expected partial, output level minus the input partial's
                 level (both sustain-window peak magnitudes, after a common
                 gain match), dB; reported as the median and the 90th
                 percentile of |dev| over partials with an input level within
                 30 dB of the loudest
  mistuned     : partials whose loudest nearby output peak (within 1 semitone)
                 is more than 30 cents away from the expected frequency and
                 not within 30 cents of another expected partial (a major
                 third's 5th and 4th harmonics are 14 cents apart)

Synthetic plucks only; says nothing about how chords sound.

  python Tools/transpose_chord_metric.py --render build/.../ChimeraTransposeRender --out DIR
"""
import argparse, json, sys, tempfile
from pathlib import Path
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import transpose_synthetic_evidence as ev

CHORDS = {
    'E2_power':   [82.41, 123.47, 164.81],
    'E2_major':   [82.41, 103.83, 123.47],
    'D2_power':   [73.42, 110.00, 146.83],
    'B0_power':   [30.87, 46.25, 61.74],
    'E2_single':  [82.41],
    'B0_single':  [30.87],
    # Higher register, where the upper band is the STFT at a low crossover.
    'A3_power':   [220.00, 329.63, 440.00],
    'C4_major':   [261.63, 329.63, 392.00],
    'E_open':     [82.41, 123.47, 164.81, 207.65, 246.94, 329.63],
    'E4_single':  [329.63],
}
SHIFTS = [-12, -7, -5, -2, 2, 5, 7, 12]
MODES = [(0, "CLASSIC"), (2, "TIGHT"), (10, "SPLIT-UP"), (20, "AL10RA-150"), (32, "AL10RA-200"),
         (33, "AL10RA-250"), (34, "AL10RA-300"), (35, "AL10RA-400"), (36, "AL10RA-600"),
         (37, "AL10RA-800"), (38, "AL10RA-1200")]
HARMONICS, BAND = 12, (20.0, 2000.0)
SUSTAIN = (0.6, 2.0)

def chord(notes, rate):
    x = sum(ev.pluck(f, rate, harmonics=HARMONICS) for f in notes)
    x = np.concatenate([np.zeros(int(ev.PRE_S * rate)), x])
    return 0.5 * x / np.max(np.abs(x))

def partials(notes, rate):
    return sorted({k * f for f in notes for k in range(1, HARMONICS + 1) if k * f < 0.45 * rate})

def spectrum(y, rate):
    a, b = int((ev.PRE_S + SUSTAIN[0]) * rate), int((ev.PRE_S + SUSTAIN[1]) * rate)
    seg = np.asarray(y[a:b], float) * np.hanning(b - a)
    n = 1 << int(np.ceil(np.log2(len(seg) * 8)))
    return np.fft.rfftfreq(n, 1 / rate), np.abs(np.fft.rfft(seg, n))

def peak_near(f, mag, hz, cents):
    lo, hi = hz * 2 ** (-cents / 1200), hz * 2 ** (cents / 1200)
    idx = np.nonzero((f >= lo) & (f <= hi))[0]
    if len(idx) == 0: return None, 0.0
    k = idx[np.argmax(mag[idx])]
    return f[k], mag[k]

def evaluate(x, y, notes, ratio, rate, input_partials=None):
    """input_partials: the exact input partial frequencies, when they are not
    harmonics 1-12 of `notes` (e.g. an inharmonic string)."""
    fi, mi = spectrum(x, rate); fo, mo = spectrum(y, rate)
    source = input_partials if input_partials is not None else partials(notes, rate)
    expected_in = [p for p in source if BAND[0] <= p * ratio <= BAND[1] and BAND[0] <= p <= BAND[1]]
    expected_out = [p * ratio for p in expected_in]
    band = (fo >= BAND[0]) & (fo <= BAND[1])
    near = np.zeros_like(fo, dtype=bool)
    for p in expected_out:
        width = max(p * (2 ** (30 / 1200) - 1), 2.0)
        near |= np.abs(fo - p) <= width
    total = np.sum(mo[band] ** 2)
    spurious = np.sum(mo[band & ~near] ** 2)
    levels_in = np.array([peak_near(fi, mi, p, 10)[1] for p in expected_in])
    levels_out = np.array([peak_near(fo, mo, p, 30)[1] for p in expected_out])
    keep = levels_in >= levels_in.max() * 10 ** (-30 / 20)
    gain = np.sqrt(np.sum(levels_out[keep] ** 2) / max(np.sum(levels_in[keep] ** 2), 1e-30))
    dev = 20 * np.log10(np.maximum(levels_out[keep], 1e-12) / np.maximum(gain * levels_in[keep], 1e-12))
    mistuned = 0
    for p, k in zip(expected_out, keep):
        if not k: continue
        fpk, _ = peak_near(fo, mo, p, 100)
        if fpk is None: mistuned += 1; continue
        if min(abs(1200 * np.log2(fpk / q)) for q in expected_out) > 30: mistuned += 1
    return {'spurious_db': round(float(10 * np.log10(max(spurious, 1e-30) / max(total, 1e-30))), 2),
            'partial_dev_median_db': round(float(np.median(np.abs(dev))), 2),
            'partial_dev_p90_db': round(float(np.percentile(np.abs(dev), 90)), 2),
            'partials': int(np.sum(keep)), 'mistuned': mistuned}

def run(probe, out, rate=48000, block=128, modes=MODES, shifts=SHIFTS):
    rows = []
    with tempfile.TemporaryDirectory(prefix='transpose-chord-') as tmp:
        for name, notes in CHORDS.items():
            x = chord(notes, rate)
            for shift in shifts:
                for mode, label in modes:
                    y, _ = ev.render(probe, rate, block, mode, shift, x, tmp)
                    row = {'rate': rate, 'material': name, 'shift_semitones': shift, 'mode': label}
                    row.update(evaluate(x, y, notes, 2 ** (shift / 12), rate))
                    rows.append(row)
    out = Path(out); out.mkdir(parents=True, exist_ok=True)
    (out / 'chord.json').write_text(json.dumps(rows, indent=1) + '\n', encoding='utf-8')
    return rows

def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--render', required=True, help='built ChimeraTransposeRender')
    ap.add_argument('--out', required=True)
    args = ap.parse_args(argv)
    rows = run(args.render, args.out)
    print(f'{len(rows)} rows -> {Path(args.out) / "chord.json"}')
    return 0

if __name__ == '__main__':
    sys.exit(main())
