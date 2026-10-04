#!/usr/bin/env python3
"""Multi-level response audit of actual native DSP and local NAM references.
No neural weights are bundled. Sine probes measure compression and harmonics;
these measurements are not a substitute for a real guitar/bass DI listening test.
"""
import argparse, concurrent.futures, hashlib, json, subprocess
from pathlib import Path
import numpy as np
from scipy.io import wavfile
from validate_nam import SR, read
LEVELS=[-48,-36,-24,-12]
FREQUENCIES=[100,400,1200]
N=SR//2

def fixture():
    t=np.arange(N)/SR
    return np.concatenate([10**(db/20)*np.sin(2*np.pi*f*t)*np.minimum(t/.01,1) for f in FREQUENCIES for db in LEVELS])
def response(y):
    if len(y)!=N*len(LEVELS)*len(FREQUENCIES) or not np.isfinite(y).all():raise ValueError('Invalid response render')
    result=[]
    for j,f in enumerate(FREQUENCIES):
        rows=[]
        for i,db in enumerate(LEVELS):
            v=y[(j*4+i)*N+N//2:(j*4+i+1)*N];sp=abs(np.fft.rfft(v));hs=sp[[int(f*k*.25)for k in range(1,13)if f*k<SR/2]]
            rows.append({'input_peak_dbfs':db,'rms_dbfs':float(20*np.log10(np.sqrt(np.mean(v*v))+1e-15)),'thd':float(np.sqrt(sum(hs[1:]**2))/(hs[0]+1e-15))})
        result.append({'hz':f,'levels':rows})
    return result
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(cmd):subprocess.run(list(map(str,cmd)),check=True,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
def main():
    p=argparse.ArgumentParser();p.add_argument('--models',type=Path,required=True);p.add_argument('--manifest',type=Path,required=True);p.add_argument('--nam-render',type=Path,required=True);p.add_argument('--baseline',type=Path,required=True);p.add_argument('--candidate',type=Path,required=True);p.add_argument('--out',type=Path,required=True);args=p.parse_args();args.out.mkdir(parents=True,exist_ok=True)
    m=json.loads(args.manifest.read_text());x=fixture();dry=args.out/'input.wav';wavfile.write(dry,SR,x.astype('float32'))
    def native(model,state,stem,exe):
        cfg=args.out/f'{stem}.json';cfg.write_text(json.dumps(state));out=args.out/f'{stem}.wav';out.unlink(missing_ok=True)
        run([exe,dry,out,model,.5,cfg]);return response(read(out))
    def compare(pair):
        i,r=pair;model=args.models/r['file'];assert sha(model)==r['sha256'];cal=r['input_level_dbu'];g=10**((11.5-cal)/20)if cal is not None else 1
        inp=args.out/f'{i}-input.wav';out=args.out/f'{i}-nam.wav';wavfile.write(inp,SR,(x*g).astype('float32'));key=sha(model)+sha(inp);stamp=Path(str(out)+'.sha256')
        if not out.exists() or not stamp.exists() or stamp.read_text()!=key:run([args.nam_render,'--slim',1,model,inp,out]);stamp.write_text(key)
        row={'case':i,'model':r['index'],'channel':r['channel'],'file':r['file'],'state':r['state'],'sha256':r['sha256'],'input_calibration_known':cal is not None,'nam':response(read(out))}
        for label,exe in [('baseline',args.baseline),('candidate',args.candidate)]:row[label]=native(r['index'],r['state'],f'{i}-{label}',exe)
        print('GAIN',i,r['index'],r['channel'],flush=True);return row
    with concurrent.futures.ThreadPoolExecutor(max_workers=2)as pool:rows=list(pool.map(compare,enumerate(m['amps'])))
    # All active defaults, including the three models without exact NAM files.
    defaults=[]
    for model in range(24):
        if model==16:continue
        defaults.append({'model':model,'baseline':native(model,{},f'default-{model}-baseline',args.baseline),'candidate':native(model,{},f'default-{model}-candidate',args.candidate)})
    result={'manifest_sha256':sha(args.manifest),'nam_core_commit':m['nam_core_commit'],'baseline_binary_sha256':sha(args.baseline),'candidate_binary_sha256':sha(args.candidate),'input_reference_dbu':11.5,'frequencies_hz':FREQUENCIES,'levels_peak_dbfs':LEVELS,'measurement':'last 250 ms of each 500 ms sine, harmonics 2-12 / fundamental; amplitude curves are not loudness matched','cases':rows,'active_defaults':defaults,'unavailable':m['unavailable'],'excluded':m['excluded']}
    (args.out/'metrics.json').write_text(json.dumps(result,indent=2)+'\n')
if __name__=='__main__':main()
