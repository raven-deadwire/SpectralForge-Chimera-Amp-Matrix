#!/usr/bin/env python3
"""Apply TRAIN-frozen factorization to the already-revealed 20261011 witness.

This set has been inspected before and is NOT claimed to be a new independent
test. There is no fitting, selection, iteration or setting change in this file.
"""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np

from memory_factorization import diagnostic, load_eq_sos
from memory_source_poles import NAMES


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--factorization', type=Path, required=True)
    p.add_argument('--revealed-test', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    freeze_path = a.factorization / 'training-freeze.json'
    freeze = json.loads(freeze_path.read_text())
    assessment = json.loads((a.factorization / 'memory-eq-factorization.json').read_text())
    if digest(freeze_path) != assessment['training_freeze_sha256']:
        raise RuntimeError('Training freeze changed')
    previous = json.loads((a.revealed_test / 'comparison.json').read_text())
    memory = json.loads((a.revealed_test / 'memory-bound.json').read_text())
    profile = previous['quality_profile']
    if digest(a.revealed_test / 'input.npy') != previous['input_sha256']:
        raise RuntimeError('Revealed witness input changed')
    x = np.load(a.revealed_test / 'input.npy')
    result = {'scope': 'Reused revealed-test diagnosis only, not a new independent test',
              'release_approved': False, 'fit_or_selection_performed': False,
              'training_freeze_sha256': digest(freeze_path),
              'input_sha256': digest(a.revealed_test / 'input.npy'),
              'previous_comparison_sha256': digest(a.revealed_test / 'comparison.json'),
              'channels': {}}
    for name in NAMES:
        job_path = a.factorization / (name + '-factor-post-eq.json')
        if digest(job_path) != freeze['channels'][name]['job_sha256']:
            raise RuntimeError('Frozen EQ job changed')
        native_path = a.revealed_test / (name + '-native.npy')
        if digest(native_path) != memory['channels'][name]['reference_sha256']:
            raise RuntimeError('Native witness changed')
        diag = diagnostic(x, np.load(native_path), load_eq_sos(job_path),
                          profile['source_output_scale'][name], profile)
        original_start = memory['channels'][name]['worst_rms_witness']['start_sample']
        diag['original_hard_window'] = next((r for r in diag['quiet_windows']
                                            if r['start_sample'] == original_start), None)
        diag['job_sha256'] = digest(job_path)
        diag['native_sha256'] = digest(native_path)
        result['channels'][name] = diag
        print(name, 'oracle quiet RMS', diag['oracle_quiet_worst_residual_rms_dbfs'],
              'late pre-target attenuation dB', diag['late_tail_energy_reduction_db'], flush=True)
    a.out.write_text(json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    main()
