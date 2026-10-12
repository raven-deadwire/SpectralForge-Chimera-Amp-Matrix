#!/usr/bin/env python3
"""Screen a legal four-bell POST factorization of the frozen BLOOM network.

Fit inputs: frozen source coefficients and existing TRAIN native PSD only.
Each channel has one deterministic fit (no restarts or validation selection).
Freeze rounded float32 coefficients/settings, then evaluate validation once.
This does not train/export a NAM. Deembedded/oracle outputs are diagnostics and
must never be represented as a playable model or a fidelity approval.
"""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from scipy import optimize, signal

from memory_source_poles import FS, NAMES, RF, MIDPOINTS, response


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def rbj_sos(freq, gain, q, rounded=False):
    if rounded:
        # BlockEq::setBandFromVar stores parameters in float before designing.
        freq, gain, q = map(float, np.float32([freq, gain, q]))
        if abs(gain) < float(np.float32(.05)):
            return np.array([1., 0., 0., 1., 0., 0.])
    w = 2 * np.pi * freq / FS
    amp = 10 ** (gain / 40)
    alpha = np.sin(w) / (2 * q)
    c = np.cos(w)
    raw = np.array([1 + alpha * amp, -2 * c, 1 - alpha * amp,
                    1 + alpha / amp, -2 * c, 1 - alpha / amp])
    raw /= raw[3]
    return raw.astype(np.float32).astype(np.float64) if rounded else raw


def source_bloom(freqs, bloom):
    """Exact frozen 4x-rate stationary x + .25*BLOOM*resonator(low180(x))."""
    source_fs = FS * 4
    hz, tau = 95 + 40 * bloom, .004 + .04 * bloom
    radius = np.exp(-1 / (source_fs * tau))
    omega = 2 * np.pi * hz / source_fs
    feed = 2 * (1 - radius) * np.sin(omega)
    lp = -np.expm1(-2 * np.pi * 180 / source_fs)
    z = np.exp(-2j * np.pi * freqs / source_fs)
    low = lp / (1 - (1 - lp) * z)
    resonator = feed / (1 - 2 * radius * np.cos(omega) * z + radius ** 2 * z ** 2)
    return 1 + .25 * bloom * resonator * low


def response_sos(sos, freqs):
    z = np.exp(-2j * np.pi * freqs / FS)
    out = np.ones(len(freqs), dtype=np.complex128)
    for b0, b1, b2, _, a1, a2 in sos:
        out *= (b0 + b1 * z + b2 * z ** 2) / (1 + a1 * z + a2 * z ** 2)
    return out


def decode(params):
    return [(float(np.exp(params[i])), float(params[i + 1]), float(np.exp(params[i + 2])))
            for i in range(0, 12, 3)]


def sos_for(params, rounded=False):
    return np.array([rbj_sos(*band, rounded=rounded) for band in decode(params)])


def load_eq_sos(job):
    """Load this experiment's frozen legal four-bell native POST topology.

    Parameters and coefficients match BlockEq's float casts, activity threshold
    and RBJ equations. scipy.sosfilt still accumulates state in float64; verify
    that arithmetic difference using the separately built native BlockEq tool.
    This intentionally rejects arbitrary topologies instead of pretending that
    they match the verified source. Continuous filtering callers must carry zi.
    """
    if isinstance(job, (str, Path)):
        job = json.loads(Path(job).read_text())
    eq = job['eq']
    if eq.get('pre') is not False or eq.get('enabled') is not True:
        raise ValueError('Expected enabled POST EQ')
    bands = eq['bands']
    if len(bands) != 6:
        raise ValueError('Exactly six bands required')
    sos = []
    for index, band in enumerate(bands):
        freq, gain, q = float(band['freqHz']), float(band['gainDb']), float(band['q'])
        if not (20 <= freq <= 20000 and -15 <= gain <= 15 and .1 <= q <= 10):
            raise ValueError('Out-of-contract EQ parameter')
        if index in (0, 5):
            expected = 'lowshelf' if index == 0 else 'highshelf'
            if band['type'] != expected or gain != 0:
                raise ValueError('Experiment requires neutral edge shelves')
        else:
            if band['type'] != 'bell':
                raise ValueError('Middle bands must be bell')
            sos.append(rbj_sos(freq, gain, q, rounded=True))
    return np.array(sos, dtype=np.float64)


