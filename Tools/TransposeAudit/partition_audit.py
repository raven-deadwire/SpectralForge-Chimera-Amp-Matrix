#!/usr/bin/env python3
"""Diagnose block-dependence without changing the frozen DSP."""
import argparse,json,subprocess,sys
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'Tools/TransposeNext'))
from weak_attack_study import generate
from legacy_metrics import sha
p=argparse.ArgumentParser();p.add_argument('--renderer',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.renderer=a.renderer.resolve();a.out.mkdir(parents=True,exist_ok=True);rows=[]
for rate in [44100,48000,96000]:
 x,m=generate('harmonics',rate,'bass_B0');ip=a.out/'input.f32';x.tofile(ip)
 for shift in [-2,-1,1,2]:
  for engine in ['corr40-split-lpc','corr40-split-rt']:
   reference=None
   for block in [32,64,128,256,257]:
    op=a.out/'output.f32';cp=a.out/'callbacks.csv'
    r=subprocess.run([str(a.renderer),str(ip),str(op),str(rate),str(block),str(shift),engine,str(cp)],check=True,capture_output=True,text=True)
    y=np.fromfile(op,dtype='<f4');native=json.loads(r.stdout)
    if reference is None:reference=y.copy()
    error=y-reference;different=np.flatnonzero(error)
    rows.append({'rate':rate,'shift':shift,'engine':engine,'block':block,'sha256':sha(op),'bit_identical_to_block32':bool(np.array_equal(y,reference)),'peak_error':float(max(abs(error))),'rms_error':float(np.sqrt(np.mean(error.astype(float)**2))),'first_different_sample':int(different[0]//2) if len(different) else None,'splices':native['splices']})
 print(rate,'complete',flush=True)
(a.out/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
print('FAIL groups',sum(not r['bit_identical_to_block32'] for r in rows),'/',len(rows),flush=True)
