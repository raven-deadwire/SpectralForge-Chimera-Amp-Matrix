import sys,os,subprocess,json;sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy import signal
import sweep
rate=48000;F={'bass_B0':30.8677,'bass_dropA_A0':27.5,'guitar_dropA_A1':55.0}
def glitch(y,on,f0,shift):
  # Shifted fundamental via narrow band-pass (+-25%), zero-phase; per note
  # (5..60 ms after onset) the max |instantaneous frequency deviation| in cents,
  # 1 ms median-smoothed so single-sample spikes from the filter do not count.
  f=f0*2**(shift/12);sos=signal.butter(2,[f*.75,f*1.25],btype='band',fs=rate,output='sos')
  b=signal.sosfiltfilt(sos,y[:,0].astype(float));z=signal.hilbert(b)
  inst=np.gradient(np.unwrap(np.angle(z)))*rate/(2*np.pi);amp=np.abs(z);out=[]
  for o,nx in zip(on,on[1:]+[len(y)]):
    s=slice(o+int(.005*rate),min(nx,o+int(.060*rate)))
    seg=signal.medfilt(inst[s],47);a=amp[s]
    ok=a>.1*a.max()  # ignore near-silent tails
    if ok.sum()<10:continue
    out.append(float(np.max(np.abs(1200*np.log2(np.maximum(seg[ok],1)/f)))))
  return out
def evaluate(env,shift):
  g=[];gin=[];low=[]
  for name,p,x,m in sweep.cases:
    subprocess.run([sweep.R,p,sweep.OUT,'48000','128',str(shift),'corr40-split-rt',sweep.CB],check=True,capture_output=True,env={**os.environ,**env})
    y=np.fromfile(sweep.OUT,dtype='<f4').reshape(-1,2);inst=[k for k in F if k in name][0]
    g+=glitch(y,m['onsets'],F[inst],shift);gin+=glitch(x,m['onsets'],F[inst],0);low+=sweep.lowband_extra(x,y,m['onsets'],rate)
  return {'low_max':round(max(low),2),'glitch_cents_median':round(float(np.median(g)),1),'glitch_cents_p90':round(float(np.percentile(g,90)),1),'input_glitch_p90':round(float(np.percentile(gin,90)),1)}
if __name__=='__main__':
  for env in json.loads(sys.argv[1]):
    for sh in [int(s) for s in sys.argv[2].split(',')]:
      print(env,sh,evaluate({k:str(v) for k,v in env.items()},sh),flush=True)
