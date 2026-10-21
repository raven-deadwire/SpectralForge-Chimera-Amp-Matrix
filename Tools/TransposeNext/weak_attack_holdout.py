#!/usr/bin/env python3
"""Post-selection synthetic holdout; not real DI or Neural DSP comparison.

Seed 8139, random event gaps/gains/decays/phases. Run only after the candidate
is frozen. A failure is retained; this script is not used to tune thresholds.
"""
import argparse,json,subprocess,gzip
from pathlib import Path
import numpy as np
from evaluate import attacks
from legacy_metrics import sha
from weak_attack_study import FREQUENCIES,event_detection

def main():
 p=argparse.ArgumentParser();p.add_argument('--renderer',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
 a.renderer=a.renderer.resolve();a.out.mkdir(parents=True,exist_ok=True);rate=48000;rows=[];inputs={}
 for instrument,f in FREQUENCIES.items():
  for mode in ['random_plucks','random_markers']:
   rng=np.random.default_rng(8139);t=np.arange(3*rate)/rate;starts=.3+np.cumsum(rng.uniform(.08,.135,16));onsets=[round(s*rate) for s in starts]
   z=np.zeros_like(t) if mode=='random_plucks' else sum(.12/h*np.sin(2*np.pi*f*h*t+.19*h) for h in range(1,13))
   for on in onsets:
    length=round(.065*rate);u=np.arange(length)/rate
    if mode=='random_plucks':
     gain=rng.uniform(.06,1);decay=rng.uniform(.018,.04);phases=rng.uniform(-.2,.2,8)
     body=sum(.14/h*np.sin(2*np.pi*f*h*u+phases[h-1]) for h in range(1,9))
     envelope=(1-np.exp(-u/.001))*np.exp(-u/decay)
     # Smooth final 5ms prevents exact-silence cut edges being the only cue.
     envelope[-240:] *= np.linspace(1,0,240)
     z[on:on+length] += gain*(body*envelope+.05*np.exp(-u/.002)*rng.uniform(-1,1,length))
    else:z[on:on+length] += rng.uniform(.001,.008)*np.exp(-u/.002)*rng.uniform(-1,1,length)
   x=np.column_stack((z,-.65*z)).astype('<f4');key=instrument+'-'+mode;ip=a.out/(key+'.f32');x.tofile(ip)
   meta={'rate':rate,'onsets':onsets,'name':mode,'synthetic':True};before=attacks(x,meta)
   inputs[key]={'sha256':sha(ip),'seed':8139,'fundamental_hz':f,'onsets':onsets,'input_attack':before}
   for engine in ['baseline','corr40-onset','corr40-split']:
    op=a.out/'output.f32';cp=a.out/'callbacks.csv'
    r=subprocess.run([str(a.renderer),str(ip),str(op),str(rate),'128','-2',engine,str(cp)],capture_output=True,text=True,check=True)
    y=np.fromfile(op,dtype='<f4').reshape(-1,2);measured=attacks(y,meta);extra=np.array(measured['attack_lags_ms'])-np.array(before['attack_lags_ms'])
    cb=a.out/(key+'-'+engine+'.csv.gz');cb.write_bytes(gzip.compress(cp.read_bytes(),mtime=0))
    native=json.loads(r.stdout)
    rows.append({'input':key,'engine':engine,**native,'output_sha256':sha(op),'callbacks_sha256':sha(cb),
      'attack':measured,'additional_lags_ms':extra.tolist(),'additional_max_ms':float(max(extra)),
      'onset_association':event_detection(native['onset_samples'],meta)})
    print(key,engine,'max_ms',round(float(max(extra)),3),flush=True)
 (a.out/'results.json').write_text(json.dumps({'status':'POST_SELECTION_SYNTHETIC_ONLY','renderer_sha256':sha(a.renderer),'inputs':inputs,'results':rows},indent=2)+'\n')
if __name__=='__main__':main()
