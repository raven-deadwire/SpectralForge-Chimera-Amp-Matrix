#!/usr/bin/env python3
"""Crossover-region check for the research low-band/STFT hybrid (Tools/TransposeNext).

The hybrid splits at 150 Hz (LR4). The low band's delay varies during a
sustained note (2-40 ms read-head trajectory) while the STFT band is fixed at
10 ms, so a partial near the crossover arrives in both bands with a phase
difference that changes over time. This renders one steady sine at a time
through ChimeraTransposeRender and tracks the amplitude of the shifted tone by
complex demodulation:

  level_db : median output amplitude re the input sine, dB (60 ms Hann
             demodulation: at least two periods of a 35 Hz output tone)
  fluct_db : 95th minus 5th percentile of the amplitude in dB over the sustain

Tones far from the crossover (50 Hz: low band only, 400 Hz: STFT only) are the
per-engine baselines; extra fluctuation or level loss near 150 Hz is band
interaction. Synthetic sines only: says nothing about how this sounds.

  python Tools/transpose_crossover_probe.py --render build/.../ChimeraTransposeRender --out DIR
"""
import argparse, json, sys, tempfile
from pathlib import Path
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import transpose_synthetic_evidence as ev

TONES = [50, 70, 90, 110, 130, 150, 170, 190, 220, 260, 320, 400]
SHIFTS = [-12, -5, -2, 2, 5, 12]
MODES = [(0, "CLASSIC"), (2, "TIGHT"), (12, "HYB150"), (20, "HYB150-AL10RA")]
# AL10RA band solos: S^2 = L^2 + H^2 + 2 L H cos(phi) gives the inter-band phase.
SOLO_MODES = [(21, "AL10RA-LOW"), (22, "AL10RA-HIGH")]
# Same with a steeper LR8 split (narrower region where both bands are comparable).
LR8_MODES = [(23, "AL10RA-LR8"), (24, "AL10RA-LR8-LOW"), (25, "AL10RA-LR8-HIGH")]
PRE_S, TONE_S, SKIP_S, AMP = 0.5, 2.5, 0.3, 0.5

def tone(f, rate):
    t = np.arange(int(rate * TONE_S)) / rate
    x = AMP * np.sin(2 * np.pi * f * t) * np.minimum(1.0, t / 0.005)
    return np.concatenate([np.zeros(int(PRE_S * rate)), x])

def amplitude(y, f, rate, window_s=0.06):
    """Amplitude of the component at f: Hann-weighted complex demodulation.
    A rectangular 20 ms average leaked the 2f term below ~100 Hz output."""
    t = np.arange(len(y)) / rate
    z = y * np.exp(-2j * np.pi * f * t)
    w = np.hanning(int(window_s * rate)); w /= w.sum()
    return 2 * np.abs(np.convolve(z, w, mode='valid'))

def measure(y, f_out, rate):
    a = amplitude(np.asarray(y, dtype=float), f_out, rate)
    a = a[int((PRE_S + SKIP_S) * rate):int((PRE_S + TONE_S) * rate) - int(0.06 * rate)]
    db = 20 * np.log10(np.maximum(a, 1e-9) / AMP)
    return round(float(np.median(db)), 2), round(float(np.percentile(db, 95) - np.percentile(db, 5)), 2)

def run(probe, out, rate=48000, block=128, modes=MODES + SOLO_MODES + LR8_MODES):
    rows = []
    with tempfile.TemporaryDirectory(prefix='transpose-crossover-') as tmp:
        for f in TONES:
            x = tone(f, rate)
            for shift in SHIFTS:
                f_out = f * 2 ** (shift / 12)
                levels = {}
                for mode, label in modes:
                    y, _ = ev.render(probe, rate, block, mode, shift, x, tmp)
                    level, fluct = measure(y, f_out, rate); levels[label] = level
                    rows.append({'rate': rate, 'tone_hz': f, 'shift_semitones': shift, 'mode': label,
                                 'level_db': level, 'fluct_db': fluct})
                for both in ('HYB150-AL10RA', 'AL10RA-LR8'):
                    low, high = (both if both != 'HYB150-AL10RA' else 'AL10RA') + '-LOW', (both if both != 'HYB150-AL10RA' else 'AL10RA') + '-HIGH'
                    if not all(k in levels for k in (both, low, high)): continue
                    s, l, h = (10 ** (levels[k] / 20) for k in (both, low, high))
                    cos = np.clip((s * s - l * l - h * h) / (2 * l * h), -1, 1) if l * h > 1e-6 else None
                    row = next(r for r in rows[::-1] if r['mode'] == both)
                    row['band_phase_deg'] = round(float(np.degrees(np.arccos(cos))), 1) if cos is not None else None
                    row['power_sum_db'] = round(float(10 * np.log10(l * l + h * h)), 2)
    out = Path(out); out.mkdir(parents=True, exist_ok=True)
    (out / 'crossover.json').write_text(json.dumps(rows, indent=1) + '\n', encoding='utf-8')
    return rows

def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--render', required=True, help='built ChimeraTransposeRender')
    ap.add_argument('--out', required=True)
    ap.add_argument('--rate', type=int, default=48000)
    args = ap.parse_args(argv)
    rows = run(args.render, args.out, rate=args.rate)
    print(f'{len(rows)} measurements -> {Path(args.out) / "crossover.json"}')
    return 0

if __name__ == '__main__':
    sys.exit(main())
