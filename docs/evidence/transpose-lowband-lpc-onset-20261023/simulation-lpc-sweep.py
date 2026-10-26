import sys;sys.path.insert(0,'.')
import numpy as np
from scipy import signal
from scipy.linalg import solve_toeplitz
from weak_attack_study import generate
rate=48000;D=12;fd=rate//D
sos=signal.butter(2,350,fs=rate,output='sos');sos=np.vstack([sos,sos])
def holdout(inst,mode):
  from weak_attack_study import FREQUENCIES
  f=FREQUENCIES[inst];rng=np.random.default_rng(8139);t=np.arange(3*rate)/rate;starts=.3+np.cumsum(rng.uniform(.08,.135,16));on=[round(s*rate) for s in starts]
  z=np.zeros_like(t) if mode=='random_plucks' else sum(.12/h*np.sin(2*np.pi*f*h*t+.19*h) for h in range(1,13))
  for o in on:
    L=round(.065*rate);u=np.arange(L)/rate
    if mode=='random_plucks':
      g=rng.uniform(.06,1);dec=rng.uniform(.018,.04);ph=rng.uniform(-.2,.2,8);body=sum(.14/h*np.sin(2*np.pi*f*h*u+ph[h-1]) for h in range(1,9))
      env=(1-np.exp(-u/.001))*np.exp(-u/dec);env[-240:]*=np.linspace(1,0,240);z[o:o+L]+=g*(body*env+.05*np.exp(-u/.002)*rng.uniform(-1,1,L))
    else:z[o:o+L]+=rng.uniform(.001,.008)*np.exp(-u/.002)*rng.uniform(-1,1,L)
  return np.column_stack((z,-.65*z)),{'onsets':on}
def features(x,order=10,win=.032,hop=.001):
  low=signal.sosfilt(sos,x.astype(float),axis=0).mean(1);y=low[::D]  # decimated low band (mono sum-ish)
  n=len(y);W=int(win*fd);H=max(1,int(hop*fd));a=np.zeros(order);res=np.zeros(n)
  for i in range(order,n):
    if i%H==0 and i>=W:
      seg=y[i-W:i]*np.hanning(W);r=np.correlate(seg,seg,'full')[W-1:W+order]
      if r[0]>1e-14:
        r[0]*=1.0001;a=solve_toeplitz(r[:-1],r[1:])
    res[i]=y[i]-np.dot(a,y[i-order:i][::-1])
  return y,res
def smooth(e,tau):
  c=1-np.exp(-1/(fd*tau));return signal.lfilter([c],[1,-(1-c)],e)
CASES=[]
for inst in ['bass_B0','bass_dropA_A0','guitar_dropA_A1']:
  for name in ['riff75','riff50','alternating_velocity','impulses']:CASES.append((inst,name,True))
  for name in ['quiet_markers_over_sustain','quieter_markers']:CASES.append((inst,name,False))
  CASES+= [(inst,'H:random_plucks',True),(inst,'H:random_markers',False)]
data={}
for inst,name,want in CASES:
  x,m=holdout(inst,name[2:]) if name.startswith('H:') else generate(name,rate,inst)
  y,res=features(x);data[inst,name]=(y,res,m['onsets'],want)
def evaluate(look=.025,win=.012,thr_e=2.,thr_r=4.,tau=.001,use_e=True,use_r=True):
  hit=tot=fp=fpt=0;fails=[]
  for (inst,name),(y,res,on,want) in data.items():
    E=smooth(y**2,tau);R=smooth(res**2,tau);L=int(look*fd);Wn=int(win*fd);n_hit=0
    for o in on:
      od=o//D;pe=E[max(0,od-L):od].max() if od>0 else 0;pr=R[max(0,od-L):od].max() if od>0 else 0
      seg=slice(od,od+Wn)
      ok=(use_e and np.any(E[seg]>thr_e*pe+1e-12)) or (use_r and np.any(R[seg]>thr_r*pr+1e-12) and np.any(R[seg]>0.05*E[seg]))
      n_hit+=ok
    if want: hit+=n_hit;tot+=len(on)
    else: fp+=n_hit;fpt+=len(on)
    if (want and n_hit<len(on)) or (not want and n_hit): fails.append(f'{inst}/{name} {n_hit}/{len(on)}')
  return hit,tot,fp,fpt,fails
import sys
if 0: print('energy only',evaluate(use_r=False))
for thr in ([] if 1 else [3]):
  print('energy|resid thr',thr,evaluate(thr_r=thr))
  print('resid only thr',thr,evaluate(thr_r=thr,use_e=False))
print('--- sweep')
for thr in [1.5,2,2.5,3]:
  for look in [.015,.025,.040]:
    for tau in [.0005,.001,.002]:
      r=evaluate(thr_r=thr,look=look,tau=tau);print(thr,look,tau,r[:4],r[4])
def evaluate_norm(thr=3,look=.025,win=.012,tau=.001,floor=.02):
  hit=tot=fp=fpt=0;fails=[]
  for (inst,name),(y,res,on,want) in data.items():
    E=smooth(y**2,tau);R=smooth(res**2,tau);N=R/np.maximum(E,1e-12);L=int(look*fd);Wn=int(win*fd);k=0
    for o in on:
      od=o//D;pe=E[max(0,od-L):od].max();pn=N[max(0,od-L):od].max();seg=slice(od,od+Wn)
      k+=bool(np.any(E[seg]>2*pe+1e-12) or np.any((N[seg]>thr*pn)&(N[seg]>floor)))
    if want:hit+=k;tot+=len(on)
    else:fp+=k;fpt+=len(on)
    if (want and k<len(on)) or (not want and k):fails.append(f'{inst}/{name} {k}/{len(on)}')
  return hit,tot,fp,fpt,fails
print('--- normalized')
for thr in [1.5,2,3,5]:
  for floor in [.01,.05,.2]:
    r=evaluate_norm(thr=thr,floor=floor);print(thr,floor,r[:4],r[4])
