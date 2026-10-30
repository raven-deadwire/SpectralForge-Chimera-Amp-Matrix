#!/usr/bin/env python3
"""The five Náströnd product channels, at their own defaults, for TONE3000 NAM A2.

Each capture selects a channel and keeps that channel's 13 default controls,
exactly what a player hears on selecting it. The controls are read back from
the renderer catalog and recorded, never re-typed here.

Keep the production path's whole latency: an oversampler's group delay must not
be cropped as though it were a pure delay (that can create a noncausal target).
"""
import argparse, concurrent.futures, json, subprocess, time
from pathlib import Path
import numpy as np
import soundfile as sf
from common import signal, digest, write_json, SOURCE

# Names stay ASCII: they are file names and quality-profile keys.
VOICES=[('Fenrir','Early bite, lean lows, fast firm recovery'),
 ('Surtr','Distributed compression, forward vocal mids'),
 ('Nidhoggr','Uneven drive, asymmetric recovery'),
 ('Fimbulvetr','Broad body, yielding supply, open upper mids'),
 ('Ragnarok','Late-stage saturation, deep hit, stiff recovery')]
# Niflheimr (bass). Each channel is captured at its factory rig voicing; the
# renderer reads it from NiflheimrPresets.h and reports it in its catalog.
NIFLHEIMR_VOICES=[('Hrimfaxi','Modern tight: controlled lows, clear string attack'),
 ('Garmr','Death grind: upper-band bite'),
 ('Nidavellir','Industrial bite: driven mids'),
 ('Ymir','Slam impact: mid punch'),
 ('Hel','Sludge mass: sustained doom fuzz')]
AMPS={'nastrond':{'voices':VOICES,'display':'Náströnd'},'niflheimr':{'voices':NIFLHEIMR_VOICES,'display':'Niflheimr'}}


def amp_of(source):
    """The amp a capture's source record names; captures from before Niflheimr are Náströnd."""
    return source.get('amp','nastrond')


def voices_for(data):
    """The channel list of a capture dataset directory."""
    return AMPS[amp_of(json.loads((Path(data)/'manifest.json').read_text())['source'])]['voices']


def channel_controls(catalog, channel):
    """The channel's default controls in renderer units, from the renderer itself."""
    row=catalog['channel_defaults'][channel]
    return {m['source_key']:row[i]*m['source_scale'] for i,m in enumerate(catalog['mapping'])}

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--renderer',type=Path,required=True);ap.add_argument('--out',type=Path,required=True)
    ap.add_argument('--amp',choices=sorted(AMPS),default='nastrond')
    ap.add_argument('--workers',type=int,default=4);ap.add_argument('--train-seconds',type=float,default=96)
    ap.add_argument('--train-seed',type=int,default=640500,help='Training-split seed only; held-out splits keep their fixed seeds')
    ap.add_argument('--smoke-seconds',type=float,help='Shorten all splits for CI only; never a fidelity qualification');args=ap.parse_args()
    if args.smoke_seconds is not None and args.smoke_seconds < 2:raise ValueError('Smoke splits must be at least two seconds')
    if args.out.exists() and any(args.out.iterdir()):raise RuntimeError('Output directory must be empty')
    args.out.mkdir(parents=True,exist_ok=True);catalog=json.loads(subprocess.check_output([str(args.renderer),'--catalog']+([args.amp] if args.amp!='nastrond' else []),text=True))
    voices=AMPS[args.amp]['voices']
    if len(catalog['channel_defaults'])!=len(voices):raise RuntimeError('Renderer channel count differs from the voice list')
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
      'input_trim_db':0,'output_trim_db':0,'cabinet':False,'pre_post_effects':False,'input_kind':'synthetic; not recorded instrument DI','train_seed':args.train_seed}
    if args.amp!='nastrond':
        # Náströnd source records stay as they were, so its warm starts still match.
        voicing='Source/NiflheimrPresets.h'
        source.update(amp=args.amp,voicing_source={voicing:digest(source_root/voicing)},voicing=catalog.get('channel_default_source'))
    jobs=[]
    for i,(split,seconds) in enumerate([('train',args.train_seconds),('validation',24),('test',24),('audition',8)]):
        if args.smoke_seconds is not None:seconds=args.smoke_seconds
        d=args.out/split;d.mkdir();x=signal(args.train_seed if split=='train' else 640500+i*1703,seconds)
        np.save(d/'input.npy',x);x.tofile(d/'input.f32');sf.write(d/'input.wav',x,48000,subtype='FLOAT')
        for ch,(name,character) in enumerate(voices):
            job={'channel':ch}
            if args.amp!='nastrond':job['amp']=args.amp
            write_json(d/(name+'.json'),job)
            jobs.append((split,name,character,job,d,len(x)))
    def render(item):
        split,name,character,job,d,n=item;raw=d/(name+'.f32')
        info=json.loads(subprocess.check_output([str(args.renderer),str(d/'input.f32'),str(raw),str(d/(name+'.json'))],text=True))
        y=np.fromfile(raw,dtype='<f4')
        if len(y)!=n or not np.isfinite(y).all():raise RuntimeError('Invalid native capture')
        np.save(d/(name+'.npy'),y);raw.unlink()
        return {'split':split,'name':name,'character':character,'channel':job['channel'],'controls':channel_controls(catalog,job['channel']),
          'input':str((d/'input.npy').relative_to(args.out)),'output':str((d/(name+'.npy')).relative_to(args.out)),
          'input_sha256':digest(d/'input.npy'),'output_sha256':digest(d/(name+'.npy')),'render':info}
    t=time.monotonic()
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as pool:
        captures=[]
        for result in pool.map(render,jobs):
            captures.append(result);print(result['split'],result['name'],'captured',round(time.monotonic()-t,1),flush=True)
    write_json(args.out/'manifest.json',{'schema':1,'source':source,'catalog':catalog,'captures':captures,'elapsed_seconds':time.monotonic()-t})
    write_json(args.out/'channel-settings.json',[{'name':n,'character':c,'channel':i,'controls':channel_controls(catalog,i)} for i,(n,c) in enumerate(voices)])

if __name__=='__main__':main()
