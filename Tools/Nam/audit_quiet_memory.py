#!/usr/bin/env python3
"""Audit unchanged absolute quiet limits against a bare A2 memory bound."""
import argparse
import json
from pathlib import Path
import numpy as np

from audit_a2_memory import receptive_field, zero_history_mask
from common import digest, write_json
from quality_profile import PROFILE, fidelity


def absolute_bounds(reference, mask):
    """Permit a separately optimal constant per window and zero error elsewhere.

    A real model has less freedom. The minimum squared error on the identical
    input subset is its target variance; the minimum maximum absolute error is
    half its target range. These are separate generous lower bounds.
    """
    reference = np.asarray(reference, dtype=np.float64)
    mask = np.asarray(mask, dtype=bool)
    if reference.ndim != 1 or mask.shape != reference.shape or not len(reference) or not np.isfinite(reference).all():
        raise ValueError('Expected finite mono window and matching mask')
    values = reference[mask]
    if not len(values):
        return 0., 0.
    return float(np.sum((values-values.mean())**2) / len(reference)), float(np.ptp(values) / 2)


def audit(evaluation, models):
    result = json.loads((evaluation / 'comparison.json').read_text())
    if result['quality_profile'] != PROFILE:
        raise RuntimeError('Changed fidelity policy')
    if digest(evaluation / 'input.npy') != result['input_sha256']:
        raise RuntimeError('Changed evaluation input')
    x = np.load(evaluation / 'input.npy')
    report = {'input_sha256': result['input_sha256'], 'comparison_sha256': digest(evaluation / 'comparison.json'),
              'quality_profile': PROFILE, 'release_approved': False,
              'scope': 'Post-selection diagnosis, not training or selection. Bare A2 at 48 kHz only; no conclusion about another architecture or a NAM-plus-recursive-filter chain.',
              'bound': 'RMS^2 >= sum((ref[Z]-mean(ref[Z]))^2)/window_size; peak >= ptp(ref[Z])/2. Independently optimal constants and zero error elsewhere are allowed.',
              'channels': {}}
    for name, scale in PROFILE['source_output_scale'].items():
        model_path = models / ('Nastrond-' + name + '.nam')
        if digest(model_path) != result['channels'][name]['candidate']['model_sha256']:
            raise RuntimeError('Different evaluated model')
        model = json.loads(model_path.read_text())
        rf = receptive_field(model)
        if rf != 6347 or model['sample_rate'] != 48000:
            raise RuntimeError('Unexpected architecture or rate')
        raw = np.load(evaluation / (name + '-native.npy'))
        pred = np.load(evaluation / (name + '-candidate.npy'))
        measured = fidelity(pred, raw, name)
        for key in ('full_esr', 'quiet_worst_residual_rms_dbfs', 'quiet_worst_residual_peak_dbfs'):
            if not np.isclose(measured[key], result['channels'][name]['candidate']['fidelity'][key], rtol=1e-8, atol=1e-8):
                raise RuntimeError('Evaluation audio changed')
        mask = zero_history_mask(x, rf)
        spread = float(np.ptp(pred[mask]))
        if spread > 1e-6:
            raise RuntimeError('Target engine did not settle after receptive field')
        ref = raw.astype(np.float64) * scale
        rows = []
        size = PROFILE['window_samples']
        for start in range(PROFILE['skip_samples'], len(x)-size+1, size):
            window, selected = ref[start:start+size], mask[start:start+size]
            level = 10*np.log10(max(np.mean(window**2), 1e-20))
            if level >= PROFILE['active_reference_rms_min_dbfs'] or selected.sum() < 2:
                continue
            mse, peak = absolute_bounds(window, selected)
            rms_db, peak_db = float(10*np.log10(max(mse, 1e-20))), float(20*np.log10(max(peak, 1e-10)))
            rows.append({'start_sample': start, 'zero_history_samples': int(selected.sum()),
                         'quiet_rms_lower_bound_dbfs': rms_db, 'quiet_peak_lower_bound_dbfs': peak_db,
                         'rms_impossible': rms_db > PROFILE['quiet_residual_rms_max_dbfs'],
                         'peak_impossible': peak_db > PROFILE['quiet_residual_peak_max_dbfs']})
        report['channels'][name] = {'rf_samples': rf, 'engine_zero_history_peak_to_peak': spread,
            'reference_sha256': digest(evaluation / (name + '-native.npy')),
            'worst_rms_witness': max(rows, key=lambda r: r['quiet_rms_lower_bound_dbfs'], default=None),
            'impossible_quiet_windows': sum(r['rms_impossible'] or r['peak_impossible'] for r in rows)}
    report['status'] = 'BLOCKED_BY_FIXED_A2_MEMORY' if any(v['impossible_quiet_windows'] for v in report['channels'].values()) else 'NO_BLOCKER_PROVEN'
    return report


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('evaluation', 'models', 'out'):
        p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    write_json(a.out, audit(a.evaluation, a.models))
