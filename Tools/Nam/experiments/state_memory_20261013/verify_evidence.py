#!/usr/bin/env python3
"""Verify retained arrays and reports without training or rerendering."""
import argparse
import json
from pathlib import Path
import numpy as np
from probe_rf import PROFILE,sha


def verify(a):
    # Verify all reused validation arrays against the original capture manifest.
    data=json.loads((a.data/'manifest.json').read_text())
    entries=[r for r in data['captures'] if r['split']=='validation']
    for r in entries:
        for p,k in [(r['input'],'input_sha256'),(r['output'],'output_sha256')]:
            if sha(a.data/p)!=r[k]:raise ValueError('Changed capture '+p)
    inventory=dict(line.rstrip().split('  ',1)[::-1] for line in (a.refinement/'SHA256SUMS').read_text().splitlines())
    for p in (a.refinement/'fresh-test').glob('*.npy'):
        if sha(p)!=inventory[str(p.relative_to(a.refinement))]:raise ValueError('Changed prior test '+p.name)
    rf=json.loads((a.rf/'results.json').read_text())
    if rf['quality_profile']!=PROFILE or rf['release_approved']:raise ValueError('Policy/approval changed')
    for multiplier,row in rf['variants'].items():
        expected_a2=multiplier=='1'
        arrays=[]
        for b,r in row['runtime'].items():
            p=a.rf/f'rf{multiplier}-block{b}.f32'
            if sha(p)!=r['output_sha256'] or r['a2_gate']!=expected_a2:raise ValueError('RF engine evidence changed')
            arrays.append(np.fromfile(p,dtype='<f4'))
        if not np.array_equal(*arrays):raise ValueError('RF block parity failed')
    states=json.loads((a.states/'results.json').read_text())
    if states['quality_profile']!=PROFILE or states['release_approved'] or len(states['cases'])!=6:raise ValueError('State report changed')
    renders=0
    for c in states['cases']:
        for mode,m in c['modes'].items():
            ys={}
            for history in ['high','low']:
                for level in [0.,.03]:
                    key=f"{c['channel']}-mode{mode}-gap{c['gap_ms']}-{history}-level{level:.3f}.f32"
                    p=a.states/key; row=m['outputs'][history+str(level)]
                    if sha(p)!=row['output_sha256']:raise ValueError('Render hash changed')
                    ys[history,level]=np.fromfile(p,dtype='<f4')[c['onset']:].astype(np.float64);renders+=1
            d=((ys['high',.03]-ys['high',0.])-(ys['low',.03]-ys['low',0.]))*PROFILE['source_output_scale'][c['channel']]
            value=float(10*np.log10(max(float(np.mean(d[:9600]**2)),1e-30)))
            if abs(value-m['whole200ms_delta_rms_dbfs'])>1e-10:raise ValueError('State statistic changed')
    split=json.loads(a.split.read_text())
    if len(split['cases'])!=10 or any(c['max_abs_delta']!=0 for c in split['cases']) or split['release_approved']:
        raise ValueError('Hybrid split changed')
    return {'verified':True,'validation_channels':len(entries),'rf_variants':len(rf['variants']),
            'state_audio_files_and_statistics':renders,'hybrid_split_cases':len(split['cases']),
            'release_approved':False,'scope':'retained evidence integrity/math only, no additional learning or approval'}


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['data','refinement','rf','states','split']:p.add_argument('--'+name,type=Path,required=True)
    print(json.dumps(verify(p.parse_args()),indent=2))
