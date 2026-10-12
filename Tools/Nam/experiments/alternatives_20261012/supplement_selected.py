#!/usr/bin/env python3
"""Qualify the sole preselected Nidhoggr omitted from the first test launch.

Reuse completed measurements for exact selected model/job pairs. The original
selection predates the shared test. This supplement does not fit, tune, choose
a candidate, generate excitation, or rerender a native reference.
"""
import argparse
import itertools
import json
from pathlib import Path
import uuid

import numpy as np

import evaluate_candidates as ev


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('evaluation', 'experiment', 'tool', 'out'):
        p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    for name in vars(a):
        setattr(a, name, getattr(a, name).resolve())
    ev.torch.set_num_threads(2)
    if a.out.exists() and any(a.out.iterdir()):
        raise RuntimeError('Fresh supplemental output directory required')
    source_plan_path = a.evaluation / 'test-plan.json'
    source_report_path = a.evaluation / 'comparison.json'
    source_plan = json.loads(source_plan_path.read_text())
    source_report = json.loads(source_report_path.read_text())
    if source_report.get('complete') is not True or source_report['plan_sha256'] != ev.digest(source_plan_path):
        raise RuntimeError('Original evaluation must be complete and unchanged')
    selection_path = a.experiment / 'selection.json'
    selection = json.loads(selection_path.read_text())
    if (ev.digest(selection_path) != source_plan['selection_sha256']
            or selection['plan_sha256'] != ev.digest(a.experiment/'plan.json')
            or selection['validation_sha256'] != ev.digest(a.experiment/'validation.json')
            or selection.get('release_approved') is not False):
        raise RuntimeError('Original pre-test selection changed')
    if (source_plan['quality_profile'] != ev.PROFILE
            or source_plan['policy_sha256'] != ev.digest(ev.NAM_DIR/'quality_profile.py')
            or source_plan['script_sha256'] != ev.digest(Path(ev.__file__))
            or source_plan['tool_sha256'] != ev.digest(a.tool)):
        raise RuntimeError('Original metric helpers, policy or native runtime changed')
    for file, expected in source_plan['sealed_files'].items():
        if ev.digest(file) != expected:
            raise RuntimeError('Original sealed dependency changed: ' + file)

    channels, aliases, missing = {}, {}, []
    selected_paths = []
    for name in ev.NAMES:
        chosen = selection['selected'][name]
        model_path = a.experiment/'selected'/('Nastrond-'+name+'.nam')
        job_path = a.experiment/'selected'/(name+'-job.json')
        if (ev.digest(model_path) != chosen['model_sha256']
                or ev.digest(job_path) != chosen['job_sha256']):
            raise RuntimeError('Selected model/job bytes changed: ' + name)
        selected_paths += [model_path, job_path]
        matches = [(arm, rows[name]) for arm, rows in source_report['arms'].items()
                   if rows[name]['model_sha256'] == chosen['model_sha256']
                   and rows[name]['job_sha256'] == chosen['job_sha256']]
        if matches:
            arm, row = matches[0]
            channels[name] = row
            aliases[name] = {'kind': 'reuse exact previously measured model/job pair',
                             'arm': arm, 'channel': name,
                             'comparison_sha256': ev.digest(source_report_path)}
        else:
            missing.append(name)
    if missing != ['Nidhoggr']:
        raise RuntimeError('Expected only the preselected Nidhoggr omission: ' + repr(missing))

    name = 'Nidhoggr'
    input_path = a.evaluation/'new-input.npy'
    reference_path = a.evaluation/('new-'+name+'-native.npy')
    if ev.digest(input_path) != source_report['input_sha256']:
        raise RuntimeError('Original shared test input changed')
    model_path = a.experiment/'selected'/('Nastrond-'+name+'.nam')
    job_path = a.experiment/'selected'/(name+'-job.json')
    job = json.loads(job_path.read_text())
    if job.get('normalize') is not False or job.get('block_size') != 64:
        raise RuntimeError('Unexpected selected gain or block size')
    if job.get('eq', {}).get('enabled', False):
        ev.load_eq_sos(job)
    old_test = None
    if source_plan.get('old_test'):
        candidates = [Path(file).parent for file, expected in source_plan['sealed_files'].items()
                      if Path(file).name == 'input.npy'
                      and expected == source_plan['old_test']['input_sha256']]
        if len(candidates) != 1:
            raise RuntimeError('Cannot unambiguously reuse original revealed diagnostic')
        old_test = candidates[0]
    sealed = {**source_plan['sealed_files'], **{str(file): ev.digest(file) for file in
        [source_plan_path, source_report_path, selection_path, input_path, reference_path,
         Path(__file__), *selected_paths]}}
    a.out.mkdir(parents=True, exist_ok=True)
    plan = {'scope': 'Supplemental qualification of sole previously selected Nidhoggr omitted from initial launch',
        'reason': 'Orchestration omitted selected raw lambda=0.01 while launching descriptive best-ESR arms',
        'selection_timing': 'Original selection hash was sealed in the initial test plan before the shared input was generated',
        'selection_sha256': ev.digest(selection_path),
        'source_evaluation_plan_sha256': ev.digest(source_plan_path),
        'source_comparison_sha256': ev.digest(source_report_path),
        'reuse': 'Same already generated synthetic input and native reference; no new independent test claim',
        'selected_pairs': selection['selected'], 'existing_result_aliases': aliases,
        'new_native_model_renders': [name], 'new_reference_renders': 0,
        'quality_profile': ev.PROFILE, 'sealed_files': sealed,
        'candidate_adjustment_after_test': False, 'candidate_selection_after_test': False,
        'release_approved': False}
    plan_path = a.out/'supplement-plan.json'
    ev.write_json(plan_path, plan)
    plan_hash = ev.digest(plan_path)
    # All original candidate seals and the supplemental measurement plan are
    # checked/written before decoding the reused audio or rendering this model.
    x = np.load(input_path, allow_pickle=False)
    expected_samples = round(source_plan['seconds'] * 48000)
    ev.check_audio(x, expected_samples, input_path)
    reference = ev.check_audio(np.load(reference_path, allow_pickle=False), len(x), reference_path)
    raw, output = a.out/'shared-input.f32', a.out/'working-output.f32'
    x.astype('<f4').tofile(raw)
    runtime = ev.run(a.tool, 'render', model_path, raw, output, job_path)
    ev.check_runtime(runtime, len(x))
    y = ev.read_audio(output, len(x))
    np.save(a.out/('new-selected-'+name+'.npy'), y)
    model = ev.init_from_nam(json.loads(model_path.read_text())).eval()
    python = ev.check_audio(ev.apply_post_eq(ev.predict(model, x), job), len(x), 'Python parity')
    residual = (y.astype(np.float64)-python)*ev.PROFILE['source_output_scale'][name]
    parity = float(10*np.log10(max(np.mean(residual*residual), 1e-20)))
    block_job = a.out/'block256-job.json'
    ev.write_json(block_job, {**job, 'block_size': 256})
    runtime256 = ev.run(a.tool, 'render', model_path, raw, output, block_job)
    ev.check_runtime(runtime256, len(x))
    block_delta = float(np.max(np.abs(ev.read_audio(output, len(x))-y)))
    settings = a.out/'selected-Nidhoggr-preset-settings.json'
    ev.write_json(settings, {**job, 'name': 'Nastrond Nidhoggr selected Research',
        'id': str(uuid.uuid5(uuid.NAMESPACE_URL, 'nastrond/alternatives-20261012/selected/Nidhoggr'))})
    preset = a.out/'Research-selected-Nidhoggr.t3kpreset'
    preset_metadata = ev.run(a.tool, 'preset', model_path, preset, settings)
    preset_runtime = ev.run(a.tool, 'render-preset', preset, raw, output)
    ev.check_runtime(preset_runtime, len(x))
    preset_delta = float(np.max(np.abs(ev.read_audio(output, len(x))-y)))
    zero = a.out/'silence.f32'
    np.zeros(48000, dtype='<f4').tofile(zero)
    silence_runtime = ev.run(a.tool, 'render', model_path, zero, output, job_path)
    ev.check_runtime(silence_runtime, 48000)
    silence = ev.read_audio(output, 48000)[ev.PROFILE['skip_samples']:].astype(np.float64)
    silence *= ev.PROFILE['source_output_scale'][name]
    silence_db = float(10*np.log10(max(np.mean(silence*silence), 1e-20)))
    row = {'model_sha256': ev.digest(model_path), 'job_sha256': ev.digest(job_path),
        'new_test': ev.fidelity(y, reference, name), 'character': ev.character(y, reference),
        'runtime64': runtime, 'runtime256': runtime256, 'block64_256_max_abs': block_delta,
        'python_native_parity_residual_dbfs': parity, 'preset_restore_max_abs': preset_delta,
        'preset_sha256': ev.digest(preset), 'preset_metadata': preset_metadata,
        'preset_runtime': preset_runtime, 'steady_silence_dbfs': silence_db,
        'silence_runtime': silence_runtime,
        'frozen_validation_roles': ev.frozen_roles(selection, name, ev.digest(model_path), ev.digest(job_path))}
    row['engine_compatibility_pass'] = bool(parity <= ev.PROFILE['engine_parity_residual_max_dbfs']
        and preset_delta <= 1e-6 and block_delta <= 1e-6
        and silence_db <= ev.PROFILE['steady_silence_rms_max_dbfs'])
    if old_test:
        old_x = np.load(old_test/'input.npy', allow_pickle=False)
        ev.check_audio(old_x, len(old_x), 'Previously revealed input')
        old_raw = a.out/'old-input.f32'
        old_x.astype('<f4').tofile(old_raw)
        old_runtime = ev.run(a.tool, 'render', model_path, old_raw, output, job_path)
        ev.check_runtime(old_runtime, len(old_x))
        old_y = ev.read_audio(output, len(old_x))
        np.save(a.out/('old-selected-'+name+'.npy'), old_y)
        row['old_revealed_regression'] = ev.fidelity(old_y,
            np.load(old_test/(name+'-native.npy'), allow_pickle=False), name)
    channels[name] = row
    aliases[name] = {'kind': 'sole omitted preselected model; supplemental measurement on shared test',
                     'channel': name, 'supplement_plan_sha256': plan_hash}
    contrasts = []
    for left, right in itertools.combinations(ev.NAMES, 2):
        l, r = channels[left]['character'], channels[right]['character']
        native = np.array(l['reference']['band_power_db'])-r['reference']['band_power_db']
        candidate = np.array(l['prediction']['band_power_db'])-r['prediction']['band_power_db']
        contrasts.append({'channels': [left, right], 'native_band_contrast_db': native.tolist(),
                          'candidate_band_contrast_db': candidate.tolist(),
                          'absolute_contrast_error_db': np.abs(candidate-native).tolist()})
    for file, expected in {**sealed, str(plan_path): plan_hash}.items():
        if ev.digest(file) != expected:
            raise RuntimeError('Supplemental or original seal changed: ' + file)
    summary = {'complete': True, 'selection_sha256': ev.digest(selection_path),
        'source_evaluation_plan_sha256': ev.digest(source_plan_path),
        'source_comparison_sha256': ev.digest(source_report_path),
        'supplement_plan_sha256': plan_hash, 'input_sha256': ev.digest(input_path),
        'quality_profile': ev.PROFILE, 'provenance_aliases': aliases,
        'selected_modes': {n: selection['selected'][n]['mode'] for n in ev.NAMES},
        'channels': {n: channels[n] for n in ev.NAMES}, 'channel_character_contrasts': contrasts,
        'numerical_pass_count': sum(r['new_test']['fidelity_pass'] for r in channels.values()),
        'engine_pass_count': sum(r['engine_compatibility_pass'] for r in channels.values()),
        'candidate_adjustment_after_test': False, 'candidate_selection_after_test': False,
        'test_scope': 'Same shared synthetic test; supplemental model was selected before initial test generation',
        'old_test_scope': 'Already revealed 20261011 diagnostic regression only',
        'compatibility_scope': source_report['compatibility_scope'],
        'timing_scope': source_report['timing_scope'],
        'instrument_DI_listening': 'NOT_EXECUTED', 'channel_role_listening': 'NOT_EXECUTED',
        'TONE3000_GUI_DAW_acceptance': 'NOT_EXECUTED', 'release_approved': False}
    ev.write_json(a.out/'selected-summary.json', summary)
    for file in (raw, output, zero, a.out/'old-input.f32', block_job):
        file.unlink(missing_ok=True)
    print(json.dumps({'complete': True, 'supplemental_channel': name,
        'esr': row['new_test']['full_esr'], 'fidelity_pass': row['new_test']['fidelity_pass'],
        'engine_pass': row['engine_compatibility_pass'], 'reused_channels': 4,
        'release_approved': False}), flush=True)


if __name__ == '__main__':
    main()
