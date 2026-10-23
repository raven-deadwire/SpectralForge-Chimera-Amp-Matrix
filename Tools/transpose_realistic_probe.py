#!/usr/bin/env python3
"""Harder synthetic material for the research Transpose engines.

The plucks in transpose_synthetic_evidence.py are exactly harmonic and steady,
which favours a time-domain read head. This adds, still synthetic:

  inharmonic strings   partial k at k f0 sqrt(1 + B k^2) (B0 bass B = 3e-4,
                       guitar B = 1e-4), with 3 ms of pick noise
  detuned chords       an inharmonic E2 power chord and open E chord with each
                       note detuned by a few cents
  palm-muted 16ths     E2, ~80 ms decay, a new pick every 125 ms
  vibrato / bend       E2 and A3 with +-20 cents at 5.5 Hz; E2 bent +200 cents
                       over 300 ms and held

Fixed-partial material is scored with transpose_chord_metric.evaluate on the
exact input partials (spurious energy, mistuned partials). Moving-pitch
material is scored by a YIN pitch track of the output (100 ms frames, 5 ms
hop) against the known input pitch x ratio, at the best lag in 0-45 ms:
median and 95th percentile |cents|, and the share of frames off by > 50 cents.
The palm-muted line also reports the per-pick low-band attack delay.

Synthetic only; DI recordings and listening are still required.

  python Tools/transpose_realistic_probe.py --render build/.../ChimeraTransposeRender --out DIR
"""
import argparse, json, sys, tempfile
from pathlib import Path
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import transpose_synthetic_evidence as ev
import transpose_chord_metric as cm

SHIFTS = [-12, -7, -5, -2, 2, 5, 7, 12]
MODES = [(0, "CLASSIC"), (2, "TIGHT"), (10, "SPLIT-UP"), (20, "AL10RA-150"), (36, "AL10RA-600"), (37, "AL10RA-800")]
HARMONICS, LENGTH_S = 12, 2.5

def string(f0, rate, B, seconds=LENGTH_S, decay=1.0, cents=None, seed=1):
    """Inharmonic pluck. cents: optional pitch offset track (cents, per sample)."""
    t = np.arange(int(rate * seconds)) / rate
    bend = 2 ** ((cents if cents is not None else np.zeros_like(t)) / 1200)
    x, freqs = np.zeros_like(t), []
    for k in range(1, HARMONICS + 1):
        fk = k * f0 * np.sqrt(1 + B * k * k)
        if fk * bend.max() >= 0.45 * rate: break
        freqs.append(fk)
        phase = 2 * np.pi * np.cumsum(fk * bend) / rate
        x += (1.0 / k) * np.sin(phase) * np.exp(-t / (decay * 1.2 / (1 + 0.35 * k)))
    x *= np.minimum(1.0, t / 0.002)
    noise = np.random.default_rng(seed).standard_normal(int(0.003 * rate))
    noise = np.diff(noise, prepend=0) * np.hanning(len(noise))
    x[:len(noise)] += 0.3 * np.max(np.abs(x)) * noise / np.max(np.abs(noise))
    return x, freqs

def lead_in(x):
    return np.concatenate([np.zeros(int(ev.PRE_S * RATE)), x])

def normalise(x):
    return 0.5 * x / np.max(np.abs(x))

RATE = 48000

def fixed_materials(rate):
    """name -> (signal, exact input partials, pick times or None)."""
    out = {}
    for name, f0, B in [('B0_inharmonic', 30.87, 3e-4), ('E2_inharmonic', 82.41, 1e-4)]:
        x, p = string(f0, rate, B); out[name] = (normalise(lead_in(x)), p, None)
    for name, notes, detune in [('E2_power_detuned', [82.41, 123.47, 164.81], [0, 5, -4]),
                                ('E_open_detuned', [82.41, 123.47, 164.81, 207.65, 246.94, 329.63], [0, 4, -3, 5, -2, 3])]:
        x, parts = 0, []
        for i, (f, c) in enumerate(zip(notes, detune)):
            y, p = string(f * 2 ** (c / 1200), rate, 1e-4, seed=i + 2); x = x + y; parts += p
        out[name] = (normalise(lead_in(x)), sorted(parts), None)
    # Palm-muted sixteenths: a short note every 125 ms, each cut by the next.
    step, x, parts = 0.125, np.zeros(int(rate * LENGTH_S)), None
    for i in range(int(LENGTH_S / step)):
        y, parts = string(82.41, rate, 1e-4, seconds=step, decay=0.07, seed=10 + i)
        s = int(i * step * rate); x[s:s + len(y)] = y[:len(x) - s]
    out['E2_palm16'] = (normalise(lead_in(x)), parts, [ev.PRE_S + step * i for i in range(int(LENGTH_S / step))])
    return out

def moving_materials(rate):
    """name -> (signal, input pitch track in Hz per sample)."""
    t = np.arange(int(rate * LENGTH_S)) / rate
    vib = np.where(t > 0.3, 20 * np.sin(2 * np.pi * 5.5 * (t - 0.3)), 0.0)
    bend = 200 * np.clip((t - 0.4) / 0.3, 0, 1)
    out = {}
    for name, f0, cents in [('E2_vibrato', 82.41, vib), ('A3_vibrato', 220.0, vib), ('E2_bend', 82.41, bend)]:
        x, _ = string(f0, rate, 1e-4, cents=cents)
        track = np.concatenate([np.zeros(int(ev.PRE_S * rate)), f0 * 2 ** (cents / 1200)])
        out[name] = (normalise(lead_in(x)), track)
    return out

