import sys,glob,json;sys.path.insert(0,'.');sys.path.insert(0,'/tmp/claude-0/sim')
import numpy as np
from det_replica import novelty
from weak_attack_study import generate
rate=48000
def detect(nov,slow,valid,R,peakwin=.002,cool=.045):
  out=[];last=-10**9;peak=0
  for i in range(len(nov)):
    if i-last<int(peakwin*rate):peak=max(peak,nov[i]);continue
    if not(valid[i] and nov[i]>4*slow[i]):continue
    if i-last>=int(cool*rate) or (R and nov[i]>R*peak):out.append(i);last=i;peak=nov[i]
  return out
cases=[]
for inst in ['bass_B0','bass_dropA_A0','guitar_dropA_A1']:
  for name in ['riff75','riff50','alternating_velocity','impulses','quiet_markers_over_sustain','quieter_markers','tremolo','vibrato','bright_sustain','noise_floor','harmonics','weakfundamental']:
    x,m=generate(name,rate,inst);cases.append((f'{inst}/{name}',x,m.get('onsets',[])))
for sd in ['8139','20261024']:
  for f in sorted(glob.glob(f'/tmp/claude-0/lpc/holdout-{sd}/*-random_*.f32')):
    key=f.split('/')[-1][:-4];on=json.load(open(f'/tmp/claude-0/lpc/holdout-{sd}/results.json'))['inputs'][key]['onsets']
    cases.append((f'H{sd}/{key}',np.fromfile(f,dtype='<f4').reshape(-1,2),on))
feats=[(k,novelty(x),on) for k,x,on in cases]
for R in [None,4,8,16,64]:
  miss=false=0;det=[]
  for k,(nov,slow,valid,_,_),on in feats:
    nov=nov[:];idx=[s for s in detect(nov,slow,valid,R) if s>int(.01*rate)]
    m=[min([s-t for s in idx if 0<=s-t<=int(.01*rate)],default=None) for t in on]
    mi=sum(v is None for v in m);matched=set(s for s in idx for t in on if 0<=s-t<=int(.01*rate))
    fa=len([s for s in idx if s not in matched]);miss+=mi;false+=fa
    if mi or fa:det.append(f'{k} miss{mi} false{fa}')
  print('R',R,'miss',miss,'false',false,det,flush=True)
