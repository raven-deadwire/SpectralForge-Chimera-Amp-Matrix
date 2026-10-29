"""Known-seed reproduction and exploratory envelope-dip comparison (not a gate)."""
import sys,json,subprocess,tempfile
from pathlib import Path
import numpy as np
from scipy import signal
sys.path.insert(0,'Tools/TransposeNext')
from latency_targets import inputs
from weak_attack_study import FREQUENCIES
from weak_attack_holdout import lowband_extra
renderer=Path(sys.argv[1]).resolve();out=Path(sys.argv[2]);rows=[]
def dips(y,on,f0,shift,rate):
 f=f0*2**(shift/12);sos=signal.butter(2,[f*.7,f*1.3],btype='band',fs=rate,output='sos')
 env=np.abs(signal.hilbert(signal.sosfiltfilt(sos,y[:,0].astype(float))))
 vals=[]
 for o,nx in zip(on,on[1:]+[len(y)]):
  seg=env[o+int(.005*rate):min(nx,o+int(.060*rate))]
  future_max=np.maximum.accumulate(seg[::-1])[::-1]
  vals.append(float(20*np.log10(np.max(future_max/np.maximum(seg,1e-12)))))
 return np.array(vals)
with tempfile.TemporaryDirectory() as tmp:
 tmp=Path(tmp)
 for rate in (44100,48000,96000):
  for key,x,m,roles in inputs(rate,(),1414):
   if key.split('/')[1] not in ('riff75','alternating_velocity','long_notes_seed1414'):continue
   x.tofile(tmp/'in.f32');f=FREQUENCIES[key.split('/')[0]]
   for shift in (1,2):
    for engine in ('corr40-split-v3','corr40-split-v4'):
     r=subprocess.run([str(renderer),str(tmp/'in.f32'),str(tmp/'out.f32'),str(rate),'128',str(shift),engine,str(tmp/'cb.csv')],check=True,capture_output=True,text=True)
     y=np.fromfile(tmp/'out.f32',dtype='<f4').reshape(-1,2)
     row={'input':key,'rate':rate,'shift':shift,'engine':engine,'lowband_additional_ms':lowband_extra(x,y,m['onsets'],rate)}
     if rate==48000:row['extra_dip_db']=(dips(y,m['onsets'],f,shift,rate)-dips(x,m['onsets'],f,0,rate)).tolist()
     rows.append(row)
summary={}
for engine in ('corr40-split-v3','corr40-split-v4'):
 d=[v for r in rows if r['engine']==engine for v in r.get('extra_dip_db',[])]
 summary[engine]={'known_B0_long_max_ms':max(max(r['lowband_additional_ms']) for r in rows if r['engine']==engine and r['input']=='bass_B0/long_notes_seed1414'),'dip_extra_db_p90':float(np.quantile(d,.9)),'dip_extra_db_max':max(d),'dip_events':len(d)}
out.write_text(json.dumps({'summary':summary,'note':'Exploratory old envelope metric, sensitive to later amplitude growth and natural modulation; no pass/fail or listening claim','rows':rows},indent=1)+'\n');print(summary)