def yin(y, rate, frame_s=0.1, hop_s=0.005, fmin=30.0, fmax=1000.0, threshold=0.15):
    """Frame centres (s) and f0 (Hz, nan when unvoiced)."""
    n, h = int(frame_s * rate), int(hop_s * rate)
    lo, hi = int(rate / fmax), int(rate / fmin)
    centres, f0 = [], []
    for s in range(0, len(y) - n - hi, h):
        a = np.asarray(y[s:s + n + hi], float)
        if np.max(np.abs(a[:n])) < 1e-4: centres.append((s + n / 2) / rate); f0.append(np.nan); continue
        x = a[:n]
        energy = np.cumsum(a * a)
        e0 = energy[n - 1]
        # d(tau) = sum (x_j - x_{j+tau})^2 over the frame, via FFT correlation.
        m = 1 << int(np.ceil(np.log2(2 * (n + hi))))
        corr = np.fft.irfft(np.fft.rfft(a, m) * np.conj(np.fft.rfft(x, m)), m)[:hi + 1]
        et = np.array([energy[tau + n - 1] - (energy[tau - 1] if tau > 0 else 0) for tau in range(hi + 1)])
        d = e0 + et - 2 * corr
        cmnd = np.ones_like(d); taus = np.arange(1, hi + 1)
        cmnd[1:] = d[1:] * taus / np.maximum(np.cumsum(d[1:]), 1e-12)
        below = np.nonzero(cmnd[lo:hi] < threshold)[0]
        if len(below) == 0: centres.append((s + n / 2) / rate); f0.append(np.nan); continue
        tau = lo + below[0]
        while tau + 1 < hi and cmnd[tau + 1] < cmnd[tau]: tau += 1
        if 1 <= tau < hi:
            y0, y1, y2 = cmnd[tau - 1], cmnd[tau], cmnd[tau + 1]
            den = y0 - 2 * y1 + y2
            tau = tau + (0.5 * (y0 - y2) / den if den > 0 else 0)
        centres.append((s + n / 2) / rate); f0.append(rate / tau)
    return np.array(centres), np.array(f0)

def pitch_error(y, track, ratio, rate):
    t, f = yin(y, rate)
    best = None
    for lag_ms in range(0, 46, 1):
        idx = np.clip(((t - lag_ms / 1000) * rate).astype(int), 0, len(track) - 1)
        expected = track[idx] * ratio
        ok = np.isfinite(f) & (expected > 0) & (t > ev.PRE_S + 0.15) & (t < ev.PRE_S + LENGTH_S - 0.2)
        if ok.sum() < 20: continue
        err = np.abs(1200 * np.log2(f[ok] / expected[ok]))
        score = np.median(err)
        if best is None or score < best[0]: best = (score, err, lag_ms, ok.mean())
    if best is None: return {'pitch_median_cents': None, 'pitch_p95_cents': None, 'off50_share': None, 'lag_ms': None, 'voiced': 0.0}
    score, err, lag, voiced = best
    return {'pitch_median_cents': round(float(score), 2), 'pitch_p95_cents': round(float(np.percentile(err, 95)), 1),
            'off50_share': round(float(np.mean(err > 50)), 3), 'lag_ms': lag, 'voiced': round(float(voiced), 3)}

def selftest(rate=RATE):
    """Ideal transposition (resynthesised at f0 x ratio) must score ~0 cents."""
    report = {}
    for name, (x, track) in moving_materials(rate).items():
        f0 = track[int((ev.PRE_S + 0.1) * rate)]
        for shift in (-7, 5):
            r = 2 ** (shift / 12)
            t = np.arange(int(rate * LENGTH_S)) / rate
            cents = 1200 * np.log2(track[int(ev.PRE_S * rate):] / f0)
            ideal, _ = string(f0 * r, rate, 1e-4, cents=cents)
            report[f'{name}{shift:+d}'] = pitch_error(normalise(lead_in(ideal)), track, r, rate)
    return report

def run(probe, out, rate=RATE, block=128, modes=MODES, shifts=SHIFTS):
    rows = []
    with tempfile.TemporaryDirectory(prefix='transpose-realistic-') as tmp:
        low_cache = {}
        for name, (x, parts, picks) in fixed_materials(rate).items():
            for shift in shifts:
                ratio = 2 ** (shift / 12)
                for mode, label in modes:
                    y, _ = ev.render(probe, rate, block, mode, shift, x, tmp)
                    row = {'rate': rate, 'material': name, 'shift_semitones': shift, 'mode': label}
                    row.update(cm.evaluate(x, y, None, ratio, rate, input_partials=parts))
                    if picks:
                        if name not in low_cache: low_cache[name] = ev.lowpass_lr4(x, rate)
                        row['low_onset_ms'] = ev.onset_latency_ms(low_cache[name], ev.lowpass_lr4(y, rate), rate, picks)
                    rows.append(row)
        for name, (x, track) in moving_materials(rate).items():
            for shift in shifts:
                ratio = 2 ** (shift / 12)
                for mode, label in modes:
                    y, _ = ev.render(probe, rate, block, mode, shift, x, tmp)
                    row = {'rate': rate, 'material': name, 'shift_semitones': shift, 'mode': label}
                    row.update(pitch_error(y, track, ratio, rate))
                    rows.append(row)
    out = Path(out); out.mkdir(parents=True, exist_ok=True)
    (out / 'realistic.json').write_text(json.dumps(rows, indent=1) + '\n', encoding='utf-8')
    return rows

def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--render', help='built ChimeraTransposeRender')
    ap.add_argument('--out')
    ap.add_argument('--selftest', action='store_true')
    args = ap.parse_args(argv)
    if args.selftest:
        print(json.dumps(selftest(), indent=1)); return 0
    rows = run(args.render, args.out)
    print(f'{len(rows)} rows -> {Path(args.out) / "realistic.json"}')
    return 0

if __name__ == '__main__':
    sys.exit(main())
