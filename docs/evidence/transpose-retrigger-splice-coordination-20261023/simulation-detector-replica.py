import numpy as np
from scipy import signal
from scipy.ndimage import maximum_filter1d
rate=48000
def novelty(x,btau=.004):
  x=x.astype(float);a=1-np.exp(-2*np.pi*2500/rate);h=x.copy()
  for k in range(4):h=h-signal.lfilter([a],[1,-(1-a)],h,axis=0)
  te=(h**2).mean(1);ie=(x**2).sum(1)
  fc=1-np.exp(-1/(rate*.0003));bc=1-np.exp(-1/(rate*btau))
  fast=signal.lfilter([fc],[1,-(1-fc)],te);broad=signal.lfilter([bc],[1,-(1-bc)],ie/2)
  nov=fast/np.maximum(broad,1e-12);hist=np.concatenate([np.zeros(48),nov[:-48]])
  slow=maximum_filter1d(hist,size=41*48,origin=(41*48)//2-1)
  valid=(fast>1e-10)&(ie>1e-14)
  return nov,slow,valid,fast,broad
def detect(x,ratio=4.,cool=.045,btau=.004,retrigger=None):
  nov,slow,valid,fast,broad=novelty(x,btau);cand=valid&(nov>ratio*slow);out=[];last=-10**9;lastpeak=0
  for i in np.flatnonzero(cand):
    if i-last>=int(cool*rate):out.append(i);last=i;lastpeak=nov[i:i+48].max()
    elif retrigger and nov[i]>retrigger*lastpeak and i-last>int(.002*rate):out.append(i);last=i;lastpeak=nov[i:i+48].max()
  return out
