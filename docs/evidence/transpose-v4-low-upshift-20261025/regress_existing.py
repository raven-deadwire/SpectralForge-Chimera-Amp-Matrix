"""Compare pre-change and final renderers; output and event logs, not CPU."""
import sys,json,subprocess,tempfile,hashlib
from pathlib import Path
sys.path.insert(0,'Tools/TransposeNext')
from latency_targets import inputs
old,new,out=map(Path,sys.argv[1:]);rows=[]
with tempfile.TemporaryDirectory() as w:
 w=Path(w)
 for rate in (44100,48000,96000):
  for key,x,_,_ in inputs(rate,(14142,),1414):
   if key.split('/')[1] not in ('riff75','quieter_markers','long_notes_seed1414'):continue
   x.tofile(w/'in.f32')
   for engine in ('baseline','corr40-split-v2','corr40-split-v3','corr40-split-rt','corr40-split-lpc'):
    for shift in (-2,-1,1,2):
     obs=[]
     for binary in (old,new):
      r=subprocess.run([str(binary.resolve()),str(w/'in.f32'),str(w/'out.f32'),str(rate),'128',str(shift),engine,str(w/'cb.csv')],capture_output=True,text=True,check=True)
      m=json.loads(r.stdout);obs.append({'sha':hashlib.sha256((w/'out.f32').read_bytes()).hexdigest(),'events':m['onset_samples'],'low':m['low_onset_resets']})
     rows.append({'rate':rate,'input':key,'engine':engine,'shift':shift,'equal':obs[0]==obs[1]})
  print(rate,len(rows),flush=True)
summary={'cases':len(rows),'equal':sum(r['equal'] for r in rows),'mismatches':[r for r in rows if not r['equal']]}
out.write_text(json.dumps({'summary':summary,'rows':rows},indent=1)+'\n');print(summary)
