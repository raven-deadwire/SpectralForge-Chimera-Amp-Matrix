#!/usr/bin/env python3
"""Measure extended-history bounds, actual Core loading and offline CPU, no fitting."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from audit_a2_memory import receptive_field, zero_history_mask
from audit_quiet_memory import absolute_bounds
from quality_profile import PROFILE


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write(path, value):
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + '\n')


def support(config):
    layer = config['layers'][0]
    reachable = np.ones(1, dtype=bool)
    for k, d in list(zip(layer['kernel_sizes'], layer['dilations'])) + [(layer['head']['kernel_size'], 1)]:
        out = np.zeros(len(reachable) + (k-1)*d, dtype=bool)
        for t in range(k):
            out[t*d:t*d+len(reachable)] |= reachable
        reachable = out
    return bool(reachable.all())


def run(a):
    a.out.mkdir(parents=True, exist_ok=False)
    model_path = a.refinement/'refinement/legacy/Nastrond-Fimbulvetr.nam'
    if sha(model_path) != 'df28b1397a973d28c50d53d3bdab2e4c63df8850ebb39ba2efa8c1a27f17668d':
        raise ValueError('Wrong archived checkpoint export')
    model = json.loads(model_path.read_text())
    validation = a.data/'validation/input.npy'
    x = np.load(validation)[:4*48000]
    raw = a.out/'runtime-input.f32'; x.astype('<f4').tofile(raw)
    old = a.refinement/'fresh-test'
    comparison = json.loads((old/'comparison.json').read_text())
    if comparison['quality_profile'] != PROFILE or sha(old/'input.npy') != comparison['input_sha256']:
        raise ValueError('Policy or old input changed')
    hx = np.load(old/'input.npy')
    report = {'plan_sha256': sha(Path(__file__).with_name('plan.json')),
              'model_sha256': sha(model_path), 'probe_binary_sha256': sha(a.tool),
              'validation_input_sha256': sha(validation), 'quality_profile': PROFILE,
              'optimizer_updates': 0, 'release_approved': False,
              'fidelity': 'NOT_EVALUATED: changed dilation with old weights is a runtime fixture only',
              'timing_scope': 'shared Linux CPU, offline mono 48k; not DAW/realtime qualification',
              'variants': {}}
    for mult in [1,2,4,8]:
        candidate = json.loads(json.dumps(model))
        layer = candidate['config']['layers'][0]
        layer['dilations'] = [d*mult if d>1 else 1 for d in layer['dilations']]
        rf = receptive_field(candidate)
        path = a.out/f'runtime-only-rf{mult}.nam'; write(path,candidate)
        row = {'rf_samples': rf, 'history_ms': rf/48.,
               'dense_support': support(candidate['config']),
               'weights_unchanged': candidate['weights']==model['weights'],
               'nam_sha256': sha(path), 'bounds': {}, 'runtime': {}}
        mask = zero_history_mask(hx,rf)
        for name,scale in PROFILE['source_output_scale'].items():
            refpath=old/f'{name}-native.npy'
            ref=np.load(refpath).astype(np.float64)*scale
            bounds=[]
            size=PROFILE['window_samples']
            for start in range(PROFILE['skip_samples'],len(ref)-size+1,size):
                window=ref[start:start+size]
                if 10*np.log10(max(np.mean(window**2),1e-20)) >= PROFILE['active_reference_rms_min_dbfs']:
                    continue
                mse,peak=absolute_bounds(window,mask[start:start+size])
                bounds.append({'start':start,'rms_lower_bound_dbfs':float(10*np.log10(max(mse,1e-20))),
                               'peak_lower_bound_dbfs':float(20*np.log10(max(peak,1e-10)))})
            row['bounds'][name]={'native_sha256':sha(refpath),
               'worst_rms':max(b['rms_lower_bound_dbfs'] for b in bounds),
               'scope':'necessary lower bound only, no sufficiency or trained fidelity claim'}
        for block in [64,256]:
            output=a.out/f'rf{mult}-block{block}.f32'
            measured=json.loads(subprocess.check_output([str(a.tool),str(path),str(raw),str(output),str(block)],text=True))
            measured['output_sha256']=sha(output)
            row['runtime'][str(block)]=measured
        y64=np.fromfile(a.out/f'rf{mult}-block64.f32',dtype='<f4')
        y256=np.fromfile(a.out/f'rf{mult}-block256.f32',dtype='<f4')
        row['block_max_abs_delta']=float(np.max(np.abs(y64-y256)))
        report['variants'][str(mult)]=row
        write(a.out/'results.json',report)
        print(mult,rf,row['runtime']['64']['a2_gate'],row['runtime']['64']['p99_seconds'],flush=True)
    return report


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['refinement','data','tool','out']:
        p.add_argument('--'+name,type=Path,required=True)
    run(p.parse_args())
