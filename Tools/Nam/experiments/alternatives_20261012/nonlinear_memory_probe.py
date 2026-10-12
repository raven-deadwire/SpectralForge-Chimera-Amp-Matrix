#!/usr/bin/env python3
"""One predeclared difference-in-differences probe of active nonlinear memory.

No fitting or candidate tuning. A common silent gap exceeds the fixed A2
receptive field. Four-render incremental differences remove any fixed linear
POST filter's inherited tail. Full fidelity gates are not applied to this probe.
"""
import argparse
import concurrent.futures
import hashlib
import json
import subprocess
import time
from datetime import datetime, timezone
from pathlib import Path

import numpy as np

FS = 48000
RF = 6347
SOURCE = 'd93560445c78552768a0ed592e5ec62c3c648fb6'
NAMES = ['Fenrir', 'Surtr', 'Nidhoggr', 'Fimbulvetr', 'Ragnarok']
GAPS_MS = [150, 250, 500, 750, 1000]
LEVELS = [.03, .3]
PREFIX_SAMPLES = FS
PROBE_SAMPLES = FS // 5
TAIL_SAMPLES = FS // 5
WINDOWS = {'onset_20ms': [0, 960], 'late_probe_20ms': [8640, 9600],
           'whole_probe_200ms': [0, 9600], 'post_probe_tail_200ms': [9600, 19200]}


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write(path, value):
    Path(path).write_text(json.dumps(value, indent=2, ensure_ascii=False) + '\n')


def db_rms(x):
    return float(10 * np.log10(max(float(np.mean(np.asarray(x, dtype=np.float64) ** 2)), 1e-30)))


def db_peak(x):
    return float(20 * np.log10(max(float(np.max(np.abs(x))), 1e-15)))


def carrier(n):
    t = np.arange(n, dtype=np.float64) / FS
    return (np.sin(2 * np.pi * 55 * t) + .35 * np.sin(2 * np.pi * 110 * t)
            + .15 * np.sin(2 * np.pi * 165 * t)) / 1.5


def probe_wave():
    t = np.arange(PROBE_SAMPLES, dtype=np.float64) / FS
    envelope = (1 - np.exp(-t / .003)) * np.exp(-t / .080)
    fade = t >= .180
    envelope[fade] *= .5 * (1 + np.cos(np.pi * (t[fade] - .180) / .020))
    return carrier(PROBE_SAMPLES) * envelope


def stimulus(gap_ms, conditioning, level):
    gap = int(gap_ms * FS / 1000)
    if gap <= RF:
        raise RuntimeError('Gap must exceed receptive field')
    onset = PREFIX_SAMPLES + gap
    x = np.zeros(onset + PROBE_SAMPLES + TAIL_SAMPLES, dtype='<f4')
    x[:PREFIX_SAMPLES] = carrier(PREFIX_SAMPLES) * conditioning
    if level:
        x[onset:onset + PROBE_SAMPLES] = probe_wave() * level
    return x, onset


def key(gap_ms, history, level):
    return f'gap{gap_ms}-{history}-level{level:.3f}'


