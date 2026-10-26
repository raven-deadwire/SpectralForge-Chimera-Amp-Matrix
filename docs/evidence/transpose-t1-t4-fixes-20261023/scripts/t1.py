import sys,os,subprocess,json;sys.path.insert(0,'/home/user/SpectralForge-Chimera-Amp-Matrix/Tools/TransposeNext')
import numpy as np
from scipy import signal
from weak_attack_study import generate
from evaluate import attacks
S=os.path.dirname(os.path.abspath(__file__));R=S+'/bin/TransposeProbe';rate=48000
OUT=f'{S}/t1_{os.getpid()}.f32'
hp=signal.butter(4,[1000,8000],btype='band',fs=rate,output='sos')
def env(z):
  h=signal.sosfilt(hp,z[:,0].astype(float));k=int(.002*rate);return np.sqrt(np.convolve(h**2,np.ones(k)/k,'same'))
def run(name,inst,shift,e):
  x,m=generate(name,rate,inst);ip=f'{S}/t1in_{os.getpid()}.f32';x.tofile(ip)
  subprocess.run([R,ip,OUT,str(rate),'128',str(shift),'corr40-split-rt',OUT+'.csv'],check=True,capture_output=True,env={**os.environ,**e})
  y=np.fromfile(OUT,dtype='<f4').reshape(-1,2);return x,y,m
if __name__=='__main__':
  for e in json.loads(sys.argv[1]):
    for inst in ['guitar_dropA_A1','bass_B0']:
      x,y,m=run('quieter_markers',inst,2,{k:str(v) for k,v in e.items()})
      b=attacks(x,m);a=attacks(y,m);lag=np.array(a['attack_lags_ms'])-np.array(b['attack_lags_ms'])
      xs,ys,_=run('harmonics',inst,2,{k:str(v) for k,v in e.items()})
      hs=env(ys)[48000:120000];hx=env(xs)[48000:120000]
      print(e,inst,'quieter_markers +2 attack max %.2f ms'%lag.max(),'| sustained harmonics +2: HF env p99.9 %.1e (input %.1e)'%(np.percentile(hs,99.9),np.percentile(hx,99.9)),flush=True)
