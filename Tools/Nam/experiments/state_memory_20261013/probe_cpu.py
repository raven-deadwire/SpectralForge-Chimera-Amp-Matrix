#!/usr/bin/env python3
"""Separate process CPU consumption from scheduler-stalled wall callbacks.

Subtract a one-sample load/reset/prewarm control from the four-second run.
This estimates steady processing CPU; it does not establish callback deadlines.
"""
import argparse
import json
from pathlib import Path
import resource
import subprocess
import numpy as np

from probe_rf import sha, write


def measured(command):
    before=resource.getrusage(resource.RUSAGE_CHILDREN)
    runtime=json.loads(subprocess.check_output(command,text=True))
    after=resource.getrusage(resource.RUSAGE_CHILDREN)
    return {'user_seconds':after.ru_utime-before.ru_utime,
            'system_seconds':after.ru_stime-before.ru_stime,'runtime':runtime}


def run(a):
    a.out.mkdir(parents=True,exist_ok=False)
    one=a.out/'one.f32';np.zeros(1,dtype='<f4').tofile(one)
    report={'reason':'Initial shared-host wall timings had scheduler stalls and contradictory rankings',
            'protocol':'one-sample startup/prewarm control followed by four-second archived input per variant/block; subtract child user+system CPU',
            'source_results_sha256':sha(a.rf/'results.json'),'tool_sha256':sha(a.tool),
            'release_approved':False,'realtime_qualified':False,'status':'RUNNING','variants':{}}
    write(a.out/'plan.json',report)
    for mult in [1,2,4,8]:
        rows={}
        for block in [64,256]:
            model=a.rf/f'runtime-only-rf{mult}.nam'
            control=measured([str(a.tool),str(model),str(one),str(a.out/'control.f32'),str(block)])
            run=measured([str(a.tool),str(model),str(a.rf/'runtime-input.f32'),str(a.out/f'rf{mult}-{block}.f32'),str(block)])
            seconds=sum(run[k]-control[k] for k in ['user_seconds','system_seconds'])
            rows[str(block)]={'startup_control':control,'full_run':run,
                             'estimated_cpu_seconds':seconds,'estimated_one_core_percent':seconds/(4-1/48000)*100}
            if seconds<=0:
                report['variants'][str(mult)]=rows
                report['status']='INVALID_MEASUREMENT'
                report['ranking_usable']=False
                write(a.out/'results.json',report)
                raise ValueError('Invalid CPU estimate; all rankings unusable')
        report['variants'][str(mult)]=rows;write(a.out/'results.json',report)
        print(mult,{k:round(v['estimated_one_core_percent'],2) for k,v in rows.items()},flush=True)
    report['status']='DIAGNOSTIC_ONLY'
    report['ranking_usable']=False
    write(a.out/'results.json',report)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for n in ['rf','tool','out']:p.add_argument('--'+n,type=Path,required=True)
    run(p.parse_args())
