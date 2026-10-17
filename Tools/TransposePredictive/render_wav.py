#!/usr/bin/env python3
"""Render a supplied DI WAV without silently treating it as verified real DI.

No pitch-accuracy score is inferred without labelled notes/reference audio.
Produces absolute-level and RMS-matched candidates, with provenance and no
Neural DSP measurement claim. Raw renders are never automatically published.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import numpy as np
from scipy.io import wavfile

p=argparse.ArgumentParser()
p.add_argument('--input',type=Path,required=True)
p.add_argument('--renderer',type=Path,required=True)
p.add_argument('--out',type=Path,required=True)
p.add_argument('--description',required=True,help='Instrument, song/take, tuning, effects bypass and recording provenance')
a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
rate,data=wavfile.read(a.input)
if rate!=48000:raise SystemExit('Provide a native 48 kHz file; no silent resampling')
if data.ndim==1:data=np.column_stack((data,data))
if data.ndim!=2 or data.shape[1]!=2:raise SystemExit('Only mono/stereo WAV supported')
if np.issubdtype(data.dtype,np.integer):
    if data.dtype==np.uint8:x=(data.astype(np.float64)-128)/128
    else:x=data.astype(np.float64)/float(2**(np.iinfo(data.dtype).bits-1))
else:x=data.astype(np.float64)
if not len(x) or not np.isfinite(x).all() or np.max(abs(x))==0:raise SystemExit('Empty, nonfinite or silent input')
x=x.astype('<f4');raw=a.out/'input.f32';x.tofile(raw)
hashfile=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
manifest={'description_supplied_by_operator':a.description,'input_sha256':hashfile(a.input),'input_name':a.input.name,'render_input_sha256':hashfile(raw),'sample_rate':rate,'renderer_sha256':hashfile(a.renderer),'actual_DI_provenance_verified':False,'neural_reference_available':False,'listening_acceptance':'NOT_RUN','outputs':[]}
for shift in [-2,-1,1,2]:
    for engine in ['baseline','predict1024','predict2048','predict4096']:
        stem=f'{engine}_{shift}';output=a.out/(stem+'.f32');callback=a.out/(stem+'_callbacks.csv')
        r=subprocess.run([str(a.renderer.resolve()),str(raw.resolve()),str(output.resolve()),str(rate),'128',str(shift),engine,str(callback.resolve())],capture_output=True,text=True,check=True)
        y=np.fromfile(output,dtype='<f4').reshape(-1,2)
        if y.shape!=x.shape or not np.isfinite(y).all():raise SystemExit('Invalid render')
        gain=float(np.sqrt(np.mean(x.astype(float)**2))/max(np.sqrt(np.mean(y.astype(float)**2)),1.e-20))
        gain=min(gain,.95/max(float(np.max(abs(y))),1.e-20))
        absolute=a.out/(stem+'_RAW.wav');matched=a.out/(stem+'_RMSmatched.wav')
        wavfile.write(absolute,rate,y);wavfile.write(matched,rate,(y*gain).astype('<f4'))
        manifest['outputs'].append({'engine':engine,'semitones':shift,'raw_wav':absolute.name,'raw_sha256':hashfile(absolute),'matched_wav':matched.name,'matched_sha256':hashfile(matched),'matched_gain':gain,'native':json.loads(r.stdout)})
(a.out/'manifest.json').write_text(json.dumps(manifest,indent=2,allow_nan=False)+'\n')
print('Rendered 16 comparisons. No listening, host, Neural-reference or DI-provenance PASS inferred.')
