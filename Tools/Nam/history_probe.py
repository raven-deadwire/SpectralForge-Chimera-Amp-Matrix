#!/usr/bin/env python3
"""Measure a finite-receptive-field representation bound, without fitting NAM."""
import argparse, json, subprocess, tempfile
from pathlib import Path
import numpy as np
from common import write_json

def main():
    p=argparse.ArgumentParser();p.add_argument('--renderer',type=Path,required=True)
    p.add_argument('--settings',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    rate=48000;rf=6347;prefix=rate;common=int(.8*rate)
    t=np.arange(prefix+common)/rate
    carrier=(np.sin(2*np.pi*55*t)+.35*np.sin(2*np.pi*110*t)+.15*np.sin(2*np.pi*165*t))/1.5
    x1=(carrier*.65).astype('<f4');x2=(carrier*.005).astype('<f4')
    tail=(carrier[prefix:]*.1*np.exp(-np.arange(common)/rate)).astype('<f4')
    x1[prefix:]=tail;x2[prefix:]=tail
    report={'description':'Different 1-second histories followed by exactly identical 0.8-second input. No trained NAM involved.',
      'rf_samples':rf,'rf_ms':1000*rf/rate,'analysis_start':'one complete receptive field after inputs become identical',
      'bound':'For two targets a,b with identical available inputs, the minimum pair mean squared error is mean((a-b)^2)/4.',
      'scope':'Representation bound for this constructed pair only; does not establish a floor for other inputs.', 'channels':{}}
    with tempfile.TemporaryDirectory() as td:
        temp=Path(td);raw=temp/'x.f32';out=temp/'y.f32';job=temp/'job.json'
        for ch in json.loads(a.settings.read_text()):
            write_json(job,{'channel':ch['channel'],'controls':ch['controls']});ys=[]
            for x in [x1,x2]:
                x.tofile(raw);subprocess.check_output([str(a.renderer),str(raw),str(out),str(job)],text=True)
                ys.append(np.fromfile(out,dtype='<f4')[prefix+rf:].astype(np.float64))
            aa,bb=ys;energy=np.mean((aa**2+bb**2)/2)
            bound=np.mean((aa-bb)**2)/4/max(energy,1e-20)
            bins=[]
            for s in range(0,len(aa)-960,960):
                va,vb=aa[s:s+960],bb[s:s+960]
                bins.append(float(np.mean((va-vb)**2)/4/max(np.mean((va**2+vb**2)/2),1e-20)))
            report['channels'][ch['name']]={'normalized_pair_mse_bound':float(bound),'worst_20ms_bound':max(bins),
              'native_history_difference_peak':float(np.max(np.abs(aa-bb)))}
    write_json(a.out,report);print(json.dumps(report['channels']))

if __name__=='__main__':main()
