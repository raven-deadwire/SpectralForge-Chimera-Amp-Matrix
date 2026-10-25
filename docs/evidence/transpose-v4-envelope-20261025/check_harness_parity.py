import sys,json,gzip,hashlib,subprocess
from pathlib import Path
import numpy as np
sys.path.insert(0,'Tools/TransposeNext')
from latency_targets import inputs
root=Path.cwd();w=root/'build/envelope-parity-unique';w.mkdir(exist_ok=True);rows=[]
for rate in [44100,48000,96000]:
 for key,x,m,roles in inputs(rate,(),1414):
  if key.split('/')[1]not in ['alternating_velocity','long_notes_seed1414','quiet_markers_over_sustain']:continue
  ip=w/(str(rate)+'-'+key.replace('/','-')+'.f32');x.tofile(ip)
  for shift in [-2,-1,1,2]:
   for engine in ['corr40-split-v3','corr40-split-v4']:
    result=[];outs=[];uid=f'{rate}-{key.replace("/","-")}-{shift}-{engine}'
    for version,binpath in [('original','build/transpose-original/TransposeProbe'),('trial-harness','build/envelope-final/EnvelopeProbe')]:
     op=w/(uid+'-'+version+'.f32');cp=w/(uid+'-'+version+'.csv')
     r=subprocess.run([str(root/binpath),str(ip),str(op),str(rate),'128',str(shift),engine,str(cp)],capture_output=True,text=True,check=True);result.append(json.loads(r.stdout));outs.append(op.read_bytes())
    fields=['splices','onset_resets','onset_samples','low_onset_resets','read_min_samples','read_max_samples']
    row={'input':key,'rate':rate,'shift':shift,'engine':engine,'input_sha256':hashlib.sha256(ip.read_bytes()).hexdigest(),'output_bytes':[len(v)for v in outs],'expected_bytes':x.nbytes,'bit_equal':outs[0]==outs[1], 'events_equal':all(result[0][k]==result[1][k]for k in fields)};rows.append(row)
    Path(__file__).with_name('harness-parity.json.gz').write_bytes(gzip.compress(json.dumps({'rows':rows,'passed':sum(r['bit_equal']and r['events_equal']and all(n==r['expected_bytes']for n in r['output_bytes'])for r in rows)}).encode(),mtime=0))
    if not(row['bit_equal']and row['events_equal']and all(n==x.nbytes for n in row['output_bytes'])):raise RuntimeError(row)
 print(rate,flush=True)
print('PASS',len(rows))
