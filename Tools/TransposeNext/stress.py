#!/usr/bin/env python3
"""Probe missed low-level attacks against a sustained low-B background."""
import argparse
import json
import subprocess
from pathlib import Path
import numpy as np
from evaluate import attacks, B0
from legacy_metrics import fixture,sha

p=argparse.ArgumentParser();p.add_argument('--renderer',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
a=p.parse_args();a.renderer=a.renderer.resolve();a.out.mkdir(parents=True,exist_ok=True)
rate=48000;rows=[];inputs={}
for name in ['alternating_velocity','quiet_markers_over_sustain']:
    x,meta=fixture('riff75' if name=='alternating_velocity' else 'harmonics',rate)
    if name=='alternating_velocity':
        for i,onset in enumerate(meta['onsets']):
            if i%2:x[onset:onset+round(.075*rate)]*=.15
    else:
        meta['onsets']=[round((.4+.075*i)*rate) for i in range(16)]
        rng=np.random.default_rng(318)
        for onset in meta['onsets']:
            t=np.arange(round(.012*rate))/rate
            marker=(.004*np.exp(-t/.002)*rng.uniform(-1,1,len(t))).astype('<f4')
            x[onset:onset+len(t),0]+=marker
            x[onset:onset+len(t),1]-=.65*marker
    meta['name']=name;meta['synthetic']=True
    ip=a.out/(name+'.f32');x.tofile(ip)
    before=attacks(x,meta);inputs[name]={**meta,'sha256':sha(ip),'input_attack':before}
    for engine in ['baseline','corr40','corr40-onset']:
        key=name+'-'+engine;op=a.out/(key+'.f32');cp=a.out/(key+'.csv')
        r=subprocess.run([str(a.renderer),str(ip),str(op),str(rate),'128','-2',engine,str(cp)],capture_output=True,text=True,check=True)
        y=np.fromfile(op,dtype='<f4').reshape(-1,2);m=attacks(y,meta)
        extra=np.asarray(m['attack_lags_ms'])-np.asarray(before['attack_lags_ms'])
        rows.append({'input':name,'engine':engine,**json.loads(r.stdout),'output_sha256':sha(op),
                     'callback_sha256':sha(cp),'attack':m,'additional_lags_ms':extra.tolist(),
                     'additional_peak_lag_max_ms':float(max(extra))})
        print(key,flush=True)
(a.out/'stress.json').write_text(json.dumps({'status':'DIAGNOSTIC_SYNTHETIC_ONLY',
    'renderer_sha256':sha(a.renderer),'inputs':inputs,'results':rows},indent=2)+'\n')
