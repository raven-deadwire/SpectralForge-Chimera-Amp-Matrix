# Report-only analysis of residual.json (P5): adds the ideal tone's own inter-harmonic floor and the
# absolute residual (re harmonic energy) per case, and prints medians by f0 band. No rendering.
# usage: python residual_diag.py residual.json residual-diag.json
import sys,json,numpy as np
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[3]/'Tools'/'TransposeNext'))
import v3sinc_validation as V
rows=json.load(open(sys.argv[1]))['rows'];cases={c['case']:c for c in V.residual_cases()}
for r in rows:
    c=cases[r['case']];rate=r['rate'];fi=c['f0_hz']*2**(r['shift']/12)
    z=V.tone(fi,rate,c['tilt'],c['phase']);h,res=V.interharm(z[round(.5*rate):len(z)-round(.3*rate)],fi,rate)
    r['ideal_residual_db_re_harm']=10*np.log10(res/h)
    for t in('v3','v3sinc'):r[t]['residual_db_re_harm']=r[t]['excess_db']+r['ideal_residual_db_re_harm']
json.dump(rows,open(sys.argv[2],'w'))
for rate in V.RATES:
    for lab,lo,hi in [('f0<=60',0,60),('60<f0<=130',60,130),('f0>130',130,1e9)]:
        s=[r for r in rows if r['rate']==rate and lo<r['f0_hz']<=hi]
        med=lambda f:float(np.median([f(r) for r in s]))
        print(rate,lab,len(s),'ideal %.0f'%med(lambda r:r['ideal_residual_db_re_harm']),
              'resid v3 %.0f v3sinc %.0f'%(med(lambda r:r['v3']['residual_db_re_harm']),med(lambda r:r['v3sinc']['residual_db_re_harm'])),
              'excess v3 %.1f v3sinc %.1f'%(med(lambda r:r['v3']['excess_db']),med(lambda r:r['v3sinc']['excess_db'])))
