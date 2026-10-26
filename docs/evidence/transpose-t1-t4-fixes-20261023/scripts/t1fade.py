import sys,os,subprocess,json;sys.path.insert(0,'/home/user/SpectralForge-Chimera-Amp-Matrix/Tools/TransposeNext')
import numpy as np
from weak_attack_holdout import holdout_input
from weak_attack_study import FREQUENCIES, generate
from evaluate import attacks
R='/tmp/claude-0/tn-exp/TransposeProbe';OUT=f'/tmp/claude-0/sim/fd_{os.getpid()}.f32'
cases=[]
for rate in [48000,96000]:
  for inst,f in FREQUENCIES.items():
    for sd in [8139,20261024]:
      for mode in ['random_markers','random_plucks']:
        x,on=holdout_input(f,mode,sd,rate);cases.append((f'{inst}/{sd}/{mode}',rate,x,{'rate':rate,'onsets':on,'name':mode}))
    for n in ['quieter_markers','quiet_markers_over_sustain','alternating_velocity']:
      x,m=generate(n,rate,inst);cases.append((f'{inst}/{n}',rate,x,m))
for fade in json.loads(sys.argv[1]):
  for sh in [1,2]:
    worst=(0,'');vals=[]
    for name,rate,x,m in cases:
      b=attacks(x,m)
      if b['attack_lag_max_ms']>10:continue
      ip=OUT+'.in';x.tofile(ip);env={**os.environ};env.pop('TN_SPLICE_FADE',None)
      if fade:env['TN_SPLICE_FADE']=str(fade)
      subprocess.run([R,ip,OUT,str(rate),'128',str(sh),'corr40-split-v2',OUT+'.csv'],check=True,capture_output=True,env=env)
      y=np.fromfile(OUT,dtype='<f4').reshape(-1,2);lag=max(np.array(attacks(y,m)['attack_lags_ms'])-np.array(b['attack_lags_ms']));vals.append(lag)
      if lag>worst[0]:worst=(lag,f'{name}@{rate}')
    print('splice fade',fade or 'default 2ms','shift',sh,'attack max %.2f'%worst[0],worst[1],'| >10ms cases',sum(v>10 for v in vals),'of',len(vals),flush=True)
