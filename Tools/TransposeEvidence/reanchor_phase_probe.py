#!/usr/bin/env python3
"""Re-anchor jump check for the research hybrid (docs/TRANSPOSE_HYBRID_LATENCY_CONTRACT.md 7.2).

For the legato / hammer-on material of Tools/transpose_latency_contract.py:
read-head arrival error (engine ground truth), the first re-anchor jump within
15 ms of each change (read delay steps > 1.5 samples) scored as the peak of the
squared second difference of the 1 kHz low-passed output within +-2.5 ms re
its median 10-30 ms before (dB; an ideal output without a jump scores a median
6.0-6.3 dB on this measure), and irregular cycles.

  python Tools/TransposeEvidence/reanchor_phase_probe.py OUT.json 40,41,42,43
"""
import sys,os,tempfile,json,numpy as np
sys.path.insert(0,str(__import__('pathlib').Path(__file__).resolve().parents[1]))
import transpose_latency_contract as lc, transpose_synthetic_evidence as ev
from pathlib import Path as _P
probe=str(_P(__file__).resolve().parents[2]/'build/ChimeraTransposeRender_artefacts/Release/ChimeraTransposeRender'); rate=48000; L=480
modes=[int(a) for a in sys.argv[2].split(',')]
def jump_click_db(y,D,tc):
    s=lc.lowpass(y,1000,rate); d2=np.diff(s,2)**2
    n0,n1=int(tc),int(tc+0.015*rate)
    jumps=np.nonzero(np.abs(np.diff(D[n0:n1]))>1.5)[0]+n0
    if len(jumps)==0: return None
    j=jumps[0]; w=int(.0025*rate)
    base=np.median([np.mean(d2[k:k+w]) for k in range(j-int(.03*rate),j-int(.01*rate),w)])+1e-20
    return round(float(10*np.log10(np.max(d2[j-w:j+w])/base)),1)
rows=[]
with tempfile.TemporaryDirectory() as t:
  for name,f0,steps in [('E2_up',82.41,[2,1,2]),('E2_down',82.41,[-1,-2,-2]),('A2_up',110.0,[2,2,1])]:
    for hammer in (False,True):
      x,notes=lc.legato(f0,steps,rate,hammer)
      for sh in (-12,-7,-5,-2,2,5,7,12):
        r=2**(sh/12)
        for m in modes:
          y,lat,D,tau=lc.render_with_delays(probe,rate,128,m,sh,x,t)
          heads=lc.read_head_arrivals(D,tau,rate); irr=lc.irregular_cycles(y,notes,r,rate)
          for k,at in enumerate(lc.LEGATO_AT):
            tc=(ev.PRE_S+at)*rate
            rows.append({'material':name,'kind':'hammer' if hammer else 'legato','shift':sh,'mode':m,'event':at,
                         'true_ms':None if heads[k] is None else round(1000*heads[k]-1000*(ev.PRE_S+at)-10,2),
                         'click_db':jump_click_db(np.asarray(y,float),D,tc),'irregular':irr[k]})
json.dump(rows,open(sys.argv[1],'w'),indent=1)
for m in modes:
  for kind in ('legato','hammer'):
    sel=[r for r in rows if r['mode']==m and r['kind']==kind]
    tv=np.array([r['true_ms'] for r in sel if r['true_ms'] is not None]); cl=[r['click_db'] for r in sel if r['click_db'] is not None]
    print(f"mode {m} {kind:6s} true: median {np.median(tv):5.2f} min {tv.min():6.2f} max {tv.max():6.2f} |e|<=2 {np.mean(np.abs(tv)<=2):.2f} |e|<=5 {np.mean(np.abs(tv)<=5):.2f} | re-anchor jumps {len(cl)} click dB median {np.median(cl) if cl else float('nan'):5.1f} p90 {np.percentile(cl,90) if cl else float('nan'):5.1f} | irregular {sum(r['irregular'] is not None for r in sel)}/{len(sel)}",flush=True)
