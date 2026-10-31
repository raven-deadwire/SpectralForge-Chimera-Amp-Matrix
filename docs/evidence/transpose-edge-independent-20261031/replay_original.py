"""Replay PR234's nine worst OFF cases using the exact original RNG stream.

This is retrospective reproduction, separate from the preregistered holdout.
Usage: python replay_original.py ORIGINAL_EVIDENCE_DIR BIN_DIR OUTPUT_DIR
"""
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'Tools/TransposeNext'))
from edge_validation import travel, render, write
from v3sinc_validation import tone, interharm
from residual_validation import net_db

original, binary, out = map(lambda x: Path(x).resolve(), sys.argv[1:])
manifest = json.loads((original/'SHA256.json').read_text())
audit = {n: hashlib.sha256((original/n).read_bytes()).hexdigest() == h for n,h in manifest.items()}
assert all(audit.values())
rows = []
with tempfile.TemporaryDirectory() as d:
    tmp = Path(d)
    for k in (1,2,3):
        names = ('fine-base.json','fine-edgeT.json') if k==1 else (f'fineK{k}-base.json',f'fineK{k}-edgeT.json')
        old = json.loads((original/names[0]).read_text())
        fixed = json.loads((original/names[1]).read_text())
        selected = {r:max((x for x in old if x['rate']==r),key=lambda x:x['net_db']) for r in (44100,48000,96000)}
        rng = np.random.default_rng(11)
        for rate in (44100,48000,96000):
            for shift in (-2,-1,1,2):
                fc=k*rate/travel(rate,shift)
                for f in np.round(np.arange(fc-k,fc+k+.0001,.04*k),3):
                    phase=rng.uniform(0,2*np.pi,int(8000//f))
                    target=selected[rate]
                    if shift!=target['shift'] or f!=target['f0']:
                        continue
                    z=tone(f,rate,.8,phase,secs=2)
                    x=np.column_stack((z,-.65*z)).astype('<f4')
                    fi=f*2**(shift/12);lo,hi=round(.5*rate),round(1.8*rate)
                    ih,ir=interharm(tone(fi,rate,.8,phase,secs=2)[lo:hi],fi,rate)
                    ideal=10*np.log10(ir/ih)
                    expected=next(q for q in fixed if (q['rate'],q['shift'],q['f0'])==(rate,shift,f))
                    row=dict(multiple=k,rate=rate,shift=shift,f0=float(f),old_off=target['net_db'],old_on=expected['net_db'])
                    for eng,tag in [('v3-sinc','off'),('v3-sinc-edge','on')]:
                        y,_=render(binary/'TransposeProbe',tmp,x,rate,shift,'corr40-split-'+eng)
                        h,r=interharm(y[lo:hi,0].astype(float),fi,rate)
                        row[tag]=float(net_db(10*np.log10(r/h),ideal))
                        trace=subprocess.run([str(binary/'EdgeTrace'),str(tmp/'in.f32'),str(tmp/'trace.f32'),str(rate),str(shift),eng],check=True,capture_output=True,text=True)
                        traced=np.fromfile(tmp/'trace.f32',dtype='<f4').reshape(-1,2)
                        row[tag+'_trace_bit_equal']=traced.tobytes()==y.tobytes()
                        jumps=[abs(float(s.split()[3])-float(s.split()[2])) for s in trace.stdout.splitlines() if s.startswith('T ')][1:]
                        row[tag+'_jump_error_max_samples']=max(abs(j-round(j/(rate/f))*(rate/f)) for j in jumps)
                        row[tag+'_net_difference_to_original']=row[tag]-row['old_'+tag]
                    rows.append(row)
write(out/'original-replay.json',dict(hash_audit=audit,rows=rows,
      trace_source_sha256=hashlib.sha256(Path(__file__).with_name('EdgeTrace.cpp').read_bytes()).hexdigest(),
      trace_binary_sha256=hashlib.sha256((binary/'EdgeTrace').read_bytes()).hexdigest()))
print(json.dumps(rows,indent=2))
