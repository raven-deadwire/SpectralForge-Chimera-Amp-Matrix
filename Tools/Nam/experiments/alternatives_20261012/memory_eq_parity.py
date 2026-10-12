#!/usr/bin/env python3
"""Verify frozen filter jobs against the unmodified native BlockEq component."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
from scipy import signal

from memory_factorization import load_eq_sos
from memory_source_poles import FS, NAMES


def digest(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def rms_db(x):
    return float(10 * np.log10(max(float(np.mean(np.asarray(x, dtype=np.float64) ** 2)), 1e-30)))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--probe', type=Path, required=True)
    p.add_argument('--factorization', type=Path, required=True)
    p.add_argument('--scratch', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    a.scratch.mkdir(parents=True, exist_ok=True)
    # Fixed component probe: impulse, DC steps, steady multitone/noise, then1s
    # zero. No training, response fit, model choice or metric change occurs.
    x = np.zeros(8 * FS, dtype=np.float32)
    x[0] = .5
    x[FS:2 * FS] = .25
    x[2 * FS:3 * FS] = -.25
    t = np.arange(4 * FS, dtype=np.float64) / FS
    rng = np.random.default_rng(26101281)
    x[3 * FS:7 * FS] = (.11 * np.sin(2 * np.pi * 55 * t) +
                         .12 * np.sin(2 * np.pi * 153.6 * t) +
                         .10 * np.sin(2 * np.pi * 997 * t) +
                         .01 * rng.standard_normal(len(t))).astype(np.float32)
    input_path = a.scratch / 'memory-eq-component-input.f32'
    x.tofile(input_path)
    report = {'sample_rate': FS, 'input_sha256': digest(input_path),
              'probe_binary_sha256': digest(a.probe), 'release_approved': False,
              'scope': 'Actual BlockEq component parity and native Eq state roundtrip; not full NAM/preset/DAW acceptance',
              'parity_rms_limit_dbfs': -80., 'channels': {}}
    for name in NAMES:
        job_path = a.factorization / (name + '-factor-post-eq.json')
        original_job = json.loads(job_path.read_text())
        sos = load_eq_sos(original_job)
        python64 = signal.sosfilt(sos, x.astype(np.float64)).astype(np.float32)
        python32 = signal.sosfilt(sos.astype(np.float32), x)
        rendered = []
        metas = []
        for block in [64, 257]:
            job = dict(original_job, block_size=block)
            tmp = a.scratch / (name + '-block-' + str(block) + '.json')
            tmp.write_text(json.dumps(job))
            output = a.scratch / (name + '-block-' + str(block) + '.f32')
            completed = subprocess.run([str(a.probe), str(input_path), str(output), str(tmp)],
                                       text=True, check=True, capture_output=True)
            meta = json.loads(completed.stdout)
            y = np.fromfile(output, dtype=np.float32)
            if len(y) != len(x) or not np.isfinite(y).all():
                raise RuntimeError('Invalid native filter output')
            rendered.append(y)
            metas.append(meta)
        native = rendered[0]
        error64 = native.astype(np.float64) - python64
        error32 = native.astype(np.float64) - python32
        value = {'job_sha256': digest(job_path), 'effective_eq': metas[0].get('eq'),
                 'native_float32_vs_python_float64_rms_dbfs': rms_db(error64),
                 'native_float32_vs_python_float64_peak_abs': float(np.max(np.abs(error64))),
                 'native_float32_vs_python_float32_rms_dbfs': rms_db(error32),
                 'native_float32_vs_python_float32_peak_abs': float(np.max(np.abs(error32))),
                 'block64_vs257_bit_exact': bool(np.array_equal(rendered[0], rendered[1])),
                 'last_100ms_silence_rms_dbfs': rms_db(native[-4800:]),
                 'native_metadata': metas,
                 'python_float64_parity_pass': rms_db(error64) <= -80.}
        report['channels'][name] = value
        print(name, 'native-vs-double RMS', value['native_float32_vs_python_float64_rms_dbfs'],
              'native-vs-float RMS', value['native_float32_vs_python_float32_rms_dbfs'],
              'block parity', value['block64_vs257_bit_exact'], flush=True)
    a.out.write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
