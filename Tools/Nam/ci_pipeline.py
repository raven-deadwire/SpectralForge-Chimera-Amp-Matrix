#!/usr/bin/env python3
"""Exact-source NAM CI evidence. Compatibility PASS never grants release approval."""
import argparse
import json
import math
import os
import shutil
from pathlib import Path
import subprocess
import sys
import traceback

from ci_sources import ROOT, LOCK, sha256, verify

NAMES = ('Fenrir', 'Surtr', 'Nidhoggr', 'Fimbulvetr', 'Ragnarok')
THRESHOLDS = {'window_esr_median': .005, 'window_esr_p95': .01, 'window_esr_worst': .02,
              'rms_error_db': .5, 'peak_error_db': 1.}


def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + '\n')


def require(condition, reason):
    if not condition:
        raise RuntimeError(reason)


def numeric_pass(row):
    return all(key in row and isinstance(row[key], (int, float)) and math.isfinite(row[key])
               and abs(row[key]) <= limit for key, limit in THRESHOLDS.items())


def training_plan(profile, steps):
    if profile == 'smoke':
        return [('run', 'legacy', 2, None)]
    if profile != 'full' or not 1 <= steps <= 50000:
        raise ValueError('Invalid full training request')
    # Common physical initialization; both arms reset the optimizer and RNG.
    return [('initialization', 'legacy', 2, None),
            ('recipes/legacy', 'legacy', steps, 'initialization'),
            ('recipes/official-a2', 'official-a2', steps, 'initialization')]


def accuracy_diagnostics(out):
    from quality_profile import PROFILE
    checks = {
        'active_median': ('active_window_esr_median_max', False, False),
        'active_p95': ('active_window_esr_p95_max', False, False),
        'active_worst': ('active_window_esr_worst_max', False, False),
        'full_esr': ('full_esr_max_exclusive', False, True),
        'quiet_worst_residual_rms_dbfs': ('quiet_residual_rms_max_dbfs', False, False),
        'quiet_worst_residual_peak_dbfs': ('quiet_residual_peak_max_dbfs', False, False),
        'rms_error_db': ('rms_error_max_db', True, False),
        'peak_error_db': ('peak_error_max_db', True, False),
    }
    report = {'quality_profile': PROFILE, 'release_approved': False, 'splits': {}}
    for split, file in [('validation', 'validation.json'), ('heldout', 'comparison.json')]:
        rows = json.loads((out / 'package/Validation' / file).read_text())['channels']
        report['splits'][split] = {}
        for name in NAMES:
            quality = rows[name]['tone3000_fidelity']
            failed = []
            for metric, (policy, absolute, exclusive) in checks.items():
                value = quality[metric]
                comparable = abs(value) if absolute and value is not None else value
                valid = type(comparable) in (int, float) and math.isfinite(comparable)
                passed = valid and (comparable < PROFILE[policy] if exclusive else comparable <= PROFILE[policy])
                if not passed:
                    failed.append({'metric': metric, 'observed': value, 'limit': PROFILE[policy],
                                   'absolute': absolute, 'exclusive': exclusive})
            report['splits'][split][name] = {'fidelity_pass': quality['fidelity_pass'], 'failed_conditions': failed}
            print(json.dumps({'split': split, 'channel': name, 'failed_conditions': failed}), flush=True)
    write(out / 'accuracy-diagnostics.json', report)
    return report


def verify_dataset(data):
    manifest = json.loads((data / 'manifest.json').read_text())
    require(manifest['source']['source_commit'] == LOCK['capture'], 'Capture source mismatch')
    seen = set()
    for row in manifest['captures']:
        key = (row['split'], row['name'])
        require(key not in seen, 'Duplicate capture')
        seen.add(key)
        for kind in ('input', 'output'):
            path = (data / row[kind]).resolve()
            require(path.is_relative_to(data.resolve()), 'Capture path escapes dataset')
            require(sha256(path) == row[kind + '_sha256'], 'Audio hash mismatch: ' + str(path))
    require(seen == {(s, n) for s in ('train', 'validation', 'test', 'audition') for n in NAMES},
            'Missing or unexpected channel/split')
    return manifest


