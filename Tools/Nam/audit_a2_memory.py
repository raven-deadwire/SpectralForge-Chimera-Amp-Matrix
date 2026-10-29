#!/usr/bin/env python3
"""Prove finite-memory error bounds on existing captures, without training.

Once every sample in a causal convolutional model's receptive field is zero,
its output is one constant. For the zero-history subset Z of an evaluation
window, even an independently optimal constant leaves SSE >= variance(Z)*|Z|.
Allowing a different constant in every window is more permissive than an A2
model, so the resulting ESR is a lower bound for every choice of its weights.
This does not estimate the attainable error on nonzero-history samples.
"""
import argparse
import concurrent.futures
import json
import subprocess
import tempfile
from pathlib import Path

import numpy as np

from common import digest, write_json
from capture_channels import VOICES


def zero_history_mask(x, rf):
    counts = np.r_[0, np.cumsum(x != 0)]
    end = np.arange(len(x)) + 1
    return (end >= rf) & (counts[end] == counts[np.maximum(0, end-rf)])


def window_bound(ref, mask):
    selected = np.asarray(ref, dtype=np.float64)[mask]
    if not len(selected):
        return 0.0
    denominator = float(np.sum(np.asarray(ref, dtype=np.float64)**2))
    return float(np.sum((selected-selected.mean())**2)/max(denominator, 1e-30))


def receptive_field(model):
    config = model['config']
    if model['architecture'] != 'WaveNet' or len(config['layers']) != 1 or config.get('head') is not None:
        raise ValueError('This audit requires a bare single-array WaveNet')
    layer = config['layers'][0]
    if layer['input_size'] != 1 or layer['condition_size'] != 1:
        raise ValueError('External conditioning is outside this proof')
    return 1 + sum((k-1)*d for k, d in zip(layer['kernel_sizes'], layer['dilations'])) + layer['head']['kernel_size']-1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ['data', 'models', 'package', 'tool', 'player', 'out']:
        parser.add_argument('--'+key, type=Path, required=True)
    args = parser.parse_args()
    manifest = json.loads((args.data/'manifest.json').read_text())
    package_report = json.loads((args.package/'Validation/validation.json').read_text())
    report = {
        'status': 'AUDITING',
        'protocol': 'Existing validation and reserved synthetic inputs. No fitting, gain changes, time alignment, cropping, or altered thresholds.',
        'source_commit': manifest['source']['source_commit'],
        'capture_manifest_sha256': digest(args.data/'manifest.json'),
        'target_player_commit': subprocess.check_output(['git', '-C', str(args.player), 'rev-parse', 'HEAD'], text=True).strip(),
        'target_core_commit': subprocess.check_output(['git', '-C', str(args.player/'plugin/NeuralAmpModelerCore'), 'rev-parse', 'HEAD'], text=True).strip(),
        'player_source_hashes': {p: digest(args.player/p) for p in [
            'plugin/src/ProcessorModelLoader.cpp',
            'plugin/NeuralAmpModelerCore/NAM/wavenet/a2_fast.h',
            'plugin/NeuralAmpModelerCore/NAM/wavenet/a2_fast.cpp']},
        'engine_tool_sha256': digest(args.tool),
        'window_samples': 4096,
        'worst_window_esr_threshold': .02,
        'bound': 'sum((reference[Z]-mean(reference[Z]))**2) / sum(reference**2), where Z has an entirely zero input receptive field',
        'scope': 'Bare A2 NAM at the target 48 kHz chain rate, without external EQ. Covers arbitrary model weights, including arbitrary zero-input bias. Does not prove a floor for active passages, another architecture, or a combined NAM-plus-filter comparison.',
        'inputs': {},
    }
    entries = {(r['split'], r['name']): r for r in manifest['captures']}
    with tempfile.TemporaryDirectory() as temporary:
        temp = Path(temporary)
        job = temp/'job.json'
        write_json(job, {'normalize': False, 'block_size': 64})
        for split in ['validation', 'test']:
            x = np.load(args.data/split/'input.npy')
            raw = temp/'input.f32'
            x.astype('<f4').tofile(raw)
            result = {'input_sha256': digest(args.data/split/'input.npy'), 'channels': {}}

            def audit(voice):
                name = voice[0]
                reference_file = args.data/split/(name+'.npy')
                entry = entries[split, name]
                if digest(reference_file) != entry['output_sha256'] or result['input_sha256'] != entry['input_sha256']:
                    raise ValueError('Capture identity changed')
                ref = np.load(reference_file).astype(np.float64)
                model_file = args.models/('Nastrond-'+name+'.nam')
                model = json.loads(model_file.read_text())
                rf = receptive_field(model)
                if rf != 6347 or model['sample_rate'] != 48000:
                    raise ValueError('Unexpected A2 receptive field or rate')
                mask = zero_history_mask(x, rf)
                rendered = temp/(name+'.f32')
                runtime = json.loads(subprocess.check_output([
                    str(args.tool), 'render', str(model_file), str(raw), str(rendered), str(job)], text=True))
                pred = np.fromfile(rendered, dtype='<f4').astype(np.float64)
                if len(pred) != len(ref) or not np.isfinite(pred).all():
                    raise ValueError('Invalid engine output')
                windows = []
                scale = package_report['channels'][name]['reference_output_scale']
                for start in range(rf, len(x)-4096, 4096):
                    end = start+4096
                    selected = mask[start:end]
                    power = float(np.mean(ref[start:end]**2))
                    if not selected.any() or power <= 1e-6:
                        continue
                    bound = window_bound(ref[start:end], selected)
                    windows.append({
                        'start_sample': start, 'start_seconds': start/48000,
                        'zero_history_samples': int(selected.sum()),
                        'native_rms': power**.5,
                        'packaged_native_rms_dbfs': float(10*np.log10(power*scale**2)),
                        'eligible_under_packaged_validation_floor': power*scale**2 > 1e-8,
                        'best_constant_esr_lower_bound': bound,
                        'exceeds_unchanged_worst_window_gate': bound > .02,
                        'actual_a2_zero_history_peak_to_peak': float(np.ptp(pred[start:end][selected])),
                        'actual_a2_window_esr': float(np.mean((pred[start:end]-ref[start:end])**2)/power),
                    })
                zeros = pred[mask]
                # The real target engine corroborates the finite-support calculation.
                spread = float(np.ptp(zeros)) if len(zeros) else 0.
                if spread > 1e-6:
                    raise ValueError('Engine did not settle to a constant after the receptive field')
                return name, {
                    'model_sha256': digest(model_file), 'reference_sha256': digest(reference_file),
                    'rf_samples': rf, 'rf_ms': rf/48., 'engine_a2_gate': runtime['a2_gate'],
                    'zero_history_engine_peak_to_peak': spread,
                    'witnesses': windows,
                    'gate_impossible_for_bare_a2': any(w['exceeds_unchanged_worst_window_gate'] for w in windows),
                }

            with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
                for name, row in pool.map(audit, VOICES):
                    result['channels'][name] = row
                    print(json.dumps({'split': split, 'channel': name, 'blocked': row['gate_impossible_for_bare_a2'],
                                      'max_esr_lower_bound': max((w['best_constant_esr_lower_bound'] for w in row['witnesses']), default=0)}), flush=True)
            report['inputs'][split] = result
    report['status'] = 'BLOCKED_BY_FIXED_A2_MEMORY' if any(
        c['gate_impossible_for_bare_a2'] for split in report['inputs'].values() for c in split['channels'].values()) else 'NO_BLOCKER_PROVEN'
    write_json(args.out, report)


if __name__ == '__main__':
    main()
