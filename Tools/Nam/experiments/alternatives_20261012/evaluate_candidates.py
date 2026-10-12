#!/usr/bin/env python3
"""Frozen model/POST-EQ candidates: actual engine, new input, old regression.

Measurement completion is separate from sonic acceptance. Never changes a
candidate, a policy, or release approval after opening a test.
"""
import argparse
import itertools
import json
from pathlib import Path
import re
import subprocess
import sys
import uuid

import numpy as np
from scipy import signal as scipy_signal
import torch
from nam.models._from_nam import init_from_nam

NAM_DIR = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(NAM_DIR))
from common import SOURCE, digest, signal, write_json
from quality_profile import PROFILE, fidelity
from train_a2 import predict
from memory_factorization import load_eq_sos

NAMES = ('Fenrir', 'Surtr', 'Nidhoggr', 'Fimbulvetr', 'Ragnarok')
BANDS = ((20, 120), (120, 400), (400, 2000), (2000, 6000), (6000, 20000))
PINNED_DEPENDENCIES = {
    'player': 'ef6f178ae1ac6412b55fec6f058d86e640e5aaaf',
    'core': '1f42f88535884450104b8711d7595019afa0495b',
    'juce': 'd6181bde38d858c283c3b7bf699ce6340c050b5d'}


def run(*args):
    result = subprocess.run(list(map(str, args)), capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(json.dumps({'args': list(map(str, args)),
                                      'code': result.returncode, 'stderr': result.stderr}))
    return json.loads(result.stdout)


def check_audio(values, samples, context):
    values = np.asarray(values)
    if values.ndim != 1 or len(values) != samples or not np.isfinite(values).all():
        raise RuntimeError('Incomplete or nonfinite audio: ' + str(context))
    return values


def read_audio(path, samples):
    if path.stat().st_size != samples * 4:
        raise RuntimeError('Incomplete float32 output: ' + str(path))
    return check_audio(np.fromfile(path, dtype='<f4'), samples, path)


def check_runtime(runtime, samples, *, target=True):
    if runtime.get('samples') != samples or runtime.get('sample_rate') != 48000:
        raise RuntimeError('Runtime returned wrong sample count or rate')
    if target and (runtime.get('a2_gate') is not True or runtime.get('a2_channels') != 8
                   or runtime.get('normalize') is not False or runtime.get('normalization_gain') != 1):
        raise RuntimeError('Target rejects A2 Full or applies unexpected normalization')


def apply_post_eq(values, job):
    # Native BlockEq parity was verified separately for all five frozen jobs.
    # Coefficients AND recursive states must be float32; float64 states describe
    # a different numerical filter, especially for the cancelling bell pairs.
    values = np.asarray(values, dtype=np.float32)
    if job.get('eq', {}).get('enabled', False):
        values = scipy_signal.sosfilt(load_eq_sos(job).astype(np.float32), values)
    return values


def frozen_selection(experiment, seed, manifest_path):
    selection = json.loads((experiment / 'selection.json').read_text())
    fit_plan = json.loads((experiment / 'plan.json').read_text())
    validation = json.loads((experiment / 'validation.json').read_text())
    if (selection['plan_sha256'] != digest(experiment / 'plan.json')
            or selection['validation_sha256'] != digest(experiment / 'validation.json')
            or validation['plan_sha256'] != selection['plan_sha256']
            or set(validation['channels']) != set(NAMES)):
        raise RuntimeError('Incomplete or changed validation selection')
    if fit_plan['future_test_seed'] != seed or fit_plan['future_test_seconds'] != 24:
        raise RuntimeError('Test seed/duration differs from the plan frozen before fitting')
    if (fit_plan['data_manifest_sha256'] != digest(manifest_path)
            or fit_plan['policy_sha256'] != digest(NAM_DIR / 'quality_profile.py')
            or fit_plan['quality_profile'] != PROFILE
            or fit_plan['eq_implementation_sha256'] != digest(Path(__file__).with_name('memory_factorization.py'))):
        raise RuntimeError('Fit/evaluation source, policy, or EQ implementation differs')
    if selection.get('release_approved') is not False:
        raise RuntimeError('Research selection must not approve release')
    return selection


def frozen_roles(selection, name, model_hash, job_hash):
    entries = {'baseline': selection['baseline'], 'selected': selection['selected'],
               **{'best-esr-' + mode: rows for mode, rows in selection['best_esr'].items()}}
    roles = [role for role, rows in entries.items()
             if rows[name]['model_sha256'] == model_hash and rows[name]['job_sha256'] == job_hash]
    if not roles:
        raise RuntimeError('Model/job pair is absent from frozen validation selection: ' + name)
    return roles


def character(y, ref):
    """Numerical spectral/dynamic descriptions, never listening approval."""
    def describe(x):
        x = np.asarray(x, np.float64)[PROFILE['skip_samples']:]
        f, p = scipy_signal.welch(x, fs=48000, nperseg=8192, noverlap=4096)
        levels = [float(10 * np.log10(max(np.sum(p[(f >= lo) & (f < hi)]) * (f[1] - f[0]), 1e-20)))
                  for lo, hi in BANDS]
        crest = float(20 * np.log10(max(np.max(np.abs(x)), 1e-20) /
                                   max(np.sqrt(np.mean(x*x)), 1e-20)))
        return {'band_power_db': levels, 'crest_db': crest}
    a, b = describe(y), describe(ref)
    return {'prediction': a, 'reference': b,
            'band_error_db': [x-y for x, y in zip(a['band_power_db'], b['band_power_db'])],
            'crest_error_db': a['crest_db'] - b['crest_db'],
            'scope': 'Fixed channel snapshot, synthetic excitation, numerical proxy only; no musical/DI listening verdict'}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for item in ('data', 'experiment', 'renderer', 'tool', 'out'):
        p.add_argument('--' + item, type=Path, required=True)
    p.add_argument('--arm', action='append', required=True, help='LABEL=MODEL_FOLDER; optional Name-job.json per channel')
    p.add_argument('--old-test', type=Path)
    p.add_argument('--build-record', type=Path,
                   default=Path(__file__).with_name('native-tool-build.json'))
    p.add_argument('--seed', type=int, default=2610122201)
    a = p.parse_args()
    for item in ('data', 'experiment', 'renderer', 'tool', 'out', 'build_record', 'old_test'):
        if getattr(a, item) is not None:
            setattr(a, item, getattr(a, item).resolve())
    torch.set_num_threads(2)
    if a.out.exists() and any(a.out.iterdir()):
        raise RuntimeError('Fresh output directory required')
    arms = dict(entry.split('=', 1) for entry in a.arm)
    if len(arms) != len(a.arm):
        raise ValueError('Duplicate arm label')
    if any(not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_-]*', label) for label in arms):
        raise ValueError('Arm labels must be safe filename components')
    manifest_path = a.data / 'manifest.json'
    manifest = json.loads(manifest_path.read_text())
    if manifest['source']['source_commit'] != SOURCE:
        raise RuntimeError('Wrong teacher source')
    for path, expected in manifest['source']['source_hashes'].items():
        if digest(NAM_DIR.parent.parent / path) != expected:
            raise RuntimeError('Teacher source changed')
    selection = a.experiment / 'selection.json'
    if not selection.is_file():
        raise RuntimeError('No frozen validation selection')
    selection_data = frozen_selection(a.experiment, a.seed, manifest_path)
    build = json.loads(a.build_record.read_text())
    if (build['status'] != 'pass' or build['capture_source'] != SOURCE
            or build['dependencies'] != PINNED_DEPENDENCIES
            or digest(a.tool) != build['binaries']['ChimeraTone3000Tool']['sha256']
            or digest(a.renderer) != build['binaries']['ChimeraNamRender']['sha256']
            or {row['channel'] for row in build['channels'] if row['bit_exact']} != set(NAMES)):
        raise RuntimeError('Runtime does not match verified pinned native build')
    sealed_files = [selection, a.experiment/'plan.json', a.experiment/'validation.json',
                    manifest_path, a.build_record, Path(__file__), NAM_DIR/'common.py',
                    NAM_DIR/'train_a2.py', NAM_DIR/'quality_profile.py',
                    Path(__file__).with_name('memory_factorization.py'), a.renderer, a.tool]
    sealed_files += [NAM_DIR.parent.parent/path for path in manifest['source']['source_hashes']]
    if a.old_test:
        sealed_files += [a.old_test/'input.npy'] + [a.old_test/(n+'-native.npy') for n in NAMES]
    jobs, files, roles = {}, {}, {}
    a.out.mkdir(parents=True, exist_ok=True)
    for label, folder in arms.items():
        jobs[label], files[label], roles[label] = {}, {}, {}
        for name in NAMES:
            folder_path = Path(folder).resolve()
            model = folder_path / ('Nastrond-' + name + '.nam')
            jobfile = folder_path / (name + '-job.json')
            job = json.loads(jobfile.read_text()) if jobfile.is_file() else {'normalize': False, 'block_size': 64}
            if job.get('normalize', False) or job.get('block_size', 64) != 64:
                raise RuntimeError('Unexpected gain/normalization or block size')
            if 'eq' in job and job['eq'].get('enabled', False):
                load_eq_sos(job)  # Reject unvalidated topology, PRE mode or limits.
            job['normalize'] = False
            job['block_size'] = 64
            target_job = a.out / (label + '-' + name + '-job.json')
            write_json(target_job, job)
            source_job = jobfile if jobfile.is_file() else target_job
            roles[label][name] = frozen_roles(selection_data, name, digest(model), digest(source_job))
            sealed_files += [model, source_job, target_job]
            jobs[label][name] = target_job
            files[label][name] = model
    native_jobs = {}
    for name in NAMES:
        captures = [c for c in manifest['captures'] if c['name'] == name and c['split'] in ('train', 'validation')]
        if len(captures) != 2 or any((c['channel'], c['controls']) !=
                                    (captures[0]['channel'], captures[0]['controls']) for c in captures):
            raise RuntimeError('Training/validation teacher controls differ: ' + name)
        native_jobs[name] = a.out / (name + '-native-job.json')
        write_json(native_jobs[name], {'channel': captures[0]['channel'], 'controls': captures[0]['controls']})
        sealed_files.append(native_jobs[name])
    plan = {'parent_source': '859fea9bc49c4b9981c926a314655e485c51027c',
        'selection_sha256': digest(selection), 'seed': a.seed, 'seconds': 24,
        'quality_profile': PROFILE, 'policy_sha256': digest(NAM_DIR / 'quality_profile.py'),
        'script_sha256': digest(Path(__file__)), 'native_source': manifest['source'],
        'renderer_sha256': digest(a.renderer), 'tool_sha256': digest(a.tool),
        'models': {label: {n: digest(f) for n, f in rows.items()} for label, rows in files.items()},
        'jobs': {label: {n: digest(f) for n, f in rows.items()} for label, rows in jobs.items()},
        'frozen_validation_roles': roles,
        'role_scope': 'best-esr arms are descriptive even when selection guards rejected them; no role is release approval',
        'native_jobs': {n: digest(f) for n, f in native_jobs.items()},
        'sealed_files': {str(f): digest(f) for f in sealed_files},
        'eq_arithmetic': 'Native-rounded float32 SOS coefficients, input and recursive states; zero initial state, continuous full input',
        'old_test': None if a.old_test is None else {
            'kind': 'already revealed 20261011 test, immutable diagnostic regression only',
            'input_sha256': digest(a.old_test / 'input.npy'),
            'native_sha256': {n: digest(a.old_test / (n+'-native.npy')) for n in NAMES}},
        'release_approved': False, 'candidate_adjustment_after_test': False,
        'character_bands_hz': BANDS}
    write_json(a.out / 'test-plan.json', plan)
    plan_hash = digest(a.out / 'test-plan.json')
    # No fresh test input/reference exists before the complete candidate seal.
    x = signal(a.seed, 24)
    np.save(a.out / 'new-input.npy', x)
    known_inputs = {c['input_sha256'] for c in manifest['captures']}
    if a.old_test:
        known_inputs.add(digest(a.old_test / 'input.npy'))
    if digest(a.out / 'new-input.npy') in known_inputs:
        raise RuntimeError('New input duplicates an old split')
    raw = a.out / 'new-input.f32'
    x.tofile(raw)
    input_hashes = {str(f): digest(f) for f in (raw, a.out/'new-input.npy')}
    output = a.out / 'working-output.f32'
    refs = {}
    reference_runtime = {}
    for name in NAMES:
        reference_runtime[name] = run(a.renderer, raw, output, native_jobs[name])
        check_runtime(reference_runtime[name], len(x), target=False)
        refs[name] = read_audio(output, len(x))
        np.save(a.out / ('new-' + name + '-native.npy'), refs[name])
    report = {'plan_sha256': plan_hash, 'input_sha256': digest(a.out / 'new-input.npy'),
        'quality_profile': PROFILE, 'release_approved': False,
        'instrument_DI_listening': 'NOT_EXECUTED', 'channel_role_listening': 'NOT_EXECUTED',
        'TONE3000_GUI_DAW_acceptance': 'NOT_EXECUTED',
        'compatibility_scope': 'Pinned unmodified NamEngine/BlockEq/PresetFile in console harness; no GUI/DAW claim',
        'timing_scope': 'Offline diagnostic under shared compute; no real-time qualification',
        'reference_runtime': reference_runtime,
        'frozen_validation_roles': roles,
        'arms': {label: {} for label in arms}}
    zero = a.out / 'silence.f32'
    np.zeros(48000, dtype='<f4').tofile(zero)
    for label in arms:
        for name in NAMES:
            file, jobfile = files[label][name], jobs[label][name]
            job = json.loads(jobfile.read_text())
            runtime = run(a.tool, 'render', file, raw, output, jobfile)
            check_runtime(runtime, len(x))
            y = read_audio(output, len(x))
            np.save(a.out / ('new-' + label + '-' + name + '.npy'), y)
            model = init_from_nam(json.loads(file.read_text())).eval()
            py = check_audio(apply_post_eq(predict(model, x), job), len(x), 'Python parity')
            residual = (y.astype(np.float64) - py) * PROFILE['source_output_scale'][name]
            parity = float(10*np.log10(max(np.mean(residual*residual), 1e-20)))
            # Compare actual block-state continuity on a different block size.
            block_job = a.out / 'block256-job.json'
            write_json(block_job, {**job, 'block_size': 256})
            runtime256 = run(a.tool, 'render', file, raw, output, block_job)
            check_runtime(runtime256, len(x))
            block_delta = float(np.max(np.abs(read_audio(output, len(x)) - y)))
            settings = a.out / (label + '-' + name + '-preset-settings.json')
            write_json(settings, {**job, 'name': 'Nastrond '+name+' '+label+' Research',
                'id': str(uuid.uuid5(uuid.NAMESPACE_URL, 'nastrond/alternatives-20261012/'+label+'/'+name))})
            preset = a.out / ('Research-' + label + '-' + name + '.t3kpreset')
            preset_metadata = run(a.tool, 'preset', file, preset, settings)
            preset_runtime = run(a.tool, 'render-preset', preset, raw, output)
            check_runtime(preset_runtime, len(x))
            preset_delta = float(np.max(np.abs(read_audio(output, len(x)) - y)))
            silence_runtime = run(a.tool, 'render', file, zero, output, jobfile)
            check_runtime(silence_runtime, 48000)
            silence = read_audio(output, 48000)[PROFILE['skip_samples']:].astype(np.float64)
            silence *= PROFILE['source_output_scale'][name]
            silence_db = float(10*np.log10(max(np.mean(silence*silence), 1e-20)))
            row = {'model_sha256': digest(file), 'job_sha256': digest(jobfile),
                   'new_test': fidelity(y, refs[name], name), 'character': character(y, refs[name]),
                   'runtime64': runtime, 'runtime256': runtime256,
                   'block64_256_max_abs': block_delta,
                   'python_native_parity_residual_dbfs': parity,
                   'preset_restore_max_abs': preset_delta, 'preset_sha256': digest(preset),
                   'preset_metadata': preset_metadata, 'preset_runtime': preset_runtime,
                   'steady_silence_dbfs': silence_db, 'silence_runtime': silence_runtime,
                   'frozen_validation_roles': roles[label][name]}
            row['engine_compatibility_pass'] = bool(parity <= PROFILE['engine_parity_residual_max_dbfs']
                and preset_delta <= 1e-6 and block_delta <= 1e-6
                and silence_db <= PROFILE['steady_silence_rms_max_dbfs'])
            if a.old_test:
                old_x = np.load(a.old_test / 'input.npy', allow_pickle=False)
                check_audio(old_x, len(old_x), 'Previously revealed input')
                old_raw = a.out / 'old-input.f32'
                old_x.tofile(old_raw)
                old_runtime = run(a.tool, 'render', file, old_raw, output, jobfile)
                check_runtime(old_runtime, len(old_x))
                old_y = read_audio(output, len(old_x))
                np.save(a.out / ('old-' + label + '-' + name + '.npy'), old_y)
                row['old_revealed_regression'] = fidelity(old_y,
                    np.load(a.old_test / (name+'-native.npy')), name)
            report['arms'][label][name] = row
            write_json(a.out / 'comparison.json', report)
            print(json.dumps({'arm': label, 'channel': name,
                              'esr': row['new_test']['full_esr'],
                              'fidelity_pass': row['new_test']['fidelity_pass'],
                              'engine_pass': row['engine_compatibility_pass']}), flush=True)
    for label in arms:
        contrasts = []
        for left, right in itertools.combinations(NAMES, 2):
            l = report['arms'][label][left]['character']
            r = report['arms'][label][right]['character']
            native = np.array(l['reference']['band_power_db']) - r['reference']['band_power_db']
            candidate = np.array(l['prediction']['band_power_db']) - r['prediction']['band_power_db']
            contrasts.append({'channels': [left, right], 'native_band_contrast_db': native.tolist(),
                              'candidate_band_contrast_db': candidate.tolist(),
                              'absolute_contrast_error_db': np.abs(candidate-native).tolist()})
        report.setdefault('channel_character_contrasts', {})[label] = contrasts
    if digest(selection) != plan['selection_sha256'] or digest(a.out/'test-plan.json') != plan_hash:
        raise RuntimeError('Selection or plan changed')
    for path, expected in {**plan['sealed_files'], **input_hashes}.items():
        if digest(path) != expected:
            raise RuntimeError('Sealed input/code/job/runtime changed during evaluation: ' + path)
    for label in arms:
        for name in NAMES:
            if digest(files[label][name]) != plan['models'][label][name] or digest(jobs[label][name]) != plan['jobs'][label][name]:
                raise RuntimeError('Candidate changed during test')
    if digest(NAM_DIR/'quality_profile.py') != plan['policy_sha256']:
        raise RuntimeError('Quality policy changed')
    if digest(a.renderer) != plan['renderer_sha256'] or digest(a.tool) != plan['tool_sha256']:
        raise RuntimeError('Runtime binary changed')
    report['complete'] = True
    report['numerical_pass_counts'] = {label: sum(row['new_test']['fidelity_pass'] for row in rows.values())
                                       for label, rows in report['arms'].items()}
    report['engine_pass_counts'] = {label: sum(row['engine_compatibility_pass'] for row in rows.values())
                                   for label, rows in report['arms'].items()}
    write_json(a.out / 'comparison.json', report)
    for path in (output, zero, a.out/'old-input.f32', a.out/'block256-job.json'):
        path.unlink(missing_ok=True)


if __name__ == '__main__':
    main()
