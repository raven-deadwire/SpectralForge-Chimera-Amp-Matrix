import sys,os,json,subprocess,itertools
sys.path.insert(0,'/home/user/SpectralForge-Chimera-Amp-Matrix/Tools/TransposeNext')
import numpy as np
from weak_attack_study import generate
from weak_attack_holdout import lowband_extra
from evaluate import attacks
from scipy import signal
LP=signal.butter(4,120,fs=48000,output='sos')
def onset_energy(x,y,on,rate,ms=8):
  # Output/input low-band energy in the first `ms` after each onset, output
  # window shifted by that note's best lag (0-20 ms) so delay is not counted.
  a=(signal.sosfilt(LP,x.astype(float),axis=0)**2).sum(1);b=(signal.sosfilt(LP,y.astype(float),axis=0)**2).sum(1);n=int(ms*rate/1000);r=[]
  for o in on:
    ein=a[o:o+n].sum();best=max(b[o+l:o+l+n].sum() for l in range(0,int(.02*rate),24));r.append(best/max(ein,1e-30))
  return r
S=os.path.dirname(os.path.abspath(__file__));R=S+'/bin/TransposeProbe';rate=48000
# Per-process scratch outputs: concurrent runs must not share files.
OUT=f'{S}/o_{os.getpid()}.f32';CB=f'{S}/cb_{os.getpid()}.csv'
cases=[]
for inst in ['bass_B0','bass_dropA_A0','guitar_dropA_A1']:
  for n in ['riff75','riff50','alternating_velocity']:
    x,m=generate(n,rate,inst);p=f'{S}/in_{inst}_{n}.f32';x.tofile(p);cases.append((f'{inst}/{n}',p,x,m))
  for sd in ['8139','20261024']:
    for mode in ['random_plucks','random_soft_after_loud']:
      key=f'{inst}-{mode}';p=f'/tmp/claude-0/rt2/holdout-{sd}/{key}.f32';on=json.load(open(f'/tmp/claude-0/rt2/holdout-{sd}/results.json'))['inputs'][key]['onsets']
      x=np.fromfile(p,dtype='<f4').reshape(-1,2);cases.append((f'H{sd}/{key}',p,x,{'rate':rate,'onsets':on,'name':mode}))
base={c[0]:attacks(c[2],c[3])['attack_lags_ms'] for c in cases}
def run(env,shift,engine='corr40-split-rt'):
  low=[];att=[];per={};eng=[]
  for name,p,x,m in cases:
    subprocess.run([R,p,OUT,str(rate),'128',str(shift),engine,CB],check=True,capture_output=True,env={**os.environ,**env})
    y=np.fromfile(OUT,dtype='<f4').reshape(-1,2)
    lb=lowband_extra(x,y,m['onsets'],rate);eng+=onset_energy(x,y,m['onsets'],rate);a=np.array(attacks(y,m)['attack_lags_ms'])-np.array(base[name])
    low+=lb;att+=list(a);per[name]=(max(lb),float(np.median(lb)))
  worst=max(per.items(),key=lambda kv:kv[1][0])
  return {'low_max':max(low),'low_p90':float(np.percentile(low,90)),'low_median':float(np.median(low)),'attack_max':float(max(att)),'onset_energy_ratio_min':float(min(eng)),'onset_energy_ratio_p10':float(np.percentile(eng,10)),'worst':worst[0],'worst_val':round(worst[1][0],2)}
if __name__=='__main__':
  configs=json.loads(sys.argv[1]);shifts=[int(s) for s in sys.argv[2].split(',')]
  for env in configs:
    for sh in shifts:
      r=run({k:str(v) for k,v in env.items()},sh);print(json.dumps({'env':env,'shift':sh,**{k:(round(v,2) if isinstance(v,float) else v) for k,v in r.items()}}),flush=True)
