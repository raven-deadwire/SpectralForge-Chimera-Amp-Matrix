#!/usr/bin/env python3
"""Derive anchored summaries and exact quartet-energy bounds, without fitting.

The initial probe retained hashes/statistics but not quartet-window energies.
Regenerate only each channel's existing largest-case quartet, demand byte-exact
original output SHA equality, and retain the extra energy statistic. No stimulus,
control, model or observation condition is added or changed.
"""
import argparse
import hashlib
import json
import subprocess
from datetime import datetime, timezone
from pathlib import Path

import numpy as np


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write(path, value):
    Path(path).write_text(json.dumps(value, indent=2, ensure_ascii=False) + '\n')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--results', type=Path, required=True)
    p.add_argument('--plan', type=Path, required=True)
    p.add_argument('--renderer', type=Path, required=True)
    p.add_argument('--scratch', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    if a.out.exists():
        raise RuntimeError('Summary already exists; refuse silent replay')
    raw = json.loads(a.results.read_text())
    plan = json.loads(a.plan.read_text())
    if digest(a.plan) != raw['plan_sha256'] or digest(a.renderer) != plan['native_renderer_sha256']:
        raise RuntimeError('Frozen source or plan changed')
    names = list(plan['channels'])
    selected = {}
    for name in names:
        candidates = [c for g in raw['native_groups'] if g['name'] == name for c in g['cases']]
        selected[name] = max(candidates, key=lambda c: c['statistics']['whole_probe_200ms']['delta_rms'])
    addendum = {
        'created_utc_before_evidence_reconstruction': datetime.now(timezone.utc).isoformat(),
        'script_sha256': digest(__file__), 'initial_plan_sha256': digest(a.plan),
        'initial_results_sha256': digest(a.results),
        'purpose': 'Recover previously unrecorded same-window quartet energies and apply the original anchor to raw player numerical-control statistics',
        'regeneration_count': 20, 'new_unique_stimuli_or_settings': 0,
        'require_original_output_sha256_equality': True,
        'selected_existing_cases': {name: {k: c[k] for k in ['name', 'gap_ms', 'probe_level', 'probe_onset_sample']} for name, c in selected.items()},
        'fit_or_tuning': False, 'release_approved': False
    }
    addendum_path = a.out.parent / 'window-energy-reconstruction-plan.json'
    write(addendum_path, addendum)
    report = {'initial_plan_sha256': digest(a.plan), 'initial_results_sha256': digest(a.results),
              'energy_reconstruction_plan_sha256': digest(addendum_path),
              'scope': 'Same-window structural diagnostic only; no application or modification of product fidelity gates',
              'exact_arithmetic_class': 'finite6347-sample A2 followed by a fixed linear time-invariant POST filter',
              'bounds': {'minimax_individual_norm': 'max_i ||e_i|| >= ||Delta||/4',
                         'pooled_four_trace_probe_esr': 'sum_i||e_i||^2/sum_i||r_i||^2 >= ||Delta||^2/(4*sum_i||r_i||^2)',
                         'derivation': 'Delta_hat=0 implies Delta=plus/minus signed sum of four errors. Triangle inequality gives minimax; Cauchy-Schwarz gives ||Delta||^2<=4*sum||e_i||^2. All norms use identical sample ranges and frozen per-channel anchors.'},
              'channels': {}, 'fit_or_tuning': False, 'release_approved': False}
    for name, case in selected.items():
        gap, level, onset = case['gap_ms'], case['probe_level'], case['probe_onset_sample']
        original_group = next(g for g in raw['native_groups'] if g['name'] == name and g['gap_ms'] == gap)
        expected = {v['input_key']: v for v in original_group['render_evidence']}
        job = a.scratch / (name + '-native-job.json')
        if digest(job) != plan['channels'][name]['native_job_sha256']:
            raise RuntimeError('Native control job changed')
        scale = plan['channels'][name]['reference_output_scale']
        traces = {}
        proofs = []
        for history in ['high', 'low']:
            for lev in [0., level]:
                key = f'gap{gap}-{history}-level{lev:.3f}'
                inp = a.scratch / (key + '.f32')
                if digest(inp) != plan['inputs'][key]['sha256']:
                    raise RuntimeError('Frozen input changed')
                out = a.scratch / (name + '-' + key + '-energy-reconstruction.f32')
                subprocess.run([str(a.renderer), str(inp), str(out), str(job)], check=True,
                               text=True, capture_output=True)
                actual = digest(out)
                if actual != expected[key]['output_sha256']:
                    raise RuntimeError('Reconstructed output is not byte-identical')
                traces[(history, lev)] = np.fromfile(out, dtype='<f4').astype(np.float64)[onset:] * scale
                proofs.append({'input_key': key, 'original_and_reconstructed_output_sha256': actual})
                out.unlink()
        delta = (traces[('high', level)] - traces[('high', 0.)]) - (traces[('low', level)] - traces[('low', 0.)])
        windows = {}
        for label, (start, end) in plan['windows_relative_to_probe_onset'].items():
            dd = delta[start:end]
            energies = {f'{history}-level{lev:.3f}': float(np.sum(y[start:end] ** 2))
                        for (history, lev), y in traces.items()}
            energy_sum = sum(energies.values())
            delta_energy = float(np.sum(dd ** 2))
            known = case['statistics'][label]
            if not np.isclose(np.sqrt(np.mean(dd ** 2)), known['delta_rms'], rtol=1e-10, atol=1e-14):
                raise RuntimeError('Original delta statistic not reproduced')
            windows[label] = dict(known,
                                 reference_quartet_energy=energies,
                                 reference_quartet_energy_sum=energy_sum,
                                 delta_energy=delta_energy,
                                 pooled_four_trace_esr_lower_bound=delta_energy / (4 * energy_sum) if energy_sum else 0.)
        all_cases = [c for g in raw['native_groups'] if g['name'] == name for c in g['cases']]
        report['channels'][name] = {
            'max_whole_probe_case': {k: case[k] for k in ['gap_ms', 'probe_level', 'probe_onset_sample']},
            'reference_output_scale': scale, 'max_case_windows': windows,
            'all_cases_whole_probe': [{'gap_ms': c['gap_ms'], 'probe_level': c['probe_level'],
                                      **c['statistics']['whole_probe_200ms']} for c in all_cases],
            'energy_reconstruction_byte_exact': True, 'energy_reconstruction_proofs': proofs
        }
        print(name, 'max200msDelta', windows['whole_probe_200ms']['delta_rms_dbfs'],
              'pooledESRfloor', windows['whole_probe_200ms']['pooled_four_trace_esr_lower_bound'], flush=True)
    control = raw['numerical_control']
    name = control['selected_native_case']['name']
    scale = plan['channels'][name]['reference_output_scale']
    pad = 20 * np.log10(scale)
    anchored = {}
    for label, values in control['statistics'].items():
        fixed = {}
        for k, value in values.items():
            if k.endswith('_dbfs'):
                fixed[k] = float(value + pad)
            elif k in ['delta_rms', 'delta_peak']:
                fixed[k] = float(value * scale)
            else:
                fixed[k] = value
        anchored[label] = fixed
    report['numerical_control'] = {
        'case': control['selected_native_case'],
        'original_statistics_units': 'unscaled raw NAM output; retained unchanged in initial results.json',
        'reference_output_scale': scale, 'reference_output_scale_db': float(pad),
        'anchored_statistics': anchored,
        'native_whole_probe_delta_rms_dbfs': report['channels'][name]['max_case_windows']['whole_probe_200ms']['delta_rms_dbfs'],
        'native_over_control_whole_probe_delta_db': report['channels'][name]['max_case_windows']['whole_probe_200ms']['delta_rms_dbfs'] - anchored['whole_probe_200ms']['delta_rms_dbfs'],
        'scope': 'Floating-point control for one existing A2+fixedPOST chain; not a uniform error bound over all weights/filtersettings'
    }
    write(a.out, report)


if __name__ == '__main__':
    main()
