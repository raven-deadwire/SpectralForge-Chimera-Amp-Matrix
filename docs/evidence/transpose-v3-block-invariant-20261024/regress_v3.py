import sys;sys.path.insert(0,"Tools/TransposeNext")  # run from the repository root
import sys,subprocess,hashlib,numpy as np,tempfile,json
from pathlib import Path
from latency_targets import inputs
OLD,NEW=sys.argv[1],sys.argv[2]
res={'same':0,'diff':[]}
with tempfile.TemporaryDirectory() as w:
    w=Path(w)
    for rate in (44100,48000,96000):
        for key,x,meta,roles in inputs(rate):
            x.tofile(w/'in.f32')
            for eng in ('corr40-split-v2','corr40-split-rt','corr40-split-lpc','baseline'):
                for sh in (-2,-1,1,2):
                    h=[]
                    for R in (OLD,NEW):
                        r=subprocess.run([R,str(w/'in.f32'),str(w/'o.f32'),str(rate),'128',str(sh),eng,str(w/'cb.csv')],check=True,capture_output=True,text=True)
                        h.append((hashlib.sha256((w/'o.f32').read_bytes()).hexdigest(),json.loads(r.stdout)['onset_samples']))
                    if h[0]==h[1]:res['same']+=1
                    else:res['diff'].append((rate,key,eng,sh))
        print(rate,res['same'],len(res['diff']),flush=True)
print(json.dumps(res))
