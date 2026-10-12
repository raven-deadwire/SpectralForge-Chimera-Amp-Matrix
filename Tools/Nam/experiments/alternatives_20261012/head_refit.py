#!/usr/bin/env python3
"""Train-only ridge refit of 128 existing A2 head weights, raw and legal POST EQ.

The nonlinear backbone, architecture, receptive field, native teacher and
quality policy stay fixed. No optimizer updates or exposed-test fitting occur.
Run with the pinned official trainer on PYTHONPATH. All audio stays outside git.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import sys
import time

import numpy as np
import torch
from scipy import signal
from nam.models._from_nam import init_from_nam

NAM_DIR = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(NAM_DIR))
from common import digest, write_json
from quality_profile import PROFILE, fidelity
from training_contract import verify_audio_manifest
from train_a2 import predict
from memory_factorization import load_eq_sos

NAMES = ('Fenrir', 'Surtr', 'Nidhoggr', 'Fimbulvetr', 'Ragnarok')
RIDGES = (1e-6, 1e-4, 1e-2)
MODES = ('raw', 'eq-chain')
CHUNK = 32768


def head(model):
    assert model.receptive_field == 6347
    assert len(model._net._layer_arrays) == 1 and model._net._head is None
    h = model._net._layer_arrays[0]._head_rechannel
    assert tuple(h.weight.shape) == (1, 8, 16)
    return h


def capture(model, x):
    """Return real upstream output and its actual causal head-input features."""
    seen = []
    handle = head(model).register_forward_pre_hook(lambda _m, args: seen.append(args[0]))
    try:
        with torch.inference_mode():
            y = model(torch.from_numpy(np.asarray(x, dtype=np.float32).copy()), pad_start=False)
        assert len(seen) == 1 and seen[0].shape[0:2] == (1, 8)
        features = seen[0].unfold(2, 16, 1)[0].permute(1, 0, 2).reshape(-1, 128)
        assert features.shape[0] == y.numel()
        return y.numpy().copy(), features.numpy().astype(np.float64)
    finally:
        handle.remove()


def batches(model, x):
    rf = model.receptive_field
    padded = np.pad(x, (rf - 1, 0))
    for start in range(0, len(x), CHUNK):
        n = min(CHUNK, len(x) - start)
        y, features = capture(model, padded[start:start + rf - 1 + n])
        yield start, y, features


def filter_feature_block(sos, features, state=None, include_constant=False):
    """Apply one fixed causal POST EQ independently to every feature column.

    Carry returned state across all chunks, including the first 6,347 samples.
    The optional constant column propagates the exported head's tiny numerical
    zero-state residual. That residual is computed from exported coefficients;
    it is never a new fitted offset or a target-derived correction.
    """
    values = np.asarray(features, dtype=np.float64)
    if values.ndim != 2:
        raise ValueError('Expected sample-by-feature matrix')
    if include_constant:
        values = np.column_stack((values, np.ones(len(values))))
    if state is None:
        state = np.zeros((len(sos), 2, values.shape[1]), dtype=np.float64)
    return signal.sosfilt(sos, values, axis=0, zi=state)


def ridge_solution(gram, rhs, original_weights, strength):
    """Training-only MSE plus a scale-normalized ridge toward original weights."""
    unit = float(np.trace(gram) / len(original_weights))
    if not np.isfinite(unit) or unit <= 0 or strength not in RIDGES:
        raise ValueError('Invalid frozen ridge fit')
    penalty = strength * unit
    weights = np.linalg.solve(gram + penalty * np.eye(len(original_weights)),
                              rhs + penalty * original_weights)
    if not np.isfinite(weights).all():
        raise RuntimeError('Nonfinite fitted head')
    return weights


def filter_native_output_block(sos, values, state=None):
    """Native-equivalent BlockEq coefficient and state precision for scoring."""
    coefficients = np.asarray(sos, dtype=np.float32)
    values = np.asarray(values, dtype=np.float32)
    if values.ndim != 1:
        raise ValueError('Expected summed mono head output')
    if state is None:
        state = np.zeros((len(coefficients), 2), dtype=np.float32)
    return signal.sosfilt(coefficients, values, zi=state)


def allowed(candidate, baseline):
    """Validation-only guard; these are selection rules, not relaxed acceptance."""
    reasons = []
    for key in ('active_pass', 'quiet_pass', 'level_pass', 'full_esr_pass'):
        if baseline[key] and not candidate[key]:
            reasons.append('new_failure:' + key)
    for key, bound in [('rms_error_db', .5), ('peak_error_db', 1.)]:
        if abs(candidate[key]) > max(bound, abs(baseline[key])) + 1e-6:
            reasons.append('level_regression:' + key)
    for key, bound in [('quiet_worst_residual_rms_dbfs', -70.),
                       ('quiet_worst_residual_peak_dbfs', -60.)]:
        if candidate[key] > max(bound, baseline[key]) + .01:
            reasons.append('quiet_regression:' + key)
    if candidate['full_esr'] > baseline['full_esr'] * .999:
        reasons.append('no_0.1percent_esr_improvement')
    return reasons


def export_head(model, weights, zero_features, original, out, name):
    candidate = copy.deepcopy(model).eval()
    h = head(candidate)
    with torch.no_grad():
        h.weight.copy_(torch.tensor(weights, dtype=torch.float32).reshape(1, 8, 16))
        h.bias.copy_(torch.tensor([-float(zero_features @ weights)], dtype=torch.float32))
        # Same known zero-history bias correction used by PR22 export.
        dc = candidate(torch.zeros(candidate.receptive_field + 512), pad_start=False)[-1]
        h.bias.sub_(dc / candidate._net._head_scale)
    candidate.export(out, basename='Nastrond-' + name, include_snapshot=False,
        other_metadata={'name': 'Nastrond ' + name + ' head-refit research',
            'status': 'RESEARCH_NOT_RELEASE_APPROVED', 'release_approved': False,
            'source_model_sha256': digest(original),
            'method': '128 existing output coefficients; frozen nonlinear backbone'})
    file = out / ('Nastrond-' + name + '.nam')
    obj = json.loads(file.read_text())
    if obj['config'] != json.loads(original.read_text())['config']:
        raise RuntimeError('Export changed A2 config')
    restored = init_from_nam(obj).eval()
    # Compare the fitted pre-export state with the import, not two imports of
    # the same file. In particular this validates the newly fitted head's tap
    # order and bias, which the frozen-backbone check below does not cover.
    restored_state = restored.state_dict()
    for key, value in candidate.state_dict().items():
        if key not in restored_state or not torch.equal(value, restored_state[key]):
            raise RuntimeError('Fitted model export roundtrip changed: ' + key)
    for key, value in model.state_dict().items():
        if '_head_rechannel.' not in key and not torch.equal(value, restored_state[key]):
            raise RuntimeError('Backbone changed: ' + key)
    return restored, file


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('data', 'models', 'out'):
        p.add_argument('--' + name, type=Path, required=True)
    p.add_argument('--eq-jobs', type=Path,
                   default=Path(__file__).resolve().parent / 'memory-factorization')
    a = p.parse_args()
    if a.out.exists() and any(a.out.iterdir()):
        raise RuntimeError('Fresh experiment directory required')
    torch.set_num_threads(2)
    torch.manual_seed(261012)
    manifest = verify_audio_manifest(a.data)
    for path, expected in manifest['source']['source_hashes'].items():
        if digest(NAM_DIR.parent.parent / path) != expected:
            raise RuntimeError('Frozen native source mismatch: ' + path)
    # Both modes, all three penalties, and all source-derived EQ settings are
    # fixed and hash-recorded before any train fit or validation inference.
    eq_source_paths = {name: a.eq_jobs / (name + '-factor-post-eq.json') for name in NAMES}
    eq_source_hashes = {name: digest(path) for name, path in eq_source_paths.items()}
    jobs = {'raw': {}, 'eq-chain': {}}
    soses = {}
    for name in NAMES:
        source_job = json.loads(eq_source_paths[name].read_text())
        if source_job.get('normalize') is not False:
            raise RuntimeError('EQ job must explicitly disable normalization')
        soses[name] = load_eq_sos(source_job)
        if any(np.max(np.abs(np.roots(row[3:]))) >= 1 for row in soses[name]):
            raise RuntimeError('Unstable frozen POST EQ')
        jobs['raw'][name] = {'normalize': False, 'block_size': 64}
        jobs['eq-chain'][name] = {**source_job, 'block_size': 64}
    a.out.mkdir(parents=True, exist_ok=True)
    frozen_jobs = a.out / 'frozen-jobs'
    frozen_jobs.mkdir()
    job_paths = {mode: {} for mode in MODES}
    for mode in MODES:
        (frozen_jobs / mode).mkdir()
        for name in NAMES:
            path = frozen_jobs / mode / (name + '-job.json')
            write_json(path, jobs[mode][name])
            job_paths[mode][name] = path
    plan = {'method': 'train-only centered-head ridge regression; raw and fixed legal POST EQ',
        'parent_source': '859fea9bc49c4b9981c926a314655e485c51027c',
        'native_source': manifest['source'], 'quality_profile': PROFILE,
        'policy_sha256': digest(NAM_DIR / 'quality_profile.py'),
        'script_sha256': digest(Path(__file__)), 'model_sha256': {
            name: digest(a.models / ('Nastrond-' + name + '.nam')) for name in NAMES},
        'eq_implementation_sha256': digest(Path(__file__).with_name('memory_factorization.py')),
        'eq_source_job_sha256': eq_source_hashes,
        'frozen_job_sha256': {mode: {name: digest(path) for name, path in paths.items()}
                              for mode, paths in job_paths.items()},
        'rounded_post_eq_sos': {name: sos.tolist() for name, sos in soses.items()},
        'data_manifest_sha256': digest(a.data / 'manifest.json'),
        'fit_split': 'train only; all frames after original 6347-sample skip',
        'selection_split': 'validation only; old test bytes hash-verified but never decoded or used for fitting/selection',
        'modes': MODES, 'ridge_strengths': RIDGES,
        'ridge_units': 'lambda * trace(mode-specific train_gram)/128',
        'objective': 'unweighted physical-output MSE + ridge toward original head',
        'eq_chain_fit': 'continuous H((features-zero_features)*head_scale), then original skip; unchanged native output target',
        'eq_chain_arithmetic': 'native-rounded coefficients; float64 SOS feature states for linear fit; float32 SOS output states for validation; actual target engine comparison still required',
        'selection': 'minimum validation ESR among six guard-passing candidates; raw first on ties; else original raw baseline',
        'descriptive_comparison': 'best ESR within each mode frozen separately even if rejected; neither is approval',
        'guard': 'no newly failed components; level errors cannot exceed max(baseline absolute error, original limit); quiet errors cannot exceed max(baseline, original limit)+0.01dB; ESR reduction>=0.1%',
        'future_test_seed': 2610122201, 'future_test_seconds': 24,
        'torch': torch.__version__, 'threads': 2, 'release_approved': False}
    write_json(a.out / 'plan.json', plan)
    plan_hash = digest(a.out / 'plan.json')
    x = np.load(a.data / 'train/input.npy')
    vx = np.load(a.data / 'validation/input.npy')
    report = {'plan_sha256': plan_hash, 'quality_profile': PROFILE,
              'release_approved': False, 'channels': {}}
    for name in NAMES:
        started = time.monotonic()
        original = a.models / ('Nastrond-' + name + '.nam')
        model = init_from_nam(json.loads(original.read_text())).eval()
        w0 = head(model).weight.detach().numpy().reshape(128).astype(np.float64)
        scale = float(model._net._head_scale)
        zpred, zfeatures = capture(model, np.zeros(model.receptive_field + 1, np.float32))
        if not np.array_equal(zfeatures[0], zfeatures[1]):
            raise RuntimeError('Nonconstant zero-history features')
        z = zfeatures[-1]
        y = np.load(a.data / 'train' / (name + '.npy'))
        grams = {mode: np.zeros((128, 128), np.float64) for mode in MODES}
        right_sides = {mode: np.zeros(128, np.float64) for mode in MODES}
        baseline_train = np.empty_like(y)
        count = 0
        reconstruction = 0.
        train_eq_state = None
        for start, raw, features in batches(model, x):
            stop = start + len(raw)
            baseline_train[start:stop] = raw
            reconstructed = scale * (features @ w0 + float(head(model).bias.detach()[0]))
            reconstruction = max(reconstruction, float(np.max(np.abs(raw - reconstructed))))
            local_skip = max(0, PROFILE['skip_samples'] - start)
            phi = (features - z) * scale
            filtered, train_eq_state = filter_feature_block(soses[name], phi, train_eq_state)
            target = y[start + local_skip:stop].astype(np.float64)
            for mode, full_phi in [('raw', phi), ('eq-chain', filtered)]:
                scored_phi = full_phi[local_skip:]
                grams[mode] += scored_phi.T @ scored_phi
                right_sides[mode] += scored_phi.T @ target
            count += len(target)
            if stop % (CHUNK * 40) == 0 or stop == len(x):
                print(json.dumps({'channel': name, 'phase': 'train_features',
                                  'samples': stop, 'total': len(x),
                                  'seconds': round(time.monotonic() - started, 2)}), flush=True)
        if reconstruction > 2e-5:
            raise RuntimeError('Causal feature reconstruction failed: ' + str(reconstruction))
        (a.out / 'baseline').mkdir(exist_ok=True)
        np.save(a.out / 'baseline' / (name + '-train.npy'), baseline_train)
        (a.out / 'baseline' / original.name).write_bytes(original.read_bytes())
        (a.out / 'baseline' / (name + '-job.json')).write_bytes(job_paths['raw'][name].read_bytes())
        for mode in MODES:
            grams[mode] /= count
            right_sides[mode] /= count
            np.savez(a.out / (name + '-' + mode + '-sufficient-statistics.npz'),
                     gram=grams[mode], rhs=right_sides[mode], original_weights=w0,
                     zero_features=z, samples=count, head_scale=scale)
        arms = []
        for mode in MODES:
            for lam in RIDGES:
                weights = ridge_solution(grams[mode], right_sides[mode], w0, lam)
                folder = a.out / ('ridge-' + mode + '-' + str(lam))
                folder.mkdir(exist_ok=True)
                candidate, file = export_head(model, weights, z, original, folder, name)
                job_file = folder / (name + '-job.json')
                job_file.write_bytes(job_paths[mode][name].read_bytes())
                exported_head = head(candidate)
                exported_weights = exported_head.weight.detach().numpy().reshape(-1).astype(np.float64)
                exported_bias = float(exported_head.bias.detach()[0])
                residual = scale * (z @ exported_weights + exported_bias)
                arms.append({'mode': mode, 'ridge': lam, 'model': candidate,
                             'file': file, 'job_file': job_file,
                             'weights': exported_weights,
                             'zero_state_projection_residual': float(residual),
                             'eq_state': None,
                             'prediction': np.empty_like(vx)})
        base_validation = np.empty_like(vx)
        for start, raw, features in batches(model, vx):
            stop = start + len(raw)
            base_validation[start:stop] = raw
            phi = (features - z) * scale
            for arm in arms:
                prediction = (phi @ arm['weights'] + arm['zero_state_projection_residual']).astype(np.float32)
                if arm['mode'] == 'eq-chain':
                    prediction, arm['eq_state'] = filter_native_output_block(
                        soses[name], prediction, arm['eq_state'])
                arm['prediction'][start:stop] = prediction.astype(np.float32)
        ref = np.load(a.data / 'validation' / (name + '.npy'))
        base_metrics = fidelity(base_validation, ref, name)
        np.save(a.out / 'baseline' / (name + '-validation.npy'), base_validation)
        records = []
        for arm in arms:
            candidate, file, pred = arm['model'], arm['file'], arm['prediction']
            with torch.inference_mode():
                roundtrip = init_from_nam(json.loads(file.read_text())).eval()
                delta = float((candidate(torch.from_numpy(vx[:32768])) -
                               roundtrip(torch.from_numpy(vx[:32768]))).abs().max())
            if delta > 1e-6 or not np.isfinite(delta):
                raise RuntimeError('NAM official roundtrip failed')
            metrics = fidelity(pred, ref, name)
            records.append({'mode': arm['mode'], 'ridge': arm['ridge'],
                            'model_sha256': digest(file), 'job_sha256': digest(arm['job_file']),
                            'validation': metrics, 'selection_rejections': allowed(metrics, base_metrics),
                            'zero_state_projection_residual': arm['zero_state_projection_residual'],
                            'official_roundtrip_max_abs': delta})
            np.save(file.parent / (name + '-validation-projection.npy'), pred)
        eligible = [i for i, r in enumerate(records) if not r['selection_rejections']]
        chosen = min(eligible, key=lambda i: records[i]['validation']['full_esr']) if eligible else None
        best_esr = {mode: min((i for i, r in enumerate(records) if r['mode'] == mode),
                             key=lambda i: records[i]['validation']['full_esr']) for mode in MODES}
        # Rejected methods retain a frozen descriptive arm, never release approval.
        copies = [(index, 'best-esr-' + mode) for mode, index in best_esr.items()]
        copies.append((chosen, 'selected'))
        for index, label in copies:
            folder = a.out / label
            folder.mkdir(exist_ok=True)
            src = original if index is None else arms[index]['file']
            job_src = job_paths['raw'][name] if index is None else arms[index]['job_file']
            (folder / src.name).write_bytes(src.read_bytes())
            (folder / (name + '-job.json')).write_bytes(job_src.read_bytes())
        row = {'baseline_validation': base_metrics, 'candidates': records,
               'selected_ridge': None if chosen is None else records[chosen]['ridge'],
               'selected_mode': 'baseline' if chosen is None else records[chosen]['mode'],
               'selected_is_baseline': chosen is None,
               'best_esr_ridge': {mode: records[index]['ridge'] for mode, index in best_esr.items()},
               'causal_feature_reconstruction_max_abs': reconstruction,
               'head_coefficients': 128, 'train_samples_used': count,
               'gram_condition': {mode: float(np.linalg.cond(grams[mode])) for mode in MODES},
               'seconds': round(time.monotonic() - started, 3)}
        report['channels'][name] = row
        write_json(a.out / 'validation.json', report)
        print(json.dumps({'channel': name, 'baseline_esr': base_metrics['full_esr'],
            'head_best_esr': {mode: records[index]['validation']['full_esr'] for mode, index in best_esr.items()},
            'selected_mode': row['selected_mode'], 'selected_ridge': row['selected_ridge'],
            'seconds': row['seconds']}), flush=True)
    if digest(a.out / 'plan.json') != plan_hash:
        raise RuntimeError('Plan mutated')
    for name in NAMES:
        if digest(a.models / ('Nastrond-' + name + '.nam')) != plan['model_sha256'][name]:
            raise RuntimeError('Original model changed')
        if digest(eq_source_paths[name]) != eq_source_hashes[name]:
            raise RuntimeError('Frozen EQ source job changed')
    if digest(Path(__file__)) != plan['script_sha256']:
        raise RuntimeError('Experiment script changed during run')
    if digest(NAM_DIR / 'quality_profile.py') != plan['policy_sha256']:
        raise RuntimeError('Quality policy changed during run')
    if digest(a.data / 'manifest.json') != plan['data_manifest_sha256']:
        raise RuntimeError('Data manifest changed during run')
    verify_audio_manifest(a.data)
    def model_job_hashes(folder, name):
        return {'model_sha256': digest(folder / ('Nastrond-' + name + '.nam')),
                'job_sha256': digest(folder / (name + '-job.json'))}
    selection = {'plan_sha256': plan_hash, 'validation_sha256': digest(a.out / 'validation.json'),
        'baseline': {name: model_job_hashes(a.out / 'baseline', name) for name in NAMES},
        'selected': {name: {**model_job_hashes(a.out / 'selected', name),
                           'mode': report['channels'][name]['selected_mode']} for name in NAMES},
        'best_esr': {mode: {name: model_job_hashes(a.out / ('best-esr-' + mode), name)
                           for name in NAMES} for mode in MODES},
        'release_approved': False, 'instrument_DI_listening': 'NOT_EXECUTED', 'GUI_DAW': 'NOT_EXECUTED'}
    write_json(a.out / 'selection.json', selection)


if __name__ == '__main__':
    main()
