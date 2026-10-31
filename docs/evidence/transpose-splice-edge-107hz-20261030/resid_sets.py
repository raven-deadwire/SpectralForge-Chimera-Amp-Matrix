# v3-sinc vs v3-sinc-edge on the residual tone sets of seeds 70711 (12 tones) and 86603 (16 tones).
import sys,json,tempfile,numpy as np
from pathlib import Path
sys.path.insert(0,str(__import__('pathlib').Path(__file__).resolve().parents[3]/'Tools'/'TransposeNext'))
import v3sinc_validation as V, residual_validation as R
exe,out=sys.argv[1],sys.argv[2];rows=[]
with tempfile.TemporaryDirectory() as tmp:
  tmp=Path(tmp)
  for seed,n in ((70711,12),(86603,16)):
    R.TONES=n
    for rate in (44100,48000,96000):
      for c in R.cases(seed):
        z=V.tone(c['f0_hz'],rate,c['tilt'],c['phase']);np.column_stack((z,-.65*z)).astype('<f4').tofile(tmp/'i.f32')
        lo,hi=round(.5*rate),len(z)-round(.3*rate)
        for sh in (-2,-1,1,2):
          fi=c['f0_hz']*2**(sh/12);h0,r0=V.interharm(V.tone(fi,rate,c['tilt'],c['phase'])[lo:hi],fi,rate);ideal=10*np.log10(r0/h0)
          row={'seed':seed,'rate':rate,'case':c['case'],'f0':c['f0_hz'],'shift':sh}
          for eng,tag in (('corr40-split-v3-sinc','sinc'),('corr40-split-v3-sinc-edge','edge')):
            y=V.render(exe,tmp/'i.f32',tmp/'o.f32',rate,sh,eng,tmp/'cb.csv')[lo:hi,0].astype(float)
            h,r=V.interharm(y,fi,rate);row[tag]=R.net_db(10*np.log10(r/h),ideal)
          rows.append(row)
      print(seed,rate,flush=True)
json.dump(rows,open(out,'w'),indent=1)
