#!/usr/bin/env python3
"""Offline, repeatable NAM comparisons. Requires numpy/scipy and user-owned files.
No download, authentication, neural weights, or IR redistribution is performed.
"""
import argparse, hashlib, json, subprocess
from pathlib import Path
import numpy as np
from scipy import signal, optimize
from scipy.io import wavfile
SR = 48000

def read(path):
    rate,x=wavfile.read(path)
    if rate != SR: raise ValueError(f'Expected {SR} Hz: {path}')
    return x.astype(np.float64)/(float(2**(x.dtype.itemsize*8-1)) if x.dtype.kind=='i' else 1.)

def fixture(chords=False):
    t=np.arange(SR*8)/SR;x=np.zeros_like(t)
    for i,f in enumerate([41.2034,65.4064,98,130.813,174.614,261.626,392] if chords else [55,82.4069,110,146.832,196,246.942,329.628]):
        a=t-(.5+i);mask=(a>=0)&(a<.9);v=a[mask]
        fs=[f,f*1.5,f*2] if chords else [f]
        note=sum(sum(np.sin(2*np.pi*hz*k*v+.13*k)*np.exp(-v*(2+k*.32))/k**1.3 for k in range(1,18)) for hz in fs)/len(fs)
        x[mask]+=note*([.055,.18,.35][i%3] if chords else [.03,.1,.3][i%3])*np.minimum(v/.002,1)
    return x

def biquad(kind,hz,db,q=.707):
    a=10**(db/40);w=2*np.pi*hz/SR;c=np.cos(w);alpha=np.sin(w)/(2*q);beta=2*np.sqrt(a)*alpha
    if kind=='peak':b=[1+alpha*a,-2*c,1-alpha*a];d=[1+alpha/a,-2*c,1-alpha/a]
    elif kind=='low':b=[a*((a+1)-(a-1)*c+beta),2*a*((a-1)-(a+1)*c),a*((a+1)-(a-1)*c-beta)];d=[(a+1)+(a-1)*c+beta,-2*((a-1)+(a+1)*c),(a+1)+(a-1)*c-beta]
    else:b=[a*((a+1)+(a-1)*c+beta),-2*a*((a-1)+(a+1)*c),a*((a+1)+(a-1)*c-beta)];d=[(a+1)-(a-1)*c+beta,2*((a-1)-(a+1)*c),(a+1)-(a-1)*c-beta]
    return np.array(b)/d[0],np.array(d)/d[0]

def spectrum(x):return signal.welch(x,SR,nperseg=8192)
def fit(reference,candidate):
    f,pa=spectrum(reference);_,pb=spectrum(candidate);mask=(f>=65)&(f<=8000)&(pa>pa.max()*1e-5);freq=f[mask];target=10*np.log10((pa[mask]+1e-20)/(pb[mask]+1e-20))
    def residual(v):
        response=np.ones(len(freq),dtype=complex)
        for kind,hz,db,q in [('low',100,v[0],.707),('peak',500,v[1],.65),('high',2000,v[2],.707)]:
            b,a=biquad(kind,hz,db,q);response*=signal.freqz(b,a,worN=2*np.pi*freq/SR)[1]
        return 20*np.log10(abs(response))+v[3]-target
    return optimize.least_squares(residual,[0,-4,4,0],bounds=([-9,-12,-6,-40],[9,6,12,40])).x[:3]

def contour(x,gains):
    for kind,hz,db,q in [('low',100,gains[0],.707),('peak',500,gains[1],.65),('high',2000,gains[2],.707)]:x=signal.lfilter(*biquad(kind,hz,db,q),x)
    return x