def statistics(delta, increment_high, increment_low):
    out = {}
    for label, (start, end) in WINDOWS.items():
        d, hi, lo = delta[start:end], increment_high[start:end], increment_low[start:end]
        out[label] = {
            'delta_rms_dbfs': db_rms(d), 'delta_peak_dbfs': db_peak(d),
            'delta_rms': float(np.sqrt(np.mean(d ** 2))),
            'delta_peak': float(np.max(np.abs(d))),
            'increment_high_rms_dbfs': db_rms(hi), 'increment_low_rms_dbfs': db_rms(lo),
            'increment_level_high_minus_low_db': db_rms(hi) - db_rms(lo),
            'minimax_individual_error_rms_lower_bound_dbfs': db_rms(d / 4),
            'minimax_individual_error_peak_lower_bound_dbfs': db_peak(d / 4)
        }
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--renderer', type=Path, required=True)
    ap.add_argument('--build-evidence', type=Path, required=True)
    ap.add_argument('--settings', type=Path, required=True)
    ap.add_argument('--profile', type=Path, required=True)
    ap.add_argument('--scratch', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--player', type=Path, required=True)
    ap.add_argument('--baseline-models', type=Path, required=True)
    ap.add_argument('--factorization', type=Path, required=True)
    ap.add_argument('--workers', type=int, default=3)
    a = ap.parse_args()
    if a.out.exists():
        raise RuntimeError('Output directory exists; this probe must not rerun silently')
    build = json.loads(a.build_evidence.read_text())
    if build['capture_source'] != SOURCE:
        raise RuntimeError('Native source changed')
    if digest(a.renderer) != build['binaries']['ChimeraNamRender']['sha256']:
        raise RuntimeError('Native renderer differs from verified build')
    if digest(a.player) != build['binaries']['ChimeraTone3000Tool']['sha256']:
        raise RuntimeError('Player differs from verified build')
    settings = json.loads(a.settings.read_text())
    if [v['name'] for v in settings] != NAMES:
        raise RuntimeError('Channel settings mismatch')
    profile = json.loads(a.profile.read_text())
    if profile['id'] != 'nastrond-tone3000-fidelity-v1':
        raise RuntimeError('Unexpected original output-anchor profile')
    a.out.mkdir(parents=True)
    a.scratch.mkdir(parents=True, exist_ok=True)
    input_paths = {}
    input_manifest = {}
    for gap_ms in GAPS_MS:
        for history, conditioning in [('high', .65), ('low', .005)]:
            for level in [0.] + LEVELS:
                x, onset = stimulus(gap_ms, conditioning, level)
                k = key(gap_ms, history, level)
                p = a.scratch / (k + '.f32')
                x.tofile(p)
                input_paths[k] = p
                input_manifest[k] = {'sha256': digest(p), 'samples': len(x),
                                     'probe_onset_sample': onset,
                                     'probe_end_sample': onset + PROBE_SAMPLES}
    jobs = {}
    channel_manifest = {}
    for setting in settings:
        name = setting['name']
        job = {'channel': setting['channel'], 'controls': setting['controls']}
        path = a.scratch / (name + '-native-job.json')
        write(path, job)
        jobs[name] = path
        model = a.baseline_models / ('Nastrond-' + name + '.nam')
        eq_job = a.factorization / (name + '-factor-post-eq.json')
        channel_manifest[name] = {'native_job': job, 'native_job_sha256': digest(path),
                                  'reference_output_scale': profile['source_output_scale'][name],
                                  'baseline_model_sha256': digest(model),
                                  'frozen_eq_job_sha256': digest(eq_job)}
    plan = {
        'protocol_id': 'nastrond-nonlinear-incremental-memory-v1',
        'created_utc_before_rendering': datetime.now(timezone.utc).isoformat(),
        'source_commit': SOURCE, 'native_renderer_sha256': digest(a.renderer),
        'player_sha256': digest(a.player), 'script_sha256': digest(__file__),
        'build_evidence_sha256': digest(a.build_evidence),
        'settings_sha256': digest(a.settings), 'profile_sha256': digest(a.profile),
        'sample_rate': FS, 'native_oversampling': 4, 'native_block_size': 256,
        'rf_samples': RF, 'conditioning_samples': PREFIX_SAMPLES,
        'conditioning_levels': [.65, .005], 'gaps_ms': GAPS_MS,
        'probe_levels': LEVELS, 'probe_samples': PROBE_SAMPLES,
        'post_probe_tail_samples': TAIL_SAMPLES,
        'carrier': '(sin(2*pi*55*t)+.35*sin(2*pi*110*t)+.15*sin(2*pi*165*t))/1.5',
        'probe_envelope': '(1-exp(-t/.003))*exp(-t/.080), multiplied by half-cosine fade over180-200ms',
        'windows_relative_to_probe_onset': WINDOWS,
        'delta_definition': '(y_high_probe-y_high_zero)-(y_low_probe-y_low_zero)',
        'render_count_native': 150,
        'zero_controls_reused_only_within_same_channel_gap': True,
        'positive_control_selection': 'one largest native whole_probe_200ms delta RMS across all50 predefined cases; no model selection or tuning',
        'player_control': 'four renders of unchanged legacy baseline A2 plus its already-frozen legal POST EQ on that witness; normalizefalse',
        'floating_point_scope': 'Fixed finite-history A2 plus fixed linear POST has delta0 in exact arithmetic; actual component control quantifies float32 residual',
        'formal_bound': 'If delta_hat=0, triangle inequality gives max_i norm(y_i-yhat_i)>=norm(delta)/4 for the same output window; report RMS and infinity norms only',
        'limitations': 'Diagnostic-specific class result, not whole-program fidelity fail/pass, universal recurrent-model impossibility, perceptual audibility, or state-specific sag attribution',
        'fit_or_tuning': False, 'release_approved': False,
        'inputs': input_manifest, 'channels': channel_manifest
    }
    plan_path = a.out / 'plan.json'
    write(plan_path, plan)
    plan_sha = digest(plan_path)
    print('PLAN_FROZEN', plan_sha, 'before150native renders', flush=True)
    start_time = time.monotonic()
    all_deltas = {}

    def render(name, gap_ms, history, level):
        k = key(gap_ms, history, level)
        output = a.scratch / (name + '-' + k + '-native.f32')
        completed = subprocess.run([str(a.renderer), str(input_paths[k]), str(output), str(jobs[name])],
                                   text=True, check=True, capture_output=True)
        info = json.loads(completed.stdout)
        y = np.fromfile(output, dtype='<f4')
        if len(y) != input_manifest[k]['samples'] or not np.isfinite(y).all():
            raise RuntimeError('Invalid native output')
        proof = {'input_key': k, 'output_sha256': digest(output), 'renderer_metadata': info}
        output.unlink()
        return y.astype(np.float64), proof

    def channel_gap(name, gap_ms):
        onset = PREFIX_SAMPLES + int(gap_ms * FS / 1000)
        zhi, mhi = render(name, gap_ms, 'high', 0.)
        zlo, mlo = render(name, gap_ms, 'low', 0.)
        evidence = [mhi, mlo]
        cases = []
        values = {}
        scale = profile['source_output_scale'][name]
        for level in LEVELS:
            hi, mhi = render(name, gap_ms, 'high', level)
            lo, mlo = render(name, gap_ms, 'low', level)
            evidence.extend([mhi, mlo])
            # Convert to float64 before subtraction; retain the original fixed
            # output anchor. No per-input output normalization or alignment.
            inc_hi = (hi[onset:] - zhi[onset:]) * scale
            inc_lo = (lo[onset:] - zlo[onset:]) * scale
            delta = inc_hi - inc_lo
            values[f'{name}_gap{gap_ms}_level{level:.3f}'] = delta.astype(np.float32)
            cases.append({'name': name, 'gap_ms': gap_ms, 'probe_level': level,
                          'probe_onset_sample': onset,
                          'samples': len(hi), 'statistics': statistics(delta, inc_hi, inc_lo)})
        return {'name': name, 'gap_ms': gap_ms, 'cases': cases,
                'render_evidence': evidence}, values

    groups = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=a.workers) as pool:
        futures = [pool.submit(channel_gap, name, gap) for name in NAMES for gap in GAPS_MS]
        for i, future in enumerate(concurrent.futures.as_completed(futures)):
            group, values = future.result()
            groups.append(group)
            all_deltas.update(values)
            print('NATIVE_GROUP', i + 1, '/25', group['name'], group['gap_ms'],
                  'elapsed', round(time.monotonic() - start_time, 1), flush=True)
    groups.sort(key=lambda g: (NAMES.index(g['name']), g['gap_ms']))
    cases = [c for g in groups for c in g['cases']]
    selected = max(cases, key=lambda c: c['statistics']['whole_probe_200ms']['delta_rms'])
    # This rule was declared before any render. This is an arithmetic control,
    # not an extra model candidate, fitting split or whole-engine retest.
    name, gap_ms, level = selected['name'], selected['gap_ms'], selected['probe_level']
    model = a.baseline_models / ('Nastrond-' + name + '.nam')
    eq_job = a.factorization / (name + '-factor-post-eq.json')
    if digest(model) != plan['channels'][name]['baseline_model_sha256'] or digest(eq_job) != plan['channels'][name]['frozen_eq_job_sha256']:
        raise RuntimeError('Frozen numerical-control candidate changed')
    player_values = {}
    player_proofs = []
    onset = selected['probe_onset_sample']
    for history in ['high', 'low']:
        for lv in [0., level]:
            k = key(gap_ms, history, lv)
            out = a.scratch / (name + '-' + k + '-player.f32')
            completed = subprocess.run([str(a.player), 'render', str(model), str(input_paths[k]), str(out), str(eq_job)],
                                       text=True, check=True, capture_output=True)
            yy = np.fromfile(out, dtype='<f4').astype(np.float64)
            if len(yy) != selected['samples'] or not np.isfinite(yy).all():
                raise RuntimeError('Invalid player control output')
            player_values[(history, lv)] = yy[onset:]
            player_proofs.append({'input_key': k, 'output_sha256': digest(out),
                                  'player_metadata': json.loads(completed.stdout)})
            out.unlink()
    hi = player_values[('high', level)] - player_values[('high', 0.)]
    lo = player_values[('low', level)] - player_values[('low', 0.)]
    player_delta = hi - lo
    numerical_control = {'selected_native_case': {k: selected[k] for k in ['name', 'gap_ms', 'probe_level', 'probe_onset_sample']},
                         'scope': 'unchanged legacy baseline A2 plus already-frozen legal POST EQ; floating-point class control only',
                         'model_sha256': digest(model), 'eq_job_sha256': digest(eq_job),
                         'statistics': statistics(player_delta, hi, lo),
                         'render_evidence': player_proofs}
    all_deltas['numerical_control_selected_case'] = player_delta.astype(np.float32)
    arrays_path = a.out / 'delta-waveforms.npz'
    np.savez_compressed(arrays_path, **all_deltas)
    report = {'protocol_id': plan['protocol_id'], 'plan_sha256': plan_sha,
              'plan_was_frozen_before_rendering': True,
              'native_groups': groups, 'native_case_count': len(cases),
              'native_render_count': sum(len(g['render_evidence']) for g in groups),
              'numerical_control': numerical_control,
              'delta_waveforms_sha256': digest(arrays_path),
              'elapsed_seconds': time.monotonic() - start_time,
              'fit_or_tuning': False, 'release_approved': False,
              'scope': plan['limitations']}
    write(a.out / 'results.json', report)
    print('COMPLETE', len(cases), 'cases; numericalcontrol', name, gap_ms, level,
          'nativeDeltaRMS', selected['statistics']['whole_probe_200ms']['delta_rms_dbfs'],
          'playerDeltaRMS', numerical_control['statistics']['whole_probe_200ms']['delta_rms_dbfs'], flush=True)


if __name__ == '__main__':
    main()
