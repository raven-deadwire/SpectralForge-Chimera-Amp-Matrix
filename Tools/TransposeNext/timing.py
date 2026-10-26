#!/usr/bin/env python3
"""Five alternating process-order passes; wall and Linux thread CPU reported apart."""
import argparse
import json
import platform
import subprocess
from pathlib import Path
import numpy as np
from legacy_metrics import fixture, sha

p=argparse.ArgumentParser();p.add_argument('--renderer',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
a=p.parse_args();a.renderer=a.renderer.resolve();a.out.mkdir(parents=True,exist_ok=True)
x,meta=fixture('riff75',48000)
# Repeat the actual fixture, including its trailing silence, to exercise a range
# of splice phases. Identical file and process startup are used for all engines.
x=np.tile(x,(4,1));ip=a.out/'input.f32';x.tofile(ip)
engines=['baseline','corr40','corr40-onset'];results=[]
for block in [64,128,256]:
    for repeat in range(5):
        order=engines if repeat%2==0 else engines[::-1]
        for engine in order:
            key=f'{block}-{repeat}-{engine}';cp=a.out/(key+'.csv');op=a.out/'output.f32'
            r=subprocess.run([str(a.renderer),str(ip),str(op),'48000',str(block),'-2',engine,str(cp),'cpu'],check=True,capture_output=True,text=True)
            row={'block':block,'repeat':repeat,'engine':engine,'order':order,**json.loads(r.stdout),
                 'output_sha256':sha(op),'callback_sha256':sha(cp)}
            data=np.genfromtxt(cp,delimiter=',',names=True)
            # The first 0.5 s is excluded from summary quantiles. Raw data,
            # complete-run max and misses are retained; no outliers are dropped.
            settled=data[np.cumsum(data['samples'])>24000]
            row['settled_thread_cpu_p95_us']=float(np.percentile(settled['thread_cpu_us'],95))
            row['settled_thread_cpu_p99_us']=float(np.percentile(settled['thread_cpu_us'],99))
            row['settled_wall_p99_us']=float(np.percentile(settled['microseconds'],99))
            row['thread_cpu_audio_percent']=float(data['thread_cpu_us'].sum()/(len(x)/48000*1e6)*100)
            row['wall_audio_percent']=float(data['microseconds'].sum()/(len(x)/48000*1e6)*100)
            results.append(row)
            (a.out/'timing.json').write_text(json.dumps({'status':'SHARED_LINUX_DIAGNOSTIC_NOT_DAW_ACCEPTANCE',
                'platform':platform.platform(),'renderer_sha256':sha(a.renderer),'input_sha256':sha(ip),
                'frames':len(x),'rate':48000,'shift':-2,'results':results},indent=2)+'\n')
            print(key,flush=True)
