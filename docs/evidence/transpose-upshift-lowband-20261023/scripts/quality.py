import sys,os,subprocess,json;sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy import signal
import sweep
rate=48000
def note_dips(y,on,f0,shift):
  # Envelope of the shifted fundamental (band-pass +-30%), per note: deepest
  # dip below the running maximum envelope in the note's first 60 ms after
  # 5 ms. A phase-misaligned splice cross-fade shows as a sharp dip.
  f=f0*2**(shift/12);sos=signal.butter(2,[f*.7,f*1.3],btype='band',fs=rate,output='sos')
  b=signal.sosfiltfilt(sos,y[:,0].astype(float));env=np.abs(signal.hilbert(b))
  out=[]
  for o,nx in zip(on,on[1:]+[len(y)]):
    seg=env[o+int(.005*rate):min(nx,o+int(.060*rate))]
    if len(seg)<10:continue
    run=np.maximum.accumulate(seg[::-1])[::-1]  # future max: decay-tolerant
    out.append(float(20*np.log10(np.max(run/np.maximum(seg,1e-12)))))
  return out
F={'bass_B0':30.8677,'bass_dropA_A0':27.5,'guitar_dropA_A1':55.0}
def evaluate(env,shift):
  dips=[];low=[];per={}
  for name,p,x,m in sweep.cases:
    subprocess.run([sweep.R,p,sweep.OUT,'48000','128',str(shift),'corr40-split-rt',sweep.CB],check=True,capture_output=True,env={**os.environ,**env})
    y=np.fromfile(sweep.OUT,dtype='<f4').reshape(-1,2);inst=[k for k in F if k in name][0]
    d=note_dips(y,m['onsets'],F[inst],shift);dx=note_dips(x,m['onsets'],F[inst],0)
    dips+=[a-b for a,b in zip(d,dx)];low+=sweep.lowband_extra(x,y,m['onsets'],rate)
  return {'low_max':round(max(low),2),'low_median':round(float(np.median(low)),2),'extra_dip_db_p90':round(float(np.percentile(dips,90)),2),'extra_dip_db_max':round(max(dips),2)}
if __name__=='__main__':
  for env in json.loads(sys.argv[1]):
    for sh in [int(s) for s in sys.argv[2].split(',')]:
      print(env,sh,evaluate({k:str(v) for k,v in env.items()},sh),flush=True)
