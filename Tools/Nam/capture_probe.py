#!/usr/bin/env python3
"""Capture a separately sourced evaluation input through frozen channel settings."""
import argparse, concurrent.futures, json, subprocess
from pathlib import Path
import numpy as np
import soundfile as sf
from common import digest, write_json

def main():
    p=argparse.ArgumentParser();p.add_argument('--input',type=Path,required=True)
    p.add_argument('--reference-data',type=Path,required=True);p.add_argument('--renderer',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True);p.add_argument('--source-url',required=True);args=p.parse_args()
    frozen=json.loads((args.reference_data/'manifest.json').read_text())['source']
    if digest(args.renderer)!=frozen['renderer_sha256']:raise RuntimeError('Renderer differs from frozen capture')
    x,rate=sf.read(args.input,dtype='float32')
    if rate!=48000 or x.ndim!=1 or not np.isfinite(x).all():raise RuntimeError('Expected finite mono 48 kHz input')
    if args.out.exists() and any(args.out.iterdir()):raise RuntimeError('Output must be empty')
    args.out.mkdir(parents=True,exist_ok=True);x.astype('<f4').tofile(args.out/'input.f32');np.save(args.out/'input.npy',x)
    settings=json.loads((args.reference_data/'channel-settings.json').read_text())
    def render(setting):
        name=setting['name'];job=args.out/(name+'.json');raw=args.out/(name+'.f32')
        write_json(job,{'channel':setting['channel'],'controls':setting['controls']})
        info=json.loads(subprocess.check_output([str(args.renderer),str(args.out/'input.f32'),str(raw),str(job)],text=True))
        y=np.fromfile(raw,dtype='<f4');raw.unlink()
        if len(x)!=len(y) or not np.isfinite(y).all():raise RuntimeError('Invalid capture output')
        np.save(args.out/(name+'.npy'),y)
        print(name,'captured',flush=True)
        return {'name':name,'output_sha256':digest(args.out/(name+'.npy')),'render':info,'settings':setting}
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:results=list(pool.map(render,settings))
    write_json(args.out/'manifest.json',{'role':'unseen external evaluation only; never used in training or checkpoint selection',
      'input_source_url':args.source_url,'input_file_sha256':digest(args.input),'input_npy_sha256':digest(args.out/'input.npy'),
      'input_seconds':len(x)/rate,'sample_rate':rate,'source':frozen,'channels':results})

if __name__=='__main__':main()
