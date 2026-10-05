#!/usr/bin/env python3
"""Paired 4x JUCE channel comparison. Synthetic observations, not listening PASS.

Two renderer binaries must use the same JUCE wrapper/compiler and only differ
in the Original core. Audio stays in --work; share the JSON/CSV in --out.
"""
import argparse
import concurrent.futures
import csv
import hashlib
import itertools
import json
import subprocess
from pathlib import Path
import numpy as np
from scipy.io import wavfile

SR=48000
CHANNELS=['fenrir','surtr','nidhoggr','fimbulvetr','ragnarok']
def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def rms(x):return float(np.sqrt(np.mean(x*x)))
def profile(x):
    n=4096;window=np.hanning(n)
    frames=np.stack([x[i:i+n]*window for i in range(0,len(x)-n+1,n//2)])
    power=np.mean(abs(np.fft.rfft(frames))**2,axis=0);f=np.fft.rfftfreq(n,1/SR)
    edges=np.geomspace(60,7000,15)
    bands=np.array([power[(f>=lo)&(f<hi)].sum() for lo,hi in zip(edges[:-1],edges[1:])])
    bands/=bands.sum()
    return 10*np.log10(np.maximum(bands,1e-7))
def fixture(kind,level):
    t=np.arange(SR*2)/SR;local=t%.25;env=np.where(local<.16,np.exp(-18*local),0)
    x=np.sin(2*np.pi*61.735*t)+.35*np.sin(2*np.pi*123.47*t)+.16*np.sin(2*np.pi*493.88*t)
    if kind=='chord':x+=.5*np.sin(2*np.pi*92.499*t)+.35*np.sin(2*np.pi*155.56*t)
    x*=env*level
    if kind=='driven':x=.20*np.tanh(20*x)
    return x.astype(np.float32)
def main():
    ap=argparse.ArgumentParser()
    for key in ['before-render','after-render','work','out']:ap.add_argument('--'+key,type=Path,required=True)
    args=ap.parse_args();args.work.mkdir(parents=True,exist_ok=True);args.out.mkdir(parents=True,exist_ok=True)
    binaries={'before':args.before_render.resolve(),'after':args.after_render.resolve()}
    tasks=[];inputs={};states={}
    for kind,level in itertools.product(['pluck','chord','driven'],[.012,.12]):
        key=f'{kind}-{level}';p=args.work/f'{key}.wav';wavfile.write(p,SR,fixture(kind,level));inputs[key]=p
    for c in range(5):
        p=args.work/f'channel-{c}.json';p.write_text(json.dumps({'channel':c,'modern':True}));states[c]=p
    for version,exe in binaries.items():
        for key,input_file in inputs.items():
            for c,state in states.items():tasks.append((version,key,c,exe,input_file,state,args.work/f'{version}-{key}-{c}.f32'))
    def render(task):
        version,key,c,exe,source,state,dest=task
        r=subprocess.run([str(exe),str(source.resolve()),str(dest.resolve()),str(state.resolve())],capture_output=True,text=True)
        if r.returncode:raise RuntimeError(r.stderr)
        x=np.fromfile(dest,np.float32).astype(float)
        if len(x)!=SR*2 or not np.isfinite(x).all():raise ValueError('invalid render')
        return (version,key,c),x
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:audio=dict(pool.map(render,tasks))
    rows=[];levels=[]
    for version,key in itertools.product(binaries,inputs):
        data=[audio[version,key,c] for c in range(5)];spectra=[profile(x) for x in data]
        for c,x in enumerate(data):levels.append(dict(version=version,probe=key,channel=CHANNELS[c],rms_dbfs=20*np.log10(rms(x)),peak_dbfs=20*np.log10(max(abs(x)))))
        for a,b in itertools.combinations(range(5),2):
            rows.append(dict(version=version,probe=key,pair=f'{CHANNELS[a]}/{CHANNELS[b]}',matched_residual=rms(data[a]/rms(data[a])-data[b]/rms(data[b])),spectral_distance_db=float(np.sqrt(np.mean((spectra[a]-spectra[b])**2)))))
    summary={}
    for version in binaries:
        v=[r for r in rows if r['version']==version]
        summary[version]={'minimum_matched_residual':min(r['matched_residual'] for r in v),'minimum_spectral_distance_db':min(r['spectral_distance_db'] for r in v),'mean_spectral_distance_db':float(np.mean([r['spectral_distance_db'] for r in v]))}
    result={'sample_rate':SR,'oversampling':4,'input_levels':[.012,.12],'probe_duration_seconds':2,'band_edges_hz':np.geomspace(60,7000,15).tolist(),'spectral_method':'14 energy bands, 4096 Hann / 2048 hop, normalized band power, -70 dB floor; RMS dB-profile distance','limits':'Synthetic inputs, including a generic tanh pre-drive probe. No real DI, no NAM rerun and no hardware/listening PASS. Synthetic fixtures informed revision.','renderers':{v:{'path':str(p),'sha256':digest(p)} for v,p in binaries.items()},'inputs':{k:digest(p) for k,p in inputs.items()},'summary':summary,'levels':levels,'pairs':rows}
    (args.out/'channel-separation.json').write_text(json.dumps(result,indent=2)+'\n')
    with (args.out/'channel-separation.csv').open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0]),lineterminator="\n");w.writeheader();w.writerows(rows)
    print(json.dumps(summary,indent=2))
if __name__=='__main__':main()
