#!/usr/bin/env python3
"""Reuse prior quartets and intervene in frozen teacher state; no training."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import numpy as np

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from quality_profile import PROFILE


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def write(p,x):
    p.write_text(json.dumps(x,indent=2,ensure_ascii=False)+'\n')


def db(x):
    return float(10*np.log10(max(float(np.mean(np.asarray(x,dtype=np.float64)**2)),1e-30)))


def run(a):
    a.out.mkdir(parents=True,exist_ok=False)
    plan=json.loads(Path(__file__).with_name('plan.json').read_text())
    manifest=json.loads((a.previous/'manifest.json').read_text())
    inventory={f['path']:f for f in manifest['files']}
    def verified(relative):
        p=a.previous/relative
        if sha(p)!=inventory[relative]['sha256']:
            raise ValueError('Prior evidence changed: '+relative)
        return p
    report={'plan_sha256':sha(Path(__file__).with_name('plan.json')),
            'probe_binary_sha256':sha(a.tool),'quality_profile':PROFILE,
            'release_approved':False,'optimizer_updates':0,
            'scope':'state ablation diagnostic, not model performance; altered teacher arms cannot qualify fidelity',
            'baseline_prefix_max_delta':{},'cases':[]}
    old=json.loads(verified('nonlinear-diagnostics/results.json').read_text())
    # Baseline prefix equality verifies that the inactive hook preserves the
    # existing production route (including JUCE oversampling and six-sample delay).
    vx=np.load(a.data/'validation/input.npy')[:48000]
    vx.astype('<f4').tofile(a.out/'validation-prefix.f32')
    for name in plan['state_probe']['channels']:
        job=json.loads(verified(f'nonlinear-inputs/{name}-native-job.json').read_text())
        jp=a.out/f'{name}-baseline-job.json';write(jp,job)
        op=a.out/f'{name}-baseline-prefix.f32'
        subprocess.run([str(a.tool),str(a.out/'validation-prefix.f32'),str(op),str(jp)],check=True,stdout=subprocess.DEVNULL)
        y=np.fromfile(op,dtype='<f4');ref=np.load(a.data/f'validation/{name}.npy')[:len(y)]
        delta=float(np.max(np.abs(y-ref)))
        if delta!=0:raise ValueError(f'Frozen reference changed: {name} {delta}')
        report['baseline_prefix_max_delta'][name]=delta
        for gap in plan['state_probe']['gaps_ms']:
            onset=48000+gap*48
            group=next(g for g in old['native_groups'] if g['name']==name and g['gap_ms']==gap)
            oldcase=next(c for c in group['cases'] if c['probe_level']==.03)
            case={'channel':name,'gap_ms':gap,'onset':onset,'modes':{}}
            for mode,label in plan['state_probe']['modes'].items():
                ys={};outputs={}
                for history in ['high','low']:
                    for level in [0.,.03]:
                        key=f'gap{gap}-{history}-level{level:.3f}'
                        ip=verified(f'nonlinear-inputs/{key}.f32')
                        outkey=f'{name}-mode{mode}-{key}'
                        jp=a.out/f'{outkey}.json';op=a.out/f'{outkey}.f32'
                        write(jp,{**job,'research_mode':int(mode),'research_onset':onset})
                        runtime=json.loads(subprocess.check_output([str(a.tool),str(ip),str(op),str(jp)],text=True))
                        yy=np.fromfile(op,dtype='<f4')
                        if not np.isfinite(yy).all() or len(yy)!=ip.stat().st_size//4:raise ValueError('Invalid render')
                        ys[history,level]=yy[onset:].astype(np.float64)
                        outputs[history+str(level)]={'input_sha256':sha(ip),'output_sha256':sha(op),'runtime':runtime}
                delta=((ys['high',.03]-ys['high',0.])-(ys['low',.03]-ys['low',0.]))*PROFILE['source_output_scale'][name]
                row={'label':label,'whole200ms_delta_rms_dbfs':db(delta[:9600]),
                     'onset20ms_delta_rms_dbfs':db(delta[:960]),'outputs':outputs}
                if mode=='0':
                    expected=oldcase['statistics']['whole_probe_200ms']['delta_rms_dbfs']
                    row['prior_delta_rms_dbfs']=expected
                    if abs(row['whole200ms_delta_rms_dbfs']-expected)>1e-7:
                        raise ValueError('No-op diagnostic differs from archived teacher')
                case['modes'][mode]=row
            report['cases'].append(case)
            write(a.out/'results.json',report)
            print(name,gap,{m:round(r['whole200ms_delta_rms_dbfs'],2) for m,r in case['modes'].items()},flush=True)
    return report


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['previous','data','tool','out']:p.add_argument('--'+name,type=Path,required=True)
    run(p.parse_args())
