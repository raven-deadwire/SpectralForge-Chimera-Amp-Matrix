#!/usr/bin/env python3
"""Recompute source-bound failed-run evidence without modifying any models."""
import argparse
import json
from pathlib import Path
import numpy as np
from scipy.signal import butter, correlate, sosfilt

from common import digest, write_json
from quality_profile import PROFILE, fidelity
from recipe_selection import NAMES, select_recipe
from training_contract import verify_audio_manifest


def audit(root):
    manifest = json.loads((root / 'artifact-manifest.json').read_text())
    for name, expected in manifest['files'].items():
        path = (root / name).resolve()
        if not path.is_relative_to(root.resolve()) or digest(path) != expected:
            raise RuntimeError('Artifact integrity failure: ' + name)
    verify_audio_manifest(root / 'data')
    arms = {r: {'config': json.loads((root / 'recipes' / r / 'training-config.json').read_text()),
                'validation': json.loads((root / 'recipes' / r / 'validation.json').read_text())}
            for r in ('legacy', 'official-a2')}
    selection = select_recipe(arms)
    if selection['selected_recipe'] != json.loads((root / 'recipe-selection.json').read_text())['selected_recipe']:
        raise RuntimeError('Recipe selection disagreement')
    report = {'exact_source': manifest['exact_source'], 'verified_artifact_files': len(manifest['files']),
              'artifact_manifest_sha256': digest(root / 'artifact-manifest.json'),
              'acceptance': json.loads((root / 'acceptance.json').read_text()),
              'quality_profile': PROFILE, 'selection': selection, 'channels': {},
              'release_approved': False}
    for name in NAMES:
        ref = np.load(root / 'data/validation' / (name + '.npy'))
        count = len(ref)
        parts = {'plucks_before': slice(6347, count // 3), 'broadband': slice(count // 3, count // 2),
                 'plucks_after': slice(count // 2, count - 24000), 'terminal_tail': slice(count - 24000, count)}
        rows = {}
        for recipe, arm in arms.items():
            folder = root / 'recipes' / recipe
            pred = np.load(folder / (name + '-validation.npy'))
            saved = arm['validation']['channels'][name]
            measured = fidelity(pred, ref, name, np.load(root / 'data/validation/input.npy'))
            for key, value in measured.items():
                if isinstance(value, (int, float)) and not isinstance(value, bool):
                    if not np.isclose(value, saved['tone3000_fidelity'][key], rtol=1e-5, atol=1e-6):
                        raise RuntimeError('Recomputed fidelity disagrees: ' + name + '/' + key)
            err = (pred.astype(np.float64) - ref) ** 2
            r, y = ref[6347:].astype(np.float64), pred[6347:].astype(np.float64)
            correlation = correlate(y, r, mode='full', method='fft')
            lag = int(correlation[len(r)-1-32:len(r)+32].argmax()) - 32
            bands = {}
            for lo, hi in ((30, 250), (250, 1000), (1000, 4000), (4000, 12000)):
                sos = butter(3, [lo, hi], btype='bandpass', fs=48000, output='sos')
                br, by = sosfilt(sos, r), sosfilt(sos, y)
                bands[f'{lo}-{hi}Hz'] = {'esr': float(np.mean((by-br)**2) / np.mean(br**2)),
                    'level_error_db': float(10*np.log10(np.mean(by**2) / np.mean(br**2)))}
            rows[recipe] = {'fidelity': measured, 'selected_step': saved['selected_step'],
                'correlation_peak_lag_samples_within_32': lag, 'band_diagnostics': bands,
                'export_zero_bias_removed_raw_units': saved['head_bias_correction'],
                'sections': {label: {'esr': float(err[s].sum() / np.square(ref[s], dtype=np.float64).sum()),
                                    'fraction_total_error': float(err[s].sum() / err[6347:].sum())}
                             for label, s in parts.items()},
                'validation_curve': {int(p.stem.rsplit('-', 1)[1]): json.loads(p.read_text())['esr_by_channel'][name]
                                     for p in sorted(folder.glob('validation-step-*.json'))}}
        report['channels'][name] = rows
    return report


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--evidence', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    write_json(a.out, audit(a.evidence))
