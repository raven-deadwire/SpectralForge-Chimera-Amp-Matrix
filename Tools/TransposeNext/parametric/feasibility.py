#!/usr/bin/env python3
"""E1 feasibility gate: how short can a window be and still find a low pitch?

Splice/read-head transposers average >= min + T/2 added delay on sustained
low notes (the delay sawtooth must jump by a whole period). An analysis/
resynthesis transposer escapes that only if it can estimate the pitch of a
new low note faster than ~T. This measures, per window length, the f0 error
of two estimators that are NOT bound by FFT bin resolution:

- esprit: least-squares ESPRIT with the oracle model order (2 x partials).
- harmonic: one-parameter harmonic least squares with a BIC order penalty.

Inputs: all partials below 350 Hz (the split band), random 1/k-weighted
amplitudes and phases, white noise at 60/40/20 dB SNR, inharmonicity 0 and
2e-4. Synthetic only, seeded, deterministic. A cell passes only if >= 90% of
trials are within 5 cents; octave/sub-harmonic errors count as errors.
"""
import argparse, json
from pathlib import Path
import numpy as np
from scipy.optimize import minimize_scalar

CUTOFF = 350.

def tone(rng, f0, fs, n, snr_db, inharm):
    ks = [k for k in range(1, 60) if k*f0*np.sqrt(1+inharm*k*k) < CUTOFF]
    amps = rng.uniform(.2, 1, len(ks))/np.arange(1, len(ks)+1)
    phases = rng.uniform(0, 2*np.pi, len(ks))
    t = np.arange(n)/fs
    x = sum(a*np.cos(2*np.pi*k*f0*np.sqrt(1+inharm*k*k)*t+p) for k, a, p in zip(ks, amps, phases))
    return x + rng.normal(0, np.std(x)*10**(-snr_db/20), n), len(ks)

def esprit_f0(x, fs, partials):
    order = 2*partials
    cols = len(x)//2
    if cols <= order:
        return None  # too few samples for the model at all
    H = np.lib.stride_tricks.sliding_window_view(x, cols)
    U = np.linalg.svd(H, full_matrices=False)[0][:, :order]
    poles = np.linalg.eigvals(np.linalg.lstsq(U[:-1], U[1:], rcond=None)[0])
    f = np.sort(np.angle(poles[np.imag(poles) > 1e-9])*fs/(2*np.pi))
    return float(f[0]) if len(f) else None  # lowest component as f0

def harmonic_f0(x, fs, lo=20., hi=130.):
    n = len(x); t = np.arange(n)/fs
    def bic(f0):
        k = np.arange(1, int(CUTOFF//f0)+1)
        B = np.hstack([np.cos(2*np.pi*np.outer(t, k*f0)), np.sin(2*np.pi*np.outer(t, k*f0)), np.ones((n, 1))])
        if B.shape[1] >= n:
            return np.inf
        c = np.linalg.lstsq(B, x, rcond=None)[0]
        return n*np.log(np.sum((x-B@c)**2)/n+1e-30)+B.shape[1]*np.log(n)
    grid = np.exp(np.linspace(np.log(lo), np.log(hi), 600))
    i = int(np.argmin([bic(f) for f in grid]))
    return float(minimize_scalar(bic, bounds=(grid[max(0, i-1)], grid[min(len(grid)-1, i+1)]), method='bounded').x)

def main():
    p = argparse.ArgumentParser(); p.add_argument('--out', type=Path, required=True)
    p.add_argument('--trials', type=int, default=12); a = p.parse_args()
    rng = np.random.default_rng(20261023); rows = []
    for estimator in ['esprit', 'harmonic']:
        fs = 4000 if estimator == 'esprit' else 2000
        for f0 in [27.5, 30.8677063285, 55.0]:
            for snr in [60, 40, 20]:
                for inharm in [0., 2e-4]:
                    for window in [4, 8, 12, 16, 24, 32, 48]:
                        n = int(round(window*1e-3*fs)); errors = []
                        for _ in range(a.trials):
                            x, partials = tone(rng, f0, fs, n, snr, inharm)
                            est = esprit_f0(x, fs, partials) if estimator == 'esprit' else harmonic_f0(x, fs)
                            errors.append(np.inf if est is None or est <= 0 else abs(1200*np.log2(est/f0)))
                        e = np.array(errors)
                        row = {'estimator': estimator, 'fs': fs, 'f0': f0, 'snr_db': snr, 'inharmonicity': inharm,
                               'window_ms': window, 'periods': window*1e-3*f0, 'trials': a.trials,
                               'abs_error_cents_median': float(np.median(e)), 'within_5_cents': float(np.mean(e < 5)),
                               'pass': bool(np.mean(e < 5) >= .9)}
                        rows.append(row); print(json.dumps(row), flush=True)
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(json.dumps({'status': 'SYNTHETIC_FEASIBILITY_ONLY', 'seed': 20261023, 'rows': rows}, indent=1)+'\n')

if __name__ == '__main__':
    main()