def metrics(a,b):
    if not np.isfinite(a).all() or not np.isfinite(b).all():raise ValueError('Non-finite render')
    gain=np.sqrt(np.sum(a*a)/max(np.sum(b*b),1e-20));matched=b*gain
    f,pa=spectrum(a);_,pb=spectrum(matched);mask=(f>=65)&(f<=8000)&(pa>pa.max()*1e-5)
    # Bound alignment to 128 samples; periodic notes can otherwise yield false
    # correlation peaks a complete note-cycle away. Spectral score is phase-free.
    corr=signal.correlate(a,matched,method='fft');mid=len(matched)-1;part=corr[mid-128:mid+129];lag=int(np.argmax(abs(part))-128)
    a2=a[max(lag,0):len(a)+min(lag,0)];b2=matched[max(-lag,0):len(matched)-max(lag,0)]
    return dict(reference_rms_dbfs=float(20*np.log10(np.sqrt(np.mean(a*a)))),match_gain_db=float(20*np.log10(gain)),lag_samples=lag,correlation=float(np.corrcoef(a2,b2)[0,1]),log_spectrum_rms_db=float(np.sqrt(np.mean((10*np.log10((pb[mask]+1e-20)/(pa[mask]+1e-20)))**2))))

def main():
    p=argparse.ArgumentParser();p.add_argument('--models',type=Path,required=True);p.add_argument('--manifest',type=Path,required=True);p.add_argument('--nam-render',type=Path,required=True);p.add_argument('--chimera-render',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--fit',action='store_true');p.add_argument('--ir',type=Path);p.add_argument('--bass-ir',type=Path);args=p.parse_args();args.out.mkdir(parents=True,exist_ok=True)
    rows=[]
    for item in json.loads(args.manifest.read_text())['amps']:
        model=args.models/item['file'];model_bytes=model.read_bytes();model_hash=hashlib.sha256(model_bytes).hexdigest()
        if item.get('sha256') and item['sha256'] != model_hash:raise ValueError(f"Reference hash mismatch: {item['file']}")
        data=json.loads(model_bytes);cal=data.get('metadata',{}).get('input_level_dbu');gain=10**((11.5-cal)/20) if cal is not None else 1
        case=dict(item,sha256=model_hash,input_level_dbu=cal,reference_input_gain_db=float(20*np.log10(gain)),calibration_status='metadata, common source 11.5 dBu' if cal is not None else 'unknown; same digital input only',measurements={});fitted=None
        for kind in ['pluck','chord']:
            base=args.out/f"{item['index']}-{kind}";dry=fixture(kind=='chord');wavfile.write(str(base)+'-di.wav',SR,dry.astype('float32'));wavfile.write(str(base)+'-calibrated.wav',SR,(dry*gain).astype('float32'))
            subprocess.run([str(args.nam_render.resolve()),'--slim','1',str(model.resolve()),str(base)+'-calibrated.wav',str(base)+'-nam.wav'],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
            subprocess.run([str(args.chimera_render.resolve()),str(base)+'-di.wav',str(base)+'-chimera.wav',str(item['index']),str(item.get('drive',.4))],check=True,stdout=subprocess.DEVNULL)
            a,b=read(str(base)+'-nam.wav'),read(str(base)+'-chimera.wav');result={'baseline':metrics(a,b)}
            if args.fit:
                if fitted is None:fitted=fit(a,b);case['proposed_contour_db']=list(np.round(fitted,2))
                result['proposed']=metrics(a,contour(b,fitted))
            if args.ir:
                ir=read(args.bass_ir if item.get('instrument', 'bass' if 5 <= item['index'] <= 7 else 'guitar') == 'bass' and args.bass_ir else args.ir);ir=ir[:,0] if ir.ndim>1 else ir;ir/=np.max(abs(ir));aa=signal.fftconvolve(a,ir);bb=signal.fftconvolve(b,ir);result['same_ir']=metrics(aa,bb);bb*=np.sqrt(np.sum(aa*aa)/np.sum(bb*bb));scale=.9/max(np.max(abs(aa)),np.max(abs(bb)));wavfile.write(str(base)+'-A-NAM.wav',SR,(aa*scale).astype('float32'));wavfile.write(str(base)+'-B-Chimera-matched.wav',SR,(bb*scale).astype('float32'))
            case['measurements'][kind]=result
        rows.append(case);print(item['name'],case['measurements'],flush=True);(args.out/'metrics.json').write_text(json.dumps(rows,indent=2)+'\n')
if __name__=='__main__':main()
