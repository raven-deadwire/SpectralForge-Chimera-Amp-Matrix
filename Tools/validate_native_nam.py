#!/usr/bin/env python3
"""Compare actual native amp paths with local NAM captures; never distributes weights.
Fit broad output contours on deterministic plucks; reserve chords for validation.
Metrics are capture comparisons, not hardware-fidelity or listening acceptance.
"""
import argparse, concurrent.futures, hashlib, json, subprocess
from pathlib import Path
import numpy as np
from scipy import signal, optimize
from scipy.io import wavfile
from validate_nam import SR, read, fixture, biquad, contour, metrics

def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def run(cmd): subprocess.run([str(x) for x in cmd], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
def fit_many(pairs):
    spectra=[]
    for a,b in pairs:
        f,pa=signal.welch(a,SR,nperseg=8192);_,pb=signal.welch(b,SR,nperseg=8192)
        mask=(f>=65)&(f<=8000)&(pa>pa.max()*1e-5)
        spectra.append((f[mask],10*np.log10((pa[mask]+1e-20)/(pb[mask]+1e-20))))
    def residual(v):
        residuals=[]
        for i,(f,target) in enumerate(spectra):
            response=np.ones(len(f),complex)
            for kind,hz,db,q in [('low',100,v[0],.707),('peak',500,v[1],.65),('high',2000,v[2],.707)]:
                response*=signal.freqz(*biquad(kind,hz,db,q),worN=2*np.pi*f/SR)[1]
            # Each capture has an independent arbitrary output recording level.
            residuals.extend((20*np.log10(abs(response))+v[3+i]-target)/np.sqrt(len(f)))
        return np.r_[residuals,.025*v[:3]]
    return np.round(optimize.least_squares(residual,np.zeros(3+len(pairs)),bounds=([-6,-6,-6]+[-60]*len(pairs),[6,6,6]+[60]*len(pairs))).x[:3],2)
def main():
    p=argparse.ArgumentParser();p.add_argument('--models',type=Path,required=True);p.add_argument('--manifest',type=Path,required=True);p.add_argument('--nam-render',type=Path,required=True);p.add_argument('--baseline',type=Path,required=True);p.add_argument('--candidate',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--fit',action='store_true');p.add_argument('--workers',type=int,default=2);args=p.parse_args();args.out.mkdir(parents=True,exist_ok=True)
    manifest=json.loads(args.manifest.read_text());sources=manifest['amps'];rows=[];waves={}
    for kind in ['pluck','chord']:
        dry=fixture(kind=='chord');wavfile.write(args.out/f'{kind}.wav',SR,dry.astype('float32'));waves[kind]=dry
    def case(pair):
        i,item=pair;model=args.models/item['file'];assert digest(model)==item['sha256'],model
        data=json.loads(model.read_text());cal=data.get('metadata',{}).get('input_level_dbu');g=10**((11.5-cal)/20) if cal is not None else 1
        result={'case':i,'model':item['index'],'channel':item['channel'],'file':item['file'],'sha256':item['sha256'],'qualification':item['qualification'],'fit_eligible':item.get('fit_eligible',True),'state':item['state'],'input_gain_db':float(20*np.log10(g)),'absolute_input_calibration':cal is not None,'measurements':{}}
        state=args.out/f'{i}-state.json';state.write_text(json.dumps(item['state']))
        for kind,dry in waves.items():
            base=args.out/f'{i}-{kind}';inp=Path(str(base)+'-nam-input.wav');wavfile.write(inp,SR,(dry*g).astype('float32'));nam=Path(str(base)+'-nam.wav')
            # Cache only under the exact file and fixture hash, independent of DSP builds.
            stamp=Path(str(nam)+'.sha256');key=digest(model)+digest(inp)
            if not nam.exists() or not stamp.exists() or stamp.read_text()!=key:run([args.nam_render,'--slim','1',model,inp,nam]);stamp.write_text(key)
            for label,exe in [('baseline',args.baseline),('candidate',args.candidate)]:
                dest=Path(str(base)+'-'+label+'.wav');dest.unlink(missing_ok=True);run([exe,args.out/f'{kind}.wav',dest,item['index'],.5,state]);y=read(dest)
                if not np.isfinite(y).all() or np.max(abs(y))>64:raise ValueError(f'Invalid renderer: {dest}')
                result['measurements'].setdefault(kind,{})[label]={**metrics(read(nam),y),'candidate_peak_dbfs':float(20*np.log10(np.max(abs(y))+1e-20))}
        print('rendered',i,item['index'],item['channel'],flush=True);return result
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers)as pool:
        for row in pool.map(case,enumerate(sources)):rows.append(row)
    if args.fit:
        contours=[]
        groups=sorted(set((r['model'],r['channel'])for r in rows))
        for model,ch in groups:
            cases=[r for r in rows if r['model']==model and r['channel']==ch and r['fit_eligible']]
            if not cases:continue
            pairs=[(read(args.out/f'{r["case"]}-pluck-nam.wav'),read(args.out/f'{r["case"]}-pluck-candidate.wav'))for r in cases]
            gains=fit_many(pairs);ratios=[np.sqrt(np.sum(read(args.out/f'{r["case"]}-pluck-baseline.wav')**2)/np.sum(contour(b,gains)**2))for r,(a,b) in zip(cases,pairs)];trim=float(np.round(20*np.log10(np.exp(np.mean(np.log(ratios)))),2))
            errors=[]
            for r in cases:
                for kind in waves:
                    base=args.out/f'{r["case"]}-{kind}';a=read(str(base)+'-nam.wav');b=read(str(base)+'-candidate.wav');r['measurements'][kind]['proposed']=metrics(a,contour(b,gains))
                errors.append((r['measurements']['chord']['candidate']['log_spectrum_rms_db'],r['measurements']['chord']['proposed']['log_spectrum_rms_db']))
            before,after=np.mean(errors,axis=0);accepted=bool(after<before*.95)
            contours.append({'model':model,'channel':ch,'gains_db':gains.tolist(),'level_db':trim,'heldout_before_db':float(before),'heldout_after_db':float(after),'accepted':accepted})
        (args.out/'contours.json').write_text(json.dumps(contours,indent=2)+'\n')
    report={'manifest_sha256':digest(args.manifest),'nam_core_commit':manifest['nam_core_commit'],'baseline_binary_sha256':digest(args.baseline),'candidate_binary_sha256':digest(args.candidate),'renderer':'actual Amp native path, 4x oversampling, no IR','fixture':'validate_nam.fixture; pluck training / held-out chord; 11.5 dBu common input when calibration known','unavailable':manifest['unavailable'],'excluded':manifest['excluded'],'cases':rows}
    (args.out/'metrics.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