def verify_training(out, manifest, run=None, expected_steps=None):
    run = out / 'run' if run is None else run
    config = json.loads((run / 'training-config.json').read_text())
    require(config['dataset_sha256'] == sha256(out / 'data/manifest.json'), 'Training dataset identity mismatch')
    require(config['source'] == manifest['source'], 'Training capture source mismatch')
    require(config['trainer_commit'] == LOCK['trainer']['commit'] and
            config['target_player_commit'] == LOCK['player']['commit'], 'Training dependency mismatch')
    require((run / 'checkpoint.pt').is_file(), 'Missing training checkpoint')
    validation = json.loads((run / 'validation.json').read_text())
    if expected_steps is not None:
        import torch
        checkpoint = torch.load(run / 'checkpoint.pt', map_location='cpu', weights_only=False)
        require(checkpoint['step'] == validation['checkpoint_step'] == expected_steps, 'Training budget mismatch')
        require(checkpoint['identity'] == validation['identity'] == config['identity'], 'Checkpoint identity mismatch')
        require(checkpoint['recipe'] == validation['recipe'] == config['recipe'], 'Checkpoint recipe mismatch')
        require(config['quality_profile'] == validation['quality_profile'], 'Training quality profile mismatch')
        require(checkpoint['best_steps'] == [validation['channels'][n]['selected_step'] for n in NAMES],
                'Checkpoint selection mismatch')
    require(set(validation['channels']) == set(NAMES), 'Missing training channel')
    for name in NAMES:
        row = validation['channels'][name]
        require(row['sha256'] == sha256(run / f'Nastrond-{name}.nam'), 'Training model hash mismatch')
        delta = row['official_roundtrip_max_abs']
        require(math.isfinite(delta) and delta <= 1e-6, 'Official A2 roundtrip failed')


