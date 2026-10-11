"""Project fidelity policy for TONE3000, not an official TONE3000 certification.

Classification and residual dBFS use the frozen v0.2 level anchor for both
reference and prediction. Neither a candidate's loudness nor output pad can
move a difficult window into the quiet category.
"""
import numpy as np

PROFILE = {
    'id': 'nastrond-tone3000-fidelity-v1',
    'authority': 'project acceptance policy; not a TONE3000 certification',
    'window_samples': 4096,
    'skip_samples': 6347,
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
    'level_anchor': 'delivered v0.2 reference_output_scale, frozen per channel',
    'source_output_scale': {
        'Fenrir': .09195752759643712,
        'Surtr': .09034985741800312,
        'Nidhoggr': .0860012970451488,
        'Fimbulvetr': .09721294970620417,
        'Ragnarok': .05162876564505051,
    },
}


def db_power(value):
    return float(10*np.log10(max(float(value), 1e-20)))


def fidelity(pred, ref, channel):
    pred = np.asarray(pred, dtype=np.float64)
    ref = np.asarray(ref, dtype=np.float64)
    if pred.shape != ref.shape or pred.ndim != 1 or len(ref) <= PROFILE['skip_samples']+PROFILE['window_samples']:
        raise ValueError('Expected same-length mono reference and prediction')
    if not np.isfinite(pred).all() or not np.isfinite(ref).all():
        raise ValueError('Nonfinite audio')
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
        if is_active:
            active.append(esr)
        else:
            quiet_rms.append(rms)
            quiet_peak.append(peak)
        windows.append({'start_sample': start, 'active': is_active, 'reference_rms_dbfs': reference_db,
                        'esr': esr, 'residual_rms_dbfs': rms, 'residual_peak_dbfs': peak})
    r, p = ref[PROFILE['skip_samples']:], pred[PROFILE['skip_samples']:]
    result = {
        'profile': PROFILE['id'], 'level_anchor': scale,
        'active_windows': len(active), 'quiet_windows': len(quiet_rms),
        'active_median': float(np.median(active)) if active else None,
        'active_p95': float(np.quantile(active, .95)) if active else None,
        'active_worst': float(max(active)) if active else None,
        'quiet_worst_residual_rms_dbfs': max(quiet_rms, default=-200.),
        'quiet_worst_residual_peak_dbfs': max(quiet_peak, default=-200.),
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
