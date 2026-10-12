#!/usr/bin/env python3
"""TRAIN-only attribution of native tails to source-defined stationary poles.

This is a diagnosis, not a candidate fit. The source pole frequencies/decays are
fixed from d935604; only nuisance sine/cosine/DC amplitudes are least-squares fit
to the existing TRAIN terminal decay. No model/preset is selected or modified.
"""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np

NAMES = ['Fenrir', 'Surtr', 'Nidhoggr', 'Fimbulvetr', 'Ragnarok']
MIDPOINTS = [(0.55, .65), (.70, .85), (1., 1.), (.85, 1.15), (.95, .95)]
RELEASE = [.045, .160, .090, .300, .055]
RECOVERY = [.55, 1., 2.4, 1.8, .75]
FS = 48000
RF = 6347


def digest(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def response(value, midpoint, maximum):
    return value * 2 * midpoint if value <= .5 else midpoint + (value - .5) * 2 * (maximum - midpoint)


def pole_basis(n, freq, tau):
    t = np.arange(n, dtype=np.float64) / FS
    envelope = np.exp(-t / tau)
    return np.stack([envelope * np.sin(2 * np.pi * freq * t),
                     envelope * np.cos(2 * np.pi * freq * t),
                     np.exp(-2 * np.pi * 8 * t)], axis=1)


def db_rms(x):
    return float(10 * np.log10(max(float(np.mean(x * x)), 1e-30)))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--data', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    args = ap.parse_args()
    manifest = json.loads((args.data / 'manifest.json').read_text())
    settings = json.loads((args.data / 'channel-settings.json').read_text())
    train_rows = {r['name']: r for r in manifest['captures'] if r['split'] == 'train'}
    if manifest['source']['source_commit'] != 'd93560445c78552768a0ed592e5ec62c3c648fb6':
        raise RuntimeError('Unexpected native source')
    input_path = args.data / 'train/input.npy'
    x = np.load(input_path)
    if digest(input_path) != train_rows[NAMES[0]]['input_sha256']:
        raise RuntimeError('TRAIN input hash mismatch')
    silence = int(np.flatnonzero(x != 0)[-1]) + 1
    report = {
        'source_commit': manifest['source']['source_commit'],
        'sample_rate': FS, 'source_oversampling': 4, 'rf_samples': RF,
        'scope': 'TRAIN-only post-selection source attribution; no training, preset fit or new independent-test claim',
        'input_sha256': digest(input_path), 'terminal_zero_start': silence,
        'samples': len(x), 'diagnostic_start': silence + RF,
        'release_approved': False, 'channels': {}
    }
    for i, name in enumerate(NAMES):
        p = args.data / 'train' / (name + '.npy')
        if digest(p) != train_rows[name]['output_sha256']:
            raise RuntimeError('TRAIN native hash mismatch: ' + name)
        y = np.load(p).astype(np.float64)
        controls = settings[i]['controls']
        # Frozen C++ State controls and channel-midpoint literals are float32.
        rot_mid, bloom_mid = np.float32(MIDPOINTS[i])
        rot = response(float(np.float32(controls['rot'])), float(rot_mid), 1.5)
        bloom = response(float(np.float32(controls['bloom'])), float(bloom_mid), 1.5)
        freq, tau = 95 + 40 * bloom, .004 + .04 * bloom
        observed = y[silence + RF:]
        basis = pole_basis(len(observed), freq, tau)
        coeff = np.linalg.lstsq(basis, observed, rcond=None)[0]
        prediction = basis @ coeff
        residual = observed - prediction
        resonance = basis[:, :2] @ coeff[:2]
        dc = basis[:, 2] * coeff[2]
        # Exact digital pole Q limit for RBJ bell, at same oscillator frequency.
        # Q*A is the low-frequency analog limit; exact pole is also reported.
        gain_db, q = 15., 10.
        amp = 10 ** (gain_db / 40)
        w = 2 * np.pi * freq / FS
        alpha = np.sin(w) / (2 * q)
        denominator = np.array([1 + alpha / amp, -2 * np.cos(w), 1 - alpha / amp])
        poles = np.roots(denominator)
        rbj_tau = -1 / (FS * np.log(abs(poles[0])))
        rbj_freq = abs(np.angle(poles[0])) * FS / (2 * np.pi)
        source_q = np.pi * freq * tau
        required_bell_gain = 40 * np.log10(source_q / 10)
        report['channels'][name] = {
            'native_sha256': digest(p), 'rot_effective': rot,
            'bloom_effective': bloom, 'source_resonator_hz': freq,
            'source_resonator_tau_ms': 1000 * tau,
            'source_sag_release_ms': 1000 * RELEASE[i],
            'source_blocking_recovery_ms': 1000 * (.012 + .16 * rot) * RECOVERY[i],
            'source_dc_tau_ms': 1000 / (2 * np.pi * 8),
            'diagnostic_tail_native_rms_dbfs_unscaled': db_rms(observed),
            'source_pole_fit_residual_rms_dbfs_unscaled': db_rms(residual),
            'source_pole_fit_relative_esr': float(np.mean(residual ** 2) / max(np.mean(observed ** 2), 1e-30)),
            'fitted_resonance_rms_dbfs_unscaled': db_rms(resonance),
            'fitted_dc_rms_dbfs_unscaled': db_rms(dc),
            'source_resonator_physical_q': source_q,
            'max_supported_single_bell_same_frequency': {
                'frequency_hz': freq, 'gain_db': gain_db, 'q_parameter': q,
                'pole_frequency_hz': rbj_freq, 'pole_tau_ms': float(1000 * rbj_tau),
                'analog_effective_q_limit': float(q * amp)
            },
            'approx_gain_db_required_at_q_10_for_source_decay': float(required_bell_gain),
            'single_supported_bell_can_match_source_pole': bool(required_bell_gain <= 15),
            'limitation': 'A pole-range diagnostic only. Multiple bands may approximate finite windows; no general supported-filter-chain impossibility is asserted.'
        }
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2) + '\n')
    for name, r in report['channels'].items():
        print(name, 'Hz/tau', r['source_resonator_hz'], r['source_resonator_tau_ms'],
              'pole-fit ESR', r['source_pole_fit_relative_esr'],
              'RBJ max tau', r['max_supported_single_bell_same_frequency']['pole_tau_ms'])


if __name__ == '__main__':
    main()
