import sys,json,os,subprocess;sys.path.insert(0,'.')
import numpy as np
import sweep
res={}
for hold in [20,22.5,25,27.5,30]:
  for sh in [1,2]:
    per={}
    for name,p,x,m in sweep.cases:
      subprocess.run([sweep.R,p,sweep.OUT,'48000','128',str(sh),'corr40-split-rt',sweep.CB],check=True,capture_output=True,env={**os.environ,'TN_BODY_HOLD':str(hold)})
      y=np.fromfile(sweep.OUT,dtype='<f4').reshape(-1,2);inst=name.split('/')[-1].split('-')[0] if name.startswith('H') else name.split('/')[0]
      per.setdefault(inst,[]).extend(sweep.lowband_extra(x,y,m['onsets'],48000))
    print(hold,sh,{k:round(max(v),2) for k,v in per.items()},flush=True)
