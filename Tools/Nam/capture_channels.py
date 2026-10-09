#!/usr/bin/env python3
"""Five character-focused, fixed Náströnd channels for TONE3000 NAM A2.

Keep the production path's whole latency: an oversampler's group delay must not
be cropped as though it were a pure delay (that can create a noncausal target).
"""
import argparse, concurrent.futures, json, subprocess, time
from pathlib import Path
import numpy as np
import soundfile as sf
from common import signal, digest, write_json, SOURCE

KEYS=['gain','bass','middle','treble','mid_frequency','presence','depth','master','clank','crush','impact','rot','bloom']
VOICES=[
 ('Fenrir','Tight attack / clank',[.56,.40,.55,.60,1050,.62,.55,.50,.95,.48,.78,.15,.12]),
 ('Surtr','Dense saturation / vocal mids',[.68,.48,.75,.52,1050,.52,.50,.50,.50,.95,.48,.35,.55]),
 ('Nidhoggr','Asymmetric grind / rot',[.64,.43,.50,.57,800,.60,.52,.50,.50,.60,.50,1.,.58]),
 ('Fimbulvetr','Broad bloom / clear upper mids',[.52,.40,.65,.72,1350,.72,.48,.50,.35,.42,.50,.40,.95]),
 ('Ragnarok','Deep impact / stiff punch',[.66,.60,.42,.55,650,.56,.86,.50,.74,.82,1.,.58,.32])]

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--renderer',type=Path,required=True);ap.add_argument('--out',type=Path,required=True)
    ap.add_argument('--workers',type=int,default=4);ap.add_argument('--train-seconds',type=float,default=96)
    ap.add_argument('--smoke-seconds',type=float,help='Shorten all splits for CI only; never a fidelity qualification');args=ap.parse_args()
    if args.smoke_seconds is not None and args.smoke_seconds < 2:raise ValueError('Smoke splits must be at least two seconds')
    if args.out.exists() and any(args.out.iterdir()):raise RuntimeError('Output directory must be empty')
    args.out.mkdir(parents=True,exist_ok=True);catalog=json.loads(subprocess.check_output([str(args.renderer),'--catalog'],text=True))
    source_root=Path(__file__).resolve().parents[2];headers=[];pending=['Source/Amplifier.h']
    import re
    while pending:
        p=pending.pop()
        if p in headers:continue
        headers.append(p)
        raw=(source_root/p).read_bytes()
        frozen=subprocess.check_output(['git','show',f'{SOURCE}:{p}'],cwd=source_root)
        if raw!=frozen:raise RuntimeError('DSP differs from frozen source: '+p)
        for inc in re.findall(r'^#include "([^"]+)"',raw.decode(),re.M):
            child=str(Path(p).parent/inc)
            if (source_root/child).is_file():pending.append(child)
    source={'source_commit':SOURCE,'source_hashes':{p:digest(source_root/p) for p in headers},'renderer_sha256':digest(args.renderer),
      'sample_rate':48000,'oversampling':4,'block_size':256,'alignment':'unshifted native output, including production latency',
      'input_trim_db':0,'output_trim_db':0,'cabinet':False,'pre_post_effects':False,'input_kind':'synthetic; not recorded instrument DI'}
    jobs=[]
    for i,(split,seconds) in enumerate([('train',args.train_seconds),('validation',24),('test',24),('audition',8)]):
        if args.smoke_seconds is not None:seconds=args.smoke_seconds
        d=args.out/split;d.mkdir();x=signal(640500+i*1703,seconds)
        np.save(d/'input.npy',x);x.tofile(d/'input.f32');sf.write(d/'input.wav',x,48000,subtype='FLOAT')
        for ch,(name,character,values) in enumerate(VOICES):
            job={'channel':ch,'controls':dict(zip(KEYS,values))};write_json(d/(name+'.json'),job)
            jobs.append((split,name,character,job,d,len(x)))
    def render(item):
        split,name,character,job,d,n=item;raw=d/(name+'.f32')
        info=json.loads(subprocess.check_output([str(args.renderer),str(d/'input.f32'),str(raw),str(d/(name+'.json'))],text=True))
        y=np.fromfile(raw,dtype='<f4')
        if len(y)!=n or not np.isfinite(y).all():raise RuntimeError('Invalid native capture')
        np.save(d/(name+'.npy'),y);raw.unlink()
        return {'split':split,'name':name,'character':character,'channel':job['channel'],'controls':job['controls'],
          'input':str((d/'input.npy').relative_to(args.out)),'output':str((d/(name+'.npy')).relative_to(args.out)),
          'input_sha256':digest(d/'input.npy'),'output_sha256':digest(d/(name+'.npy')),'render':info}
    t=time.monotonic()
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as pool:
        captures=[]
        for result in pool.map(render,jobs):
            captures.append(result);print(result['split'],result['name'],'captured',round(time.monotonic()-t,1),flush=True)
    write_json(args.out/'manifest.json',{'schema':1,'source':source,'catalog':catalog,'captures':captures,'elapsed_seconds':time.monotonic()-t})
    write_json(args.out/'channel-settings.json',[{'name':n,'character':c,'channel':i,'controls':dict(zip(KEYS,v))} for i,(n,c,v) in enumerate(VOICES)])

if __name__=='__main__':main()
