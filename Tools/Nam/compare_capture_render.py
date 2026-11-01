#!/usr/bin/env python3
"""Compare two ChimeraNamRender builds of the same renderer source, one built
against the capture commit's Source/ and one against another Source/ tree.
Exit 0 when every Náströnd render is byte-identical (README: 2026-11-01).

Usage: compare_capture_render.py CAPTURE_RENDERER OTHER_RENDERER
"""
import json, subprocess, random, sys, numpy as np
if len(sys.argv) != 3: sys.exit(__doc__)
cap, head = sys.argv[1], sys.argv[2]
cat=json.loads(subprocess.check_output([head,'--catalog']))
catc=json.loads(subprocess.check_output([cap,'--catalog']))
assert cat==catc, 'catalogues differ'
rate=48000; t=np.arange(rate*4)/rate
rng=np.random.default_rng(1)
sweep=0.5*np.sin(2*np.pi*(40*t+ (8000-40)/(2*4)*t*t))
noise=0.3*rng.standard_normal(len(t))
pl=np.zeros_like(t)
for k,f in enumerate([82.41,110,146.8,196,246.9,329.6,98,55]):
    s=int(k*0.5*rate); n=np.arange(int(0.5*rate)); env=np.exp(-n/rate*6)
    pl[s:s+len(n)]+=sum(0.8/h*np.sin(2*np.pi*f*h*n/rate) for h in range(1,7))*env
inp=np.concatenate([sweep,noise,pl,0.05*pl,1.5*pl]).astype(np.float32)
inp.tofile('nam_compare_in.f32')
maps=[m for m in cat['mapping']]
random.seed(7); jobs=[]
for ch in range(5):
    jobs.append({'channel':ch})
    for _ in range(20):
        ctl={}
        for m in maps:
            lo,hi,sc=m['min'],m['max'],m['source_scale']
            ctl[m['source_key']]=round(random.uniform(lo,hi)*sc,4)
        jobs.append({'channel':ch,'controls':ctl})
same=0;diff=[]
for i,j in enumerate(jobs):
    open('nam_compare_job.json','w').write(json.dumps(j))
    outs=[]
    for exe,name in ((cap,'cap'),(head,'head')):
        o=f'nam_compare_out_{name}.f32'; subprocess.run([exe,'nam_compare_in.f32',o,'nam_compare_job.json'],check=True,capture_output=True)
        outs.append(open(o,'rb').read())
    if outs[0]==outs[1]: same+=1
    else:
        a=np.frombuffer(outs[0],np.float32);b=np.frombuffer(outs[1],np.float32);diff.append((i,float(np.max(np.abs(a-b)))))
energy=float(np.sqrt(np.mean(np.frombuffer(outs[1],np.float32)**2)))
print(f'jobs {len(jobs)}, identical {same}, different {len(diff)} {diff[:5]}; input {len(inp)} samples; last output RMS {energy:.4f}')
sys.exit(0 if same == len(jobs) else 1)
