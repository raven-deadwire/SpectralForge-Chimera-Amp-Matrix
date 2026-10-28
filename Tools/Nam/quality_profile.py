"""Project fidelity policy for TONE3000, not an official TONE3000 certification.

Classification and residual dBFS use a frozen per-channel level anchor for both
reference and prediction. Neither a candidate's loudness nor output pad can
move a difficult window into the quiet category.

v2 (owner decision, 2026-10-24): a sample whose whole A2 receptive field of
input is digital zero cannot be predicted by any bare A2, whose output there is
one constant (proof: ACCURACY-BLOCKER.md). Those samples are reported as
`memoryless_*` and excluded from the quiet pass/fail; every other quiet sample
keeps the unchanged -70/-60 dBFS limits, and the active, level and full-ESR
limits are unchanged. Full ESR still includes every sample.
"""
import numpy as np

# Frozen from capture data v1 (source 4ef21c16): 10**(-20/20) / validation RMS.
ANCHORS = {
    'Fenrir': 0.10206878638516259,
    'Surtr': 0.09304512903584947,
    'Nidhoggr': 0.0912341002696015,
    'Fimbulvetr': 0.09658538313914974,
    'Ragnarok': 0.06924560647333675,
}

PROFILE = {
    'id': 'nastrond-tone3000-fidelity-v2',
    'authority': 'project acceptance policy; not a TONE3000 certification',
    'window_samples': 4096,
    'skip_samples': 6347,
    'receptive_field_samples': 6347,
    'quiet_excludes': 'samples whose whole receptive field of input is zero; reported as memoryless_*',
    'active_reference_rms_min_dbfs': -60.0,
    'full_esr_max_exclusive': .01,
    'esr_guidance_source': 'https://www.tone3000.com/guides/capture-your-gear-with-nam-dry-wet',
    'active_window_esr_median_max': .005,
    'active_window_esr_p95_max': .01,
    'active_window_esr_worst_max': .02,
    'quiet_residual_rms_max_dbfs': -70.0,
    'quiet_residual_peak_max_dbfs': -60.0,
    'rms_error_max_db': .5,
    'peak_error_max_db': 1.,
    'steady_silence_rms_max_dbfs': -80.,
    'engine_parity_residual_max_dbfs': -80.,
    'level_anchor': 'scale that puts the channel\'s validation capture at -20 dBFS RMS; frozen from capture data v1 at source 4ef21c16',
    'source_output_scale': ANCHORS,
}


def db_power(value):
    return float(10*np.log10(max(float(value), 1e-20)))


def memoryless(inp, field=PROFILE['receptive_field_samples']):
    """True where the input sample and the field-1 before it are all exactly zero."""
    nonzero = np.concatenate([[0], np.cumsum(np.asarray(inp) != 0)])
    end = np.arange(1, len(inp)+1)
    return nonzero[end] - nonzero[np.maximum(end-field, 0)] == 0


def fidelity(pred, ref, channel, inp):
    pred = np.asarray(pred, dtype=np.float64)
    ref = np.asarray(ref, dtype=np.float64)
    if pred.shape != ref.shape or pred.ndim != 1 or len(ref) <= PROFILE['skip_samples']+PROFILE['window_samples']:
        raise ValueError('Expected same-length mono reference and prediction')
    if np.shape(inp) != ref.shape:
        raise ValueError('Input must align with the reference')
    if not np.isfinite(pred).all() or not np.isfinite(ref).all():
        raise ValueError('Nonfinite audio')
    zero_history = memoryless(inp)
    memoryless_rms, memoryless_peak = [], []
    scale = PROFILE['source_output_scale'][channel]
    active, quiet_rms, quiet_peak = [], [], []
    windows = []
    size = PROFILE['window_samples']
    for start in range(PROFILE['skip_samples'], len(ref)-size+1, size):
        r = ref[start:start+size]*scale
        p = pred[start:start+size]*scale
        energy = np.mean(r*r)
        error = p-r
        rms = db_power(np.mean(error*error))
        peak = db_power(np.max(error*error))
        reference_db = db_power(energy)
        is_active = reference_db >= PROFILE['active_reference_rms_min_dbfs']
        esr = float(np.mean(error*error)/max(float(energy), 1e-20))
        held = zero_history[start:start+size]
        judged = error[~held]
        if is_active:
            active.append(esr)
        elif len(judged):
            # The mean over judged samples only: excluded samples must not dilute it.
            quiet_rms.append(db_power(np.mean(judged*judged)))
            quiet_peak.append(db_power(np.max(judged*judged)))
        if held.any() and not is_active:
            memoryless_rms.append(db_power(np.mean(error[held]**2)))
            memoryless_peak.append(db_power(np.max(error[held]**2)))
        windows.append({'start_sample': start, 'active': is_active, 'reference_rms_dbfs': reference_db,
                        'esr': esr, 'residual_rms_dbfs': rms, 'residual_peak_dbfs': peak,
                        'memoryless_samples': int(held.sum())})
    r, p = ref[PROFILE['skip_samples']:], pred[PROFILE['skip_samples']:]
    result = {
        'profile': PROFILE['id'], 'level_anchor': scale,
        'active_windows': len(active), 'quiet_windows': len(quiet_rms),
        'active_median': float(np.median(active)) if active else None,
        'active_p95': float(np.quantile(active, .95)) if active else None,
        'active_worst': float(max(active)) if active else None,
        'quiet_worst_residual_rms_dbfs': max(quiet_rms, default=-200.),
        'quiet_worst_residual_peak_dbfs': max(quiet_peak, default=-200.),
        'memoryless_samples': int(zero_history[PROFILE['skip_samples']:].sum()),
        'memoryless_worst_residual_rms_dbfs': max(memoryless_rms, default=-200.),
        'memoryless_worst_residual_peak_dbfs': max(memoryless_peak, default=-200.),
        'rms_error_db': db_power(np.mean(p*p))-db_power(np.mean(r*r)),
        'peak_error_db': db_power(np.max(p*p))-db_power(np.max(r*r)),
        'full_esr': float(np.mean((p-r)**2)/max(float(np.mean(r*r)), 1e-20)),
    }
    result['active_pass'] = bool(active and result['active_median'] <= PROFILE['active_window_esr_median_max'] and result['active_p95'] <= PROFILE['active_window_esr_p95_max'] and result['active_worst'] <= PROFILE['active_window_esr_worst_max'])
    result['quiet_pass'] = bool(quiet_rms and result['quiet_worst_residual_rms_dbfs'] <= PROFILE['quiet_residual_rms_max_dbfs'] and result['quiet_worst_residual_peak_dbfs'] <= PROFILE['quiet_residual_peak_max_dbfs'])
    result['level_pass'] = abs(result['rms_error_db']) <= PROFILE['rms_error_max_db'] and abs(result['peak_error_db']) <= PROFILE['peak_error_max_db']
    result['full_esr_pass'] = result['full_esr'] < PROFILE['full_esr_max_exclusive']
    result['fidelity_pass'] = result['active_pass'] and result['quiet_pass'] and result['level_pass'] and result['full_esr_pass']
    result['worst_active_windows'] = sorted((w for w in windows if w['active']), key=lambda w: w['esr'], reverse=True)[:5]
    result['quiet_window_details'] = [w for w in windows if not w['active']]
    return result