def command(out, label, argv):
    with (out / 'logs' / (label + '.log')).open('w') as log:
        log.write(json.dumps([str(v) for v in argv]) + '\n')
        log.flush()
        result = subprocess.run([str(v) for v in argv], stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        print((out / 'logs' / (label + '.log')).read_text()[-12000:], flush=True)
        raise RuntimeError(f'{label} failed ({result.returncode})')


def grouped_equivalence(trainer):
    import copy
    import numpy as np
    import torch
    from nam.models.wavenet import PackedWaveNet
    from train_a2 import enable_grouped
    torch.set_num_threads(2)
    torch.manual_seed(6422)
    cfg = json.loads((trainer / 'nam/train/_resources/config_model_packed.json').read_text())['net']['config']['submodels'][-1]['config']
    config = {'sample_rate': 48000, 'submodels': [{'name': n, 'config': copy.deepcopy(cfg)} for n in NAMES]}
    upstream = PackedWaveNet.init_from_config(copy.deepcopy(config))
    grouped = copy.deepcopy(upstream)
    enable_grouped(grouped)
    x = torch.randn(1, upstream.receptive_field + 128) * .1
    expected, actual = upstream(x, pad_start=False), grouped(x, pad_start=False)
    expected.square().mean().backward()
    actual.square().mean().backward()
    output = float((expected - actual).abs().max().detach())
    gradient = max(float((a.grad-b.grad).abs().max()) for a, b in zip(upstream.parameters(), grouped.parameters()) if a.grad is not None)
    require(np.isfinite(output) and output <= 1e-6 and np.isfinite(gradient) and gradient <= 1e-6,
            'Grouped A2 shortcut differs from official output/gradients')
    return {'output_max_abs': output, 'gradient_max_abs': gradient, 'tolerance': 1e-6, 'status': 'PASS'}


def compare_package(out, tool):
    import numpy as np
    import soundfile as sf
    from train_a2 import scores
    from quality_profile import PROFILE, fidelity
    package, data = out / 'package', out / 'data'
    validation = json.loads((package / 'Validation/validation.json').read_text())
    require(validation['quality_profile'] == PROFILE, 'Package quality profile mismatch')
    require(set(validation['channels']) == set(NAMES), 'Expected exactly five packaged channels')
    x = np.load(data / 'test/input.npy')
    raw, rendered = out / 'comparison-input.f32', out / 'comparison-output.f32'
    x.astype('<f4').tofile(raw)
    job = out / 'comparison-job.json'
    write(job, {'normalize': False, 'block_size': 64})
    report = {'input_kind': 'reserved synthetic; not instrument DI', 'input_sha256': sha256(data / 'test/input.npy'),
              'protocol': 'Native vs final calibrated NAM; same fixed output scale, no fitted alignment or EQ',
              'legacy_thresholds': THRESHOLDS, 'quality_profile': PROFILE, 'channels': {}}
    for name in NAMES:
        row = validation['channels'][name]
        model = package / 'NAM' / f'Nastrond-{name}.nam'
        preset = package / 'TONE3000-Presets' / f'Nastrond-{name}.t3kpreset'
        require(sha256(model) == row['model_sha256'] and sha256(preset) == row['preset_sha256'], 'Package hash mismatch')
        runtime = json.loads(subprocess.check_output([str(tool), 'render', str(model), str(raw), str(rendered), str(job)], text=True))
        require(runtime['a2_gate'] and runtime['a2_channels'] == 8, 'Not A2-Full')
        y = np.fromfile(rendered, dtype='<f4')
        ref = np.load(data / 'test' / (name + '.npy')) * row['reference_output_scale']
        require(y.shape == ref.shape and np.isfinite(y).all(), 'Invalid target engine audio')
        stat = scores(y, ref)[0]
        windows = [scores(y[s:s+4096], ref[s:s+4096], 0)[0]['esr']
                   for s in range(6347, len(ref)-4096, 4096) if np.mean(ref[s:s+4096]**2) > 1e-8]
        require(bool(windows), 'No eligible held-out windows')
        stat.update(window_esr_median=float(np.median(windows)), window_esr_p95=float(np.quantile(windows, .95)),
                    window_esr_worst=float(max(windows)))
        sf.write(package / 'Audio' / f'{name}-Heldout-Native-left-NAM-right.wav', np.stack([ref, y], axis=1), 48000, subtype='FLOAT')
        settings = package / 'Settings' / (name + '.json')
        subprocess.check_output([str(tool), 'render', str(model), str(raw), str(rendered), str(settings)])
        eq = np.fromfile(rendered, dtype='<f4')
        require(float(np.max(np.abs(eq-y))) > 1e-7, 'EQ has no measured effect')
        subprocess.check_output([str(tool), 'render-preset', str(preset), str(raw), str(rendered)])
        restored = np.fromfile(rendered, dtype='<f4')
        delta = float(np.max(np.abs(eq-restored)))
        require(math.isfinite(delta) and delta <= 1e-6, 'Saved preset audio does not restore')
        quality = fidelity(y / row['reference_output_scale'], ref / row['reference_output_scale'], name)
        report['channels'][name] = {'metrics': stat, 'legacy_all_relative_pass': numeric_pass(stat),
            'tone3000_fidelity': quality, 'numerical_pass': quality['fidelity_pass'],
            'model_sha256': sha256(model), 'preset_sha256': sha256(preset),
            'preset_restore_max_abs': delta, 'a2_channels': runtime['a2_channels']}
    raw.unlink()
    rendered.unlink()
    write(package / 'Validation/comparison.json', report)
    validation['final_test'] = 'reserved synthetic evaluated after training/export; see comparison.json'
    validation['validation_accuracy_pass'] = validation['numerical_accuracy_pass']
    validation['independent_test_accuracy_pass'] = all(r['numerical_pass'] for r in report['channels'].values())
    validation['numerical_accuracy_pass'] &= validation['independent_test_accuracy_pass']
    write(package / 'Validation/validation.json', validation)
    readme = package / 'README-KO.md'
    text = readme.read_text().replace('합성 신호로 학습·검증했으며, 최종 테스트 신호는 아직 사용하지 않았습니다.',
        '합성 신호로 학습·검증했고, 학습에서 제외한 합성 테스트를 최종 모델로 평가했습니다. 결과는 `Validation/comparison.json`에 있습니다.')
    if not validation['numerical_accuracy_pass']:
        text = text.replace('설계상 수치 기준 전체 통과: 예.', '설계상 수치 기준 전체 통과: 아니오 — 검증 또는 별도 테스트 기준 미달.')
        text = text.replace('TONE3000 타깃 프로젝트 수치 기준 전체 통과: 예.', 'TONE3000 타깃 프로젝트 수치 기준 전체 통과: 아니오 — 검증 또는 별도 테스트 기준 미달.')
    readme.write_text(text + '\nCI 실행 상태와 별도 BLOCKED 항목은 상위 `acceptance.json`을 참조하세요. 릴리즈 승인은 부여하지 않습니다.\n')
    return validation


def seal(out, expected, profile):
    # Always runs, including setup/build failure. Inventory includes partial evidence.
    path = out / 'acceptance.json'
    status = json.loads(path.read_text()) if path.exists() else {'status': 'FAIL', 'error': 'Pipeline did not complete'}
    status.update(exact_source=expected, profile=profile, release_approved=False,
                  instrument_DI_listening='BLOCKED', GUI_DAW_acceptance='BLOCKED')
    write(path, status)
    package = out / 'package'
    if package.exists():
        (package / 'SHA256SUMS').write_text(''.join(
            f'{sha256(p)}  {p.relative_to(package).as_posix()}\n'
            for p in sorted(package.rglob('*')) if p.is_file() and p.name != 'SHA256SUMS'))
    sums = {p.relative_to(out).as_posix(): sha256(p) for p in sorted(out.rglob('*'))
            if p.is_file() and p not in (out / 'artifact-manifest.json', out / 'SHA256SUMS')}
    write(out / 'artifact-manifest.json', {'schema': 1, 'exact_source': expected, 'capture_source': LOCK['capture'],
        'profile': profile, 'run_id': os.environ.get('GITHUB_RUN_ID'), 'run_attempt': os.environ.get('GITHUB_RUN_ATTEMPT'),
        'release_approved': False, 'files': sums})
    sums['artifact-manifest.json'] = sha256(out / 'artifact-manifest.json')
    (out / 'SHA256SUMS').write_text(''.join(f'{h}  {p}\n' for p, h in sorted(sums.items())))


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--profile', choices=['smoke', 'full'], required=True)
    p.add_argument('--expected-source', required=True)
    p.add_argument('--deps', type=Path)
    p.add_argument('--renderer', type=Path)
    p.add_argument('--tool', type=Path)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--steps', type=int, default=5000)
    p.add_argument('--seal-only', action='store_true')
    a = p.parse_args()
    out = a.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    if a.seal_only:
        seal(out, a.expected_source, a.profile)
        return
    (out / 'logs').mkdir(exist_ok=True)
    status = {'status': 'FAIL', 'release_approved': False, 'compatibility': 'BLOCKED', 'accuracy': 'BLOCKED'}
    try:
        require(1 <= a.steps <= 50000, 'Full steps must be in 1..50000')
        before = json.loads((out / 'source.json').read_text())
        current = verify(a.deps, a.expected_source)
        require({k: before[k] for k in current} == current, 'Source changed after build preflight')
        # Fail before importing a possibly unrelated nam package.
        import nam
        require(Path(nam.__file__).resolve().is_relative_to((a.deps / 'trainer').resolve()), 'Wrong NAM Python source')
        write(out / 'grouped-equivalence.json', grouped_equivalence(a.deps / 'trainer'))
        write(out / 'executables.json', {k: sha256(v) for k, v in [('renderer', a.renderer), ('player_tool', a.tool)]})
        script = ROOT / 'Tools/Nam'
        capture = [sys.executable, script / 'capture_channels.py', '--renderer', a.renderer, '--out', out / 'data', '--workers', '2']
        if a.profile == 'smoke':
            capture += ['--smoke-seconds', '2']
        command(out, 'capture', capture)
        manifest = verify_dataset(out / 'data')
        require(manifest['source']['source_hashes'] == current['capture_headers'], 'Capture header hash mismatch')
        require(manifest['source']['renderer_sha256'] == sha256(a.renderer), 'Renderer hash mismatch')
        for folder, recipe, steps, warm_start in training_plan(a.profile, a.steps):
            argv = [sys.executable, script / 'train_a2.py', '--data', out / 'data', '--out', out / folder,
                '--trainer', a.deps / 'trainer', '--steps', str(steps), '--recipe', recipe,
                '--threads', '2', '--batch', '1' if a.profile == 'smoke' else '2',
                '--frames', '256' if a.profile == 'smoke' else '2048', '--validation-selection', 'full']
            if warm_start:
                argv += ['--warm-start', out / warm_start]
            command(out, 'train-' + folder.replace('/', '-'), argv)
        if a.profile == 'full':
            from recipe_selection import select_recipe
            for recipe in ('legacy', 'official-a2'):
                verify_training(out, manifest, out / 'recipes' / recipe, a.steps)
            arms = {r: {'config': json.loads((out / 'recipes' / r / 'training-config.json').read_text()),
                        'validation': json.loads((out / 'recipes' / r / 'validation.json').read_text())}
                    for r in ('legacy', 'official-a2')}
            selection = select_recipe(arms)
            write(out / 'recipe-selection.json', selection)
            shutil.copytree(out / 'recipes' / selection['selected_recipe'], out / 'run')
        verify_dataset(out / 'data')
        verify_training(out, manifest)
        command(out, 'package', [sys.executable, script / 'package_a2.py', '--data', out / 'data', '--run', out / 'run',
            '--out', out / 'package', '--tool', a.tool])
        result = compare_package(out, a.tool)
        accuracy_diagnostics(out)
        require((out / 'package/Training/checkpoint.pt').is_file(), 'Packaged checkpoint missing')
        require(verify(a.deps, a.expected_source) == current, 'Source changed during pipeline')
        verify_dataset(out / 'data')
        status.update(compatibility='PASS', accuracy='PASS' if result['numerical_accuracy_pass'] else 'FAIL',
                      status='PASS' if a.profile == 'smoke' or result['numerical_accuracy_pass'] else 'FAIL',
                      scope='smoke compatibility only' if a.profile == 'smoke' else 'full numerical qualification; manual acceptance blocked')
    except Exception as error:
        status['error'] = str(error)
        traceback.print_exc()
    finally:
        write(out / 'acceptance.json', status)
        seal(out, a.expected_source, a.profile)
    require(status['status'] == 'PASS', 'NAM evidence failed; see acceptance.json (artifact retained)')


if __name__ == '__main__':
    main()
