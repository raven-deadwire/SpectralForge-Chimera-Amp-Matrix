import sys,os,subprocess,json;sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy import signal
import sweep, phase
rate=48000
def make(f0,spacing=.25,decay=.08,n=10,seed=1):
  rng=np.random.default_rng(seed);t=np.arange(int((.3+spacing*n+.3)*rate))/rate;z=np.zeros_like(t);on=[]
  hp=signal.butter(4,4000,btype='high',fs=rate,output='sos')
  for k in range(n):
    s=.3+k*spacing;o=int(s*rate);on.append(o);u=t[o:]-s;ph=rng.uniform(0,2*np.pi,6)
    body=sum(.15/h*np.sin(2*np.pi*f0*h*u+ph[h-1]) for h in range(1,7))*(1-np.exp(-u/.001))*np.exp(-u/decay)
    z[o:]+=body
    click=np.zeros(int(.004*rate));click[:]=rng.uniform(-1,1,len(click))*np.exp(-np.arange(len(click))/(.0007*rate))*.05
    z[o:o+len(click)]+=signal.sosfilt(hp,click)
  x=np.column_stack((z,-.65*z)).astype('<f4');return x,on
def glitch(y,on,f,a_lim=.1):
  sos=signal.butter(2,[f*.8,f*1.2],btype='band',fs=rate,output='sos');b=signal.sosfiltfilt(sos,y[:,0].astype(float));z=signal.hilbert(b)
  inst=np.gradient(np.unwrap(np.angle(z)))*rate/(2*np.pi);amp=np.abs(z);out=[];dips=[]
  for o,nx in zip(on,on[1:]+[len(y)]):
    s=slice(o+int(.010*rate),min(nx,o+int(.200*rate)));seg=inst[s];a=amp[s];ok=a>a_lim*a.max()
    out.append(float(np.max(np.abs(1200*np.log2(np.maximum(seg[ok],1)/f)))))
    # envelope smoothness: max deviation of log-amplitude from a linear (exponential-decay) fit
    la=20*np.log10(np.maximum(a[ok],1e-12));tt=np.arange(len(la));c=np.polyfit(tt,la,1);dips.append(float(np.max(np.abs(la-np.polyval(c,tt)))))
  return out,dips
if __name__=='__main__':
  cfgs=json.loads(sys.argv[1])
  for f0,inst in [(55.,'A1'),(30.8677,'B0'),(27.5,'A0')]:
    x,on=make(f0);p=sweep.S+f'/clean_{inst}.f32';x.tofile(p)
    gi,di=glitch(x,on,f0)
    for env in cfgs:
      for sh in [2,1,-2]:
        subprocess.run([sweep.R,p,sweep.OUT,'48000','128',str(sh),'corr40-split-rt',sweep.CB],check=True,capture_output=True,env={**os.environ,**{k:str(v) for k,v in env.items()}})
        y=np.fromfile(sweep.OUT,dtype='<f4').reshape(-1,2);g,d=glitch(y,on,f0*2**(sh/12))
        low=sweep.lowband_extra(x,y,on,rate)
        print(inst,env,sh,'low max %.2f med %.2f'%(max(low),np.median(low)),'glitch cents max %.1f med %.1f (input %.1f)'%(max(g),np.median(g),max(gi)),'env dev dB max %.1f med %.1f (input %.1f)'%(max(d),np.median(d),max(di)),flush=True)