# v3 (owner decision, 2026-10-28): an A2-bounded release profile. TONE3000
# loads only A2 (width 3 or 8; reports/tone3000-a2-only-20261028.md), and every
# training lever available at width 8 has been tried (reports/channel-loss-20261028.md).
# v2 stays the reference-fidelity profile and is still reported; v3 only adds
# a release gate. It changes the active and full-ESR limits per channel class
# and keeps the quiet and level limits of v2 unchanged. Both the validation and
# the reserved test signal must pass, and release also needs the owner's
# instrument-DI listening acceptance, which no number replaces.
RELEASE_V3 = {
    'id': 'nastrond-tone3000-a2-release-v3',
    'authority': 'project release policy for A2 in TONE3000; not a TONE3000 certification',
    'measures': 'quality_profile.fidelity (profile v2 windows, anchors and memoryless rule)',
    'splits_required': ['validation', 'test'],
    'listening_acceptance_required': True,
    'classes': {
        # High-gain channels. A2 floor evidence: doubling data now gives -3.6%
        # (mean test full ESR 0.0174 at 16,320 s, extrapolated floor about
        # 0.017); width 32 (not A2) still left active p95 near 0.07.
        'high_gain': {'channels': ['Fenrir', 'Surtr', 'Nidhoggr', 'Ragnarok'],
                      'full_esr_max': .025, 'active_window_esr_median_max': .02,
                      'active_window_esr_p95_max': .12, 'active_window_esr_worst_max': .40},
        # Fimbulvetr already meets the v2 full-ESR limit; v3 keeps it.
        'bloom': {'channels': ['Fimbulvetr'],
                  'full_esr_max': .01, 'active_window_esr_median_max': .0075,
                  'active_window_esr_p95_max': .03, 'active_window_esr_worst_max': .20},
    },
    'quiet_residual_rms_max_dbfs': PROFILE['quiet_residual_rms_max_dbfs'],
    'quiet_residual_peak_max_dbfs': PROFILE['quiet_residual_peak_max_dbfs'],
    'rms_error_max_db': PROFILE['rms_error_max_db'],
    'peak_error_max_db': PROFILE['peak_error_max_db'],
}


def release_v3_limits(channel):
    for spec in RELEASE_V3['classes'].values():
        if channel in spec['channels']:
            return spec
    raise KeyError(f'No v3 class for channel {channel}')


def release_v3_check(f, channel):
    """Per-limit v3 verdict for one split, from a fidelity() result."""
    c = release_v3_limits(channel)
    checks = {
        'full_esr': f['full_esr'] <= c['full_esr_max'],
        'active_median': f['active_median'] is not None and f['active_median'] <= c['active_window_esr_median_max'],
        'active_p95': f['active_p95'] is not None and f['active_p95'] <= c['active_window_esr_p95_max'],
        'active_worst': f['active_worst'] is not None and f['active_worst'] <= c['active_window_esr_worst_max'],
        'quiet': (f['quiet_worst_residual_rms_dbfs'] <= RELEASE_V3['quiet_residual_rms_max_dbfs']
                  and f['quiet_worst_residual_peak_dbfs'] <= RELEASE_V3['quiet_residual_peak_max_dbfs']),
        'level': (abs(f['rms_error_db']) <= RELEASE_V3['rms_error_max_db']
                  and abs(f['peak_error_db']) <= RELEASE_V3['peak_error_max_db']),
    }
    checks = {k: bool(v) for k, v in checks.items()}
    return {'profile': RELEASE_V3['id'], 'checks': checks, 'pass': all(checks.values())}
