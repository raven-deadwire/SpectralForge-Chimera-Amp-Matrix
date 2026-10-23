#!/usr/bin/env python3
"""Inter-band coherence around low-band re-anchors in the research hybrid.

At a new pick the hybrid's low band re-anchors its read head (the delay
jumps), while the phase-locked STFT band (Tools/TransposeNext/PhaseLockPitch.h)
has already synthesised up to one window of output from the old trajectory.
This renders repicked harmonic notes (a new pluck every 0.6 s, cutting the
previous one) through the band solos and measures, in the crossover region
(input 100-260 Hz, scaled by the pitch ratio at the output), per 20 ms Hann
frame (2.5 ms hop) relative to each input onset:

  rho     : Re(sum L H*) / sqrt(sum |L|^2 sum |H|^2) over the region's bins
            (+1 same phase, 0 unrelated, -1 cancelling)
  sum_db  : 10 log10(sum |L+H|^2 / (sum |L|^2 + sum |H|^2))
            (+3 dB coherent equal bands, 0 dB unrelated, < 0 cancelling)

averaged over the second and later picks. Synthetic plucks only.

  python Tools/transpose_reanchor_probe.py --render build/.../ChimeraTransposeRender --out DIR
"""
import argparse, json, sys, tempfile
from pathlib import Path
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import transpose_synthetic_evidence as ev

NOTES = {'bass_E1': 41.20, 'guitar_E2': 82.41, 'guitar_A2': 110.0}
SHIFTS = [-12, -5, -2, 2, 5, 12]
# (label, low solo mode, upper solo mode)
PAIRS = [("AL10RA", 21, 22), ("AL9RA-LOCK", 30, 31)]
REGION_HZ = (100.0, 260.0)
FRAME_S, HOP_S = 0.020, 0.0025
OFFSETS_MS = (-20, 0, 5, 10, 15, 20, 30, 40, 60, 80)

def material(f0, rate):
    x = ev.repick(f0, rate)
    x = np.concatenate([np.zeros(int(ev.PRE_S * rate)), x])
    return 0.5 * x / np.max(np.abs(x))

def frames(y, rate):
    n, h = int(FRAME_S * rate), int(HOP_S * rate)
    w = np.hanning(n)
    starts = np.arange(0, len(y) - n, h)
    return starts + n // 2, np.array([np.fft.rfft(w * y[s:s + n]) for s in starts]), n

def coherence(low, high, rate, ratio):
    centres, L, n = frames(np.asarray(low, float), rate)
    _, H, _ = frames(np.asarray(high, float), rate)
    f = np.fft.rfftfreq(n, 1 / rate)
    band = (f >= REGION_HZ[0] * ratio) & (f <= REGION_HZ[1] * ratio)
    Lb, Hb = L[:, band], H[:, band]
    el, eh = np.sum(np.abs(Lb) ** 2, 1), np.sum(np.abs(Hb) ** 2, 1)
    cross = np.real(np.sum(Lb * np.conj(Hb), 1))
    rho = cross / np.sqrt(np.maximum(el * eh, 1e-30))
    sum_db = 10 * np.log10(np.maximum(np.sum(np.abs(Lb + Hb) ** 2, 1), 1e-30) / np.maximum(el + eh, 1e-30))
    return centres / rate, rho, sum_db

def run(probe, out, rate=48000, block=128):
    rows = []
    with tempfile.TemporaryDirectory(prefix='transpose-reanchor-') as tmp:
        for name, f0 in NOTES.items():
            x = material(f0, rate)
            picks = [ev.PRE_S + ev.REPICK_S * i for i in range(1, int(ev.NOTE_S / ev.REPICK_S))]
            for shift in SHIFTS:
                ratio = 2 ** (shift / 12)
                for label, low_mode, high_mode in PAIRS:
                    low, _ = ev.render(probe, rate, block, low_mode, shift, x, tmp)
                    high, _ = ev.render(probe, rate, block, high_mode, shift, x, tmp)
                    t, rho, sum_db = coherence(low, high, rate, ratio)
                    row = {'rate': rate, 'material': name, 'shift_semitones': shift, 'mode': label}
                    for offset in OFFSETS_MS:
                        idx = [int(np.argmin(np.abs(t - (p + offset / 1000)))) for p in picks]
                        row[f'rho_{offset:+d}ms'] = round(float(np.mean(rho[idx])), 3)
                        row[f'sum_db_{offset:+d}ms'] = round(float(np.mean(sum_db[idx])), 2)
                    # Sustain reference: 150-500 ms after each pick.
                    sustain = np.concatenate([np.nonzero((t > p + 0.15) & (t < p + 0.5))[0] for p in picks])
                    row['rho_sustain'] = round(float(np.mean(rho[sustain])), 3)
                    row['sum_db_sustain'] = round(float(np.mean(sum_db[sustain])), 2)
                    rows.append(row)
    out = Path(out); out.mkdir(parents=True, exist_ok=True)
    (out / 'reanchor.json').write_text(json.dumps(rows, indent=1) + '\n', encoding='utf-8')
    return rows

def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--render', required=True, help='built ChimeraTransposeRender')
    ap.add_argument('--out', required=True)
    args = ap.parse_args(argv)
    rows = run(args.render, args.out)
    print(f'{len(rows)} rows -> {Path(args.out) / "reanchor.json"}')
    return 0

if __name__ == '__main__':
    sys.exit(main())
