import sys,os,subprocess,json;sys.path.insert(0,'/home/user/SpectralForge-Chimera-Amp-Matrix/Tools/TransposeNext')
import numpy as np
from scipy import signal
from weak_attack_study import generate, FREQUENCIES
from weak_attack_holdout import holdout_input
S=os.path.dirname(os.path.abspath(__file__));R='/tmp/claude-0/tn/TransposeProbe';rate=48000;OUT=f'{S}/f2_{os.getpid()}.f32'
hp=signal.butter(4,[1000,8000],btype='band',fs=rate,output='sos')
def env(z):
  h=signal.sosfilt(hp,z[:,0].astype(float));k=int(.002*rate);return np.sqrt(np.convolve(h**2,np.ones(k)/k,'same'))
def flam(z,on):
  # Per note: strongest 1-8 kHz envelope at least 6 ms after the note's main
  # peak (before the next onset), relative to that main peak, in dB.
  e=env(z);out=[]
  for o,nx in zip(on,on[1:]+[len(z)]):
    seg=e[o:min(nx,o+int(.08*rate))];p=int(np.argmax(seg[:int(.012*rate)]));rest=seg[p+int(.006*rate):]
    out.append(20*np.log10(max(rest.max(),1e-12)/max(seg[p],1e-12)) if len(rest) else -99)
  return np.array(out)
cases=[]
for inst,f in FREQUENCIES.items():
  for n in ['riff75','alternating_velocity']:
    x,m=generate(n,rate,inst);cases.append((f'{inst}/{n}',x,m['onsets']))
  for mode in ['random_plucks','random_soft_after_loud']:
    x,on=holdout_input(f,mode,20261024,rate);cases.append((f'{inst}/{mode}',x,on))
ENGINE=sys.argv[2] if len(sys.argv)>2 else 'corr40-split-rt'
for e in json.loads(sys.argv[1]):
  for sh in [1,2]:
    worst=-99;excess=[]
    for name,x,on in cases:
      ip=f'{S}/f2in_{os.getpid()}.f32';x.tofile(ip)
      subprocess.run([R,ip,OUT,str(rate),'128',str(sh),ENGINE,OUT+'.csv'],check=True,capture_output=True,env={**os.environ,**{k:str(v) for k,v in e.items()}})
      y=np.fromfile(OUT,dtype='<f4').reshape(-1,2);d=flam(y,on)-flam(x,on);excess+=list(d)
    print(e,sh,'second-peak excess over input dB: p90 %.1f max %.1f'%(np.percentile(excess,90),max(excess)),flush=True)