def band_job(bands):
    # BlockEq defaults are retained for disabled edge shelves/cuts (gain zero).
    return {'normalize': False, 'eq': {'enabled': True, 'pre': False, 'bands': [
        {'type': 'lowshelf', 'freqHz': 80., 'gainDb': 0., 'q': .707},
        *[{'type': 'bell', 'freqHz': f, 'gainDb': g, 'q': q} for f, g, q in bands],
        {'type': 'highshelf', 'freqHz': 8000., 'gainDb': 0., 'q': .707}
    ]}}


def zero_mask(x):
    indices = np.arange(len(x))
    last_nonzero = np.maximum.accumulate(np.where(x != 0, indices, -1))
    return indices - last_nonzero >= RF


def db_rms(x):
    return float(10 * np.log10(max(float(np.mean(np.asarray(x) ** 2)), 1e-30)))


def diagnostic(x, y, sos, scale, profile):
    ref = y.astype(np.float64) * scale
    inverse = sos.copy()
    inverse[:, :3], inverse[:, 3:] = sos[:, 3:].copy(), sos[:, :3].copy()
    inverse /= inverse[:, 3, None]
    if any(np.max(np.abs(np.roots(row[3:]))) >= 1 for row in inverse):
        raise RuntimeError('Noncausal/unstable inverse')
    pretarget = signal.sosfilt(inverse, ref)
    reconstruction = signal.sosfilt(sos, pretarget)
    mask = zero_mask(x)
    limited = pretarget.copy()
    limited[mask] = 0.
    oracle = signal.sosfilt(sos, limited)
    residual = oracle - ref
    size = profile['window_samples']
    rows = []
    for start in range(profile['skip_samples'], len(x) - size + 1, size):
        rr = ref[start:start + size]
        if db_rms(rr) >= profile['active_reference_rms_min_dbfs']:
            continue
        ee = residual[start:start + size]
        mm = mask[start:start + size]
        before = pretarget[start:start + size][mm]
        raw = rr[mm]
        rows.append({'start_sample': start, 'zero_history_samples': int(mm.sum()),
                     'oracle_residual_rms_dbfs': db_rms(ee),
                     'oracle_residual_peak_dbfs': float(20 * np.log10(max(np.max(np.abs(ee)), 1e-15))),
                     'pretarget_variance_bound_dbfs': float(10 * np.log10(max(np.sum((before - before.mean()) ** 2) / size, 1e-30))) if len(before) else -300.,
                     'original_variance_bound_dbfs': float(10 * np.log10(max(np.sum((raw - raw.mean()) ** 2) / size, 1e-30))) if len(raw) else -300.})
    last = int(np.flatnonzero(x != 0)[-1]) + 1
    tail = slice(last + RF, None)
    return {
        'inverse_forward_rms_dbfs': db_rms(reconstruction - ref),
        'original_late_tail_rms_dbfs': db_rms(ref[tail]),
        'pretarget_late_tail_rms_dbfs': db_rms(pretarget[tail]),
        'late_tail_energy_reduction_db': db_rms(pretarget[tail]) - db_rms(ref[tail]),
        'late_tail_oracle_residual_rms_dbfs': db_rms(residual[tail]),
        'oracle_full_esr': float(np.mean(residual ** 2) / max(np.mean(ref ** 2), 1e-30)),
        'oracle_quiet_worst_residual_rms_dbfs': max((r['oracle_residual_rms_dbfs'] for r in rows), default=-300.),
        'oracle_quiet_worst_residual_peak_dbfs': max((r['oracle_residual_peak_dbfs'] for r in rows), default=-300.),
        'quiet_windows': rows,
        'scope': 'Inverse-target and zero-history truncation oracle only. Final profile applies to a real NAM+EQ chain; these values are not an accuracy/compatibility pass.'
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--data', type=Path, required=True)
    ap.add_argument('--quality-profile', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    a = ap.parse_args()
    a.out.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((a.data / 'manifest.json').read_text())
    if manifest['source']['source_commit'] != 'd93560445c78552768a0ed592e5ec62c3c648fb6':
        raise RuntimeError('Unexpected source')
    profile = json.loads(a.quality_profile.read_text())
    settings = json.loads((a.data / 'channel-settings.json').read_text())
    provenance = {(r['split'], r['name']): r for r in manifest['captures']}
    frozen = {}
    report = {'source_commit': manifest['source']['source_commit'], 'release_approved': False,
              'fit_split': 'train', 'validation_used_for_fit_or_selection': False,
              'independent_test_accessed': False, 'quality_profile_sha256': digest(a.quality_profile),
              'topology': 'four legal native POST bells in bands1-4; edge shelves neutral',
        'objective': 'complex source BLOOM response; 50% fixed-grid uniform and50% TRAIN native PSD; one deterministic fit per channel,600 evaluations max',
              'scope': 'Linear factorization feasibility only; no trained NAM and no release approval', 'channels': {}}
    train_x = np.load(a.data / 'train/input.npy')
    report['train_input_sha256'] = digest(a.data / 'train/input.npy')
    for i, name in enumerate(NAMES):
        p = a.data / 'train' / (name + '.npy')
        if digest(p) != provenance[('train', name)]['output_sha256']:
            raise RuntimeError('TRAIN native hash mismatch')
        y = np.load(p)
        bloom = response(float(np.float32(settings[i]['controls']['bloom'])),
                         float(np.float32(MIDPOINTS[i][1])), 1.5)
        resonance = 95 + 40 * bloom
        # Dense fixed source-centred frequencies plus broad-band guard points.
        frequencies = np.unique(np.r_[np.geomspace(20, 20000, 256),
                                      np.linspace(resonance * .70, resonance * 1.30, 769)])
        target = source_bloom(frequencies, bloom)
        psd_f, psd = signal.welch(y, fs=FS, nperseg=65536)
        powers = np.maximum(np.interp(frequencies, psd_f, psd), 1e-20)
        weights = np.sqrt(.5 + .5 * powers / powers.mean())
        initial = []
        for ratio, gain, q in [(.96, 3., 6.), (1.04, -3., 6.), (.99, 1., 9.), (1.01, -1., 9.)]:
            initial.extend([np.log(resonance * ratio), gain, np.log(q)])
        bounds = ([v for _ in range(4) for v in [np.log(max(20, resonance * .70)), -15., np.log(.1)]],
                  [v for _ in range(4) for v in [np.log(min(20000, resonance * 1.30)), 15., np.log(10.)]])
        def objective(params):
            error = (response_sos(sos_for(params), frequencies) / target - 1) * weights
            return np.r_[error.real, error.imag]
        fit = optimize.least_squares(objective, initial, bounds=bounds, max_nfev=600,
                                     ftol=1e-10, xtol=1e-10, gtol=1e-10)
        bands = decode(fit.x)
        sos = sos_for(fit.x, rounded=True)
        frozen[name] = sos
        job = band_job(bands)
        job_path = a.out / (name + '-factor-post-eq.json')
        job_path.write_text(json.dumps(job, indent=2) + '\n')
        fit_error = response_sos(sos, frequencies) / target - 1
        report['channels'][name] = {
            'train_native_sha256': digest(p), 'bloom_effective': bloom,
            'source_resonance_hz': resonance, 'solver_success': bool(fit.success),
            'solver_status': int(fit.status), 'solver_message': fit.message,
            'function_evaluations': int(fit.nfev), 'job_sha256': digest(job_path),
            'bands': [{'frequency_hz': f, 'gain_db': g, 'q': q} for f, g, q in bands],
            'rounded_sos': sos.tolist(),
            'weighted_complex_relative_rms_error': float(np.sqrt(np.mean(abs(fit_error * weights) ** 2))),
            'complex_relative_max_error': float(np.max(abs(fit_error))),
            'train': diagnostic(train_x, y, sos, profile['source_output_scale'][name], profile)
        }
        print(name, 'fit evaluations', fit.nfev, 'complex RMS', report['channels'][name]['weighted_complex_relative_rms_error'], flush=True)
    # Freeze all settings and TRAIN evidence before touching validation outputs.
    freeze_path = a.out / 'training-freeze.json'
    freeze_path.write_text(json.dumps(report, indent=2) + '\n')
    report['training_freeze_sha256'] = digest(freeze_path)
    validation_x = np.load(a.data / 'validation/input.npy')
    report['validation_input_sha256'] = digest(a.data / 'validation/input.npy')
    for name in NAMES:
        p = a.data / 'validation' / (name + '.npy')
        if digest(p) != provenance[('validation', name)]['output_sha256']:
            raise RuntimeError('Validation native hash mismatch')
        report['channels'][name]['validation_native_sha256'] = digest(p)
        report['channels'][name]['validation'] = diagnostic(validation_x, np.load(p), frozen[name],
                                                          profile['source_output_scale'][name], profile)
    (a.out / 'memory-eq-factorization.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Frozen settings + validation diagnosis:', a.out, flush=True)


if __name__ == '__main__':
    main()
